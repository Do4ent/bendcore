// Документ сборки: структура, сопряжения по геометрии OCCT, решение, замена компонента,
// развёртка. Алгоритмы — docs/assembly/ASSEMBLY_DOCUMENT.md.

#include <bendcore/assembly_v2/assembly_document.hpp>

#include <bendcore/kernel/shape.hpp>

#include <Standard_Failure.hxx>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace bendcore::assembly_v2 {

using kernel::Error;
using kernel::ErrorCode;
using kernel::Result;

namespace {

Status ok() { return std::monostate{}; }

Status fail(ErrorCode code, std::string message) { return Status::failure(code, std::move(message)); }

std::string occName(OccurrenceId id) { return "occurrence " + std::to_string(id); }

Result<std::shared_ptr<const kernel::TopologyIndex>> buildTopology(const TopoDS_Shape& shape) {
    auto index = kernel::TopologyIndex::build(kernel::Shape(shape));
    if (!index) return index.error();
    return std::make_shared<const kernel::TopologyIndex>(std::move(index).value());
}

Status checkPartData(const PartData& data) {
    if (data.shape.IsNull()) return fail(ErrorCode::InvalidInput, "part shape is null");
    if (data.elementMap && !data.elementMap->shape().IsSame(data.shape))
        return fail(ErrorCode::InvalidInput, "element map belongs to a different shape than the part");
    return ok();
}

bool pathContains(const OccurrencePath& path, OccurrenceId id) {
    return std::find(path.begin(), path.end(), id) != path.end();
}

double shapeScale(const TopoDS_Shape& shape) {
    auto box = kernel::Shape(shape).boundingBox();
    if (!box) return 1.0;
    const auto& b = box.value();
    const double d = std::sqrt((b.max[0] - b.min[0]) * (b.max[0] - b.min[0]) +
                               (b.max[1] - b.min[1]) * (b.max[1] - b.min[1]) +
                               (b.max[2] - b.min[2]) * (b.max[2] - b.min[2]));
    return d > 0.0 ? d : 1.0;
}

}  // namespace

// ---------------------------------------------------------------- MateSpec

MateSpec MateSpec::coincident(GeometryRef a, GeometryRef b, Alignment al) {
    MateSpec m;
    m.type = MateType::Coincident;
    m.a = std::move(a);
    m.b = std::move(b);
    m.alignment = al;
    return m;
}
MateSpec MateSpec::concentric(GeometryRef a, GeometryRef b, Alignment al) {
    MateSpec m = coincident(std::move(a), std::move(b), al);
    m.type = MateType::Concentric;
    return m;
}
MateSpec MateSpec::distance(GeometryRef a, GeometryRef b, double value, Alignment al) {
    MateSpec m = coincident(std::move(a), std::move(b), al);
    m.type = MateType::Distance;
    m.value = value;
    return m;
}
MateSpec MateSpec::angle(GeometryRef a, GeometryRef b, double radians) {
    MateSpec m = coincident(std::move(a), std::move(b));
    m.type = MateType::Angle;
    m.value = radians;
    return m;
}
MateSpec MateSpec::parallel(GeometryRef a, GeometryRef b, Alignment al) {
    MateSpec m = coincident(std::move(a), std::move(b), al);
    m.type = MateType::Parallel;
    return m;
}
MateSpec MateSpec::perpendicular(GeometryRef a, GeometryRef b) {
    MateSpec m = coincident(std::move(a), std::move(b));
    m.type = MateType::Perpendicular;
    return m;
}
MateSpec MateSpec::tangent(GeometryRef a, GeometryRef b, Alignment side, bool internal) {
    MateSpec m = coincident(std::move(a), std::move(b), side);
    m.type = MateType::Tangent;
    m.internalTangency = internal;
    return m;
}
MateSpec MateSpec::rigid(OccurrenceId a, OccurrenceId b) {
    MateSpec m;
    m.type = MateType::Rigid;
    m.a.path = {a};
    m.b.path = {b};
    return m;
}

const char* toString(PatternKind kind) noexcept {
    switch (kind) {
        case PatternKind::Linear: return "Linear";
        case PatternKind::Circular: return "Circular";
        case PatternKind::Feature: return "Feature";
    }
    return "Unknown";
}

// ---------------------------------------------------------------- структура

AssemblyDocument::AssemblyDocument(std::string name) {
    Definition root;
    root.id = 0;
    root.kind = DefinitionKind::Assembly;
    root.name = name.empty() ? std::string("Assembly") : std::move(name);
    definitions_.push_back(std::move(root));
}

