// Part document: feature tree, expressions, rebuild (docs/modeling/PART_DOCUMENT.md).

#include "part_document_internal.hpp"

#include <bendcore/formula.hpp>
#include <bendcore/kernel/shape.hpp>

#include <Standard_Failure.hxx>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <set>
#include <stdexcept>

namespace bendcore::modeling {

namespace {

Status fail(ErrorCode code, std::string message) { return Status::failure(code, std::move(message)); }

bool isIdentifier(const std::string& name) {
    if (name.empty()) return false;
    auto alpha = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
    if (!alpha(name[0])) return false;
    for (char c : name) {
        if (!alpha(c) && !(c >= '0' && c <= '9')) return false;
    }
    return true;
}

// Variable names used by an expression ("$x" and "x" both refer to variable x).
void collectVariables(const formula::ASTNodePtr& node, std::set<std::string>& out) {
    if (!node) return;
    switch (node->type) {
        case formula::ASTNode::Variable: {
            std::string n = node->variable_name;
            if (!n.empty() && n[0] == '$') n.erase(0, 1);
            out.insert(n);
            break;
        }
        case formula::ASTNode::UnaryOpNode: collectVariables(node->unary_operand, out); break;
        case formula::ASTNode::BinaryOpNode:
            collectVariables(node->left_operand, out);
            collectVariables(node->right_operand, out);
            break;
        case formula::ASTNode::FunctionCall:
            for (const auto& a : node->function_args) collectVariables(a, out);
            break;
        default: break;
    }
}

Result<formula::ASTNodePtr> parseExpression(const std::string& text) {
    try {
        auto ast = formula::FormulaParser::parse(text);
        if (!ast) return Result<formula::ASTNodePtr>::failure(ErrorCode::InvalidInput, "empty expression");
        return ast;
    } catch (const std::exception& e) {
        return Result<formula::ASTNodePtr>::failure(ErrorCode::InvalidInput,
                                                    "cannot parse '" + text + "': " + e.what());
    }
}

Result<double> evaluateWith(const std::string& text, const std::map<std::string, Result<double>>& vars) {
    auto ast = parseExpression(text);
    if (!ast) return Result<double>::failure(ast.error().code, ast.error().message);
    std::set<std::string> used;
    collectVariables(ast.value(), used);
    formula::EvaluationContext ctx;
    for (const auto& name : used) {
        auto it = vars.find(name);
        if (it == vars.end()) return Result<double>::failure(ErrorCode::InvalidInput, "unknown variable '" + name + "'");
        if (!it->second) {
            return Result<double>::failure(ErrorCode::InvalidInput,
                                           "variable '" + name + "': " + it->second.error().message);
        }
        ctx.variables[name] = it->second.value();
        ctx.variables["$" + name] = it->second.value();
    }
    try {
        double v = formula::FormulaEvaluator::evaluate(ast.value(), ctx);
        if (!std::isfinite(v)) {
            return Result<double>::failure(ErrorCode::InvalidInput, "'" + text + "' is not finite");
        }
        return v;
    } catch (const std::exception& e) {
        return Result<double>::failure(ErrorCode::InvalidInput, "cannot evaluate '" + text + "': " + e.what());
    }
}

}  // namespace

void Hasher::add(double v) {
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(v));
    std::memcpy(&bits, &v, sizeof(v));
    add(bits);
}

void Hasher::add(std::uint64_t v) {
    char buf[8];
    for (int i = 0; i < 8; ++i) buf[i] = static_cast<char>((v >> (8 * i)) & 0xff);
    add(std::string_view(buf, 8));
}

const char* toString(FeatureState state) noexcept {
    switch (state) {
        case FeatureState::Ok: return "ok";
        case FeatureState::Warning: return "warning";
        case FeatureState::Error: return "error";
        case FeatureState::Suppressed: return "suppressed";
        case FeatureState::RolledBack: return "rolled-back";
    }
    return "unknown";
}

