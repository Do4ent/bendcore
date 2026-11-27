// Массивы компонентов: линейный, круговой, по отверстиям детали
// (docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 5).

#include <bendcore/assembly_v2/assembly_document.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <string>

namespace bendcore::assembly_v2 {

using kernel::Error;
using kernel::ErrorCode;
using kernel::Result;

namespace {

Status ok() { return std::monostate{}; }

bool finiteVec(const Vec3& v) { return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z()); }

// Отверстие: группа соосных цилиндрических граней одного радиуса.
struct Hole {
    Vec3 point;  // точка на оси (СК опорного вхождения)
    std::vector<std::uint32_t> faces;
};

// Ключ по смещению оси отверстия: «@dx,dy,dz» с округлением до 1e-6 мм.
std::string offsetKey(const Vec3& d) {
    char buf[96];
    auto r = [](double v) {
        const double x = std::round(v * 1e6) / 1e6;
        return x == 0.0 ? 0.0 : x;  // без «-0»
    };
    std::snprintf(buf, sizeof buf, "@%.6f,%.6f,%.6f", r(d.x()), r(d.y()), r(d.z()));
    return buf;
}

// Префикс причины висячей ссылки через экземпляр массива (снимается, когда экземпляр вернулся).
constexpr const char* kDangling = "dangling pattern instance: ";

}  // namespace

Result<PatternId> AssemblyDocument::addPattern(
    PatternKind kind, OccurrenceId seed, std::optional<OccurrenceId> reference,
    std::variant<LinearPatternSpec, CircularPatternSpec, FeaturePatternSpec> spec) {
    const auto seedParent = parentOf(seed);
    if (!seedParent || *seedParent != root())
        return Error{ErrorCode::InvalidInput, "pattern seed must be a top-level occurrence"};
    if (findOccurrence(seed)->pattern) return Error{ErrorCode::InvalidInput, "pattern seed is itself a pattern instance"};
    if (reference) {
        const auto rp = parentOf(*reference);
        if (!rp || *rp != root()) return Error{ErrorCode::InvalidInput, "pattern reference must be a top-level occurrence"};
        if (*reference == seed) return Error{ErrorCode::InvalidInput, "pattern reference must differ from the seed"};
    }
    ComponentPattern p;
    p.kind = kind;
    p.seed = seed;
    p.reference = reference;
    p.spec = std::move(spec);
    if (kind == PatternKind::Feature) {
        const auto& fs = std::get<FeaturePatternSpec>(p.spec);
        BoundElement b;
        if (auto s = bindSide(fs.seedHole, b, true); !s) return s.error();
        p.holeBound = b;
    }
    auto transforms = computePatternTransforms(p);
    if (!transforms) return transforms.error();
    p.id = nextPattern_++;
    patterns_.push_back(std::move(p));
    if (auto s = syncPatternInstances(patterns_.back(), std::move(transforms).value()); !s) {
        patterns_.pop_back();
        return s.error();
    }
    return patterns_.back().id;
}

namespace {

Status checkLinear(const LinearPatternSpec& spec) {
    if (spec.count < 1 || spec.count2 < 1) return Status::failure(ErrorCode::InvalidInput, "pattern counts must be >= 1");
    if (spec.count * spec.count2 < 2) return Status::failure(ErrorCode::InvalidInput, "pattern must have at least two members");
    if (!finiteVec(spec.direction) || spec.direction.norm() == 0.0 || !std::isfinite(spec.spacing))
        return Status::failure(ErrorCode::InvalidInput, "linear pattern needs a non-zero direction and a finite spacing");
    if (spec.count2 > 1 &&
        (!finiteVec(spec.direction2) || spec.direction2.norm() == 0.0 || !std::isfinite(spec.spacing2)))
        return Status::failure(ErrorCode::InvalidInput, "second pattern direction must be non-zero with a finite spacing");
    return ok();
}

Status checkCircular(const CircularPatternSpec& spec) {
    if (spec.count < 2) return Status::failure(ErrorCode::InvalidInput, "circular pattern needs count >= 2");
    if (!finiteVec(spec.axisPoint) || !finiteVec(spec.axisDirection) || spec.axisDirection.norm() == 0.0 ||
        !std::isfinite(spec.angleStep) || spec.angleStep == 0.0)
        return Status::failure(ErrorCode::InvalidInput, "circular pattern needs a valid axis and a non-zero angle step");
    return ok();
}

Status checkFeature(const FeaturePatternSpec& spec) {
    if (spec.seedHole.path.empty()) return Status::failure(ErrorCode::InvalidInput, "seed hole reference has an empty path");
    if (spec.seedHole.element.kind != ElementKind::Face)
        return Status::failure(ErrorCode::InvalidInput, spec.partPattern.empty()
                                                            ? "seed hole reference must be a cylindrical face"
                                                            : "seed reference of a part pattern must be a face");
    if (!(spec.tolerance > 0.0)) return Status::failure(ErrorCode::InvalidInput, "tolerance must be positive");
    if (spec.radius && !(*spec.radius > 0.0)) return Status::failure(ErrorCode::InvalidInput, "hole radius must be positive");
    return ok();
}

}  // namespace

