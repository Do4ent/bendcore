// Таблица исполнений сборки: проверка, подстановка значений столбцов, активация с решением,
// синхронизация с правками документа, CSV через код таблицы параметров детали
// (docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 8.2).

#include <bendcore/assembly_v2/assembly_document.hpp>

#include <bendcore/modeling/configuration_table.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>

namespace bendcore::assembly_v2 {

using kernel::Error;
using kernel::ErrorCode;
using kernel::Result;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr const char* kAsSaved = "#";  // ячейка исполнения: компонент как сохранён

Status ok() { return std::monostate{}; }
Status fail(ErrorCode code, std::string message) { return Status::failure(code, std::move(message)); }

// Число без потерь и без лишних цифр: %.15g, если оно читается обратно точно, иначе %.17g.
std::string formatNumber(double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.15g", v);
    if (std::strtod(buf, nullptr) != v) std::snprintf(buf, sizeof buf, "%.17g", v);
    return buf;
}

// Число ячейки значения сопряжения: десятичная точка или запятая, весь текст — число.
std::optional<double> parseNumber(std::string text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.erase(0, 1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.pop_back();
    if (text.empty()) return std::nullopt;
    if (text.find('.') == std::string::npos) std::replace(text.begin(), text.end(), ',', '.');
    char* end = nullptr;
    const double v = std::strtod(text.c_str(), &end);
    if (end != text.c_str() + text.size() || !std::isfinite(v)) return std::nullopt;
    return v;
}

bool sameNumber(double a, double b) { return std::abs(a - b) <= 1e-12 * std::max(1.0, std::abs(b)); }

}  // namespace

// ---------------------------------------------------------------- таблица

const char* toString(AssemblyColumnKind kind) noexcept {
    switch (kind) {
        case AssemblyColumnKind::Suppression: return "suppression";
        case AssemblyColumnKind::Configuration: return "configuration";
        case AssemblyColumnKind::MateValue: return "mateValue";
    }
    return "suppression";
}

std::optional<AssemblyColumnKind> assemblyColumnKindFromString(std::string_view text) noexcept {
    for (auto k : {AssemblyColumnKind::Suppression, AssemblyColumnKind::Configuration, AssemblyColumnKind::MateValue})
        if (text == toString(k)) return k;
    return std::nullopt;
}

std::optional<std::size_t> AssemblyConfigurationTable::rowOf(std::string_view designation) const {
    for (std::size_t i = 0; i < rows.size(); ++i)
        if (rows[i].designation == designation) return i;
    return std::nullopt;
}

std::optional<std::size_t> AssemblyConfigurationTable::columnOf(const AssemblyConfigurationColumn& column) const {
    for (std::size_t i = 0; i < columns.size(); ++i)
        if (columns[i] == column) return i;
    return std::nullopt;
}

std::string fullDesignation(const AssemblyConfigurationTable& table, std::size_t row, const std::string& rootDesignation) {
    if (row >= table.rows.size()) return {};
    const std::string& d = table.rows[row].designation;
    if (d.empty() || d.front() != '-') return d;
    const std::string& first = table.rows.front().designation;
    if (row > 0 && !first.empty() && first.front() != '-') return first + d;
    return rootDesignation.empty() ? d : rootDesignation + d;
}

// ---------------------------------------------------------------- значения столбцов

Result<std::string> AssemblyDocument::columnValue(const AssemblyConfigurationColumn& c) const {
    switch (c.kind) {
        case AssemblyColumnKind::Suppression: {
            const Occurrence* o = findOccurrence(c.target);
            if (!o) return Error{ErrorCode::InvalidInput, "unknown occurrence " + std::to_string(c.target)};
            return std::string(o->suppressed ? "1" : "0");
        }
        case AssemblyColumnKind::Configuration: {
            const Occurrence* o = findOccurrence(c.target);
            if (!o) return Error{ErrorCode::InvalidInput, "unknown occurrence " + std::to_string(c.target)};
            const Definition& d = definitions_[o->definition];
            if (d.source.empty())
                return Error{ErrorCode::InvalidInput, "occurrence '" + o->name + "' has no source document"};
            const std::string cfg = splitConfigurationSource(d.source).second;
            return cfg.empty() ? std::string(kAsSaved) : cfg;
        }
        case AssemblyColumnKind::MateValue: {
            const DocumentMate* m = findMate(c.target);
            if (!m) return Error{ErrorCode::InvalidInput, "unknown mate " + std::to_string(c.target)};
            const double v = m->spec.type == MateType::Angle ? m->spec.value * 180.0 / kPi : m->spec.value;
            return formatNumber(v);
        }
    }
    return Error{ErrorCode::InvalidInput, "unknown column kind"};
}

Status AssemblyDocument::checkConfigurationTable(const AssemblyConfigurationTable& t) const {
    for (std::size_t k = 0; k < t.columns.size(); ++k) {
        const AssemblyConfigurationColumn& c = t.columns[k];
        const std::string col = std::to_string(k + 1);
        for (std::size_t j = 0; j < k; ++j)
            if (t.columns[j] == c) return fail(ErrorCode::InvalidInput, "repeated column '" + configurationColumnTitle(*this, c) + "'");
        if (c.kind == AssemblyColumnKind::MateValue) {
            const DocumentMate* m = findMate(c.target);
            if (!m) return fail(ErrorCode::InvalidInput, "column " + col + ": unknown mate " + std::to_string(c.target));
            if (m->spec.type != MateType::Distance && m->spec.type != MateType::Angle && m->spec.type != MateType::Gear)
                return fail(ErrorCode::InvalidInput, "column " + col + ": mate " + std::to_string(c.target) + " (" +
                                                         toString(m->spec.type) + ") has no value");
            continue;
        }
        const Occurrence* o = findOccurrence(c.target);
        if (!o) return fail(ErrorCode::InvalidInput, "column " + col + ": unknown occurrence " + std::to_string(c.target));
        if (auto e = checkEditable(c.target); !e) return fail(ErrorCode::InvalidInput, "column " + col + ": " + e.error().message);
        // Экземпляры массива создаются и удаляются массивом (число отверстий зависит от
        // исполнения детали): они следуют за исходным компонентом.
        if (o->pattern)
            return fail(ErrorCode::InvalidInput, "column " + col + ": occurrence '" + o->name +
                                                     "' is a pattern instance; it follows the pattern seed");
        if (c.kind == AssemblyColumnKind::Configuration) {
            const Definition& d = definitions_[o->definition];
            if (d.source.empty() || (d.kind == DefinitionKind::Assembly && d.external))
                return fail(ErrorCode::InvalidInput, "column " + col + ": occurrence '" + o->name +
                                                         "' has no source document: configurations need one");
        }
    }
    std::set<std::string> seen;
    for (const AssemblyConfiguration& r : t.rows) {
        const std::size_t number = static_cast<std::size_t>(&r - t.rows.data()) + 1;
        if (r.designation.empty())
            return fail(ErrorCode::InvalidInput, "configuration " + std::to_string(number) + " has no designation");
        if (!seen.insert(r.designation).second)
            return fail(ErrorCode::InvalidInput, "repeated designation '" + r.designation + "'");
        if (r.cells.size() != t.columns.size())
            return fail(ErrorCode::InvalidInput, "configuration '" + r.designation + "' has " +
                                                     std::to_string(r.cells.size()) + " cells, the table has " +
                                                     std::to_string(t.columns.size()) + " columns");
        for (std::size_t k = 0; k < r.cells.size(); ++k) {
            const std::string& cell = r.cells[k];
            if (cell.empty()) continue;
            const AssemblyConfigurationColumn& c = t.columns[k];
            const std::string title = configurationColumnTitle(*this, c);
            if (c.kind == AssemblyColumnKind::Suppression && !modeling::parseSuppressionCell(cell))
                return fail(ErrorCode::InvalidInput, "configuration '" + r.designation + "', column '" + title + "': invalid value '" + cell + "' (expected 1 or 0)");
            if (c.kind == AssemblyColumnKind::MateValue) {
                const auto v = parseNumber(cell);
                if (!v) return fail(ErrorCode::InvalidInput, "configuration '" + r.designation + "', column '" + title + "': invalid value '" + cell + "': expected a number");
                const DocumentMate* m = findMate(c.target);
                if (m->spec.type == MateType::Gear && !(*v > 0.0))
                    return fail(ErrorCode::InvalidInput, "configuration '" + r.designation + "', column '" + title + "': invalid value '" + cell + "': a gear ratio must be positive");
            }
        }
    }
    if (!t.active.empty() && !t.rowOf(t.active))
        return fail(ErrorCode::InvalidInput, "no configuration '" + t.active + "'");
    return ok();
}

Status AssemblyDocument::writeColumnValue(const AssemblyConfigurationColumn& c, const std::string& value,
                                          const LoadOptions& options, ConfigurationActivation& report) {
    switch (c.kind) {
        case AssemblyColumnKind::Suppression: {
            const auto v = modeling::parseSuppressionCell(value);
            if (!v) return fail(ErrorCode::InvalidInput, "invalid suppression value '" + value + "'");
            findOccurrenceMutable(c.target)->suppressed = *v;
            return ok();
        }
        case AssemblyColumnKind::Configuration: {
            auto r = setOccurrenceConfiguration(c.target, value == kAsSaved ? std::string() : value, options);
            if (!r) {
                const Occurrence* o = findOccurrence(c.target);
                return fail(r.error().code, "occurrence '" + (o ? o->name : std::to_string(c.target)) +
                                                "', configuration '" + value + "': " + r.error().message);
            }
            report.replaced.emplace_back(c.target, std::move(r).value());
            return ok();
        }
        case AssemblyColumnKind::MateValue: {
            const auto v = parseNumber(value);
            if (!v) return fail(ErrorCode::InvalidInput, "invalid mate value '" + value + "'");
            for (DocumentMate& m : mates_)
                if (m.id == c.target) {
                    m.spec.value = m.spec.type == MateType::Angle ? *v * kPi / 180.0 : *v;
                    return ok();
                }
            return fail(ErrorCode::InvalidInput, "unknown mate " + std::to_string(c.target));
        }
    }
    return fail(ErrorCode::InvalidInput, "unknown column kind");
}

// Текущее состояние документа → непустые ячейки активного исполнения и базовые значения
// (правки при активном исполнении не теряются при переключении). Текст ячейки, который
// задаёт то же значение («30» градусов), сохраняется как записан.
void AssemblyDocument::captureConfiguration() {
    configurationBase_.resize(configurations_.columns.size());
    const auto row = configurations_.rowOf(configurations_.active);
    for (std::size_t k = 0; k < configurations_.columns.size(); ++k) {
        const AssemblyConfigurationColumn& c = configurations_.columns[k];
        auto now = columnValue(c);
        if (!now) continue;
        std::string* slot = &configurationBase_[k];
        if (row && !configurations_.rows[*row].cells[k].empty()) slot = &configurations_.rows[*row].cells[k];
        bool same = *slot == now.value();
        if (!same && c.kind == AssemblyColumnKind::Suppression) {
            same = modeling::parseSuppressionCell(*slot) == modeling::parseSuppressionCell(now.value());
        } else if (!same && c.kind == AssemblyColumnKind::MateValue) {
            const auto a = parseNumber(*slot);
            const auto b = parseNumber(now.value());
            same = a && b && sameNumber(*a, *b);
        }
        if (!same) *slot = now.value();
    }
}

void AssemblyDocument::pruneConfigurationColumns() {
    for (std::size_t k = configurations_.columns.size(); k-- > 0;) {
        const AssemblyConfigurationColumn& c = configurations_.columns[k];
        const bool exists = c.kind == AssemblyColumnKind::MateValue ? findMate(c.target) != nullptr
                                                                    : findOccurrence(c.target) != nullptr;
        if (exists) continue;
        configurations_.columns.erase(configurations_.columns.begin() + static_cast<std::ptrdiff_t>(k));
        for (AssemblyConfiguration& r : configurations_.rows)
            if (k < r.cells.size()) r.cells.erase(r.cells.begin() + static_cast<std::ptrdiff_t>(k));
        if (k < configurationBase_.size())
            configurationBase_.erase(configurationBase_.begin() + static_cast<std::ptrdiff_t>(k));
    }
}

AssemblyConfigurationTable AssemblyDocument::configurationTable() const {
    AssemblyDocument copy = *this;  // формы — общие дескрипторы; копируются значения
    copy.captureConfiguration();
    return copy.configurations_;
}

std::vector<std::string> AssemblyDocument::configurationBase() const {
    AssemblyDocument copy = *this;
    copy.captureConfiguration();
    return copy.configurationBase_;
}

std::string AssemblyDocument::configurationFullDesignation(std::size_t row) const {
    return fullDesignation(configurations_, row, definitions_[0].properties.designation);
}

PartProperties AssemblyDocument::configurationProperties() const {
    PartProperties p = definitions_[0].properties;
    if (const auto row = configurations_.rowOf(configurations_.active)) {
        p.designation = configurationFullDesignation(*row);
        if (!configurations_.rows[*row].name.empty()) p.name = configurations_.rows[*row].name;
    }
    return p;
}

// ---------------------------------------------------------------- активация

Result<ConfigurationActivation> AssemblyDocument::applyConfiguration(const std::string& designation,
                                                                     const LoadOptions& options,
                                                                     const ActivationOptions& activation) {
    const auto row = configurations_.rowOf(designation);
    if (!designation.empty() && !row) return Error{ErrorCode::InvalidInput, "no configuration '" + designation + "'"};
    captureConfiguration();
    // Положения уходящего исполнения — чтобы вернуться к ним.
    auto& saved = configurationPlacements_[configurations_.active];
    saved.clear();
    for (const Occurrence& o : topLevel())
        if (!o.pattern) saved[o.id] = o.placement;

    ConfigurationActivation report;
    report.designation = designation;
    if (row) report.fullDesignation = configurationFullDesignation(*row);
    for (std::size_t k = 0; k < configurations_.columns.size(); ++k) {
        const AssemblyConfigurationColumn c = configurations_.columns[k];
        const std::string cell = row ? configurations_.rows[*row].cells[k] : std::string();
        const std::string target = cell.empty() ? configurationBase_[k] : cell;
        auto now = columnValue(c);
        if (now && now.value() == target) continue;
        if (target.empty()) continue;  // базовое значение неизвестно (столбец без значения)
        if (auto w = writeColumnValue(c, target, options, report); !w) return w.error();
    }
    configurations_.active = designation;
    if (auto it = configurationPlacements_.find(designation); it != configurationPlacements_.end()) {
        for (const auto& [id, placement] : it->second) {
            Occurrence* o = findOccurrenceMutable(id);
            if (o && !o->pattern && parentOf(id) == std::optional<DefinitionId>(root())) o->placement = placement;
        }
    }
    if (auto u = updatePatterns(); !u) return u.error();
    for (const AssemblyConfigurationColumn& c : configurations_.columns) {
        if (c.kind != AssemblyColumnKind::Suppression) continue;
        const Occurrence* o = findOccurrence(c.target);
        if (o && o->suppressed) report.excluded.push_back(o->id);
    }
    for (const DocumentMate& m : mates_) {
        if (m.suppressed) continue;
        for (const OccurrencePath* path : {&m.spec.a.path, &m.spec.b.path}) {
            auto it = std::find_if(path->begin(), path->end(), [&](OccurrenceId id) {
                const Occurrence* o = findOccurrence(id);
                return o && o->suppressed;
            });
            if (it != path->end()) {
                report.disabledMates.emplace_back(m.id, *it);
                break;
            }
        }
    }
    if (activation.solve) {
        auto s = solve(activation.solver);
        if (!s) return s.error();
        report.solve = std::move(s).value();
    }
    return report;
}

Result<ConfigurationActivation> AssemblyDocument::activateConfiguration(const std::string& designation,
                                                                        const LoadOptions& options,
                                                                        const ActivationOptions& activation) {
    AssemblyDocument backup = *this;
    auto r = applyConfiguration(designation, options, activation);
    if (!r) *this = std::move(backup);
    return r;
}

Status AssemblyDocument::setConfigurationTable(AssemblyConfigurationTable table, const LoadOptions& options,
                                               const ActivationOptions& activation) {
    if (auto c = checkConfigurationTable(table); !c) return c;
    AssemblyDocument backup = *this;
    auto restore = [&](Error e) -> Status {
        *this = std::move(backup);
        return e;
    };
    // К базовым значениям прежней таблицы (без решения), затем базовые значения новой таблицы:
    // прежние для общих столбцов, текущее состояние — для новых.
    ActivationOptions quiet;
    quiet.solve = false;
    if (auto r = applyConfiguration({}, options, quiet); !r) return restore(r.error());
    std::vector<std::string> base;
    for (const AssemblyConfigurationColumn& c : table.columns) {
        if (const auto k = configurations_.columnOf(c)) {
            base.push_back(configurationBase_[*k]);
        } else {
            auto v = columnValue(c);
            if (!v) return restore(v.error());
            base.push_back(v.value());
        }
    }
    const std::string active = table.active;
    table.active.clear();
    configurations_ = std::move(table);
    configurationBase_ = std::move(base);
    for (auto it = configurationPlacements_.begin(); it != configurationPlacements_.end();) {
        if (!it->first.empty() && !configurations_.rowOf(it->first)) it = configurationPlacements_.erase(it);
        else ++it;
    }
    if (active.empty() && !activation.solve) return ok();
    auto r = applyConfiguration(active, options, activation);
    if (!r) return restore(r.error());
    return ok();
}

// Путь через вхождение подсборки path[0] → тот же путь в составе newDefinition: вхождения
// сопоставляются по ID в файле подсборки (sourceId). nullopt — вхождения нет в новом составе.
std::optional<OccurrencePath> AssemblyDocument::remapPathInto(const OccurrencePath& path,
                                                              DefinitionId newDefinition) const {
    if (path.empty()) return std::nullopt;
    const Occurrence* top = findOccurrence(path[0]);
    if (!top) return std::nullopt;
    OccurrencePath out{path[0]};
    DefinitionId newCurrent = newDefinition;
    for (std::size_t k = 1; k < path.size(); ++k) {
        // Вхождение пути ищется по всему документу: путь, сломанный в прошлом исполнении, ещё
        // ссылается на состав, где вхождение было.
        const Occurrence* o = findOccurrence(path[k]);
        if (!o) return std::nullopt;
        const OccurrenceId local = o->sourceId.value_or(o->id);
        const auto& newOccs = definitions_[newCurrent].occurrences;
        auto n = std::find_if(newOccs.begin(), newOccs.end(),
                              [&](const Occurrence& x) { return x.sourceId.value_or(x.id) == local; });
        if (n == newOccs.end()) return std::nullopt;
        out.push_back(n->id);
        newCurrent = n->definition;
    }
    return out;
}

// ---------------------------------------------------------------- CSV

namespace {

const char* kindPrefix(AssemblyColumnKind kind) {
    switch (kind) {
        case AssemblyColumnKind::Suppression: return "suppress:";
        case AssemblyColumnKind::Configuration: return "configuration:";
        case AssemblyColumnKind::MateValue: return "mate:";
    }
    return "suppress:";
}

bool startsWith(std::string_view s, std::string_view prefix) { return s.substr(0, prefix.size()) == prefix; }

// Вхождения состава, который можно править (без внешних подсборок).
std::vector<const Occurrence*> editableOccurrences(const AssemblyDocument& doc) {
    std::vector<const Occurrence*> out;
    for (const Definition& d : doc.definitions()) {
        if (d.kind != DefinitionKind::Assembly || doc.isReadOnlyAssembly(d.id) || d.external) continue;
        for (const Occurrence& o : d.occurrences) out.push_back(&o);
    }
    return out;
}

std::string targetName(const AssemblyDocument& doc, const AssemblyConfigurationColumn& c) {
    const std::string byId = "#" + std::to_string(c.target);
    if (c.kind == AssemblyColumnKind::MateValue) {
        const DocumentMate* m = doc.findMate(c.target);
        if (!m || m->spec.name.empty() || startsWith(m->spec.name, "#")) return byId;
        const auto n = std::count_if(doc.mates().begin(), doc.mates().end(),
                                     [&](const DocumentMate& x) { return x.spec.name == m->spec.name; });
        return n == 1 ? m->spec.name : byId;
    }
    const Occurrence* o = doc.findOccurrence(c.target);
    if (!o || o->name.empty() || startsWith(o->name, "#")) return byId;
    const auto all = editableOccurrences(doc);
    const auto n = std::count_if(all.begin(), all.end(), [&](const Occurrence* x) { return x->name == o->name; });
    return n == 1 ? o->name : byId;
}

Result<std::uint32_t> resolveTarget(const AssemblyDocument& doc, AssemblyColumnKind kind, const std::string& name,
                                    const std::string& title) {
    if (name.size() > 1 && name.front() == '#' &&
        std::all_of(name.begin() + 1, name.end(), [](char ch) { return ch >= '0' && ch <= '9'; })) {
        const auto id = static_cast<std::uint32_t>(std::strtoul(name.c_str() + 1, nullptr, 10));
        const bool exists = kind == AssemblyColumnKind::MateValue ? doc.findMate(id) != nullptr
                                                                  : doc.findOccurrence(id) != nullptr;
        if (!exists && kind == AssemblyColumnKind::MateValue)
            return Error{ErrorCode::ParseError, "CSV column '" + title + "': no such mate"};
        if (!exists) return Error{ErrorCode::ParseError, "CSV column '" + title + "': no such occurrence"};
        return id;
    }
    std::vector<std::uint32_t> found;
    if (kind == AssemblyColumnKind::MateValue) {
        for (const DocumentMate& m : doc.mates())
            if (m.spec.name == name) found.push_back(m.id);
    } else {
        for (const Occurrence* o : editableOccurrences(doc))
            if (o->name == name) found.push_back(o->id);
    }
    const bool mate = kind == AssemblyColumnKind::MateValue;
    if (found.empty() && mate) return Error{ErrorCode::ParseError, "CSV column '" + title + "': no mate named '" + name + "'"};
    if (found.empty()) return Error{ErrorCode::ParseError, "CSV column '" + title + "': no occurrence named '" + name + "'"};
    if (found.size() > 1 && mate)
        return Error{ErrorCode::ParseError, "CSV column '" + title + "': " + std::to_string(found.size()) +
                                                " mates are named '" + name + "'; use #<id>"};
    if (found.size() > 1)
        return Error{ErrorCode::ParseError, "CSV column '" + title + "': " + std::to_string(found.size()) +
                                                " occurrences are named '" + name + "'; use #<id>"};
    return found.front();
}

}  // namespace

std::string configurationColumnTitle(const AssemblyDocument& document, const AssemblyConfigurationColumn& column) {
    return kindPrefix(column.kind) + targetName(document, column);
}

std::string configurationTableToCsv(const AssemblyDocument& document, const AssemblyConfigurationTable& table,
                                    char decimal, bool byteOrderMark) {
    // Столбцы сборки — столбцы таблицы детали: подавление — Suppression «suppress:<имя>»,
    // исполнение и значение — Variable с заголовком «configuration:<имя>», «mate:<имя>».
    modeling::ConfigurationTable t;
    for (const AssemblyConfigurationColumn& c : table.columns) {
        modeling::ConfigurationColumn mc;
        const std::string name = targetName(document, c);
        if (c.kind == AssemblyColumnKind::Suppression) {
            mc.kind = modeling::ConfigurationColumnKind::Suppression;
            mc.target = name;
        } else {
            mc.kind = modeling::ConfigurationColumnKind::Variable;
            mc.target = kindPrefix(c.kind) + name;
        }
        t.columns.push_back(std::move(mc));
    }
    for (const AssemblyConfiguration& r : table.rows) t.rows.push_back({r.designation, r.name, r.cells});
    modeling::CsvOptions options;
    options.decimal = decimal;  // у столбцов Variable: значения сопряжений (обозначение «-01» — без точки)
    options.byteOrderMark = byteOrderMark;
    return modeling::configurationTableToCsv(t, options);
}

Result<AssemblyConfigurationTable> configurationTableFromCsv(const AssemblyDocument& document, std::string_view csv) {
    auto parsed = modeling::configurationTableFromCsv(csv);
    if (!parsed) return parsed.error();
    AssemblyConfigurationTable table;
    for (const modeling::ConfigurationColumn& mc : parsed.value().columns) {
        AssemblyConfigurationColumn c;
        std::string name;
        std::string title = modeling::columnTitle(mc);
        if (mc.kind == modeling::ConfigurationColumnKind::Material) {
            return Error{ErrorCode::ParseError, "CSV column 'material': an assembly configuration has no material; "
                                                "set it in the part's design table"};
        } else if (mc.kind == modeling::ConfigurationColumnKind::Suppression) {
            c.kind = AssemblyColumnKind::Suppression;
            name = mc.target;
        } else {
            const std::string& t = mc.target;
            bool matched = false;
            for (const auto& [prefix, kind] :
                 {std::pair<std::string_view, AssemblyColumnKind>{"configuration:", AssemblyColumnKind::Configuration},
                  {"исполнение:", AssemblyColumnKind::Configuration},
                  {"Исполнение:", AssemblyColumnKind::Configuration},
                  {"mate:", AssemblyColumnKind::MateValue},
                  {"сопряжение:", AssemblyColumnKind::MateValue},
                  {"Сопряжение:", AssemblyColumnKind::MateValue}}) {
                if (startsWith(t, prefix) && t.size() > prefix.size()) {
                    c.kind = kind;
                    name = t.substr(prefix.size());
                    matched = true;
                    break;
                }
            }
            if (!matched)
                return Error{ErrorCode::ParseError,
                             "CSV column '" + t + "': expected 'suppress:<component>', 'configuration:<component>' or "
                                                  "'mate:<mate>'"};
        }
        while (!name.empty() && name.front() == ' ') name.erase(0, 1);
        while (!name.empty() && name.back() == ' ') name.pop_back();
        auto target = resolveTarget(document, c.kind, name, title);
        if (!target) return target.error();
        c.target = target.value();
        if (table.columnOf(c)) return Error{ErrorCode::ParseError, "CSV column '" + title + "' is repeated"};
        table.columns.push_back(c);
    }
    for (const modeling::Configuration& r : parsed.value().rows) table.rows.push_back({r.designation, r.name, r.cells});
    return table;
}

}  // namespace bendcore::assembly_v2