Result<DefinitionId> AssemblyDocument::addPart(std::string name, PartData data, PartProperties properties) {
    if (auto c = checkPartData(data); !c) return c.error();
    if (properties.density && !(*properties.density >= 0.0))
        return Error{ErrorCode::InvalidInput, "density must be >= 0"};
    auto topo = buildTopology(data.shape);
    if (!topo) return topo.error();
    Definition d;
    d.id = static_cast<DefinitionId>(definitions_.size());
    d.kind = DefinitionKind::Part;
    d.name = name.empty() ? "Part" + std::to_string(d.id) : std::move(name);
    d.properties = std::move(properties);
    d.shape = data.shape;
    d.elementMap = std::move(data.elementMap);
    d.topology = topo.value();
    definitions_.push_back(std::move(d));
    return definitions_.back().id;
}

Result<DefinitionId> AssemblyDocument::addPartFromSource(std::string name, std::string source,
                                                         const PartResolver& resolver, PartProperties properties) {
    if (!resolver) return Error{ErrorCode::InvalidInput, "part resolver is empty"};
    auto data = resolver(source);
    if (!data) return Error{data.error().code, "cannot load part '" + source + "': " + data.error().message};
    auto id = addPart(std::move(name), std::move(data).value(), std::move(properties));
    if (!id) return id;
    definitions_[id.value()].source = std::move(source);
    return id;
}

Result<DefinitionId> AssemblyDocument::addSubassembly(std::string name, PartProperties properties) {
    Definition d;
    d.id = static_cast<DefinitionId>(definitions_.size());
    d.kind = DefinitionKind::Assembly;
    d.name = name.empty() ? "Subassembly" + std::to_string(d.id) : std::move(name);
    d.properties = std::move(properties);
    definitions_.push_back(std::move(d));
    return definitions_.back().id;
}

Status AssemblyDocument::setProperties(DefinitionId id, PartProperties properties) {
    if (!hasDefinition(id)) return fail(ErrorCode::InvalidInput, "unknown definition " + std::to_string(id));
    if (properties.density && !(*properties.density >= 0.0)) return fail(ErrorCode::InvalidInput, "density must be >= 0");
    definitions_[id].properties = std::move(properties);
    return ok();
}

Status AssemblyDocument::setDefinitionColor(DefinitionId id, std::optional<kernel::AssemblyColor> color) {
    if (!hasDefinition(id)) return fail(ErrorCode::InvalidInput, "unknown definition " + std::to_string(id));
    definitions_[id].color = color;
    return ok();
}

bool AssemblyDocument::containsDefinition(DefinitionId container, DefinitionId target) const {
    if (container == target) return true;
    for (const Occurrence& o : definitions_[container].occurrences)
        if (definitions_[o.definition].kind == DefinitionKind::Assembly && containsDefinition(o.definition, target))
            return true;
    return false;
}

Result<OccurrenceId> AssemblyDocument::addOccurrence(DefinitionId definition, const Transform& placement,
                                                     std::string name, std::optional<DefinitionId> parent) {
    const DefinitionId p = parent.value_or(root());
    if (!hasDefinition(definition)) return Error{ErrorCode::InvalidInput, "unknown definition " + std::to_string(definition)};
    if (!hasDefinition(p)) return Error{ErrorCode::InvalidInput, "unknown parent definition " + std::to_string(p)};
    if (definitions_[p].kind != DefinitionKind::Assembly)
        return Error{ErrorCode::InvalidInput, "parent '" + definitions_[p].name + "' is not an assembly"};
    if (definition == root()) return Error{ErrorCode::InvalidInput, "the root assembly cannot be inserted"};
    if (definitions_[definition].kind == DefinitionKind::Assembly && containsDefinition(definition, p))
        return Error{ErrorCode::InvalidInput, "inserting '" + definitions_[definition].name + "' into '" +
                                                  definitions_[p].name + "' creates a cycle"};
    Occurrence o;
    o.id = nextOccurrence_++;
    o.definition = definition;
    o.placement = placement;
    o.placement.rotation.normalize();
    if (name.empty()) {
        int n = 1;
        for (const Occurrence& x : definitions_[p].occurrences)
            if (x.definition == definition) ++n;
        name = definitions_[definition].name + ":" + std::to_string(n);
    }
    o.name = std::move(name);
    definitions_[p].occurrences.push_back(std::move(o));
    return definitions_[p].occurrences.back().id;
}

