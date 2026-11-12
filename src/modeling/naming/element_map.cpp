// Element map built from BRepTools_History (docs/modeling/TOPOLOGICAL_NAMING.md).

#include <bendcore/modeling/naming.hpp>

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_List.hxx>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <set>
#include <stdexcept>

namespace bendcore::modeling {

namespace {

constexpr std::array<ElementKind, 3> kKinds{ElementKind::Face, ElementKind::Edge, ElementKind::Vertex};

std::size_t ki(ElementKind kind) { return static_cast<std::size_t>(kind); }

TopAbs_ShapeEnum topAbs(ElementKind kind) {
    switch (kind) {
        case ElementKind::Face: return TopAbs_FACE;
        case ElementKind::Edge: return TopAbs_EDGE;
        case ElementKind::Vertex: return TopAbs_VERTEX;
    }
    return TopAbs_SHAPE;
}

int dimension(ElementKind kind) { return 2 - static_cast<int>(kind); }

std::uint64_t fnv1a(std::string_view text) {
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : text) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

// Long component names are replaced by a hash so that composed names stay bounded.
std::string component(const std::string& name) {
    if (name.size() <= 64) return name;
    char buf[24];
    std::snprintf(buf, sizeof(buf), "#%016llx", static_cast<unsigned long long>(fnv1a(name)));
    return buf;
}

// Deterministic geometric order for elements that share a base name.
bool geometricLess(const GeometricSignature& a, const GeometricSignature& b) {
    for (int i = 0; i < 3; ++i) {
        double d = a.center[i] - b.center[i];
        if (std::abs(d) > 1e-7) return d < 0;
    }
    return a.size < b.size;
}

struct Candidate {
    std::string name;
    int rank = 0;  // 0 unchanged/modified, 1 split, 2 generated
    NameOrigin origin = NameOrigin::Modified;
    std::string feature;
    std::vector<std::string> inherited;
};

struct Source {
    TopoDS_Shape element;
    ElementKind kind;
    std::string name;
    std::string feature;
    NameOrigin origin;
    std::vector<std::string> aliases;
};

std::string joinSorted(std::set<std::string> parts) {
    std::string out;
    for (const auto& p : parts) {
        if (!out.empty()) out += '|';
        out += component(p);
    }
    return out;
}

}  // namespace

const char* toString(ElementKind kind) noexcept {
    switch (kind) {
        case ElementKind::Face: return "face";
        case ElementKind::Edge: return "edge";
        case ElementKind::Vertex: return "vertex";
    }
    return "unknown";
}

std::optional<ElementKind> elementKindFromString(std::string_view text) noexcept {
    if (text == "face") return ElementKind::Face;
    if (text == "edge") return ElementKind::Edge;
    if (text == "vertex") return ElementKind::Vertex;
    return std::nullopt;
}

std::optional<ElementKind> elementKindOf(const TopoDS_Shape& shape) noexcept {
    if (shape.IsNull()) return std::nullopt;
    switch (shape.ShapeType()) {
        case TopAbs_FACE: return ElementKind::Face;
        case TopAbs_EDGE: return ElementKind::Edge;
        case TopAbs_VERTEX: return ElementKind::Vertex;
        default: return std::nullopt;
    }
}

const char* toString(NameOrigin origin) noexcept {
    switch (origin) {
        case NameOrigin::Role: return "role";
        case NameOrigin::Unchanged: return "unchanged";
        case NameOrigin::Modified: return "modified";
        case NameOrigin::Split: return "split";
        case NameOrigin::Generated: return "generated";
        case NameOrigin::Derived: return "derived";
    }
    return "unknown";
}

std::optional<NameOrigin> nameOriginFromString(std::string_view text) noexcept {
    for (NameOrigin o : {NameOrigin::Role, NameOrigin::Unchanged, NameOrigin::Modified,
                         NameOrigin::Split, NameOrigin::Generated, NameOrigin::Derived}) {
        if (text == toString(o)) return o;
    }
    return std::nullopt;
}

const char* toString(ResolveMethod method) noexcept {
    switch (method) {
        case ResolveMethod::Exact: return "exact";
        case ResolveMethod::History: return "history";
        case ResolveMethod::Signature: return "signature";
        case ResolveMethod::Broken: return "broken";
    }
    return "unknown";
}

std::optional<ResolveMethod> resolveMethodFromString(std::string_view text) noexcept {
    for (ResolveMethod m : {ResolveMethod::Exact, ResolveMethod::History, ResolveMethod::Signature,
                            ResolveMethod::Broken}) {
        if (text == toString(m)) return m;
    }
    return std::nullopt;
}

// ---------------------------------------------------------------- NamedShape

void NamedShape::indexShape() {
    for (ElementKind kind : kKinds) {
        maps_[ki(kind)].Clear();
        if (!shape_.IsNull()) TopExp::MapShapes(shape_, topAbs(kind), maps_[ki(kind)]);
    }
    scale_ = 1.0;
    if (!shape_.IsNull()) {
        Bnd_Box box;
        BRepBndLib::Add(shape_, box);
        if (!box.IsVoid()) {
            double d = std::sqrt(box.SquareExtent());
            if (d > 0) scale_ = d;
        }
    }
}

void NamedShape::rebuildNameIndex() {
    for (ElementKind kind : kKinds) {
        auto& index = byName_[ki(kind)];
        index.clear();
        const auto& recs = records_[ki(kind)];
        for (std::size_t i = 0; i < recs.size(); ++i) index.emplace(recs[i].name, i);
    }
}

Result<NamedShape> NamedShape::fromRecords(TopoDS_Shape shape,
                                           std::array<std::vector<ElementRecord>, 3> records,
                                           std::map<std::string, DeletionRecord> deletions) {
    NamedShape out;
    out.shape_ = std::move(shape);
    try {
        out.indexShape();
    } catch (const Standard_Failure& f) {
        return Result<NamedShape>::failure(ErrorCode::InvalidShape, f.what());
    }
    for (ElementKind kind : kKinds) {
        std::size_t n = static_cast<std::size_t>(out.maps_[ki(kind)].Extent());
        if (records[ki(kind)].size() != n) {
            return Result<NamedShape>::failure(
                ErrorCode::InvalidInput,
                std::string("element map has ") + std::to_string(records[ki(kind)].size()) + " " +
                    toString(kind) + " records, the shape has " + std::to_string(n));
        }
    }
    out.records_ = std::move(records);
    out.deletions_ = std::move(deletions);
    out.rebuildNameIndex();
    return out;
}

std::size_t NamedShape::count(ElementKind kind) const noexcept { return records_[ki(kind)].size(); }

const TopoDS_Shape& NamedShape::element(ElementKind kind, std::size_t index) const {
    if (index >= count(kind)) throw std::out_of_range("NamedShape::element: index out of range");
    return maps_[ki(kind)].FindKey(static_cast<int>(index) + 1);
}

const ElementRecord& NamedShape::record(ElementKind kind, std::size_t index) const {
    return records_[ki(kind)].at(index);
}

const std::vector<ElementRecord>& NamedShape::records(ElementKind kind) const {
    return records_[ki(kind)];
}

std::optional<std::size_t> NamedShape::indexOf(ElementKind kind, const TopoDS_Shape& element) const {
    int i = maps_[ki(kind)].FindIndex(element);
    if (i <= 0) return std::nullopt;
    return static_cast<std::size_t>(i - 1);
}

std::optional<std::size_t> NamedShape::find(ElementKind kind, std::string_view name) const {
    const auto& index = byName_[ki(kind)];
    auto it = index.find(std::string(name));
    if (it == index.end()) return std::nullopt;
    return it->second;
}

// ---------------------------------------------------------------- building

Result<NamedShape> buildNamedShape(const NamingRequest& request) {
    if (request.result.IsNull()) {
        return Result<NamedShape>::failure(ErrorCode::InvalidInput, "naming: result shape is null");
    }
    if (request.featureId.empty()) {
        return Result<NamedShape>::failure(ErrorCode::InvalidInput, "naming: empty feature id");
    }
    try {
        NamedShape out;
        out.shape_ = request.result;
        out.indexShape();

        // Signatures. An element that passes unchanged from an input (same TShape, location and
        // orientation) keeps the input's signature: the geometry is the same, so only new and
        // modified elements are measured (BRepGProp per face and edge dominates naming).
        std::array<std::vector<GeometricSignature>, 3> sigs;
        for (ElementKind kind : kKinds) {
            const int n = out.maps_[ki(kind)].Extent();
            std::vector<const GeometricSignature*> known(static_cast<std::size_t>(n), nullptr);
            for (const NamedShape* input : request.inputs) {
                if (input == nullptr || input->isNull()) continue;
                for (std::size_t i = 0; i < input->count(kind); ++i) {
                    const ElementRecord& r = input->record(kind, i);
                    if (!r.signature.valid) continue;
                    const TopoDS_Shape& e = input->element(kind, i);
                    const int j = out.maps_[ki(kind)].FindIndex(e);
                    if (j > 0 && out.maps_[ki(kind)].FindKey(j).IsEqual(e)) known[static_cast<std::size_t>(j - 1)] = &r.signature;
                }
            }
            sigs[ki(kind)].reserve(static_cast<std::size_t>(n));
            for (int i = 1; i <= n; ++i) {
                if (const GeometricSignature* k = known[static_cast<std::size_t>(i - 1)]) {
                    sigs[ki(kind)].push_back(*k);
                    continue;
                }
                auto sig = computeSignature(out.maps_[ki(kind)].FindKey(i));
                if (!sig) return Result<NamedShape>::failure(sig.error().code, sig.error().message);
                sigs[ki(kind)].push_back(std::move(sig).value());
            }
        }

        // Sources: named elements of the inputs and the feature's own roles.
        std::vector<Source> sources;
        for (const NamedShape* input : request.inputs) {
            if (input == nullptr || input->isNull()) continue;
            for (ElementKind kind : kKinds) {
                for (std::size_t i = 0; i < input->count(kind); ++i) {
                    const ElementRecord& r = input->record(kind, i);
                    sources.push_back({input->element(kind, i), kind, r.name, r.feature, r.origin, r.aliases});
                }
            }
        }
        for (const ElementRole& role : request.roles) {
            auto kind = elementKindOf(role.element);
            if (!kind) {
                return Result<NamedShape>::failure(ErrorCode::InvalidInput,
                                                   "naming: role '" + role.role + "' is not a face, edge or vertex");
            }
            sources.push_back({role.element, *kind, request.featureId + ":" + role.role,
                               request.featureId, NameOrigin::Role, {}});
        }

        std::array<std::vector<std::vector<Candidate>>, 3> candidates;
        for (ElementKind kind : kKinds) candidates[ki(kind)].resize(static_cast<std::size_t>(out.maps_[ki(kind)].Extent()));

        auto sortedImages = [&](ElementKind kind, const NCollection_List<TopoDS_Shape>& list) {
            std::vector<std::size_t> idx;
            for (NCollection_List<TopoDS_Shape>::Iterator it(list); it.More(); it.Next()) {
                if (elementKindOf(it.Value()) != kind) continue;
                auto i = out.indexOf(kind, it.Value());
                if (i && std::find(idx.begin(), idx.end(), *i) == idx.end()) idx.push_back(*i);
            }
            std::sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
                return geometricLess(sigs[ki(kind)][a], sigs[ki(kind)][b]);
            });
            return idx;
        };

