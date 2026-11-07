// Состав изделия (docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 8).

#include <bendcore/assembly_v2/bom.hpp>

#include <bendcore/kernel/shape.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>

namespace bendcore::assembly_v2 {

using kernel::Error;
using kernel::ErrorCode;
using kernel::Result;

namespace {

constexpr double kMm3ToM3 = 1e-9;

struct Unit {
    double volume = 0.0;
    std::optional<double> mass;
};

class BomBuilder {
public:
    explicit BomBuilder(const AssemblyDocument& doc) : doc_(doc) {}

    // Объём и масса одного экземпляра определения (сборка — сумма по составу).
    Result<Unit> unit(DefinitionId id) {
        if (auto it = cache_.find(id); it != cache_.end()) return it->second;
        const Definition& d = doc_.definition(id);
        Unit u;
        if (d.kind == DefinitionKind::Part) {
            auto v = kernel::Shape(d.shape).volumeProperties();
            if (!v) return Error{v.error().code, "part '" + d.name + "': " + v.error().message};
            u.volume = std::abs(v.value().volume);
            if (d.properties.density) {
                if (*d.properties.density < 0.0) return Error{ErrorCode::InvalidInput, "negative density of '" + d.name + "'"};
                u.mass = u.volume * kMm3ToM3 * *d.properties.density;
            }
        } else {
            u.mass = 0.0;
            for (const Occurrence& o : d.occurrences) {
                auto c = unit(o.definition);
                if (!c) return c;
                u.volume += c.value().volume;
                if (u.mass && c.value().mass) *u.mass += *c.value().mass;
                else u.mass.reset();
            }
        }
        cache_[id] = u;
        return u;
    }

    BomRow row(DefinitionId id, int quantity, int level, const Unit& u) const {
        const Definition& d = doc_.definition(id);
        BomRow r;
        r.level = level;
        r.definition = id;
        r.kind = d.kind;
        r.designation = d.properties.designation;
        r.name = d.properties.name.empty() ? d.name : d.properties.name;
        r.material = d.properties.material;
        r.quantity = quantity;
        r.unitVolume = u.volume;
        r.unitMass = u.mass;
        if (u.mass) r.totalMass = *u.mass * quantity;
        return r;
    }

    // Группы вхождений определения по определению, в порядке первого появления.
    static std::vector<std::pair<DefinitionId, int>> groups(const Definition& d) {
        std::vector<std::pair<DefinitionId, int>> out;
        for (const Occurrence& o : d.occurrences) {
            auto it = std::find_if(out.begin(), out.end(), [&](const auto& g) { return g.first == o.definition; });
            if (it == out.end()) out.emplace_back(o.definition, 1);
            else ++it->second;
        }
        return out;
    }

    Status countParts(DefinitionId id, int multiplier, std::vector<std::pair<DefinitionId, int>>& out) {
        for (const auto& [child, n] : groups(doc_.definition(id))) {
            if (doc_.definition(child).kind == DefinitionKind::Part) {
                auto it = std::find_if(out.begin(), out.end(), [&](const auto& g) { return g.first == child; });
                if (it == out.end()) out.emplace_back(child, n * multiplier);
                else it->second += n * multiplier;
            } else if (auto s = countParts(child, n * multiplier, out); !s) {
                return s;
            }
        }
        return std::monostate{};
    }

    Result<std::vector<BomRow>> levelRows(DefinitionId id, int level, const std::string& prefix) {
        std::vector<BomRow> rows;
        int pos = 0;
        for (const auto& [child, n] : groups(doc_.definition(id))) {
            auto u = unit(child);
            if (!u) return u.error();
            BomRow r = row(child, n, level, u.value());
            r.position = ++pos;
            r.positionPath = prefix.empty() ? std::to_string(pos) : prefix + "." + std::to_string(pos);
            if (r.kind == DefinitionKind::Assembly) {
                auto c = levelRows(child, level + 1, r.positionPath);
                if (!c) return c;
                r.children = std::move(c).value();
            }
            rows.push_back(std::move(r));
        }
        return rows;
    }

private:
    const AssemblyDocument& doc_;
    std::map<DefinitionId, Unit> cache_;
};

void totals(Bom& bom, const std::vector<std::pair<DefinitionId, int>>& parts, BomBuilder& b) {
    bom.totalMass = 0.0;
    for (const auto& [id, n] : parts) {
        const Unit u = b.unit(id).value();
        bom.totalVolume += u.volume * n;
        bom.totalParts += n;
        if (bom.totalMass && u.mass) *bom.totalMass += *u.mass * n;
        else bom.totalMass.reset();
    }
}

}  // namespace

Result<Bom> flatBom(const AssemblyDocument& document) {
    BomBuilder b(document);
    std::vector<std::pair<DefinitionId, int>> parts;
    if (auto s = b.countParts(document.root(), 1, parts); !s) return s.error();
    Bom bom;
    int pos = 0;
    for (const auto& [id, n] : parts) {
        auto u = b.unit(id);
        if (!u) return u.error();
        BomRow r = b.row(id, n, 0, u.value());
        r.position = ++pos;
        r.positionPath = std::to_string(pos);
        bom.rows.push_back(std::move(r));
    }
    totals(bom, parts, b);
    return bom;
}

Result<Bom> structuredBom(const AssemblyDocument& document) {
    BomBuilder b(document);
    std::vector<std::pair<DefinitionId, int>> parts;
    if (auto s = b.countParts(document.root(), 1, parts); !s) return s.error();
    for (const auto& [id, n] : parts)
        if (auto u = b.unit(id); !u) return u.error();
    Bom bom;
    auto rows = b.levelRows(document.root(), 0, {});
    if (!rows) return rows.error();
    bom.rows = std::move(rows).value();
    totals(bom, parts, b);
    return bom;
}

}  // namespace bendcore::assembly_v2