Occurrence* AssemblyDocument::findOccurrenceMutable(OccurrenceId id) {
    for (Definition& d : definitions_)
        for (Occurrence& o : d.occurrences)
            if (o.id == id) return &o;
    return nullptr;
}

const Occurrence* AssemblyDocument::findOccurrence(OccurrenceId id) const {
    for (const Definition& d : definitions_)
        for (const Occurrence& o : d.occurrences)
            if (o.id == id) return &o;
    return nullptr;
}

std::optional<DefinitionId> AssemblyDocument::parentOf(OccurrenceId id) const {
    for (const Definition& d : definitions_)
        for (const Occurrence& o : d.occurrences)
            if (o.id == id) return d.id;
    return std::nullopt;
}

Status AssemblyDocument::removeOccurrence(OccurrenceId id) {
    const auto parent = parentOf(id);
    if (!parent) return fail(ErrorCode::InvalidInput, "unknown " + occName(id));
    const Occurrence* o = findOccurrence(id);
    if (o->pattern) return fail(ErrorCode::InvalidInput, occName(id) + " is a pattern instance; remove the pattern");
    std::vector<PatternId> dropPatterns;
    for (const ComponentPattern& p : patterns_)
        if (p.seed == id || p.reference == std::optional<OccurrenceId>(id)) dropPatterns.push_back(p.id);
    for (PatternId p : dropPatterns) removePattern(p);
    mates_.erase(std::remove_if(mates_.begin(), mates_.end(),
                                [&](const DocumentMate& m) {
                                    return pathContains(m.spec.a.path, id) || pathContains(m.spec.b.path, id);
                                }),
                 mates_.end());
    auto& occs = definitions_[*parent].occurrences;
    occs.erase(std::remove_if(occs.begin(), occs.end(), [&](const Occurrence& x) { return x.id == id; }), occs.end());
    return ok();
}

Status AssemblyDocument::setPlacement(OccurrenceId id, const Transform& placement) {
    Occurrence* o = findOccurrenceMutable(id);
    if (!o) return fail(ErrorCode::InvalidInput, "unknown " + occName(id));
    if (o->pattern) return fail(ErrorCode::InvalidInput, occName(id) + " is placed by its pattern");
    o->placement = placement;
    o->placement.rotation.normalize();
    return ok();
}

Status AssemblyDocument::setFixed(OccurrenceId id, bool fixed) {
    const auto parent = parentOf(id);
    if (!parent) return fail(ErrorCode::InvalidInput, "unknown " + occName(id));
    if (*parent != root()) return fail(ErrorCode::InvalidInput, "only top-level occurrences can be fixed");
    findOccurrenceMutable(id)->fixed = fixed;
    return ok();
}

Status AssemblyDocument::setOccurrenceColor(OccurrenceId id, std::optional<kernel::AssemblyColor> color) {
    Occurrence* o = findOccurrenceMutable(id);
    if (!o) return fail(ErrorCode::InvalidInput, "unknown " + occName(id));
    o->color = color;
    return ok();
}

Result<DefinitionId> AssemblyDocument::terminalDefinition(const OccurrencePath& path) const {
    if (path.empty()) return Error{ErrorCode::InvalidInput, "occurrence path is empty"};
    DefinitionId current = root();
    for (OccurrenceId id : path) {
        const auto& occs = definitions_[current].occurrences;
        auto it = std::find_if(occs.begin(), occs.end(), [&](const Occurrence& o) { return o.id == id; });
        if (it == occs.end())
            return Error{ErrorCode::InvalidInput,
                         occName(id) + " is not part of '" + definitions_[current].name + "'"};
        current = it->definition;
    }
    return current;
}

Result<Transform> AssemblyDocument::placementInTopLevel(const OccurrencePath& path) const {
    if (auto t = terminalDefinition(path); !t) return t.error();
    Transform acc = Transform::identity();
    for (std::size_t k = 1; k < path.size(); ++k) acc = acc * findOccurrence(path[k])->placement;
    return acc;
}

Result<Transform> AssemblyDocument::worldPlacement(const OccurrencePath& path) const {
    auto local = placementInTopLevel(path);
    if (!local) return local;
    return findOccurrence(path[0])->placement * local.value();
}

// ---------------------------------------------------------------- сопряжения