        // Generated names of each source, kept for deletion records.
        std::vector<std::vector<std::pair<ElementKind, std::size_t>>> generatedOf(sources.size());

        for (std::size_t s = 0; s < sources.size(); ++s) {
            const Source& src = sources[s];
            const NameOrigin keepOrigin = src.origin == NameOrigin::Role && src.feature == request.featureId
                                              ? NameOrigin::Role
                                              : NameOrigin::Unchanged;
            if (auto i = out.indexOf(src.kind, src.element)) {
                candidates[ki(src.kind)][*i].push_back(
                    {src.name, 0, keepOrigin, src.feature, src.aliases});
            }
            if (request.history.IsNull()) continue;
            const auto mods = sortedImages(src.kind, request.history->Modified(src.element));
            if (mods.size() == 1) {
                candidates[ki(src.kind)][mods[0]].push_back(
                    {src.name, 0, keepOrigin == NameOrigin::Role ? NameOrigin::Role : NameOrigin::Modified,
                     src.feature, src.aliases});
            } else {
                for (std::size_t k = 0; k < mods.size(); ++k) {
                    candidates[ki(src.kind)][mods[k]].push_back(
                        {src.name + ";S" + std::to_string(k + 1), 1, NameOrigin::Split, request.featureId, {}});
                }
            }
            const auto& generated = request.history->Generated(src.element);
            if (generated.IsEmpty()) continue;
            for (ElementKind gk : kKinds) {
                if (dimension(gk) < dimension(src.kind)) continue;  // section edges are derived instead
                const auto gens = sortedImages(gk, generated);
                for (std::size_t k = 0; k < gens.size(); ++k) {
                    std::string name = "G[" + request.featureId + "](" + component(src.name) + ")";
                    if (gens.size() > 1) name += ";G" + std::to_string(k + 1);
                    candidates[ki(gk)][gens[k]].push_back({name, 2, NameOrigin::Generated, request.featureId, {}});
                    generatedOf[s].emplace_back(gk, gens[k]);
                }
            }
        }