Result<PatternId> AssemblyDocument::addLinearPattern(const LinearPatternSpec& spec) {
    if (auto s = checkLinear(spec); !s) return s.error();
    return addPattern(PatternKind::Linear, spec.seed, spec.reference, spec);
}

Result<PatternId> AssemblyDocument::addCircularPattern(const CircularPatternSpec& spec) {
    if (auto s = checkCircular(spec); !s) return s.error();
    return addPattern(PatternKind::Circular, spec.seed, spec.reference, spec);
}

Result<PatternId> AssemblyDocument::addFeaturePattern(const FeaturePatternSpec& spec) {
    if (auto s = checkFeature(spec); !s) return s.error();
    return addPattern(PatternKind::Feature, spec.seed, spec.seedHole.path[0], spec);
}

Status AssemblyDocument::editPattern(PatternId id,
                                     std::variant<LinearPatternSpec, CircularPatternSpec, FeaturePatternSpec> spec) {
    auto it = std::find_if(patterns_.begin(), patterns_.end(), [&](const ComponentPattern& p) { return p.id == id; });
    if (it == patterns_.end()) return Status::failure(ErrorCode::InvalidInput, "unknown pattern " + std::to_string(id));
    if (spec.index() != it->spec.index())
        return Status::failure(ErrorCode::InvalidInput, "pattern " + std::to_string(id) + " is a " + toString(it->kind) +
                                                            " pattern; remove it to change the kind");
    ComponentPattern next = *it;
    std::optional<OccurrenceId> reference;
    OccurrenceId seed = 0;
    if (const auto* l = std::get_if<LinearPatternSpec>(&spec)) {
        if (auto s = checkLinear(*l); !s) return s;
        seed = l->seed;
        reference = l->reference;
    } else if (const auto* c = std::get_if<CircularPatternSpec>(&spec)) {
        if (auto s = checkCircular(*c); !s) return s;
        seed = c->seed;
        reference = c->reference;
    } else {
        const auto& f = std::get<FeaturePatternSpec>(spec);
        if (auto s = checkFeature(f); !s) return s;
        seed = f.seed;
        reference = f.seedHole.path[0];
        BoundElement b;
        if (auto s = bindSide(f.seedHole, b, true); !s) return s;
        next.holeBound = b;
    }
    if (seed != it->seed)
        return Status::failure(ErrorCode::InvalidInput, "pattern " + std::to_string(id) + ": the seed cannot change");
    if (reference) {
        const auto rp = parentOf(*reference);
        if (!rp || *rp != root())
            return Status::failure(ErrorCode::InvalidInput, "pattern reference must be a top-level occurrence");
        if (*reference == seed) return Status::failure(ErrorCode::InvalidInput, "pattern reference must differ from the seed");
    }
    next.reference = reference;
    next.spec = std::move(spec);
    auto members = computePatternTransforms(next);
    if (!members) return members.error();
    *it = std::move(next);
    if (auto s = syncPatternInstances(*it, std::move(members).value()); !s) return s;
    refreshInstanceRefs();
    return ok();
}

