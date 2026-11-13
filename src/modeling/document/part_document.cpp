// Part document: feature tree, expressions, rebuild (docs/modeling/PART_DOCUMENT.md).

#include "part_document_internal.hpp"

#include "../features/feature_common.hpp"

#include <bendcore/formula.hpp>
#include <bendcore/kernel/shape.hpp>

#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <set>
#include <stdexcept>

namespace bendcore::modeling {

namespace fc = ::bendcore::modeling::detail;


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


PartDocument::PartDocument(std::shared_ptr<const FeatureRegistry> registry)
    : registry_(std::move(registry)), transactions_(std::make_unique<Transactions>()) {
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

CachedResult PartDocument::Node::snapshot() const {
    CachedResult r;
    r.hash = hash.value_or(0);
    r.state = state;
    r.messages = messages;
    r.values = values;
    r.reports = reports;
    r.output = output;
    r.tool = tool;
    r.toolMode = toolMode;
    r.toolNamed = toolNamed;
    r.sketch = sketchResult;
    r.datum = datum;
    return r;
}

void PartDocument::Node::apply(const CachedResult& r) {
    state = r.state;
    messages = r.messages;
    values = r.values;
    reports = r.reports;
    output = r.output;
    tool = r.tool;
    toolMode = r.toolMode;
    toolNamed = r.toolNamed;
    sketchResult = r.sketch;
    datum = r.datum;
    hash = r.hash;
}

void PartDocument::Node::clearResults() {
    messages.clear();
    reports.clear();
    output.reset();
    tool.Nullify();
    toolMode.reset();
    toolNamed.reset();
    sketchResult.reset();
    datum.reset();
    hash.reset();
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
    State before = captureState();
    bool found = false;
    for (auto& [n, e] : variables_) {
        if (n == name) {
            e = expression;
            found = true;
        }
    }
    if (!found) variables_.emplace_back(name, expression);
    recordEdit("set variable " + name, std::move(before));
    return std::monostate{};
}

Status PartDocument::removeVariable(const std::string& name) {
    auto it = std::find_if(variables_.begin(), variables_.end(), [&](const auto& v) { return v.first == name; });
    if (it == variables_.end()) return fail(ErrorCode::InvalidInput, "no variable '" + name + "'");
    State before = captureState();
    variables_.erase(it);
    recordEdit("remove variable " + name, std::move(before));
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

namespace {

// Structural check of sketch data: operand indices in range, expressions parse.
Status checkSketch(const SketchData& s) {
    const int np = static_cast<int>(s.points.size());
    const int ng = static_cast<int>(s.geometry.size());
    auto point = [&](int p, const std::string& where) -> Status {
        if (p < 0 || p >= np) return fail(ErrorCode::InvalidInput, where + ": no sketch point " + std::to_string(p));
        return std::monostate{};
    };
    auto entity = [&](int e, const std::string& where) -> Status {
        if (e < 1 || e > ng) return fail(ErrorCode::InvalidInput, where + ": no sketch entity " + std::to_string(e));
        return std::monostate{};
    };
    for (std::size_t k = 0; k < s.geometry.size(); ++k) {
        const auto& g = s.geometry[k];
        const std::string where = "sketch entity " + std::to_string(k + 1);
        Status st = std::monostate{};
        switch (g.type) {
            case SketchGeometryType::Line:
                if (!(st = point(g.start, where))) return st;
                if (!(st = point(g.end, where))) return st;
                if (g.start == g.end) return fail(ErrorCode::InvalidInput, where + ": line with equal end points");
                break;
            case SketchGeometryType::Circle:
                if (!(st = point(g.center, where))) return st;
                break;
            case SketchGeometryType::Arc:
                if (!(st = point(g.center, where))) return st;
                if (!(st = point(g.start, where))) return st;
                if (!(st = point(g.end, where))) return st;
                break;
            case SketchGeometryType::Ellipse:
                if (!(st = point(g.center, where))) return st;
                if (!(st = point(g.focus, where))) return st;
                break;
            case SketchGeometryType::EllipticalArc:
                if (!(st = point(g.center, where))) return st;
                if (!(st = point(g.focus, where))) return st;
                if (!(st = point(g.start, where))) return st;
                if (!(st = point(g.end, where))) return st;
                break;
            case SketchGeometryType::BSpline:
                if (g.poles.size() < 2) return fail(ErrorCode::InvalidInput, where + ": a B-spline needs at least 2 poles");
                for (int p : g.poles) {
                    if (!(st = point(p, where))) return st;
                }
                break;
        }
    }
    for (std::size_t k = 0; k < s.constraints.size(); ++k) {
        const auto& c = s.constraints[k];
        const std::string where = std::string("sketch constraint ") + std::to_string(k + 1) + " (" + toString(c.type) + ")";
        Status st = std::monostate{};
        switch (c.type) {
            case SketchConstraintType::Coincident:
            case SketchConstraintType::Horizontal:
            case SketchConstraintType::Vertical:
            case SketchConstraintType::Distance:
            case SketchConstraintType::DistanceX:
            case SketchConstraintType::DistanceY:
                if (!(st = point(c.a, where)) || !(st = point(c.b, where))) return st;
                break;
            case SketchConstraintType::Radius:
            case SketchConstraintType::Diameter:
            case SketchConstraintType::ArcLength:
            case SketchConstraintType::MajorRadius:
            case SketchConstraintType::MinorRadius:
                if (!(st = entity(c.a, where))) return st;
                break;
            case SketchConstraintType::Fix:
                if (!(st = point(c.a, where))) return st;
                break;
            case SketchConstraintType::SymmetricPoint:
            case SketchConstraintType::Rotation:
                if (!(st = point(c.a, where)) || !(st = point(c.b, where)) || !(st = point(c.c, where))) return st;
                break;
            case SketchConstraintType::Tangent:
            case SketchConstraintType::Equal:
            case SketchConstraintType::Concentric:
                if (!(st = entity(c.a, where)) || !(st = entity(c.b, where))) return st;
                break;
            case SketchConstraintType::Midpoint:
            case SketchConstraintType::PointOn:
            case SketchConstraintType::PointCurveDistance:
                if (!(st = point(c.a, where)) || !(st = entity(c.b, where))) return st;
                break;
            case SketchConstraintType::Parallel:
            case SketchConstraintType::Perpendicular:
            case SketchConstraintType::EqualLength:
            case SketchConstraintType::Angle:
            case SketchConstraintType::EqualRadius:
                if (!(st = entity(c.a, where)) || !(st = entity(c.b, where))) return st;
                break;
            case SketchConstraintType::PointOnLine:
            case SketchConstraintType::PointOnCircle:
                if (!(st = point(c.a, where)) || !(st = entity(c.b, where))) return st;
                break;
            case SketchConstraintType::Symmetric:
                if (!(st = point(c.a, where)) || !(st = point(c.b, where)) || !(st = entity(c.c, where))) return st;
                break;
        }
    }
    for (SketchEntityId r : s.regions) {
        if (r < 1 || static_cast<int>(r) > ng) {
            return fail(ErrorCode::InvalidInput, "region selection: no sketch entity " + std::to_string(r));
        }
    }
    for (const std::string* e : sketchExpressions(s)) {
        auto ast = parseExpression(*e);
        if (!ast) return fail(ast.error().code, "sketch: " + ast.error().message);
    }
    return std::monostate{};
}

}  // namespace

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
    const auto linkSlots = feature->linkSlots();
    for (const auto& [slot, target] : spec.links) {
        auto it = std::find_if(linkSlots.begin(), linkSlots.end(), [&](const LinkSlot& l) { return l.name == slot; });
        if (it == linkSlots.end()) {
            return Result<std::string>::failure(ErrorCode::InvalidInput, spec.type + " has no link slot '" + slot + "'");
        }
        if (!target.empty()) n->links[slot] = target;
    }
    if (feature->hasSketchData()) {
        n->sketch = spec.sketch ? std::move(*spec.sketch) : SketchData{};
        auto st = checkSketch(*n->sketch);
        if (!st) return Result<std::string>::failure(st.error().code, st.error().message);
    } else if (spec.sketch) {
        return Result<std::string>::failure(ErrorCode::InvalidInput, spec.type + " does not take sketch data");
    }
    n->id = spec.id;
    n->type = spec.type;
    n->label = spec.label.empty() ? spec.id : spec.label;
    n->suppressed = spec.suppressed;
    n->feature = std::move(feature);

    State before = captureState();
    std::size_t at = nodes_.size();
    if (position) {
        at = std::min(*position, nodes_.size());
    } else if (rollback_) {
        at = std::min(*rollback_, nodes_.size());
    }
    if (rollback_ && at <= *rollback_) ++*rollback_;
    std::string id = n->id;
    nodes_.insert(nodes_.begin() + static_cast<std::ptrdiff_t>(at), std::move(n));
    recordEdit("add " + id, std::move(before));
    return id;
}

Status PartDocument::removeFeature(const std::string& id) {
    auto idx = indexOf(id);
    if (!idx) return fail(ErrorCode::InvalidInput, "no feature '" + id + "'");
    State before = captureState();
    nodes_.erase(nodes_.begin() + static_cast<std::ptrdiff_t>(*idx));
    if (rollback_ && *idx < *rollback_) --*rollback_;
    recordEdit("remove " + id, std::move(before));
    return std::monostate{};
}

Status PartDocument::moveFeature(const std::string& id, std::size_t newIndex) {
    auto idx = indexOf(id);
    if (!idx) return fail(ErrorCode::InvalidInput, "no feature '" + id + "'");
    if (newIndex >= nodes_.size()) {
        return fail(ErrorCode::InvalidInput, "index " + std::to_string(newIndex) + " is out of range");
    }
    State before = captureState();
    auto n = std::move(nodes_[*idx]);
    nodes_.erase(nodes_.begin() + static_cast<std::ptrdiff_t>(*idx));
    nodes_.insert(nodes_.begin() + static_cast<std::ptrdiff_t>(newIndex), std::move(n));
    recordEdit("move " + id, std::move(before));
    return std::monostate{};
}

Status PartDocument::setSuppressed(const std::string& id, bool suppressed) {
    Node* n = node(id);
    if (!n) return fail(ErrorCode::InvalidInput, "no feature '" + id + "'");
    State before = captureState();
    n->suppressed = suppressed;
    recordEdit((suppressed ? "suppress " : "unsuppress ") + id, std::move(before));
    return std::monostate{};
}

Status PartDocument::setParameter(const std::string& id, const std::string& name, const std::string& expression) {
    Node* n = node(id);
    if (!n) return fail(ErrorCode::InvalidInput, "no feature '" + id + "'");
    if (n->parameters.count(name) == 0) return fail(ErrorCode::InvalidInput, n->type + " has no parameter '" + name + "'");
    auto st = checkExpression(expression);
    if (!st) return st;
    State before = captureState();
    n->parameters[name] = expression;
    recordEdit("set " + id + "." + name, std::move(before));
    return std::monostate{};
}

Status PartDocument::setReferences(const std::string& id, const std::string& slot, std::vector<TopoReference> refs) {
    Node* n = node(id);
    if (!n) return fail(ErrorCode::InvalidInput, "no feature '" + id + "'");
    if (n->references.count(slot) == 0) return fail(ErrorCode::InvalidInput, n->type + " has no reference slot '" + slot + "'");
    State before = captureState();
    n->references[slot] = std::move(refs);
    recordEdit("set references " + id + "." + slot, std::move(before));
    return std::monostate{};
}

Status PartDocument::setOption(const std::string& id, const std::string& name, const std::string& value) {
    Node* n = node(id);
    if (!n) return fail(ErrorCode::InvalidInput, "no feature '" + id + "'");
    for (const auto& o : n->feature->optionSpecs()) {
        if (o.name == name && std::find(o.allowed.begin(), o.allowed.end(), value) != o.allowed.end()) {
            State before = captureState();
            n->options[name] = value;
            recordEdit("set " + id + "." + name, std::move(before));
            return std::monostate{};
        }
    }
    return fail(ErrorCode::InvalidInput, "invalid option " + name + "='" + value + "' for " + n->type);
}

Status PartDocument::setLink(const std::string& id, const std::string& slot, const std::string& target) {
    Node* n = node(id);
    if (!n) return fail(ErrorCode::InvalidInput, "no feature '" + id + "'");
    const auto slots = n->feature->linkSlots();
    if (std::none_of(slots.begin(), slots.end(), [&](const LinkSlot& l) { return l.name == slot; })) {
        return fail(ErrorCode::InvalidInput, n->type + " has no link slot '" + slot + "'");
    }
    State before = captureState();
    if (target.empty()) {
        n->links.erase(slot);
    } else {
        n->links[slot] = target;
    }
    recordEdit("link " + id + "." + slot, std::move(before));
    return std::monostate{};
}

Status PartDocument::setSketch(const std::string& id, SketchData sketch) {
    Node* n = node(id);
    if (!n) return fail(ErrorCode::InvalidInput, "no feature '" + id + "'");
    if (!n->feature->hasSketchData()) return fail(ErrorCode::InvalidInput, n->type + " does not take sketch data");
    auto st = checkSketch(sketch);
    if (!st) return st;
    State before = captureState();
    n->sketch = std::move(sketch);
    recordEdit("edit sketch " + id, std::move(before));
    return std::monostate{};
}

Status PartDocument::setLabel(const std::string& id, const std::string& label) {
    Node* n = node(id);
    if (!n) return fail(ErrorCode::InvalidInput, "no feature '" + id + "'");
    State before = captureState();
    n->label = label.empty() ? n->id : label;
    recordEdit("rename " + id, std::move(before));
    return std::monostate{};
}

Status PartDocument::setRollback(std::optional<std::size_t> position) {
    if (position && *position > nodes_.size()) {
        return fail(ErrorCode::InvalidInput, "rollback position " + std::to_string(*position) + " is out of range");
    }
    State before = captureState();
    rollback_ = position;
    recordEdit("rollback", std::move(before));
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
    info.links = n->links;
    info.sketch = n->sketch;
    info.sketchResult = n->sketchResult;
    info.datum = n->datum;
    info.tool = n->tool;
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

std::vector<BodyInfo> PartDocument::bodies() const {
    const NamedShape s = finalShape();
    std::vector<BodyInfo> out;
    for (std::size_t i = 0; i < s.bodyCount(); ++i) {
        BodyInfo b;
        b.name = s.bodyName(i);
        b.volume = fc::volumeOf(s.body(i));
        for (TopExp_Explorer ex(s.body(i), TopAbs_FACE); ex.More(); ex.Next()) ++b.faces;
        out.push_back(std::move(b));
    }
    return out;
}

std::vector<ThreadInfo> threadsOf(const NamedShape& shape) {
    std::vector<ThreadInfo> out;
    for (std::size_t i = 0; i < shape.count(ElementKind::Face); ++i) {
        const ElementRecord& r = shape.record(ElementKind::Face, i);
        if (std::none_of(r.attributes.begin(), r.attributes.end(),
                         [](const auto& kv) { return kv.first.rfind("thread.", 0) == 0; }))
            continue;
        auto spec = threadFromAttributes(r.attributes);
        if (!spec) continue;
        ThreadInfo t;
        t.face = i;
        t.name = r.name;
        if (auto b = shape.bodyOf(shape.element(ElementKind::Face, i))) t.body = shape.bodyName(*b);
        t.thread = std::move(spec).value();
        out.push_back(std::move(t));
    }
    return out;
}

Result<TopoReference> makeBodyReference(const NamedShape& shape, std::string_view bodyName) {
    auto b = shape.findBody(bodyName);
    if (!b) return Result<TopoReference>::failure(ErrorCode::InvalidInput, "no body named '" + std::string(bodyName) + "'");
    std::optional<std::size_t> best;
    for (TopExp_Explorer ex(shape.body(*b), TopAbs_FACE); ex.More(); ex.Next()) {
        auto i = shape.indexOf(ElementKind::Face, ex.Current());
        if (i && (!best || shape.record(ElementKind::Face, *i).name < shape.record(ElementKind::Face, *best).name)) best = i;
    }
    if (!best) return Result<TopoReference>::failure(ErrorCode::InvalidInput, "body '" + std::string(bodyName) + "' has no faces");
    return makeReference(shape, shape.element(ElementKind::Face, *best));
}

// ---------------------------------------------------------------- tool naming

Result<NamedShape> nameTool(const std::string& featureId, const TopoDS_Shape& tool) {
    struct Item {
        TopoDS_Shape face;
        GeometricSignature sig;
    };
    std::vector<Item> faces;
    kernel::ShapeIndexMap map;
    TopExp::MapShapes(tool, TopAbs_FACE, map);
    for (int i = 1; i <= map.Extent(); ++i) {
        auto sig = computeSignature(map.FindKey(i));
        if (!sig) return Result<NamedShape>::failure(sig.error().code, sig.error().message);
        faces.push_back({map.FindKey(i), std::move(sig).value()});
    }
    auto cmp = [](double a, double b) { return std::abs(a - b) <= 1e-6 ? 0 : (a < b ? -1 : 1); };
    std::stable_sort(faces.begin(), faces.end(), [&](const Item& x, const Item& y) {
        if (x.sig.geometry != y.sig.geometry) return x.sig.geometry < y.sig.geometry;
        for (int k = 0; k < 3; ++k) {
            if (int c = cmp(x.sig.direction[k], y.sig.direction[k])) return c < 0;
        }
        if (int c = cmp(x.sig.radius, y.sig.radius)) return c < 0;
        for (int k = 0; k < 3; ++k) {
            if (int c = cmp(x.sig.center[k], y.sig.center[k])) return c < 0;
        }
        return cmp(x.sig.size, y.sig.size) < 0;
    });
    NamingRequest req;
    req.featureId = featureId;
    req.result = tool;
    for (std::size_t k = 0; k < faces.size(); ++k) req.roles.push_back({faces[k].face, "Tool.F" + std::to_string(k + 1)});
    return buildNamedShape(req);
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

void hashSketch(Hasher& h, const SketchData& s) {
    h.add(std::uint64_t(s.points.size()));
    for (const auto& p : s.points) h.add(p.fixed ? "fixed" : "free");
    h.add(std::uint64_t(s.geometry.size()));
    for (const auto& g : s.geometry) {
        h.add(toString(g.type));
        for (int v : {g.start, g.end, g.center, g.projection, g.focus}) h.add(std::uint64_t(static_cast<std::int64_t>(v)));
        for (int v : g.poles) h.add(std::uint64_t(static_cast<std::int64_t>(v)));
        h.add(std::uint64_t(static_cast<std::int64_t>(g.degree)));
        for (double v : g.knots) h.add(v);
        for (int v : g.multiplicities) h.add(std::uint64_t(static_cast<std::int64_t>(v)));
        for (double v : g.weights) h.add(v);
        h.add(g.periodic ? "periodic" : "open");
        h.add(g.counterClockwise ? "ccw" : "cw");
        h.add(g.construction ? "construction" : "profile");
    }
    h.add(std::uint64_t(s.constraints.size()));
    for (const auto& c : s.constraints) {
        h.add(toString(c.type));
        for (int v : {c.a, c.b, c.c}) h.add(std::uint64_t(static_cast<std::int64_t>(v)));
        h.add(c.driven ? "driven" : "driving");
    }
    h.add(std::uint64_t(s.regions.size()));
    for (SketchEntityId r : s.regions) h.add(std::uint64_t(r));
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
            n.clearResults();
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
        std::vector<double> sketchValues;
        if (n.sketch) {
            const auto exprs = sketchExpressions(*n.sketch);
            for (std::size_t k = 0; k < exprs.size(); ++k) {
                auto v = evaluateWith(*exprs[k], variableValues_);
                if (v) {
                    sketchValues.push_back(v.value());
                } else {
                    sketchValues.push_back(0.0);
                    paramErrors.push_back("sketch expression #" + std::to_string(k + 1) + " '" + *exprs[k] +
                                          "': " + v.error().message);
                }
            }
        }

        // Links to earlier nodes.
        std::deque<LinkedFeature> linked;
        std::map<std::string, const LinkedFeature*> links;
        std::vector<std::string> linkErrors;
        std::vector<std::pair<std::string, std::uint64_t>> linkKeys;
        for (const auto& slot : n.feature->linkSlots()) {
            auto it = n.links.find(slot.name);
            if (it == n.links.end()) {
                if (!slot.optional) linkErrors.push_back("link '" + slot.name + "' is not set");
                continue;
            }
            const std::string& target = it->second;
            auto j = indexOf(target);
            const Node* t = j ? nodes_[*j].get() : nullptr;
            std::string problem;
            if (!t) {
                problem = "there is no feature '" + target + "'";
            } else if (*j >= i) {
                problem = "feature '" + target + "' comes after this feature";
            } else if (!slot.types.empty() &&
                       std::find(slot.types.begin(), slot.types.end(), t->type) == slot.types.end()) {
                problem = "feature '" + target + "' is a " + t->type + ", expected one of";
                for (const auto& ty : slot.types) problem += " " + ty;
            } else if (t->state == FeatureState::Suppressed) {
                problem = "feature '" + target + "' is suppressed";
            } else if (t->state != FeatureState::Ok && t->state != FeatureState::Warning) {
                problem = "feature '" + target + "' is not built (" + toString(t->state) + ")";
            }
            if (!problem.empty()) {
                linkErrors.push_back("link '" + slot.name + "': " + problem);
                continue;
            }
            linked.push_back(LinkedFeature{t->id, t->type, t->output, t->tool, t->toolMode, t->toolNamed,
                                           t->sketchResult, t->datum});
            links[slot.name] = &linked.back();
            linkKeys.emplace_back(slot.name + "=" + t->id, t->hash.value_or(0));
        }

        // Sketches and datums without references do not see the body: the chain stays out of
        // their hash, and they do not advance it.
        bool bodyIndependent = !n.feature->readsInputBody();
        for (const auto& [slot, refs] : n.references) bodyIndependent = bodyIndependent && refs.empty();
        auto inputHash = [&]() {
            Hasher h;
            h.add(bodyIndependent ? std::uint64_t(0x6e6f2d626f6479ULL) : chain);  // "no-body"
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
            for (const auto& [slot, key] : linkKeys) {
                h.add(slot);
                h.add(key);
            }
            for (const auto& e : linkErrors) h.add(e);
            if (n.sketch) {
                hashSketch(h, *n.sketch);
                for (double v : sketchValues) h.add(v);
            }
            return h.value();
        };

        std::uint64_t key = inputHash();
        if (n.hash && *n.hash == key && n.state != FeatureState::RolledBack) {
            report.reused.push_back(n.id);
            if (n.state == FeatureState::Error) report.failed.push_back(n.id);
            if (n.output) current = n.output;
            if (!bodyIndependent) chain = key;
            continue;
        }
        if (n.cache && n.cache->hash == key && !n.suppressed) {
            n.apply(*n.cache);
            n.cache.reset();
            report.fromCache.push_back(n.id);
            if (n.state == FeatureState::Error) report.failed.push_back(n.id);
            if (n.output) current = n.output;
            if (!bodyIndependent) chain = key;
            continue;
        }
        n.cache.reset();
        if (!n.suppressed) {
            auto pooled = std::find_if(n.pool.begin(), n.pool.end(), [&](const CachedResult& r) { return r.hash == key; });
            if (pooled != n.pool.end()) {
                n.apply(*pooled);
                report.reused.push_back(n.id);
                if (n.state == FeatureState::Error) report.failed.push_back(n.id);
                if (n.output) current = n.output;
                if (!bodyIndependent) chain = key;
                continue;
            }
        }
        n.clearResults();
        n.values = values;

        if (n.suppressed) {
            n.state = FeatureState::Suppressed;
            n.hash = key;
            if (!bodyIndependent) chain = key;
            continue;
        }
        report.recomputed.push_back(n.id);

        auto markError = [&](std::string message) {
            n.state = FeatureState::Error;
            n.messages.push_back(std::move(message));
            if (report.failed.empty() || report.failed.back() != n.id) report.failed.push_back(n.id);
        };

        bool warning = false;
        if (!paramErrors.empty() || !linkErrors.empty()) {
            for (auto& e : paramErrors) markError(e);
            for (auto& e : linkErrors) markError(e);
        } else {
            // References against the shape this feature sees.
            std::map<std::string, std::vector<TopoDS_Shape>> resolved;
            bool broken = false;
            for (const auto& slot : n.feature->referenceSlots()) {
                auto& refs = n.references[slot.name];
                auto& shapes = resolved[slot.name];
                if (!slot.multiple && refs.size() > 1) {
                    markError("reference slot '" + slot.name + "' needs exactly one " + toString(slot.kind) + ", has " +
                              std::to_string(refs.size()));
                    broken = true;
                    continue;
                }
                if (!slot.optional && refs.empty()) {
                    markError("reference slot '" + slot.name + "' needs " + (slot.multiple ? "at least" : "exactly") +
                              " one " + toString(slot.kind) + ", has 0");
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
                FeatureContext ctx(n.id, *current, values, std::move(resolved), n.options, links,
                                   n.sketch ? &*n.sketch : nullptr, sketchValues);
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
                } else if (out.value().shape.IsNull()) {
                    if (!out.value().sketch && !out.value().datum) {
                        markError("feature produced neither a shape nor a sketch or datum");
                    } else {
                        for (const auto& w : out.value().warnings) {
                            n.messages.push_back(w);
                            warning = true;
                        }
                        n.sketchResult = out.value().sketch;
                        n.datum = out.value().datum;
                        n.state = warning ? FeatureState::Warning : FeatureState::Ok;
                    }
                } else {
                    // Validity and volume are usually known already: the operation checked its
                    // result (detail::ShapeFacts), so BRepCheck/GProp run only for new shapes.
                    const TopoDS_Shape& result = out.value().shape;
                    kernel::ValidityReport validity;
                    validity.valid = fc::isValidShape(result);
                    if (!validity.valid) validity = kernel::Shape(result).checkValidity();
                    const bool hasSolid = TopExp_Explorer(result, TopAbs_SOLID).More();
                    const double volume = hasSolid ? fc::volumeOf(result) : 0.0;
                    if (!validity.valid) {
                        std::string issues;
                        for (std::size_t q = 0; q < validity.issues.size() && q < 3; ++q) issues += " " + validity.issues[q] + ";";
                        markError("result is not a valid shape:" + issues);
                    } else if (!hasSolid || !(volume > 0)) {
                        markError("result is not a solid with positive volume");
                    } else {
                        NamingRequest req;
                        req.featureId = n.id;
                        req.result = out.value().shape;
                        req.history = out.value().history;
                        req.inputs = {current.get()};
                        for (const auto& extra : out.value().namingInputs) {
                            if (extra) req.inputs.push_back(extra.get());
                        }
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
                            n.sketchResult = out.value().sketch;
                            n.datum = out.value().datum;
                            if (!out.value().tool.IsNull()) {
                                n.tool = out.value().tool;
                                n.toolMode = out.value().toolMode;
                                auto toolNamed = nameTool(n.id, n.tool);
                                if (toolNamed) {
                                    n.toolNamed = std::make_shared<const NamedShape>(std::move(toolNamed).value());
                                } else {
                                    n.messages.push_back("tool naming failed: " + toolNamed.error().message);
                                    warning = true;
                                }
                            }
                            n.state = warning ? FeatureState::Warning : FeatureState::Ok;
                        }
                    }
                }
            }
        }
        // References may have been renamed: the stored hash describes the stored inputs.
        key = inputHash();
        n.hash = key;
        if (!bodyIndependent) chain = key;
        if (n.output) current = n.output;
        n.pool.erase(std::remove_if(n.pool.begin(), n.pool.end(), [&](const CachedResult& r) { return r.hash == key; }),
                     n.pool.end());
        n.pool.push_back(n.snapshot());
        while (n.pool.size() > Node::kPoolSize) n.pool.pop_front();
    }
    return report;
}

}  // namespace bendcore::modeling