        // Pick a primary name per element; the rest become aliases.
        std::array<std::vector<bool>, 3> named;
        for (ElementKind kind : kKinds) {
            auto& recs = out.records_[ki(kind)];
            const std::size_t n = candidates[ki(kind)].size();
            recs.assign(n, ElementRecord{});
            named[ki(kind)].assign(n, false);
            for (std::size_t i = 0; i < n; ++i) {
                recs[i].signature = sigs[ki(kind)][i];
                auto& cands = candidates[ki(kind)][i];
                if (cands.empty()) continue;
                std::sort(cands.begin(), cands.end(), [](const Candidate& a, const Candidate& b) {
                    return a.rank != b.rank ? a.rank < b.rank : a.name < b.name;
                });
                recs[i].name = cands.front().name;
                recs[i].origin = cands.front().origin;
                recs[i].feature = cands.front().feature;
                std::set<std::string> aliases;
                for (const auto& c : cands) {
                    if (c.name != recs[i].name) aliases.insert(c.name);
                    for (const auto& a : c.inherited) {
                        if (a != recs[i].name) aliases.insert(a);
                    }
                }
                recs[i].aliases.assign(aliases.begin(), aliases.end());
                named[ki(kind)][i] = true;
            }
        }

        // Adjacency for derived names.
        NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher> edgeFaces;
        NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher> vertexFaces;
        TopExp::MapShapesAndAncestors(out.shape_, TopAbs_EDGE, TopAbs_FACE, edgeFaces);
        TopExp::MapShapesAndAncestors(out.shape_, TopAbs_VERTEX, TopAbs_FACE, vertexFaces);