Result<std::vector<AssemblyDocument::PatternMember>> AssemblyDocument::computePatternTransforms(
    const ComponentPattern& p) const {
    std::vector<PatternMember> out;
    if (const auto* l = std::get_if<LinearPatternSpec>(&p.spec)) {
        const Vec3 d1 = l->direction.normalized();
        const Vec3 d2 = l->count2 > 1 ? Vec3(l->direction2.normalized()) : Vec3::Zero();
        for (int j = 0; j < l->count2; ++j)
            for (int i = 0; i < l->count; ++i) {
                if (i == 0 && j == 0) continue;
                out.push_back({std::to_string(i) + "," + std::to_string(j),
                               Transform::fromTranslation(d1 * (i * l->spacing) + d2 * (j * l->spacing2))});
            }
        return out;
    }
    if (const auto* c = std::get_if<CircularPatternSpec>(&p.spec)) {
        for (int k = 1; k < c->count; ++k) {
            const Quat q(Eigen::AngleAxisd(k * c->angleStep, c->axisDirection.normalized()));
            Transform t;
            t.rotation = q;
            t.position = c->axisPoint - q * c->axisPoint;
            out.push_back({std::to_string(k), t});
        }
        return out;
    }
    const auto& f = std::get<FeaturePatternSpec>(p.spec);
    if (!p.holeBound) return Error{ErrorCode::InvalidInput, "feature pattern has no resolved seed hole"};
    if (!f.partPattern.empty()) {
        // По массиву операции детали: L = T·C·T⁻¹ в СК вхождения верхнего уровня.
        auto def = terminalDefinition(f.seedHole.path);
        if (!def) return def.error();
        auto toTop = placementInTopLevel(f.seedHole.path);
        if (!toTop) return toTop.error();
        const Definition& d = definitions_[def.value()];
        const PartFeaturePattern* pp = nullptr;
        for (const PartFeaturePattern& x : d.partPatterns)
            if (x.feature == f.partPattern) pp = &x;
        if (!pp) {
            std::string list;
            for (const PartFeaturePattern& x : d.partPatterns) list += (list.empty() ? "" : ", ") + x.feature;
            return Error{ErrorCode::InvalidInput, "part '" + d.name + "' has no pattern feature '" + f.partPattern +
                                                      "' (patterns: " + (list.empty() ? std::string("none") : list) + ")"};
        }
        const Transform T = toTop.value();
        const Transform Tinv = T.inverse();
        for (std::size_t k = 0; k < pp->transforms.size(); ++k) out.push_back({pp->keys.at(k), T * pp->transforms[k] * Tinv});
        return out;
    }
    auto seedEntity = entityForBound(f.seedHole, *p.holeBound);
    if (!seedEntity) return seedEntity.error();
    if (seedEntity.value().type != EntityType::Cylinder)
        return Error{ErrorCode::InvalidInput, "seed hole reference is not a cylindrical face"};
    const Vec3 d0 = seedEntity.value().direction;
    const double radius = f.radius.value_or(seedEntity.value().radius);
    const double tol = f.tolerance;
    auto def = terminalDefinition(f.seedHole.path);
    if (!def) return def.error();
    auto toTop = placementInTopLevel(f.seedHole.path);
    if (!toTop) return toTop.error();
    const Definition& d = definitions_[def.value()];

    std::vector<Hole> holes;
    std::optional<std::size_t> seedHole;
    for (std::uint32_t i = 0; i < d.topology->faces().size(); ++i) {
        if (d.topology->faces()[i].surface != kernel::SurfaceType::Cylinder) continue;
        auto e = entityFromIndex(*d.topology, ElementKind::Face, i, EntityUse::Auto);
        if (!e) continue;
        const Entity cyl = transformEntity(e.value(), toTop.value());
        if (std::abs(cyl.radius - radius) > tol * std::max(1.0, radius)) continue;
        if (d0.cross(cyl.direction).norm() > tol) continue;
        std::size_t g = holes.size();
        for (std::size_t h = 0; h < holes.size(); ++h) {
            const Vec3 w = cyl.point - holes[h].point;
            if ((w - d0 * d0.dot(w)).norm() <= tol * std::max(1.0, radius)) {
                g = h;
                break;
            }
        }
        if (g == holes.size()) holes.push_back({cyl.point, {}});
        holes[g].faces.push_back(i);
        if (i == p.holeBound->index) seedHole = g;
    }
    if (!seedHole)
        return Error{ErrorCode::InvalidInput, "seed hole face does not match the requested radius " + std::to_string(radius)};
    struct Offset {
        Vec3 delta;
        double dist;
        std::string key;
    };
    // Ключ отверстия — имя его первой грани в карте элементов, если оно есть и не повторяется
    // у других отверстий; иначе — смещение оси.
    std::vector<std::string> names(holes.size());
    if (d.elementMap) {
        for (std::size_t h = 0; h < holes.size(); ++h) {
            const std::uint32_t face = *std::min_element(holes[h].faces.begin(), holes[h].faces.end());
            if (face < d.elementMap->count(modeling::ElementKind::Face))
                names[h] = d.elementMap->record(modeling::ElementKind::Face, face).name;
        }
    }
    std::vector<Offset> offsets;
    for (std::size_t h = 0; h < holes.size(); ++h) {
        if (h == *seedHole) continue;
        const Vec3 w = holes[h].point - holes[*seedHole].point;
        const Vec3 delta = w - d0 * d0.dot(w);
        const bool unique = !names[h].empty() && std::count(names.begin(), names.end(), names[h]) == 1;
        offsets.push_back({delta, delta.norm(), unique ? names[h] : offsetKey(delta)});
    }
    // Порядок экземпляров: по расстоянию от исходного отверстия, затем по координатам.
    const double eps = tol * std::max(1.0, radius);
    std::sort(offsets.begin(), offsets.end(), [eps](const Offset& a, const Offset& b) {
        if (std::abs(a.dist - b.dist) > eps) return a.dist < b.dist;
        for (int k = 0; k < 3; ++k)
            if (std::abs(a.delta[k] - b.delta[k]) > eps) return a.delta[k] < b.delta[k];
        return false;
    });
    for (const Offset& o : offsets) out.push_back({o.key, Transform::fromTranslation(o.delta)});
    return out;
}