Result<std::uint32_t> AssemblyDocument::resolveIndex(const GeometryRef& ref) const {
    auto def = terminalDefinition(ref.path);
    if (!def) return def.error();
    const Definition& d = definitions_[def.value()];
    if (d.kind != DefinitionKind::Part)
        return Error{ErrorCode::InvalidInput, "path ends at assembly '" + d.name + "', not at a part"};
    const std::size_t n = elementCount(*d.topology, ref.element.kind);
    if (ref.element.index) {
        if (*ref.element.index >= n)
            return Error{ErrorCode::InvalidInput, std::string(modeling::toString(ref.element.kind)) + " index " +
                                                      std::to_string(*ref.element.index) + " is out of range in '" +
                                                      d.name + "' (" + std::to_string(n) + ")"};
        return *ref.element.index;
    }
    if (ref.element.name.empty()) return Error{ErrorCode::InvalidInput, "element reference has neither index nor name"};
    if (!d.elementMap)
        return Error{ErrorCode::InvalidInput, "part '" + d.name + "' has no element map; cannot resolve name '" +
                                                  ref.element.name + "'"};
    if (auto i = d.elementMap->find(ref.element.kind, ref.element.name)) return static_cast<std::uint32_t>(*i);
    modeling::TopoReference tr{ref.element.name, ref.element.kind, {}};
    modeling::Resolution r = modeling::resolveReference(tr, *d.elementMap);
    if (r.method != modeling::ResolveMethod::Broken && r.indices.size() == 1)
        return static_cast<std::uint32_t>(r.indices[0]);
    if (r.indices.size() > 1)
        return Error{ErrorCode::InvalidInput, "name '" + ref.element.name + "' resolves to " +
                                                  std::to_string(r.indices.size()) + " elements in '" + d.name + "'"};
    return Error{ErrorCode::InvalidInput, "name '" + ref.element.name + "' not found in '" + d.name + "': " + r.detail};
}

Result<Entity> AssemblyDocument::entityForBound(const GeometryRef& ref, const BoundElement& bound) const {
    auto def = terminalDefinition(ref.path);
    if (!def) return def.error();
    const Definition& d = definitions_[def.value()];
    if (d.kind != DefinitionKind::Part)
        return Error{ErrorCode::InvalidInput, "path ends at assembly '" + d.name + "', not at a part"};
    auto e = entityFromIndex(*d.topology, ref.element.kind, bound.index, ref.use);
    if (!e) return e;
    auto t = placementInTopLevel(ref.path);
    if (!t) return t.error();
    return transformEntity(e.value(), t.value());
}

Result<Entity> AssemblyDocument::resolveEntity(const GeometryRef& ref) const {
    auto idx = resolveIndex(ref);
    if (!idx) return idx.error();
    BoundElement b;
    b.index = idx.value();
    return entityForBound(ref, b);
}

Result<Entity> AssemblyDocument::worldEntity(const GeometryRef& ref) const {
    auto e = resolveEntity(ref);
    if (!e) return e;
    return transformEntity(e.value(), findOccurrence(ref.path[0])->placement);
}

Status AssemblyDocument::bindSide(const GeometryRef& ref, BoundElement& bound, bool requireElement) const {
    auto def = terminalDefinition(ref.path);
    if (!def) return def.error();
    if (!requireElement) {
        bound.definition = def.value();
        bound.revision = definitions_[def.value()].revision;
        return ok();
    }
    auto idx = resolveIndex(ref);
    if (!idx) return idx.error();
    const Definition& d = definitions_[def.value()];
    bound.index = idx.value();
    bound.definition = d.id;
    bound.revision = d.revision;
    auto sig = modeling::computeSignature(elementAt(*d.topology, ref.element.kind, bound.index));
    bound.signature = sig ? sig.value() : modeling::GeometricSignature{};
    if (auto e = entityForBound(ref, bound); !e) return e.error();
    return ok();
}

Result<Mate> AssemblyDocument::solverMate(const DocumentMate& mate, ComponentId a, ComponentId b) const {
    const MateSpec& s = mate.spec;
    if (s.type == MateType::Rigid) return Mate::rigid(a, b, mate.rigidRelative);
    auto ea = entityForBound(s.a, mate.bound[0]);
    if (!ea) return ea.error();
    auto eb = entityForBound(s.b, mate.bound[1]);
    if (!eb) return eb.error();
    Mate m;
    m.type = s.type;
    m.a = Feature{a, ea.value()};
    m.b = Feature{b, eb.value()};
    m.value = s.value;
    m.alignment = s.alignment;
    m.internalTangency = s.internalTangency;
    return m;
}