        auto faceName = [&](const TopoDS_Shape& f) -> std::string {
            auto i = out.indexOf(ElementKind::Face, f);
            if (!i || !named[ki(ElementKind::Face)][*i]) return {};
            return out.records_[ki(ElementKind::Face)][*i].name;
        };

        // Faces without history: named after their named boundary edges.
        for (std::size_t i = 0; i < out.records_[0].size(); ++i) {
            if (named[0][i]) continue;
            std::set<std::string> parts;
            for (TopExp_Explorer ex(out.element(ElementKind::Face, i), TopAbs_EDGE); ex.More(); ex.Next()) {
                auto e = out.indexOf(ElementKind::Edge, ex.Current());
                if (e && named[1][*e]) parts.insert(out.records_[1][*e].name);
            }
            auto& rec = out.records_[0][i];
            rec.name = "F(" + joinSorted(parts) + ")";
            rec.origin = NameOrigin::Derived;
            rec.feature = request.featureId;
        }
        std::fill(named[0].begin(), named[0].end(), true);

        auto deriveFromFaces = [&](ElementKind kind, const char* prefix, auto& ancestors) {
            auto& recs = out.records_[ki(kind)];
            for (std::size_t i = 0; i < recs.size(); ++i) {
                if (named[ki(kind)][i]) continue;
                std::set<std::string> parts;
                const TopoDS_Shape& el = out.element(kind, i);
                if (ancestors.Contains(el)) {
                    for (NCollection_List<TopoDS_Shape>::Iterator it(ancestors.FindFromKey(el)); it.More(); it.Next()) {
                        std::string n = faceName(it.Value());
                        if (!n.empty()) parts.insert(n);
                    }
                }
                recs[i].name = std::string(prefix) + "(" + joinSorted(parts) + ")";
                recs[i].origin = NameOrigin::Derived;
                recs[i].feature = request.featureId;
                named[ki(kind)][i] = true;
            }
        };
        deriveFromFaces(ElementKind::Edge, "E", edgeFaces);
        deriveFromFaces(ElementKind::Vertex, "V", vertexFaces);

