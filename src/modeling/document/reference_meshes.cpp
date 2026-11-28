// Reference meshes of the part document (docs/modeling/PART_DOCUMENT.md, "Справочная сетка"):
// STL/3MF files embedded in the document, placed in part coordinates and shown for reference.
// They are kept outside the feature tree (no body, no mass, no rebuild), recorded for undo, and
// their section by a base plane can become a sketch of construction lines.

#include "part_document_internal.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>

namespace bendcore::modeling {

namespace {

Status fail(ErrorCode code, std::string message) { return Status::failure(code, std::move(message)); }

constexpr std::size_t kMaxSectionSegments = 5000;

std::string number(double v) {
    if (v == 0.0) return "0";
    char buf[32];
    const auto r = std::to_chars(buf, buf + sizeof buf, v);
    return std::string(buf, r.ptr);
}

bool validOpacity(double o) { return std::isfinite(o) && o >= 0.05 && o <= 1.0; }

Result<std::shared_ptr<const kernel::TriangleMesh>> place(const PartDocument::MeshEntry& e) {
    using R = Result<std::shared_ptr<const kernel::TriangleMesh>>;
    if (e.placed) return e.placed;
    kernel::MeshPlacement p;
    p.unitScale = kernel::meshUnitScale(e.unit).value_or(1.0);
    p.rotationDegrees = e.rotationDegrees;
    p.translation = e.translation;
    auto m = kernel::placeMesh(e.parsed->mesh, p);
    if (!m) return R::failure(m.error().code, m.error().message);
    return std::make_shared<const kernel::TriangleMesh>(std::move(m).value());
}

Status checkPlacement(const std::string& unit, const kernel::Point3& rot, const kernel::Point3& trans) {
    if (!kernel::meshUnitScale(unit)) return fail(ErrorCode::InvalidInput, "unknown mesh unit '" + unit + "' (mm, cm, m, in)");
    for (int k = 0; k < 3; ++k) {
        if (!std::isfinite(rot[k]) || !std::isfinite(trans[k])) return fail(ErrorCode::InvalidInput, "mesh placement must be finite");
    }
    return std::monostate{};
}

}  // namespace

PartDocument::MeshEntry* PartDocument::meshEntry(const std::string& id) {
    for (MeshEntry& e : meshes_)
        if (e.id == id) return &e;
    return nullptr;
}

const PartDocument::MeshEntry* PartDocument::meshEntry(const std::string& id) const {
    for (const MeshEntry& e : meshes_)
        if (e.id == id) return &e;
    return nullptr;
}

Result<std::string> PartDocument::addReferenceMesh(ReferenceMeshSpec spec, const kernel::MeshReadLimits& limits) {
    using R = Result<std::string>;
    if (auto st = checkPlacement(spec.unit, spec.rotationDegrees, spec.translation); !st) return R::failure(st.error().code, st.error().message);
    if (!validOpacity(spec.opacity)) return R::failure(ErrorCode::InvalidInput, "mesh opacity must be in [0.05, 1]");
    if (spec.id.empty()) {
        for (int k = 1;; ++k) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%03d", k);
            const std::string candidate = std::string("Mesh") + buf;
            if (!meshEntry(candidate) && !node(candidate)) {
                spec.id = candidate;
                break;
            }
        }
    } else if (meshEntry(spec.id) || node(spec.id)) {
        return R::failure(ErrorCode::InvalidInput, "id '" + spec.id + "' already exists");
    }
    auto parsed = kernel::readReferenceMesh(spec.data, spec.format, limits);
    if (!parsed) return R::failure(parsed.error().code, "mesh '" + spec.fileName + "': " + parsed.error().message);
    MeshEntry e;
    e.id = spec.id;
    e.fileName = spec.fileName;
    e.label = !spec.label.empty() ? spec.label : (!spec.fileName.empty() ? spec.fileName : spec.id);
    e.format = spec.format;
    e.data = std::make_shared<const std::string>(std::move(spec.data));
    e.parsed = std::make_shared<const kernel::ReferenceMeshData>(std::move(parsed).value());
    e.unit = spec.unit;
    e.rotationDegrees = spec.rotationDegrees;
    e.translation = spec.translation;
    e.visible = spec.visible;
    e.opacity = spec.opacity;
    auto placed = place(e);
    if (!placed) return R::failure(placed.error().code, placed.error().message);
    e.placed = placed.value();
    State before = captureState();
    meshes_.push_back(std::move(e));
    recordEdit("insert mesh " + spec.id, std::move(before));
    return spec.id;
}

Status PartDocument::removeReferenceMesh(const std::string& id) {
    auto it = std::find_if(meshes_.begin(), meshes_.end(), [&](const MeshEntry& e) { return e.id == id; });
    if (it == meshes_.end()) return fail(ErrorCode::InvalidInput, "no reference mesh '" + id + "'");
    State before = captureState();
    meshes_.erase(it);
    recordEdit("remove mesh " + id, std::move(before));
    return std::monostate{};
}

Status PartDocument::setReferenceMeshPlacement(const std::string& id, const std::string& unit,
                                               const kernel::Point3& rotationDegrees, const kernel::Point3& translation) {
    if (!meshEntry(id)) return fail(ErrorCode::InvalidInput, "no reference mesh '" + id + "'");
    if (auto st = checkPlacement(unit, rotationDegrees, translation); !st) return st;
    MeshEntry moved = *meshEntry(id);
    moved.unit = unit;
    moved.rotationDegrees = rotationDegrees;
    moved.translation = translation;
    moved.placed.reset();
    auto placed = place(moved);
    if (!placed) return fail(placed.error().code, placed.error().message);
    moved.placed = placed.value();
    State before = captureState();
    *meshEntry(id) = std::move(moved);
    recordEdit("move mesh " + id, std::move(before));
    return std::monostate{};
}