std::optional<FeatureState> featureStateFromString(std::string_view text) noexcept {
    for (FeatureState s : {FeatureState::Ok, FeatureState::Warning, FeatureState::Error, FeatureState::Suppressed,
                           FeatureState::RolledBack}) {
        if (text == toString(s)) return s;
    }
    return std::nullopt;
}

// ---------------------------------------------------------------- registry

Status FeatureRegistry::add(const std::string& type, Factory factory) {
    if (type.empty() || !factory) return fail(ErrorCode::InvalidInput, "feature type needs a name and a factory");
    if (!factories_.emplace(type, std::move(factory)).second) {
        return fail(ErrorCode::InvalidInput, "feature type '" + type + "' is already registered");
    }
    return std::monostate{};
}

std::unique_ptr<Feature> FeatureRegistry::create(const std::string& type) const {
    auto it = factories_.find(type);
    if (it == factories_.end()) return nullptr;
    return it->second();
}

std::vector<std::string> FeatureRegistry::types() const {
    std::vector<std::string> out;
    for (const auto& [t, f] : factories_) out.push_back(t);
    return out;
}

// ---------------------------------------------------------------- document

PartDocument::PartDocument(std::shared_ptr<const FeatureRegistry> registry) : registry_(std::move(registry)) {
    if (!registry_) throw std::invalid_argument("PartDocument: registry is null");
}
PartDocument::PartDocument(PartDocument&&) noexcept = default;
PartDocument& PartDocument::operator=(PartDocument&&) noexcept = default;
PartDocument::~PartDocument() = default;

PartDocument::Node* PartDocument::node(const std::string& id) {
    for (auto& n : nodes_) {
        if (n->id == id) return n.get();
    }
    return nullptr;
}

const PartDocument::Node* PartDocument::node(const std::string& id) const {
    for (const auto& n : nodes_) {
        if (n->id == id) return n.get();
    }
    return nullptr;
}

Status PartDocument::checkExpression(const std::string& expression) const {
    auto ast = parseExpression(expression);
    if (!ast) return fail(ast.error().code, ast.error().message);
    return std::monostate{};
}

Status PartDocument::setVariable(const std::string& name, const std::string& expression) {
    if (!isIdentifier(name)) return fail(ErrorCode::InvalidInput, "invalid variable name '" + name + "'");
    auto st = checkExpression(expression);
    if (!st) return st;
    for (auto& [n, e] : variables_) {
        if (n == name) {
            e = expression;
            return std::monostate{};
        }
    }
    variables_.emplace_back(name, expression);
    return std::monostate{};
}

Status PartDocument::removeVariable(const std::string& name) {
    auto it = std::find_if(variables_.begin(), variables_.end(), [&](const auto& v) { return v.first == name; });
    if (it == variables_.end()) return fail(ErrorCode::InvalidInput, "no variable '" + name + "'");
    variables_.erase(it);
    return std::monostate{};
}

std::vector<std::pair<std::string, std::string>> PartDocument::variables() const { return variables_; }

Result<double> PartDocument::variableValue(const std::string& name) const {
    auto it = variableValues_.find(name);
    if (it == variableValues_.end()) {
        return Result<double>::failure(ErrorCode::InvalidInput, "variable '" + name + "' is not evaluated");
    }
    return it->second;
}