Status AssemblyDocument::syncPatternInstances(ComponentPattern& pattern, std::vector<PatternMember> members) {
    const Occurrence* seed = findOccurrence(pattern.seed);
    if (!seed) return Status::failure(ErrorCode::InvalidInput, "pattern seed no longer exists");
    const DefinitionId def = seed->definition;
    const std::string seedName = seed->name;
    // Файл без ключей (до ключей экземпляров) — ключи по порядку экземпляров.
    if (pattern.keys.size() != pattern.instances.size()) {
        pattern.keys.clear();
        for (std::size_t i = 0; i < pattern.instances.size(); ++i)
            pattern.keys.push_back(i < members.size() ? members[i].key : "#" + std::to_string(i));
    }
    // Экземпляр с тем же ключом сохраняет вхождение; исчезнувшие ключи удаляются.
    std::map<std::string, OccurrenceId> byKey;
    for (std::size_t i = 0; i < pattern.instances.size(); ++i) byKey.emplace(pattern.keys[i], pattern.instances[i]);
    std::vector<OccurrenceId> instances;
    std::vector<std::string> keys;
    std::set<OccurrenceId> kept;
    for (std::size_t i = 0; i < members.size(); ++i) {
        auto it = byKey.find(members[i].key);
        OccurrenceId id = 0;
        if (it != byKey.end() && !kept.count(it->second)) {
            id = it->second;
        } else {
            auto added = addOccurrence(def, Transform::identity(),
                                       seedName + "_P" + std::to_string(pattern.id) + "_" + std::to_string(i + 1));
            if (!added) return added.error();
            id = added.value();
            findOccurrenceMutable(id)->pattern = pattern.id;
        }
        kept.insert(id);
        instances.push_back(id);
        keys.push_back(members[i].key);
    }
    auto& occs = definitions_[root()].occurrences;
    for (OccurrenceId id : pattern.instances) {
        if (kept.count(id)) continue;
        occs.erase(std::remove_if(occs.begin(), occs.end(), [&](const Occurrence& o) { return o.id == id; }), occs.end());
    }
    pattern.instances = std::move(instances);
    pattern.keys = std::move(keys);
    pattern.transforms.clear();
    for (PatternMember& m : members) pattern.transforms.push_back(m.transform);
    const Transform seedPose = findOccurrence(pattern.seed)->placement;
    const Transform ref = pattern.reference ? findOccurrence(*pattern.reference)->placement : Transform::identity();
    const Transform refInv = ref.inverse();
    for (std::size_t i = 0; i < pattern.instances.size(); ++i) {
        Occurrence* o = findOccurrenceMutable(pattern.instances[i]);
        o->definition = def;
        o->placement = ref * pattern.transforms[i] * refInv * seedPose;
        o->placement.rotation.normalize();
        o->fixed = false;
    }
    return ok();
}

Status AssemblyDocument::updatePatterns() {
    std::optional<Error> first;
    for (ComponentPattern& p : patterns_) {
        if (p.holeBound) {
            const auto& f = std::get<FeaturePatternSpec>(p.spec);
            auto def = terminalDefinition(f.seedHole.path);
            if (!def) {
                if (!first) first = def.error();
                continue;
            }
            const Definition& d = definitions_[def.value()];
            if (d.id != p.holeBound->definition || d.revision != p.holeBound->revision) {
                auto b = remapSide(f.seedHole, *p.holeBound, d.id);
                if (!b) {
                    if (!first)
                        first = Error{b.error().code, "pattern " + std::to_string(p.id) +
                                                          ": seed hole lost: " + b.error().message};
                    continue;
                }
                p.holeBound = b.value();
            }
        }
        auto t = computePatternTransforms(p);
        if (!t) {
            if (!first) first = Error{t.error().code, "pattern " + std::to_string(p.id) + ": " + t.error().message};
            continue;
        }
        if (auto s = syncPatternInstances(p, std::move(t).value()); !s && !first) first = s.error();
    }
    refreshInstanceRefs();
    if (first) return *first;
    return ok();
}

