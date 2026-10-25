#include "bendcore/topology/brep.hpp"
#include <cmath>

namespace BendCore::CadCore {

EntityId BRepManager::createVertex(const Vector3D& point) noexcept {
    Vertex v;
    v.id = EntityId::generate();
    v.point = point;
    v.revision = 0;
    vertices_[v.id] = v;
    return v.id;
}

EntityId BRepManager::createEdge(const EntityId& startVertex, const EntityId& endVertex) noexcept {
    Edge e;
    e.id = EntityId::generate();
    e.startVertexId = startVertex;
    e.endVertexId = endVertex;

    auto start = vertices_.find(startVertex);
    auto end = vertices_.find(endVertex);
    if (start != vertices_.end() && end != vertices_.end()) {
        Vector3D diff = end->second.point - start->second.point;
        e.length = diff.magnitude();
    }

    e.revision = 0;
    edges_[e.id] = e;
    return e.id;
}

EntityId BRepManager::createFace(const std::vector<EntityId>& edges) noexcept {
    Face f;
    f.id = EntityId::generate();
    f.edgeIds = edges;
    f.normal = Vector3D(0, 0, 1);
    f.area = 0.0;
    f.revision = 0;
    faces_[f.id] = f;
    return f.id;
}

EntityId BRepManager::createShell(const std::vector<EntityId>& faces) noexcept {
    Shell s;
    s.id = EntityId::generate();
    s.faceIds = faces;
    s.volume = 0.0;
    s.revision = 0;
    shells_[s.id] = s;
    return s.id;
}

EntityId BRepManager::createSolid(const std::vector<EntityId>& shells) noexcept {
    Solid solid;
    solid.id = EntityId::generate();
    solid.shellIds = shells;
    solid.volume = 0.0;
    solid.centroid = Vector3D(0, 0, 0);
    solid.revision = 0;
    solids_[solid.id] = solid;
    return solid.id;
}

const Vertex* BRepManager::getVertex(const EntityId& id) const noexcept {
    auto it = vertices_.find(id);
    return it != vertices_.end() ? &it->second : nullptr;
}

const Edge* BRepManager::getEdge(const EntityId& id) const noexcept {
    auto it = edges_.find(id);
    return it != edges_.end() ? &it->second : nullptr;
}

const Face* BRepManager::getFace(const EntityId& id) const noexcept {
    auto it = faces_.find(id);
    return it != faces_.end() ? &it->second : nullptr;
}

const Shell* BRepManager::getShell(const EntityId& id) const noexcept {
    auto it = shells_.find(id);
    return it != shells_.end() ? &it->second : nullptr;
}

const Solid* BRepManager::getSolid(const EntityId& id) const noexcept {
    auto it = solids_.find(id);
    return it != solids_.end() ? &it->second : nullptr;
}

bool BRepManager::validateTopology(const EntityId& solidId) const noexcept {
    auto solid = getSolid(solidId);
    if (!solid) return false;

    for (const auto& shellId : solid->shellIds) {
        auto shell = getShell(shellId);
        if (!shell) return false;

        for (const auto& faceId : shell->faceIds) {
            auto face = getFace(faceId);
            if (!face) return false;

            for (const auto& edgeId : face->edgeIds) {
                auto edge = getEdge(edgeId);
                if (!edge) return false;

                if (!getVertex(edge->startVertexId) || !getVertex(edge->endVertexId)) {
                    return false;
                }
            }
        }
    }

    return true;
}

std::set<EntityId> BRepManager::getAdjacentFaces(const EntityId& faceId) const noexcept {
    std::set<EntityId> adjacent;

    auto face = getFace(faceId);
    if (!face) return adjacent;

    for (const auto& edgeId : face->edgeIds) {
        for (const auto& [otherId, otherFace] : faces_) {
            if (otherId == faceId) continue;

            for (const auto& otherEdgeId : otherFace.edgeIds) {
                if (otherEdgeId == edgeId) {
                    adjacent.insert(otherId);
                    break;
                }
            }
        }
    }

    return adjacent;
}

} // namespace BendCore::CadCore