Result<std::string> PartDocument::addFeature(FeatureSpec spec, std::optional<std::size_t> position) {
    auto feature = registry_->create(spec.type);
    if (!feature) return Result<std::string>::failure(ErrorCode::Unsupported, "unknown feature type '" + spec.type + "'");
    auto n = std::make_unique<Node>();
    if (spec.id.empty()) {
        for (int k = 1;; ++k) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%03d", k);
            std::string candidate = spec.type + buf;
            if (!node(candidate)) {
                spec.id = candidate;
                break;
            }
        }
    } else if (node(spec.id)) {
        return Result<std::string>::failure(ErrorCode::InvalidInput, "feature id '" + spec.id + "' already exists");
    }
    if (!isIdentifier(spec.id)) {
        return Result<std::string>::failure(ErrorCode::InvalidInput, "invalid feature id '" + spec.id + "'");
    }
    for (const auto& p : feature->parameterSpecs()) n->parameters[p.name] = p.defaultExpression;
    for (const auto& [name, expr] : spec.parameters) {
        if (n->parameters.count(name) == 0) {
            return Result<std::string>::failure(ErrorCode::InvalidInput,
                                                spec.type + " has no parameter '" + name + "'");
        }
        auto st = checkExpression(expr);
        if (!st) return Result<std::string>::failure(st.error().code, name + ": " + st.error().message);
        n->parameters[name] = expr;
    }
    for (const auto& o : feature->optionSpecs()) n->options[o.name] = o.allowed.front();
    for (const auto& [name, value] : spec.options) {
        const auto specs = feature->optionSpecs();
        auto it = std::find_if(specs.begin(), specs.end(), [&](const OptionSpec& o) { return o.name == name; });
        if (it == specs.end() || std::find(it->allowed.begin(), it->allowed.end(), value) == it->allowed.end()) {
            return Result<std::string>::failure(ErrorCode::InvalidInput,
                                                "invalid option " + name + "='" + value + "' for " + spec.type);
        }
        n->options[name] = value;
    }
    for (const auto& slot : feature->referenceSlots()) n->references[slot.name] = {};
    for (auto& [slot, refs] : spec.references) {
        if (n->references.count(slot) == 0) {
            return Result<std::string>::failure(ErrorCode::InvalidInput, spec.type + " has no reference slot '" + slot + "'");
        }
        n->references[slot] = std::move(refs);
    }
    n->id = spec.id;
    n->type = spec.type;
    n->label = spec.label.empty() ? spec.id : spec.label;
    n->suppressed = spec.suppressed;
    n->feature = std::move(feature);

    std::size_t at = nodes_.size();
    if (position) {
        at = std::min(*position, nodes_.size());
    } else if (rollback_) {
        at = std::min(*rollback_, nodes_.size());
    }
    if (rollback_ && at <= *rollback_) ++*rollback_;
    std::string id = n->id;
    nodes_.insert(nodes_.begin() + static_cast<std::ptrdiff_t>(at), std::move(n));
    return id;
}

Status PartDocument::removeFeature(const std::string& id) {
    auto idx = indexOf(id);
    if (!idx) return fail(ErrorCode::InvalidInput, "no feature '" + id + "'");
    nodes_.erase(nodes_.begin() + static_cast<std::ptrdiff_t>(*idx));
    if (rollback_ && *idx < *rollback_) --*rollback_;
    return std::monostate{};
}

Status PartDocument::moveFeature(const std::string& id, std::size_t newIndex) {
    auto idx = indexOf(id);
    if (!idx) return fail(ErrorCode::InvalidInput, "no feature '" + id + "'");
    if (newIndex >= nodes_.size()) {
        return fail(ErrorCode::InvalidInput, "index " + std::to_string(newIndex) + " is out of range");
    }
    auto n = std::move(nodes_[*idx]);
    nodes_.erase(nodes_.begin() + static_cast<std::ptrdiff_t>(*idx));
    nodes_.insert(nodes_.begin() + static_cast<std::ptrdiff_t>(newIndex), std::move(n));
    return std::monostate{};
}

Status PartDocument::setSuppressed(const std::string& id, bool suppressed) {
    Node* n = node(id);
    if (!n) return fail(ErrorCode::InvalidInput, "no feature '" + id + "'");
    n->suppressed = suppressed;
    return std::monostate{};
}

Status PartDocument::setParameter(const std::string& id, const std::string& name, const std::string& expression) {
    Node* n = node(id);
    if (!n) return fail(ErrorCode::InvalidInput, "no feature '" + id + "'");
    if (n->parameters.count(name) == 0) return fail(ErrorCode::InvalidInput, n->type + " has no parameter '" + name + "'");
    auto st = checkExpression(expression);
    if (!st) return st;
    n->parameters[name] = expression;
    return std::monostate{};
}