std::optional<OccurrenceId> AssemblyDocument::patternInstance(PatternId pattern, const std::string& key) const {
    for (const ComponentPattern& p : patterns_) {
        if (p.id != pattern) continue;
        for (std::size_t i = 0; i < p.instances.size() && i < p.keys.size(); ++i)
            if (p.keys[i] == key) return p.instances[i];
    }
    return std::nullopt;
}

std::optional<PatternInstanceRef> AssemblyDocument::instanceRefOf(OccurrenceId occurrence) const {
    for (const ComponentPattern& p : patterns_)
        for (std::size_t i = 0; i < p.instances.size() && i < p.keys.size(); ++i)
            if (p.instances[i] == occurrence) return PatternInstanceRef{p.id, p.keys[i]};
    return std::nullopt;
}

Status AssemblyDocument::resolveInstanceRef(GeometryRef& ref) const {
    if (!ref.instance) return ok();
    auto occ = patternInstance(ref.instance->pattern, ref.instance->key);
    if (!occ) {
        const bool known = std::any_of(patterns_.begin(), patterns_.end(),
                                       [&](const ComponentPattern& p) { return p.id == ref.instance->pattern; });
        return Status::failure(ErrorCode::InvalidInput,
                               known ? "pattern " + std::to_string(ref.instance->pattern) + " has no instance '" +
                                           ref.instance->key + "'"
                                     : "pattern " + std::to_string(ref.instance->pattern) + " no longer exists");
    }
    if (ref.path.empty()) ref.path.push_back(*occ);
    else ref.path[0] = *occ;
    return ok();
}

void AssemblyDocument::refreshInstanceRefs() {
    for (DocumentMate& m : mates_) {
        std::string dangling;
        bool rebind = false;
        GeometryRef* sides[2] = {&m.spec.a, &m.spec.b};
        for (int k = 0; k < 2; ++k) {
            GeometryRef& r = *sides[k];
            if (!r.instance) continue;
            const OccurrenceId before = r.path.empty() ? 0 : r.path[0];
            if (auto s = resolveInstanceRef(r); !s) {
                if (dangling.empty()) dangling = s.error().message;
                continue;
            }
            if (r.path[0] != before) rebind = true;
        }
        if (!dangling.empty()) {
            // Висячая ссылка: сопряжение остаётся в документе и не решается, пока экземпляр не вернётся.
            m.brokenReason = std::string(kDangling) + dangling;
            continue;
        }
        const bool wasDangling = m.brokenReason.rfind(kDangling, 0) == 0;
        if (!wasDangling && !rebind) continue;
        bool okAll = true;
        std::string why;
        const bool rigid = m.spec.type == MateType::Rigid;
        for (int k = 0; k < 2; ++k) {
            if (!sides[k]->instance && !wasDangling) continue;
            BoundElement b;
            if (auto s = bindSide(*sides[k], b, !rigid); !s) {
                okAll = false;
                why = s.error().message;
                break;
            }
            m.bound[k] = b;
        }
        if (okAll && wasDangling) m.brokenReason.clear();
        else if (!okAll) m.brokenReason = std::string(kDangling) + why;
    }
}

Status AssemblyDocument::removePattern(PatternId id) {
    auto it = std::find_if(patterns_.begin(), patterns_.end(), [&](const ComponentPattern& p) { return p.id == id; });
    if (it == patterns_.end()) return Status::failure(ErrorCode::InvalidInput, "unknown pattern " + std::to_string(id));
    auto& occs = definitions_[root()].occurrences;
    const std::vector<OccurrenceId> inst = it->instances;
    occs.erase(std::remove_if(occs.begin(), occs.end(),
                              [&](const Occurrence& o) {
                                  return std::find(inst.begin(), inst.end(), o.id) != inst.end();
                              }),
               occs.end());
    patterns_.erase(it);
    refreshInstanceRefs();
    return ok();
}

}  // namespace bendcore::assembly_v2
