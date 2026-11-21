// Таблица параметров детали в документе: проверка, подстановка значений активного исполнения,
// проверка всех исполнений (docs/modeling/PART_DOCUMENT.md, раздел «Таблица параметров»).

#include "part_document_internal.hpp"

#include <bendcore/kernel/material.hpp>

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

#include <algorithm>
#include <set>

namespace bendcore::modeling {

namespace {

Status fail(std::string message) { return Status::failure(ErrorCode::InvalidInput, std::move(message)); }

std::string where(const ConfigurationTable& t, std::size_t row, std::size_t column) {
    return "configuration '" + t.rows[row].designation + "', column '" + columnTitle(t.columns[column]) + "'";
}

}  // namespace

const ConfigurationTable& PartDocument::configurationTable() const noexcept { return table_; }

const std::string& PartDocument::activeConfiguration() const noexcept { return table_.active; }

Status PartDocument::validateTable(const ConfigurationTable& t) const {
    for (std::size_t k = 0; k < t.columns.size(); ++k) {
        const ConfigurationColumn& c = t.columns[k];
        switch (c.kind) {
            case ConfigurationColumnKind::Variable: {
                const bool known = std::any_of(variables_.begin(), variables_.end(),
                                               [&](const auto& v) { return v.first == c.target; });
                if (!known) return fail("column " + std::to_string(k + 1) + ": unknown variable '" + c.target + "'");
                break;
            }
            case ConfigurationColumnKind::Suppression:
                if (!node(c.target)) return fail("column " + std::to_string(k + 1) + ": unknown feature '" + c.target + "'");
                break;
            case ConfigurationColumnKind::Material:
                if (!c.target.empty()) return fail("column " + std::to_string(k + 1) + ": a material column has no target");
                break;
        }
        for (std::size_t j = 0; j < k; ++j) {
            if (t.columns[j] == c) return fail("repeated column '" + columnTitle(c) + "'");
        }
    }
    std::set<std::string> designations;
    for (std::size_t r = 0; r < t.rows.size(); ++r) {
        const Configuration& row = t.rows[r];
        if (row.designation.empty()) return fail("configuration " + std::to_string(r + 1) + " has no designation");
        if (!designations.insert(row.designation).second) return fail("repeated designation '" + row.designation + "'");
        if (row.cells.size() != t.columns.size()) {
            return fail("configuration '" + row.designation + "' has " + std::to_string(row.cells.size()) +
                        " cells, the table has " + std::to_string(t.columns.size()) + " columns");
        }
        for (std::size_t k = 0; k < row.cells.size(); ++k) {
            const std::string& cell = row.cells[k];
            if (cell.empty()) continue;
            switch (t.columns[k].kind) {
                case ConfigurationColumnKind::Variable:
                    if (auto st = checkExpression(cell); !st) {
                        return fail(where(t, r, k) + ": invalid value '" + cell + "': " + st.error().message);
                    }
                    break;
                case ConfigurationColumnKind::Suppression:
                    if (!parseSuppressionCell(cell)) {
                        return fail(where(t, r, k) + ": invalid value '" + cell + "' (expected 1 or 0)");
                    }
                    break;
                case ConfigurationColumnKind::Material: {
                    const auto& lib = kernel::builtInMaterials();
                    const bool known = std::any_of(lib.begin(), lib.end(), [&](const kernel::Material& m) { return m.id == cell; });
                    if (!known) return fail(where(t, r, k) + ": unknown material '" + cell + "'");
                    break;
                }
            }
        }
    }
    if (!t.active.empty() && !t.rowOf(t.active)) return fail("no configuration '" + t.active + "'");
    return std::monostate{};
}

Status PartDocument::setConfigurationTable(ConfigurationTable table) {
    if (auto st = validateTable(table); !st) return st;
    State before = captureState();
    table_ = std::move(table);
    recordEdit("configuration table", std::move(before));
    return std::monostate{};
}

Status PartDocument::setConfigurationCell(std::size_t row, std::size_t column, const std::string& value) {
    if (row >= table_.rows.size()) return fail("no configuration row " + std::to_string(row));
    if (column >= table_.columns.size()) return fail("no configuration column " + std::to_string(column));
    ConfigurationTable t = table_;
    t.rows[row].cells[column] = value;
    if (auto st = validateTable(t); !st) return st;
    State before = captureState();
    table_ = std::move(t);
    recordEdit("configuration " + table_.rows[row].designation + ": " + columnTitle(table_.columns[column]),
               std::move(before));
    return std::monostate{};
}

Status PartDocument::addConfigurationColumn(ConfigurationColumn column) {
    ConfigurationTable t = table_;
    std::string base;
    if (column.kind == ConfigurationColumnKind::Variable) {
        for (const auto& [n, e] : variables_) {
            if (n == column.target) base = e;
        }
    } else if (column.kind == ConfigurationColumnKind::Suppression) {
        if (const Node* n = node(column.target)) base = n->suppressed ? "1" : "0";
    }
    t.columns.push_back(column);
    for (auto& r : t.rows) r.cells.push_back(base);
    if (auto st = validateTable(t); !st) return st;
    State before = captureState();
    table_ = std::move(t);
    recordEdit("configuration column " + columnTitle(column), std::move(before));
    return std::monostate{};
}

Status PartDocument::activateConfiguration(const std::string& designation) {
    if (!designation.empty() && !table_.rowOf(designation)) return fail("no configuration '" + designation + "'");
    if (designation == table_.active) return std::monostate{};
    State before = captureState();
    table_.active = designation;
    recordEdit("activate configuration " + (designation.empty() ? std::string("<base>") : designation), std::move(before));
    return std::monostate{};
}

std::string PartDocument::configurationMaterial() const {
    auto row = table_.active.empty() ? std::nullopt : table_.rowOf(table_.active);
    if (!row) return {};
    for (std::size_t k = 0; k < table_.columns.size(); ++k) {
        if (table_.columns[k].kind == ConfigurationColumnKind::Material) return table_.rows[*row].cells[k];
    }
    return {};
}

std::vector<std::pair<std::string, std::string>> PartDocument::effectiveVariables() const {
    std::vector<std::pair<std::string, std::string>> out = variables_;
    auto row = table_.active.empty() ? std::nullopt : table_.rowOf(table_.active);
    if (!row) return out;
    for (std::size_t k = 0; k < table_.columns.size(); ++k) {
        const ConfigurationColumn& c = table_.columns[k];
        const std::string& cell = table_.rows[*row].cells[k];
        if (c.kind != ConfigurationColumnKind::Variable || cell.empty()) continue;
        for (auto& [n, e] : out) {
            if (n == c.target) e = cell;
        }
    }
    return out;
}

bool PartDocument::effectiveSuppressed(const Node& n) const {
    auto row = table_.active.empty() ? std::nullopt : table_.rowOf(table_.active);
    if (row) {
        for (std::size_t k = 0; k < table_.columns.size(); ++k) {
            const ConfigurationColumn& c = table_.columns[k];
            if (c.kind != ConfigurationColumnKind::Suppression || c.target != n.id) continue;
            if (auto v = parseSuppressionCell(table_.rows[*row].cells[k])) return *v;
        }
    }
    return n.suppressed;
}

bool PartDocument::writeDrivenCell(ConfigurationColumnKind kind, const std::string& target, const std::string& value) {
    auto row = table_.active.empty() ? std::nullopt : table_.rowOf(table_.active);
    if (!row) return false;
    auto col = table_.columnOf({kind, target});
    if (!col || table_.rows[*row].cells[*col].empty()) return false;
    table_.rows[*row].cells[*col] = value;
    return true;
}

void PartDocument::removeColumns(ConfigurationColumnKind kind, const std::string& target) {
    while (auto col = table_.columnOf({kind, target})) {
        table_.columns.erase(table_.columns.begin() + static_cast<std::ptrdiff_t>(*col));
        for (auto& r : table_.rows) {
            if (*col < r.cells.size()) r.cells.erase(r.cells.begin() + static_cast<std::ptrdiff_t>(*col));
        }
    }
}

std::vector<ConfigurationCheck> PartDocument::checkConfigurations() {
    std::vector<ConfigurationCheck> out;
    const std::string saved = table_.active;
    for (std::size_t r = 0; r < table_.rows.size(); ++r) {
        table_.active = table_.rows[r].designation;
        rebuild();
        ConfigurationCheck c;
        c.designation = table_.rows[r].designation;
        c.fullDesignation = fullDesignation(table_, r);
        for (const auto& [name, value] : variableValues_) {
            if (!value) c.problems.push_back("variable '" + name + "': " + value.error().message);
        }
        const std::size_t end = rollback_ ? std::min(*rollback_, nodes_.size()) : nodes_.size();
        for (std::size_t i = 0; i < end; ++i) {
            const Node& n = *nodes_[i];
            if (n.state != FeatureState::Error) continue;
            std::string msg = "feature '" + n.id + "' is not built";
            for (std::size_t m = 0; m < n.messages.size(); ++m) msg += (m == 0 ? ": " : "; ") + n.messages[m];
            c.problems.push_back(std::move(msg));
        }
        const NamedShape s = finalShape();
        if (s.isNull()) {
            c.problems.push_back("the configuration has no solid");
        } else {
            GProp_GProps props;
            BRepGProp::VolumeProperties(s.shape(), props);
            c.volume = props.Mass();
        }
        c.ok = c.problems.empty();
        out.push_back(std::move(c));
    }
    table_.active = saved;
    rebuild();
    return out;
}

}  // namespace bendcore::modeling