Status PartDocument::setReferences(const std::string& id, const std::string& slot, std::vector<TopoReference> refs) {
    Node* n = node(id);
    if (!n) return fail(ErrorCode::InvalidInput, "no feature '" + id + "'");
    if (n->references.count(slot) == 0) return fail(ErrorCode::InvalidInput, n->type + " has no reference slot '" + slot + "'");
    n->references[slot] = std::move(refs);
    return std::monostate{};
}

Status PartDocument::setOption(const std::string& id, const std::string& name, const std::string& value) {
    Node* n = node(id);
    if (!n) return fail(ErrorCode::InvalidInput, "no feature '" + id + "'");
    for (const auto& o : n->feature->optionSpecs()) {
        if (o.name == name && std::find(o.allowed.begin(), o.allowed.end(), value) != o.allowed.end()) {
            n->options[name] = value;
            return std::monostate{};
        }
    }
    return fail(ErrorCode::InvalidInput, "invalid option " + name + "='" + value + "' for " + n->type);
}

Status PartDocument::setRollback(std::optional<std::size_t> position) {
    if (position && *position > nodes_.size()) {
        return fail(ErrorCode::InvalidInput, "rollback position " + std::to_string(*position) + " is out of range");
    }
    rollback_ = position;
    return std::monostate{};
}

std::size_t PartDocument::size() const noexcept { return nodes_.size(); }

std::vector<std::string> PartDocument::featureIds() const {
    std::vector<std::string> ids;
    for (const auto& n : nodes_) ids.push_back(n->id);
    return ids;
}

std::optional<std::size_t> PartDocument::indexOf(const std::string& id) const {
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i]->id == id) return i;
    }
    return std::nullopt;
}

Result<FeatureInfo> PartDocument::feature(const std::string& id) const {
    const Node* n = node(id);
    if (!n) return Result<FeatureInfo>::failure(ErrorCode::InvalidInput, "no feature '" + id + "'");
    FeatureInfo info;
    info.id = n->id;
    info.type = n->type;
    info.label = n->label;
    info.suppressed = n->suppressed;
    info.state = n->state;
    info.messages = n->messages;
    info.parameters = n->parameters;
    info.values = n->values;
    info.references = n->references;
    info.options = n->options;
    info.referenceReports = n->reports;
    info.output = n->output;
    return info;
}

Result<NamedShape> PartDocument::inputOf(const std::string& id) const {
    auto idx = indexOf(id);
    if (!idx) return Result<NamedShape>::failure(ErrorCode::InvalidInput, "no feature '" + id + "'");
    for (std::size_t i = *idx; i-- > 0;) {
        if (nodes_[i]->output) return *nodes_[i]->output;
    }
    return NamedShape{};
}

NamedShape PartDocument::finalShape() const {
    std::size_t end = rollback_ ? std::min(*rollback_, nodes_.size()) : nodes_.size();
    for (std::size_t i = end; i-- > 0;) {
        if (nodes_[i]->output) return *nodes_[i]->output;
    }
    return NamedShape{};
}

// ---------------------------------------------------------------- rebuild

namespace {

// Variables in dependency order; cycles and unknown names become errors of the variables involved.
std::map<std::string, Result<double>> evaluateVariables(const std::vector<std::pair<std::string, std::string>>& vars) {
    std::map<std::string, std::string> exprs(vars.begin(), vars.end());
    std::map<std::string, Result<double>> values;
    std::map<std::string, int> mark;  // 1 visiting, 2 done
    std::function<void(const std::string&, std::vector<std::string>&)> visit =
        [&](const std::string& name, std::vector<std::string>& stack) {
            if (mark[name] == 2) return;
            if (mark[name] == 1) {
                std::string cycle;
                auto from = std::find(stack.begin(), stack.end(), name);
                for (auto it = from; it != stack.end(); ++it) cycle += *it + " -> ";
                cycle += name;
                for (auto it = from; it != stack.end(); ++it) {
                    values.insert_or_assign(*it, Result<double>::failure(ErrorCode::InvalidInput, "cyclic dependency " + cycle));
                    mark[*it] = 2;
                }
                return;
            }
            mark[name] = 1;
            stack.push_back(name);
            auto ast = parseExpression(exprs[name]);
            if (ast) {
                std::set<std::string> used;
                collectVariables(ast.value(), used);
                for (const auto& u : used) {
                    if (exprs.count(u)) visit(u, stack);
                }
            }
            stack.pop_back();
            if (mark[name] == 2) return;  // marked by a cycle found below
            values.insert_or_assign(name, evaluateWith(exprs[name], values));
            mark[name] = 2;
        };
    for (const auto& [name, e] : vars) {
        std::vector<std::string> stack;
        visit(name, stack);
    }
    return values;
}

}  // namespace

