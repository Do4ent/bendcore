// Справочные сетки сборки (docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 14): файл STL/3MF,
// встроенный в документ, в мировом положении. Не деталь: не попадает в развёртку деталей
// (placedParts), BOM, массы, пересечения, STEP и решатель.

#include <bendcore/assembly_v2/assembly_document.hpp>

#include <algorithm>
#include <cmath>

namespace bendcore::assembly_v2 {

using kernel::Error;
using kernel::ErrorCode;
using kernel::Result;

namespace {

Status fail(ErrorCode code, std::string message) { return Status::failure(code, std::move(message)); }

bool validOpacity(double o) { return std::isfinite(o) && o >= 0.05 && o <= 1.0; }

ReferenceMeshComponent* findIn(std::vector<ReferenceMeshComponent>& meshes, ReferenceMeshId id) {
    for (ReferenceMeshComponent& m : meshes)
        if (m.id == id) return &m;
    return nullptr;
}

bool finitePlacement(const Transform& t) {
    return t.position.allFinite() && t.rotation.coeffs().allFinite() && std::abs(t.rotation.norm() - 1.0) < 1e-6;
}

}  // namespace

Result<ReferenceMeshId> AssemblyDocument::addReferenceMesh(std::string name, std::string fileName, kernel::MeshFileFormat format,
                                                           std::string data, std::string unit, const Transform& placement,
                                                           const kernel::MeshReadLimits& limits) {
    if (!kernel::meshUnitScale(unit)) return Error{ErrorCode::InvalidInput, "unknown mesh unit '" + unit + "' (mm, cm, m, in)"};
    if (!finitePlacement(placement)) return Error{ErrorCode::InvalidInput, "mesh placement must be finite"};
    auto parsed = kernel::readReferenceMesh(data, format, limits);
    if (!parsed) return Error{parsed.error().code, "mesh '" + fileName + "': " + parsed.error().message};
    ReferenceMeshComponent m;
    m.id = nextMesh_++;
    m.name = !name.empty() ? std::move(name) : (!fileName.empty() ? fileName : "Mesh " + std::to_string(m.id));
    m.fileName = std::move(fileName);
    m.format = format;
    m.data = std::make_shared<const std::string>(std::move(data));
    m.parsed = std::make_shared<const kernel::ReferenceMeshData>(std::move(parsed).value());
    m.unit = std::move(unit);
    m.placement = placement;
    meshes_.push_back(std::move(m));
    return meshes_.back().id;
}

const ReferenceMeshComponent* AssemblyDocument::findReferenceMesh(ReferenceMeshId id) const {
    for (const ReferenceMeshComponent& m : meshes_)
        if (m.id == id) return &m;
    return nullptr;
}

Status AssemblyDocument::removeReferenceMesh(ReferenceMeshId id) {
    const auto it = std::find_if(meshes_.begin(), meshes_.end(), [&](const ReferenceMeshComponent& m) { return m.id == id; });
    if (it == meshes_.end()) return fail(ErrorCode::InvalidInput, "no reference mesh " + std::to_string(id));
    meshes_.erase(it);
    return std::monostate{};
}

Status AssemblyDocument::setReferenceMeshPlacement(ReferenceMeshId id, const Transform& placement) {
    if (!findReferenceMesh(id)) return fail(ErrorCode::InvalidInput, "no reference mesh " + std::to_string(id));
    if (!finitePlacement(placement)) return fail(ErrorCode::InvalidInput, "mesh placement must be finite");
    findIn(meshes_, id)->placement = placement;
    return std::monostate{};
}

Status AssemblyDocument::setReferenceMeshUnit(ReferenceMeshId id, const std::string& unit) {
    if (!findReferenceMesh(id)) return fail(ErrorCode::InvalidInput, "no reference mesh " + std::to_string(id));
    if (!kernel::meshUnitScale(unit)) return fail(ErrorCode::InvalidInput, "unknown mesh unit '" + unit + "' (mm, cm, m, in)");
    findIn(meshes_, id)->unit = unit;
    return std::monostate{};
}

Status AssemblyDocument::setReferenceMeshDisplay(ReferenceMeshId id, bool visible, double opacity) {
    if (!findReferenceMesh(id)) return fail(ErrorCode::InvalidInput, "no reference mesh " + std::to_string(id));
    if (!validOpacity(opacity)) return fail(ErrorCode::InvalidInput, "mesh opacity must be in [0.05, 1]");
    ReferenceMeshComponent* m = findIn(meshes_, id);
    m->visible = visible;
    m->opacity = opacity;
    return std::monostate{};
}

Status AssemblyDocument::setReferenceMeshName(ReferenceMeshId id, std::string name) {
    if (!findReferenceMesh(id)) return fail(ErrorCode::InvalidInput, "no reference mesh " + std::to_string(id));
    ReferenceMeshComponent* m = findIn(meshes_, id);
    m->name = name.empty() ? (m->fileName.empty() ? "Mesh " + std::to_string(id) : m->fileName) : std::move(name);
    return std::monostate{};
}

Result<kernel::TriangleMesh> AssemblyDocument::worldReferenceMesh(ReferenceMeshId id) const {
    const ReferenceMeshComponent* m = findReferenceMesh(id);
    if (!m) return Error{ErrorCode::InvalidInput, "no reference mesh " + std::to_string(id)};
    const double scale = kernel::meshUnitScale(m->unit).value_or(1.0);
    kernel::TriangleMesh out;
    out.triangles = m->parsed->mesh.triangles;
    out.vertices.reserve(m->parsed->mesh.vertices.size());
    for (const kernel::Point3& p : m->parsed->mesh.vertices) {
        const Vec3 w = m->placement.applyToPoint(Vec3(p[0] * scale, p[1] * scale, p[2] * scale));
        out.vertices.push_back({w.x(), w.y(), w.z()});
    }
    return out;
}

}  // namespace bendcore::assembly_v2