Status PartDocument::setReferenceMeshDisplay(const std::string& id, bool visible, double opacity) {
    if (!meshEntry(id)) return fail(ErrorCode::InvalidInput, "no reference mesh '" + id + "'");
    if (!validOpacity(opacity)) return fail(ErrorCode::InvalidInput, "mesh opacity must be in [0.05, 1]");
    State before = captureState();
    MeshEntry* e = meshEntry(id);
    e->visible = visible;
    e->opacity = opacity;
    recordEdit("display mesh " + id, std::move(before));
    return std::monostate{};
}

Status PartDocument::setReferenceMeshLabel(const std::string& id, const std::string& label) {
    if (!meshEntry(id)) return fail(ErrorCode::InvalidInput, "no reference mesh '" + id + "'");
    State before = captureState();
    MeshEntry* e = meshEntry(id);
    e->label = label.empty() ? e->id : label;
    recordEdit("rename mesh " + id, std::move(before));
    return std::monostate{};
}

std::vector<std::string> PartDocument::referenceMeshIds() const {
    std::vector<std::string> out;
    for (const MeshEntry& e : meshes_) out.push_back(e.id);
    return out;
}

Result<ReferenceMeshInfo> PartDocument::referenceMesh(const std::string& id) const {
    using R = Result<ReferenceMeshInfo>;
    const MeshEntry* e = meshEntry(id);
    if (!e) return R::failure(ErrorCode::InvalidInput, "no reference mesh '" + id + "'");
    auto placed = place(*e);
    if (!placed) return R::failure(placed.error().code, placed.error().message);
    ReferenceMeshInfo info;
    info.id = e->id;
    info.label = e->label;
    info.fileName = e->fileName;
    info.format = e->format;
    info.bytes = e->data->size();
    info.unit = e->unit;
    info.rotationDegrees = e->rotationDegrees;
    info.translation = e->translation;
    info.visible = e->visible;
    info.opacity = e->opacity;
    info.report = e->parsed->report;
    info.properties = kernel::meshProperties(*placed.value());
    return info;
}

Result<std::shared_ptr<const kernel::TriangleMesh>> PartDocument::placedReferenceMesh(const std::string& id) const {
    const MeshEntry* e = meshEntry(id);
    if (!e) return Result<std::shared_ptr<const kernel::TriangleMesh>>::failure(ErrorCode::InvalidInput, "no reference mesh '" + id + "'");
    return place(*e);
}

Result<std::string> PartDocument::referenceMeshData(const std::string& id) const {
    const MeshEntry* e = meshEntry(id);
    if (!e) return Result<std::string>::failure(ErrorCode::InvalidInput, "no reference mesh '" + id + "'");
    return *e->data;
}

Result<std::string> PartDocument::addMeshSectionSketch(const std::string& meshId, const std::string& plane, double offset,
                                                       std::string sketchId, double tolerance) {
    using R = Result<std::string>;
    const MeshEntry* e = meshEntry(meshId);
    if (!e) return R::failure(ErrorCode::InvalidInput, "no reference mesh '" + meshId + "'");
    // Frames of the base planes as the sketch feature defines them (sketch_features.cpp, basePlane).
    kernel::Point3 u, v, n;
    if (plane == "XY") {
        u = {1, 0, 0}, v = {0, 1, 0}, n = {0, 0, 1};
    } else if (plane == "XZ") {
        u = {1, 0, 0}, v = {0, 0, 1}, n = {0, -1, 0};
    } else if (plane == "YZ") {
        u = {0, 1, 0}, v = {0, 0, 1}, n = {1, 0, 0};
    } else {
        return R::failure(ErrorCode::InvalidInput, "section plane must be XY, XZ or YZ, is '" + plane + "'");
    }
    if (!std::isfinite(offset)) return R::failure(ErrorCode::InvalidInput, "section offset must be finite");
    auto placed = place(*e);
    if (!placed) return R::failure(placed.error().code, placed.error().message);
    auto section = kernel::sectionMesh(*placed.value(), kernel::SectionPlane{{n[0] * offset, n[1] * offset, n[2] * offset}, n}, tolerance);
    if (!section) return R::failure(section.error().code, section.error().message);
    if (section.value().contours.empty()) {
        return R::failure(ErrorCode::InvalidInput, "plane " + plane + " at " + number(offset) + " mm does not cut mesh '" + meshId + "'");
    }
    std::size_t segments = 0;
    for (const auto& c : section.value().contours) segments += c.closed ? c.points.size() : c.points.size() - 1;
    if (segments > kMaxSectionSegments) {
        return R::failure(ErrorCode::InvalidInput, "the section of mesh '" + meshId + "' has " + std::to_string(segments) +
                                                       " segments, more than " + std::to_string(kMaxSectionSegments) +
                                                       " for a sketch");
    }
    SketchBuilder b;
    auto dot = [](const kernel::Point3& a, const kernel::Point3& c) { return a[0] * c[0] + a[1] * c[1] + a[2] * c[2]; };
    for (const auto& c : section.value().contours) {
        std::vector<int> pts;
        for (const kernel::Point3& p : c.points) pts.push_back(b.point(number(dot(p, u)), number(dot(p, v)), true));
        for (std::size_t i = 0; i + 1 < pts.size(); ++i) b.line(pts[i], pts[i + 1], true);
        if (c.closed && pts.size() > 2) b.line(pts.back(), pts.front(), true);
    }
    FeatureSpec spec;
    spec.type = "Sketch";
    spec.id = std::move(sketchId);
    spec.parameters = {{"offset", number(offset)}};
    spec.options = {{"plane", plane}};
    spec.sketch = b.build();
    return addFeature(std::move(spec));
}

}  // namespace bendcore::modeling