Result<MateId> AssemblyDocument::addMate(MateSpec spec) {
    const bool rigid = spec.type == MateType::Rigid;
    for (const GeometryRef* r : {&spec.a, &spec.b}) {
        if (r->path.empty()) return Error{ErrorCode::InvalidInput, "mate reference has an empty occurrence path"};
        const auto parent = parentOf(r->path[0]);
        if (!parent || *parent != root())
            return Error{ErrorCode::InvalidInput, occName(r->path[0]) + " is not a top-level occurrence"};
        if (findOccurrence(r->path[0])->pattern)
            return Error{ErrorCode::InvalidInput, occName(r->path[0]) +
                                                      " is a pattern instance; mate the pattern seed instead"};
    }
    if (rigid) {
        spec.a.path.resize(1);
        spec.b.path.resize(1);
    }
    DocumentMate m;
    m.spec = std::move(spec);
    if (auto s = bindSide(m.spec.a, m.bound[0], !rigid); !s) return s.error();
    if (auto s = bindSide(m.spec.b, m.bound[1], !rigid); !s) return s.error();
    if (rigid) {
        const Transform pa = findOccurrence(m.spec.a.path[0])->placement;
        const Transform pb = findOccurrence(m.spec.b.path[0])->placement;
        m.rigidRelative = pa.inverse() * pb;
    }
    const bool same = m.spec.a.path[0] == m.spec.b.path[0];
    auto sm = solverMate(m, 0, same ? 0 : 1);
    if (!sm) return sm.error();
    if (auto p = prepareMate(sm.value()); !p) return p.error();
    m.id = nextMate_++;
    mates_.push_back(std::move(m));
    return mates_.back().id;
}

Status AssemblyDocument::removeMate(MateId id) {
    auto it = std::find_if(mates_.begin(), mates_.end(), [&](const DocumentMate& m) { return m.id == id; });
    if (it == mates_.end()) return fail(ErrorCode::InvalidInput, "unknown mate " + std::to_string(id));
    mates_.erase(it);
    return ok();
}

const DocumentMate* AssemblyDocument::findMate(MateId id) const {
    for (const DocumentMate& m : mates_)
        if (m.id == id) return &m;
    return nullptr;
}

// ---------------------------------------------------------------- решение

Result<AssemblyDocument::SolverModel> AssemblyDocument::buildSolverModel() const {
    SolverModel model;
    std::map<OccurrenceId, ComponentId> comp;
    for (const Occurrence& o : topLevel()) {
        if (o.pattern) continue;
        comp[o.id] = model.assembly.addComponent(o.name, o.placement, o.fixed);
        model.occurrenceOf.push_back(o.id);
    }
    for (const DocumentMate& m : mates_) {
        if (m.broken()) {
            model.brokenMates.push_back(m.id);
            continue;
        }
        auto ia = comp.find(m.spec.a.path[0]);
        auto ib = comp.find(m.spec.b.path[0]);
        if (ia == comp.end() || ib == comp.end()) {
            model.rejected.emplace_back(m.id, "mate references an occurrence that is not a solver component");
            continue;
        }
        auto sm = solverMate(m, ia->second, ib->second);
        if (!sm) {
            model.rejected.emplace_back(m.id, sm.error().message);
            continue;
        }
        auto id = model.assembly.addMate(sm.value());
        if (!id) {
            model.rejected.emplace_back(m.id, id.error().message);
            continue;
        }
        model.documentMateOf[id.value()] = m.id;
    }
    return model;
}

namespace {

DocumentSolveReport translate(const AssemblyDocument::SolverModel& model, SolveResult result) {
    DocumentSolveReport rep;
    auto mapMates = [&](const std::vector<MateId>& ids) {
        std::vector<MateId> out;
        for (MateId id : ids) out.push_back(model.documentMateOf.at(id));
        return out;
    };
    for (const SubsystemReport& s : result.subsystems) {
        DocumentSubsystem d;
        for (ComponentId c : s.components) d.occurrences.push_back(model.occurrenceOf[c]);
        d.mates = mapMates(s.mates);
        d.status = s.status;
        d.dof = s.dof;
        d.rank = s.rank;
        d.redundantMates = mapMates(s.redundantMates);
        d.conflictingMates = mapMates(s.conflictingMates);
        rep.subsystems.push_back(std::move(d));
    }
    rep.brokenMates = model.brokenMates;
    for (const auto& r : model.rejected) rep.brokenMates.push_back(r.first);
    rep.totalDof = result.totalDof;
    rep.residualNorm = result.residualNorm;
    rep.success = result.success();
    rep.solver = std::move(result);
    return rep;
}

}  // namespace