        // Unique names: equal names get ;D<i> in geometric order.
        for (ElementKind kind : kKinds) {
            auto& recs = out.records_[ki(kind)];
            std::map<std::string, std::vector<std::size_t>> groups;
            for (std::size_t i = 0; i < recs.size(); ++i) groups[recs[i].name].push_back(i);
            for (auto& [name, members] : groups) {
                if (members.size() < 2) continue;
                std::sort(members.begin(), members.end(), [&](std::size_t a, std::size_t b) {
                    return geometricLess(sigs[ki(kind)][a], sigs[ki(kind)][b]);
                });
                for (std::size_t k = 0; k < members.size(); ++k) {
                    auto& rec = recs[members[k]];
                    rec.aliases.push_back(rec.name);
                    rec.name = name + ";D" + std::to_string(k + 1);
                }
            }
        }
        out.rebuildNameIndex();

        // Deletion registry: inherited plus sources whose name no longer exists.
        std::set<std::string> present;
        for (ElementKind kind : kKinds) {
            for (const auto& r : out.records_[ki(kind)]) {
                present.insert(r.name);
                present.insert(r.aliases.begin(), r.aliases.end());
            }
        }
        for (const NamedShape* input : request.inputs) {
            if (input == nullptr) continue;
            for (const auto& [name, rec] : input->deletions()) out.deletions_.emplace(name, rec);
        }
        for (std::size_t s = 0; s < sources.size(); ++s) {
            const std::string& name = sources[s].name;
            if (present.count(name) != 0) continue;
            bool anyImage = false;
            for (const auto& r : out.records_[ki(sources[s].kind)]) {
                if (r.name.rfind(name + ";S", 0) == 0) {
                    anyImage = true;
                    break;
                }
            }
            if (anyImage) continue;
            DeletionRecord del;
            del.feature = request.featureId;
            for (const auto& [gk, gi] : generatedOf[s]) del.successors.push_back(out.records_[ki(gk)][gi].name);
            out.deletions_[name] = std::move(del);
        }
        for (auto it = out.deletions_.begin(); it != out.deletions_.end();) {
            if (present.count(it->first) != 0) {
                it = out.deletions_.erase(it);
            } else {
                ++it;
            }
        }
        return out;
    } catch (const Standard_Failure& failure) {
        return Result<NamedShape>::failure(ErrorCode::OperationFailed,
                                           std::string("naming failed: ") + failure.what());
    }
}

// ---------------------------------------------------------------- references

Result<TopoReference> makeReference(const NamedShape& shape, const TopoDS_Shape& element) {
    auto kind = elementKindOf(element);
    if (!kind) {
        return Result<TopoReference>::failure(ErrorCode::InvalidInput,
                                              "reference target is not a face, edge or vertex");
    }
    auto index = shape.indexOf(*kind, element);
    if (!index) {
        return Result<TopoReference>::failure(ErrorCode::InvalidInput,
                                              std::string("the ") + toString(*kind) + " is not part of the shape");
    }
    const ElementRecord& rec = shape.record(*kind, *index);
    return TopoReference{rec.name, *kind, rec.signature};
}

namespace {

// Base of an indexed name: "X;D2" -> "X"; nullopt when the name carries no ;D index.
std::optional<std::string> derivedBase(const std::string& name) {
    auto pos = name.rfind(";D");
    if (pos == std::string::npos || pos + 2 >= name.size()) return std::nullopt;
    for (std::size_t i = pos + 2; i < name.size(); ++i) {
        if (name[i] < '0' || name[i] > '9') return std::nullopt;
    }
    return name.substr(0, pos);
}

Resolution single(ResolveMethod method, std::size_t index, const std::string& name, std::string detail) {
    Resolution r;
    r.method = method;
    r.indices = {index};
    r.names = {name};
    r.detail = std::move(detail);
    return r;
}

}  // namespace

