// Массивы компонентов: линейный, круговой, по отверстиям детали
// (docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 5).

#include <bendcore/assembly_v2/assembly_document.hpp>

#include <algorithm>
#include <cmath>
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

Result<PatternId> AssemblyDocument::addLinearPattern(const LinearPatternSpec& spec) {
    if (spec.count < 1 || spec.count2 < 1) return Error{ErrorCode::InvalidInput, "pattern counts must be >= 1"};
    if (spec.count * spec.count2 < 2) return Error{ErrorCode::InvalidInput, "pattern must have at least two members"};
    if (!finiteVec(spec.direction) || spec.direction.norm() == 0.0 || !std::isfinite(spec.spacing))
        return Error{ErrorCode::InvalidInput, "linear pattern needs a non-zero direction and a finite spacing"};
    if (spec.count2 > 1 &&
        (!finiteVec(spec.direction2) || spec.direction2.norm() == 0.0 || !std::isfinite(spec.spacing2)))
        return Error{ErrorCode::InvalidInput, "second pattern direction must be non-zero with a finite spacing"};
    return addPattern(PatternKind::Linear, spec.seed, spec.reference, spec);
}

Result<PatternId> AssemblyDocument::addCircularPattern(const CircularPatternSpec& spec) {
    if (spec.count < 2) return Error{ErrorCode::InvalidInput, "circular pattern needs count >= 2"};
    if (!finiteVec(spec.axisPoint) || !finiteVec(spec.axisDirection) || spec.axisDirection.norm() == 0.0 ||
        !std::isfinite(spec.angleStep) || spec.angleStep == 0.0)
        return Error{ErrorCode::InvalidInput, "circular pattern needs a valid axis and a non-zero angle step"};
    return addPattern(PatternKind::Circular, spec.seed, spec.reference, spec);
}

Result<PatternId> AssemblyDocument::addFeaturePattern(const FeaturePatternSpec& spec) {
    if (spec.seedHole.path.empty()) return Error{ErrorCode::InvalidInput, "seed hole reference has an empty path"};
    if (spec.seedHole.element.kind != ElementKind::Face)
        return Error{ErrorCode::InvalidInput, "seed hole reference must be a cylindrical face"};
    if (!(spec.tolerance > 0.0)) return Error{ErrorCode::InvalidInput, "tolerance must be positive"};
    if (spec.radius && !(*spec.radius > 0.0)) return Error{ErrorCode::InvalidInput, "hole radius must be positive"};
    return addPattern(PatternKind::Feature, spec.seed, spec.seedHole.path[0], spec);
}

Result<std::vector<Transform>> AssemblyDocument::computePatternTransforms(const ComponentPattern& p) const {
    std::vector<Transform> out;
    if (const auto* l = std::get_if<LinearPatternSpec>(&p.spec)) {
        const Vec3 d1 = l->direction.normalized();
        const Vec3 d2 = l->count2 > 1 ? Vec3(l->direction2.normalized()) : Vec3::Zero();
        for (int j = 0; j < l->count2; ++j)
            for (int i = 0; i < l->count; ++i) {
                if (i == 0 && j == 0) continue;
                out.push_back(Transform::fromTranslation(d1 * (i * l->spacing) + d2 * (j * l->spacing2)));
            }
        return out;
    }
    if (const auto* c = std::get_if<CircularPatternSpec>(&p.spec)) {
        for (int k = 1; k < c->count; ++k) {
            const Quat q(Eigen::AngleAxisd(k * c->angleStep, c->axisDirection.normalized()));
            Transform t;
            t.rotation = q;
            t.position = c->axisPoint - q * c->axisPoint;
            out.push_back(t);
        }
        return out;
    }
    const auto& f = std::get<FeaturePatternSpec>(p.spec);
    if (!p.holeBound) return Error{ErrorCode::InvalidInput, "feature pattern has no resolved seed hole"};
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
    };
    std::vector<Offset> offsets;
    for (std::size_t h = 0; h < holes.size(); ++h) {
        if (h == *seedHole) continue;
        const Vec3 w = holes[h].point - holes[*seedHole].point;
        const Vec3 delta = w - d0 * d0.dot(w);
        offsets.push_back({delta, delta.norm()});
    }
    // Порядок экземпляров: по расстоянию от исходного отверстия, затем по координатам.
    const double eps = tol * std::max(1.0, radius);
    std::sort(offsets.begin(), offsets.end(), [eps](const Offset& a, const Offset& b) {
        if (std::abs(a.dist - b.dist) > eps) return a.dist < b.dist;
        for (int k = 0; k < 3; ++k)
            if (std::abs(a.delta[k] - b.delta[k]) > eps) return a.delta[k] < b.delta[k];
        return false;
    });
    for (const Offset& o : offsets) out.push_back(Transform::fromTranslation(o.delta));
    return out;
}

Status AssemblyDocument::syncPatternInstances(ComponentPattern& pattern, std::vector<Transform> transforms) {
    const Occurrence* seed = findOccurrence(pattern.seed);
    if (!seed) return Status::failure(ErrorCode::InvalidInput, "pattern seed no longer exists");
    const DefinitionId def = seed->definition;
    const std::string seedName = seed->name;
    while (pattern.instances.size() > transforms.size()) {
        const OccurrenceId id = pattern.instances.back();
        pattern.instances.pop_back();
        auto& occs = definitions_[root()].occurrences;
        occs.erase(std::remove_if(occs.begin(), occs.end(), [&](const Occurrence& o) { return o.id == id; }),
                   occs.end());
    }
    while (pattern.instances.size() < transforms.size()) {
        const std::string name =
            seedName + "_P" + std::to_string(pattern.id) + "_" + std::to_string(pattern.instances.size() + 1);
        auto id = addOccurrence(def, Transform::identity(), name);
        if (!id) return id.error();
        findOccurrenceMutable(id.value())->pattern = pattern.id;
        pattern.instances.push_back(id.value());
    }
    pattern.transforms = std::move(transforms);
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
    if (first) return *first;
    return ok();
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
    return ok();
}

}  // namespace bendcore::assembly_v2