Result<DocumentSolveReport> AssemblyDocument::solve(const SolverOptions& options) {
    auto model = buildSolverModel();
    if (!model) return model.error();
    for (const auto& [id, reason] : model.value().rejected)
        for (DocumentMate& m : mates_)
            if (m.id == id) m.brokenReason = reason;
    SolveResult r = assembly_v2::solve(model.value().assembly, options);
    for (std::size_t i = 0; i < model.value().occurrenceOf.size(); ++i)
        findOccurrenceMutable(model.value().occurrenceOf[i])->placement = r.poses[i];
    if (auto u = updatePatterns(); !u) return u.error();
    return translate(model.value(), std::move(r));
}

Result<DocumentSolveReport> AssemblyDocument::drag(OccurrenceId occurrence, const Vec3& localPoint,
                                                   const Vec3& target, const SolverOptions& options) {
    auto model = buildSolverModel();
    if (!model) return model.error();
    const auto& occs = model.value().occurrenceOf;
    auto it = std::find(occs.begin(), occs.end(), occurrence);
    if (it == occs.end())
        return Error{ErrorCode::InvalidInput, occName(occurrence) + " is not a top-level occurrence that can be dragged"};
    DragGoal goal;
    goal.component = static_cast<ComponentId>(it - occs.begin());
    goal.localPoint = localPoint;
    goal.target = target;
    auto r = assembly_v2::drag(model.value().assembly, goal, options);
    if (!r) return r.error();
    for (std::size_t i = 0; i < occs.size(); ++i) findOccurrenceMutable(occs[i])->placement = r.value().poses[i];
    if (auto u = updatePatterns(); !u) return u.error();
    return translate(model.value(), std::move(r).value());
}

// ---------------------------------------------------------------- замена

Result<BoundElement> AssemblyDocument::remapSide(const GeometryRef& ref, const BoundElement& old,
                                                 DefinitionId newDefinition) const {
    const Definition& d = definitions_[newDefinition];
    if (d.kind != DefinitionKind::Part)
        return Error{ErrorCode::InvalidInput, "path ends at assembly '" + d.name + "', not at a part"};
    const ElementKind kind = ref.element.kind;
    const std::size_t n = elementCount(*d.topology, kind);
    std::optional<std::uint32_t> found;
    std::string why;
    if (!ref.element.name.empty() && d.elementMap) {
        modeling::TopoReference tr{ref.element.name, kind, old.signature};
        modeling::Resolution r = modeling::resolveReference(tr, *d.elementMap);
        if (r.method != modeling::ResolveMethod::Broken && r.indices.size() == 1)
            found = static_cast<std::uint32_t>(r.indices[0]);
        else if (r.indices.size() > 1)
            why = "name '" + ref.element.name + "' resolves to " + std::to_string(r.indices.size()) + " elements";
        else
            why = "name '" + ref.element.name + "': " + r.detail;
    } else {
        const double scale = shapeScale(d.shape);
        if (old.index < n) {
            auto sig = modeling::computeSignature(elementAt(*d.topology, kind, old.index));
            if (!old.signature.valid ||
                (sig && std::isfinite(modeling::signatureDistance(old.signature, sig.value(), scale))))
                found = old.index;
        }
        if (!found && old.signature.valid) {
            double best = std::numeric_limits<double>::infinity(), second = best;
            std::uint32_t bestIndex = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
                auto sig = modeling::computeSignature(elementAt(*d.topology, kind, i));
                if (!sig) continue;
                const double dist = modeling::signatureDistance(old.signature, sig.value(), scale);
                if (dist < best) {
                    second = best;
                    best = dist;
                    bestIndex = i;
                } else if (dist < second) {
                    second = dist;
                }
            }
            const modeling::ResolveOptions ro;
            if (best <= ro.maxSignatureDistance && best < ro.ambiguityRatio * second) {
                found = bestIndex;
            } else {
                std::ostringstream s;
                s << "no " << modeling::toString(kind) << " of '" << d.name << "' matches the referenced "
                  << old.signature.geometry << " (closest signature distance " << best;
                if (std::isfinite(second)) s << ", next " << second;
                s << ")";
                why = s.str();
            }
        } else if (!found) {
            why = std::string(modeling::toString(kind)) + " index " + std::to_string(old.index) +
                  " does not exist in '" + d.name + "'";
        }
    }
    if (!found) return Error{ErrorCode::InvalidInput, why};
    BoundElement b;
    b.index = *found;
    b.definition = d.id;
    b.revision = d.revision;
    auto sig = modeling::computeSignature(elementAt(*d.topology, kind, b.index));
    b.signature = sig ? sig.value() : modeling::GeometricSignature{};
    if (auto e = entityForBound(ref, b); !e) return e.error();
    return b;
}