Resolution resolveReference(const TopoReference& reference, const NamedShape& shape,
                            const ResolveOptions& options) {
    const ElementKind kind = reference.kind;
    const auto& recs = shape.records(kind);
    const std::string& name = reference.name;
    const char* kindText = toString(kind);

    // 1. Exact name. An index (;D<i>) can be reassigned when the geometric order of equally
    // named elements changes, so for indexed names the signature picks among the siblings.
    if (auto i = shape.find(kind, name)) {
        auto base = derivedBase(name);
        if (base && reference.signature.valid) {
            std::size_t best = *i;
            double bestScore = signatureDistance(reference.signature, recs[*i].signature, shape.scale());
            for (std::size_t j = 0; j < recs.size(); ++j) {
                if (j == *i || derivedBase(recs[j].name) != base) continue;
                double s = signatureDistance(reference.signature, recs[j].signature, shape.scale());
                if (s < bestScore * 0.5) {
                    best = j;
                    bestScore = s;
                }
            }
            if (best != *i) {
                return single(ResolveMethod::Signature, best, recs[best].name,
                              "indexed name '" + name + "' re-resolved by signature among equally named elements");
            }
        }
        return single(ResolveMethod::Exact, *i, name, "exact name");
    }

    // 2. History: aliases (merged or renamed elements) and split images.
    {
        Resolution r;
        for (std::size_t j = 0; j < recs.size(); ++j) {
            const auto& al = recs[j].aliases;
            if (std::find(al.begin(), al.end(), name) != al.end()) {
                r.indices.push_back(j);
                r.names.push_back(recs[j].name);
            }
        }
        if (!r.indices.empty()) {
            r.method = ResolveMethod::History;
            r.detail = std::string(kindText) + " '" + name + "' now named '" + r.names.front() + "'" +
                       (r.indices.size() > 1 ? " and " + std::to_string(r.indices.size() - 1) + " more" : "");
            return r;
        }
        const std::string prefix = name + ";S";
        for (std::size_t j = 0; j < recs.size(); ++j) {
            if (recs[j].name.rfind(prefix, 0) == 0) {
                r.indices.push_back(j);
                r.names.push_back(recs[j].name);
            }
        }
        if (!r.indices.empty()) {
            r.method = ResolveMethod::History;
            r.detail = std::string(kindText) + " '" + name + "' was split into " +
                       std::to_string(r.indices.size()) + " parts";
            return r;
        }
    }

    // 3. Deletion evidence: the element was consumed by a known feature.
    auto del = shape.deletions().find(name);
    if (del != shape.deletions().end()) {
        Resolution r;
        for (const auto& succ : del->second.successors) {
            if (auto j = shape.find(kind, succ)) {
                r.indices.push_back(*j);
                r.names.push_back(succ);
            }
        }
        if (!r.indices.empty()) {
            r.method = ResolveMethod::History;
            r.detail = std::string(kindText) + " '" + name + "' was replaced by feature '" +
                       del->second.feature + "' with '" + r.names.front() + "'";
            return r;
        }
        Resolution broken;
        broken.method = ResolveMethod::Broken;
        broken.detail = std::string(kindText) + " '" + name + "' was removed by feature '" + del->second.feature + "'";
        if (!del->second.successors.empty()) {
            broken.detail += " (replaced by '" + del->second.successors.front() + "'";
            if (del->second.successors.size() > 1) {
                broken.detail += " and " + std::to_string(del->second.successors.size() - 1) + " more";
            }
            broken.detail += ")";
        }
        return broken;
    }

    // 4. Signature: the unique, clearly closest element of the same geometry.
    Resolution broken;
    broken.method = ResolveMethod::Broken;
    if (!reference.signature.valid) {
        broken.detail = std::string("no ") + kindText + " named '" + name +
                        "' and the reference has no geometric signature";
        return broken;
    }
    constexpr double kInf = std::numeric_limits<double>::infinity();
    double best = kInf, second = kInf;
    std::size_t bestIndex = 0;
    for (std::size_t j = 0; j < recs.size(); ++j) {
        double s = signatureDistance(reference.signature, recs[j].signature, shape.scale());
        if (s < best) {
            second = best;
            best = s;
            bestIndex = j;
        } else if (s < second) {
            second = s;
        }
    }
    char buf[64];
    if (best > options.maxSignatureDistance) {
        broken.detail = std::string("no ") + kindText + " named '" + name + "' and no " +
                        reference.signature.geometry + " " + kindText + " with a matching signature";
        if (best < kInf) {
            std::snprintf(buf, sizeof(buf), " (closest distance %.3g)", best);
            broken.detail += buf;
        }
        return broken;
    }
    if (second < kInf && !(best < options.ambiguityRatio * second)) {
        std::snprintf(buf, sizeof(buf), " (distances %.3g and %.3g)", best, second);
        broken.detail = std::string("no ") + kindText + " named '" + name +
                        "' and the signature is ambiguous" + buf;
        return broken;
    }
    std::snprintf(buf, sizeof(buf), "%.3g", best);
    return single(ResolveMethod::Signature, bestIndex, recs[bestIndex].name,
                  std::string("no ") + kindText + " named '" + name + "'; matched '" + recs[bestIndex].name +
                      "' by signature (distance " + buf + ")");
}

}  // namespace bendcore::modeling