RebuildReport PartDocument::rebuild() {
    RebuildReport report;
    variableValues_ = evaluateVariables(variables_);

    auto empty = std::make_shared<const NamedShape>();
    std::shared_ptr<const NamedShape> current = empty;
    std::uint64_t chain = 0x62656e64636f7265ULL;  // "bendcore"

    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        Node& n = *nodes_[i];
        if (rollback_ && i >= *rollback_) {
            n.state = FeatureState::RolledBack;
            n.messages.clear();
            n.reports.clear();
            n.output.reset();
            n.hash.reset();
            continue;
        }

        // Evaluated parameters.
        std::map<std::string, double> values;
        std::vector<std::string> paramErrors;
        for (const auto& [name, expr] : n.parameters) {
            auto v = evaluateWith(expr, variableValues_);
            if (v) {
                values[name] = v.value();
            } else {
                paramErrors.push_back("parameter '" + name + "' = '" + expr + "': " + v.error().message);
            }
        }

        auto inputHash = [&]() {
            Hasher h;
            h.add(chain);
            h.add(n.id);
            h.add(n.type);
            h.add(n.suppressed ? "suppressed" : "active");
            for (const auto& [name, v] : values) {
                h.add(name);
                h.add(v);
            }
            for (const auto& e : paramErrors) h.add(e);
            for (const auto& [name, v] : n.options) {
                h.add(name);
                h.add(v);
            }
            for (const auto& [slot, refs] : n.references) {
                h.add(slot);
                for (const auto& r : refs) {
                    h.add(toString(r.kind));
                    h.add(r.name);
                }
            }
            return h.value();
        };

        std::uint64_t key = inputHash();
        if (n.hash && *n.hash == key && n.state != FeatureState::RolledBack) {
            report.reused.push_back(n.id);
            if (n.state == FeatureState::Error) report.failed.push_back(n.id);
            if (n.output) current = n.output;
            chain = key;
            continue;
        }
        if (n.cache && n.cache->hash == key && !n.suppressed) {
            n.state = n.cache->state;
            n.messages = n.cache->messages;
            n.values = n.cache->values;
            n.reports = n.cache->reports;
            n.output = n.cache->output;
            n.hash = key;
            n.cache.reset();
            report.fromCache.push_back(n.id);
            if (n.output) current = n.output;
            chain = key;
            continue;
        }
        n.cache.reset();
        n.messages.clear();
        n.reports.clear();
        n.output.reset();
        n.values = values;

        if (n.suppressed) {
            n.state = FeatureState::Suppressed;
            n.hash = key;
            chain = key;
            continue;
        }
        report.recomputed.push_back(n.id);

        auto markError = [&](std::string message) {
            n.state = FeatureState::Error;
            n.messages.push_back(std::move(message));
            report.failed.push_back(n.id);
        };

        bool warning = false;
        if (!paramErrors.empty()) {
            for (auto& e : paramErrors) markError(e);
        } else {
            // References against the shape this feature sees.
            std::map<std::string, std::vector<TopoDS_Shape>> resolved;
            bool broken = false;
            for (const auto& slot : n.feature->referenceSlots()) {
                auto& refs = n.references[slot.name];
                auto& shapes = resolved[slot.name];
                if (!slot.multiple && refs.size() != 1) {
                    markError("reference slot '" + slot.name + "' needs exactly one " + toString(slot.kind) + ", has " +
                              std::to_string(refs.size()));
                    broken = true;
                    continue;
                }
                for (std::size_t k = 0; k < refs.size(); ++k) {
                    TopoReference& ref = refs[k];
                    ReferenceReport rep{slot.name, ref, {}};
                    if (ref.kind != slot.kind) {
                        rep.resolution.detail = std::string("reference is a ") + toString(ref.kind) + ", slot expects a " +
                                                toString(slot.kind);
                    } else if (current->isNull()) {
                        rep.resolution.detail = "there is no shape before this feature";
                    } else {
                        rep.resolution = resolveReference(ref, *current);
                    }
                    const std::string where = slot.name + "[" + std::to_string(k) + "] '" + ref.name + "'";
                    switch (rep.resolution.method) {
                        case ResolveMethod::Broken:
                            markError("broken reference " + where + ": " + rep.resolution.detail);
                            broken = true;
                            break;
                        case ResolveMethod::Signature:
                            warning = true;
                            n.messages.push_back("reference " + where + " resolved by geometry: " + rep.resolution.detail);
                            break;
                        case ResolveMethod::History:
                            n.messages.push_back("reference " + where + " resolved through history: " +
                                                 rep.resolution.detail);
                            break;
                        case ResolveMethod::Exact: break;
                    }
                    if (rep.resolution.method != ResolveMethod::Broken) {
                        for (std::size_t idx : rep.resolution.indices) shapes.push_back(current->element(ref.kind, idx));
                        // Follow the element: the stored signature tracks gradual edits; the name is
                        // replaced when the old one no longer designates a single element.
                        const std::size_t first = rep.resolution.indices.front();
                        ref.signature = current->record(ref.kind, first).signature;
                        if (rep.resolution.indices.size() == 1 && rep.resolution.method != ResolveMethod::Exact) {
                            ref.name = rep.resolution.names.front();
                        }
                    }
                    n.reports.push_back(std::move(rep));
                }
            }

            if (!broken) {
                FeatureContext ctx(n.id, *current, values, std::move(resolved), n.options);
                Result<FeatureOutput> out = Result<FeatureOutput>::failure(ErrorCode::OperationFailed, "not executed");
                try {
                    out = n.feature->execute(ctx);
                } catch (const Standard_Failure& f) {
                    out = Result<FeatureOutput>::failure(ErrorCode::OperationFailed,
                                                         std::string("OCCT exception: ") + f.what());
                } catch (const std::exception& e) {
                    out = Result<FeatureOutput>::failure(ErrorCode::OperationFailed, e.what());
                }
                if (!out) {
                    markError(out.error().message);
                } else {
                    kernel::Shape shape(out.value().shape);
                    auto validity = shape.checkValidity();
                    auto volume = shape.volumeProperties();
                    if (!validity.valid) {
                        std::string issues;
                        for (std::size_t q = 0; q < validity.issues.size() && q < 3; ++q) issues += " " + validity.issues[q] + ";";
                        markError("result is not a valid shape:" + issues);
                    } else if (!volume || !(volume.value().volume > 0)) {
                        markError("result is not a solid with positive volume");
                    } else {
                        NamingRequest req;
                        req.featureId = n.id;
                        req.result = out.value().shape;
                        req.history = out.value().history;
                        req.inputs = {current.get()};
                        req.roles = out.value().roles;
                        auto named = buildNamedShape(req);
                        if (!named) {
                            markError(named.error().message);
                        } else {
                            for (const auto& w : out.value().warnings) {
                                n.messages.push_back(w);
                                warning = true;
                            }
                            n.output = std::make_shared<const NamedShape>(std::move(named).value());
                            n.state = warning ? FeatureState::Warning : FeatureState::Ok;
                        }
                    }
                }
            }
        }
        // References may have been renamed: the stored hash describes the stored inputs.
        key = inputHash();
        n.hash = key;
        chain = key;
        if (n.output) current = n.output;
    }
    return report;
}

}  // namespace bendcore::modeling