void AssemblyDocument::revalidateMates(ReplaceReport& report) {
    for (DocumentMate& m : mates_) {
        const bool rigid = m.spec.type == MateType::Rigid;
        bool remapped = false;
        std::string reason;
        std::array<BoundElement, 2> nb = m.bound;
        const GeometryRef* sides[2] = {&m.spec.a, &m.spec.b};
        for (int k = 0; k < 2 && reason.empty(); ++k) {
            auto def = terminalDefinition(sides[k]->path);
            if (!def) {
                reason = def.error().message;
                break;
            }
            if (rigid) continue;
            const Definition& d = definitions_[def.value()];
            if (d.id == m.bound[k].definition && d.revision == m.bound[k].revision && !m.broken()) continue;
            auto b = remapSide(*sides[k], m.bound[k], d.id);
            if (!b) {
                reason = b.error().message;
                break;
            }
            nb[k] = b.value();
            remapped = true;
        }
        if (reason.empty() && remapped) {
            DocumentMate trial = m;
            trial.bound = nb;
            const bool same = m.spec.a.path[0] == m.spec.b.path[0];
            auto sm = solverMate(trial, 0, same ? 0 : 1);
            if (!sm) reason = sm.error().message;
            else if (auto p = prepareMate(sm.value()); !p) reason = p.error().message;
        }
        if (!reason.empty()) {
            m.brokenReason = reason;
            report.broken.emplace_back(m.id, reason);
        } else if (remapped) {
            m.bound = nb;
            m.brokenReason.clear();
            // Ссылка по имени следует за элементом (новое основное имя).
            for (int k = 0; k < 2; ++k) {
                GeometryRef& ref = k == 0 ? m.spec.a : m.spec.b;
                const Definition& d = definitions_[nb[k].definition];
                if (!ref.element.name.empty() && d.elementMap)
                    ref.element.name = d.elementMap->record(ref.element.kind, nb[k].index).name;
                else if (ref.element.index)
                    ref.element.index = nb[k].index;
            }
            report.remapped.push_back(m.id);
        } else if (m.broken()) {
            report.broken.emplace_back(m.id, m.brokenReason);
        } else {
            report.kept.push_back(m.id);
        }
    }
}

Result<ReplaceReport> AssemblyDocument::replaceOccurrenceDefinition(OccurrenceId id, DefinitionId definition) {
    Occurrence* o = findOccurrenceMutable(id);
    if (!o) return Error{ErrorCode::InvalidInput, "unknown " + occName(id)};
    if (o->pattern) return Error{ErrorCode::InvalidInput, occName(id) + " is a pattern instance; replace the seed"};
    if (!hasDefinition(definition) || definition == root())
        return Error{ErrorCode::InvalidInput, "invalid replacement definition " + std::to_string(definition)};
    const Definition& nd = definitions_[definition];
    if (nd.kind != definitions_[o->definition].kind)
        return Error{ErrorCode::InvalidInput, "replacement must be of the same kind (part or assembly)"};
    const DefinitionId parent = *parentOf(id);
    if (nd.kind == DefinitionKind::Assembly && containsDefinition(definition, parent))
        return Error{ErrorCode::InvalidInput, "replacement creates a cycle of subassemblies"};
    o->definition = definition;
    ReplaceReport report;
    for (ComponentPattern& p : patterns_) {
        if (p.seed == id) {
            for (OccurrenceId inst : p.instances) findOccurrenceMutable(inst)->definition = definition;
            report.patternsUpdated.push_back(p.id);
        } else if (p.reference == std::optional<OccurrenceId>(id)) {
            report.patternsUpdated.push_back(p.id);
        }
    }
    revalidateMates(report);
    if (auto u = updatePatterns(); !u) return u.error();
    return report;
}

Result<ReplaceReport> AssemblyDocument::replacePartGeometry(DefinitionId id, PartData data) {
    if (!hasDefinition(id) || definitions_[id].kind != DefinitionKind::Part)
        return Error{ErrorCode::InvalidInput, "definition " + std::to_string(id) + " is not a part"};
    if (auto c = checkPartData(data); !c) return c.error();
    auto topo = buildTopology(data.shape);
    if (!topo) return topo.error();
    Definition& d = definitions_[id];
    d.shape = data.shape;
    d.elementMap = std::move(data.elementMap);
    d.topology = topo.value();
    d.faceColors.clear();
    ++d.revision;
    ReplaceReport report;
    for (const ComponentPattern& p : patterns_)
        if (p.holeBound && p.holeBound->definition == id) report.patternsUpdated.push_back(p.id);
    revalidateMates(report);
    if (auto u = updatePatterns(); !u) return u.error();
    return report;
}

Result<ReplaceReport> AssemblyDocument::reloadPart(DefinitionId id, const PartResolver& resolver) {
    if (!hasDefinition(id) || definitions_[id].kind != DefinitionKind::Part)
        return Error{ErrorCode::InvalidInput, "definition " + std::to_string(id) + " is not a part"};
    if (definitions_[id].source.empty())
        return Error{ErrorCode::InvalidInput, "part '" + definitions_[id].name + "' has no source document"};
    if (!resolver) return Error{ErrorCode::InvalidInput, "part resolver is empty"};
    auto data = resolver(definitions_[id].source);
    if (!data) return data.error();
    return replacePartGeometry(id, std::move(data).value());
}

// ---------------------------------------------------------------- развёртка

void AssemblyDocument::collectPlaced(DefinitionId def, const Transform& world, OccurrencePath& path,
                                     const std::string& prefix, const std::map<OccurrenceId, Transform>* overrides,
                                     std::vector<PlacedOccurrence>& out) const {
    for (const Occurrence& o : definitions_[def].occurrences) {
        Transform w = world * o.placement;
        if (path.empty() && overrides) {
            auto it = overrides->find(o.id);
            if (it != overrides->end()) w = it->second;
        }
        path.push_back(o.id);
        const std::string name = prefix.empty() ? o.name : prefix + "/" + o.name;
        const Definition& d = definitions_[o.definition];
        if (d.kind == DefinitionKind::Part) {
            PlacedOccurrence p;
            p.path = path;
            p.definition = d.id;
            p.pathName = name;
            p.world = w;
            p.shape = d.shape.Moved(toLocation(w));
            out.push_back(std::move(p));
        } else {
            collectPlaced(d.id, w, path, name, overrides, out);
        }
        path.pop_back();
    }
}

Result<std::vector<PlacedOccurrence>> AssemblyDocument::placedParts(
    const std::map<OccurrenceId, Transform>* overrides) const {
    std::vector<PlacedOccurrence> out;
    OccurrencePath path;
    try {
        collectPlaced(root(), Transform::identity(), path, {}, overrides, out);
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, std::string("OCCT exception while placing parts: ") + e.what()};
    }
    return out;
}

std::size_t AssemblyDocument::addExplodedView(ExplodedView view) {
    views_.push_back(std::move(view));
    return views_.size() - 1;
}

Result<std::map<OccurrenceId, Transform>> explodedPlacements(const AssemblyDocument& document,
                                                             const ExplodedView& view, double progress) {
    if (!std::isfinite(progress)) return Error{ErrorCode::InvalidInput, "progress must be finite"};
    std::map<OccurrenceId, Transform> out;
    for (const Occurrence& o : document.topLevel()) out[o.id] = o.placement;
    for (std::size_t s = 0; s < view.steps.size(); ++s) {
        const ExplodeStep& step = view.steps[s];
        const double n = step.direction.norm();
        if (!(n > 0.0) || !std::isfinite(n) || !std::isfinite(step.distance))
            return Error{ErrorCode::InvalidInput, "explode step " + std::to_string(s) + " has an invalid direction or distance"};
        const double f = std::clamp(progress - static_cast<double>(s), 0.0, 1.0);
        const Vec3 offset = step.direction / n * (step.distance * f);
        for (OccurrenceId id : step.occurrences) {
            auto it = out.find(id);
            if (it == out.end())
                return Error{ErrorCode::InvalidInput, "explode step " + std::to_string(s) + ": " + occName(id) +
                                                          " is not a top-level occurrence"};
            it->second.position += offset;
        }
    }
    return out;
}

}  // namespace bendcore::assembly_v2
