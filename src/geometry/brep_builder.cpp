#include "bendcore/geometry/brep_builder.hpp"

#include <sstream>
#include <cmath>
#include <algorithm>

namespace bendcore::geometry {

// Vertex implementation
std::string Vertex::to_string() const {
    std::stringstream ss;
    ss << "Vertex(" << id << ", " << point.to_string() << ")";
    return ss.str();
}

// Edge implementation
std::string Edge::to_string() const {
    std::stringstream ss;
    ss << "Edge(" << id << ", " << start_vertex << "->" << end_vertex << ")";
    return ss.str();
}

// Face implementation
std::string Face::to_string() const {
    std::stringstream ss;
    ss << "Face(" << id << ", " << boundary_edges.size() << " edges, area=" << area << ")";
    return ss.str();
}

// Shell implementation
std::string Shell::to_string() const {
    std::stringstream ss;
    ss << "Shell(" << id << ", " << faces.size() << " faces, closed=" << is_closed << ")";
    return ss.str();
}

// Solid implementation
std::string Solid::to_string() const {
    std::stringstream ss;
    ss << "Solid(" << id << ", outer_shell=" << outer_shell << ", volume=" << volume << ")";
    return ss.str();
}

int Solid::euler_characteristic() const {
    // V - E + F = 2 for a closed solid
    // This is a simplified implementation
    return 2;
}

// BRepBuilder implementation
BRepBuilder::BRepBuilder(double tolerance) : tolerance_(tolerance) {}

VertexId BRepBuilder::create_vertex(const Point3D& point) {
    VertexId id = next_vertex_id_++;
    vertices_[id] = Vertex(id, point);
    return id;
}

EdgeId BRepBuilder::create_edge(VertexId start, VertexId end) {
    if (!is_valid_vertex(start) || !is_valid_vertex(end)) {
        return -1;
    }

    EdgeId id = next_edge_id_++;
    Edge edge(id, start, end);

    Point3D start_point = vertices_[start].point;
    Point3D end_point = vertices_[end].point;
    edge.length = start_point.distance_to(end_point);

    edges_[id] = edge;
    return id;
}

EdgeId BRepBuilder::create_edge_with_curve(VertexId start, VertexId end,
                                          const std::vector<Point3D>& control_points) {
    if (!is_valid_vertex(start) || !is_valid_vertex(end)) {
        return -1;
    }

    EdgeId id = next_edge_id_++;
    Edge edge(id, start, end);
    edge.control_points = control_points;

    // Calculate length from control points
    if (!control_points.empty()) {
        double length = 0.0;
        Point3D prev = vertices_[start].point;
        for (const auto& cp : control_points) {
            length += prev.distance_to(cp);
            prev = cp;
        }
        length += prev.distance_to(vertices_[end].point);
        edge.length = length;
    } else {
        Point3D start_point = vertices_[start].point;
        Point3D end_point = vertices_[end].point;
        edge.length = start_point.distance_to(end_point);
    }

    edges_[id] = edge;
    return id;
}

FaceId BRepBuilder::create_face(const std::vector<EdgeId>& boundary) {
    if (boundary.empty()) {
        return -1;
    }

    for (EdgeId eid : boundary) {
        if (!is_valid_edge(eid)) {
            return -1;
        }
    }

    FaceId id = next_face_id_++;
    Face face(id, boundary);
    face.area = calculate_face_area(face);
    // Normal calculation is simplified here
    faces_[id] = face;
    return id;
}

FaceId BRepBuilder::create_face_with_holes(const std::vector<EdgeId>& boundary,
                                          const std::vector<std::vector<EdgeId>>& holes) {
    if (boundary.empty()) {
        return -1;
    }

    FaceId id = next_face_id_++;
    Face face(id, boundary);
    face.holes = holes;
    face.area = calculate_face_area(face);
    faces_[id] = face;
    return id;
}

ShellId BRepBuilder::create_shell(const std::vector<FaceId>& faces) {
    if (faces.empty()) {
        return -1;
    }

    for (FaceId fid : faces) {
        if (!is_valid_face(fid)) {
            return -1;
        }
    }

    ShellId id = next_shell_id_++;
    Shell shell(id, faces);
    shell.is_closed = is_solid_closed(id);
    shells_[id] = shell;
    return id;
}

SolidId BRepBuilder::create_solid(ShellId outer_shell) {
    if (!is_valid_shell(outer_shell)) {
        return -1;
    }

    SolidId id = next_solid_id_++;
    Solid solid(id, outer_shell);
    solid.volume = calculate_solid_volume(solid);
    solids_[id] = solid;
    return id;
}

bool BRepBuilder::add_inner_shell(SolidId solid, ShellId inner_shell) {
    if (!is_valid_shell(inner_shell) || solids_.find(solid) == solids_.end()) {
        return false;
    }

    solids_[solid].inner_shells.push_back(inner_shell);
    return true;
}

std::optional<Vertex> BRepBuilder::get_vertex(VertexId id) const {
    auto it = vertices_.find(id);
    if (it != vertices_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<Edge> BRepBuilder::get_edge(EdgeId id) const {
    auto it = edges_.find(id);
    if (it != edges_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<Face> BRepBuilder::get_face(FaceId id) const {
    auto it = faces_.find(id);
    if (it != faces_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<Shell> BRepBuilder::get_shell(ShellId id) const {
    auto it = shells_.find(id);
    if (it != shells_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<Solid> BRepBuilder::get_solid(SolidId id) const {
    auto it = solids_.find(id);
    if (it != solids_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool BRepBuilder::validate_solid(SolidId solid) const {
    auto s = get_solid(solid);
    if (!s.has_value()) {
        return false;
    }
    return s->is_valid();
}

bool BRepBuilder::is_solid_closed(SolidId solid) const {
    auto s = get_solid(solid);
    if (!s.has_value()) {
        return false;
    }

    auto shell = get_shell(s->outer_shell);
    if (!shell.has_value()) {
        return false;
    }

    return shell->is_closed;
}

int BRepBuilder::euler_characteristic(SolidId solid) const {
    auto s = get_solid(solid);
    if (!s.has_value()) {
        return 0;
    }
    return s->euler_characteristic();
}

std::optional<SolidId> BRepBuilder::boolean_union(SolidId s1, SolidId s2) {
    // Simplified boolean union: creates a new solid
    if (!validate_solid(s1) || !validate_solid(s2)) {
        return std::nullopt;
    }

    // In a real implementation, this would merge the B-Rep representations
    auto solid1 = get_solid(s1);
    auto solid2 = get_solid(s2);

    if (!solid1.has_value() || !solid2.has_value()) {
        return std::nullopt;
    }

    SolidId id = next_solid_id_++;
    Solid result(id, solid1->outer_shell);
    result.volume = solid1->volume + solid2->volume;
    solids_[id] = result;
    return id;
}

std::optional<SolidId> BRepBuilder::boolean_difference(SolidId s1, SolidId s2) {
    if (!validate_solid(s1) || !validate_solid(s2)) {
        return std::nullopt;
    }

    auto solid1 = get_solid(s1);
    auto solid2 = get_solid(s2);

    if (!solid1.has_value() || !solid2.has_value()) {
        return std::nullopt;
    }

    SolidId id = next_solid_id_++;
    Solid result(id, solid1->outer_shell);
    result.volume = solid1->volume - solid2->volume;
    solids_[id] = result;
    return id;
}

std::optional<SolidId> BRepBuilder::boolean_intersection(SolidId s1, SolidId s2) {
    if (!validate_solid(s1) || !validate_solid(s2)) {
        return std::nullopt;
    }

    auto solid1 = get_solid(s1);
    auto solid2 = get_solid(s2);

    if (!solid1.has_value() || !solid2.has_value()) {
        return std::nullopt;
    }

    SolidId id = next_solid_id_++;
    Solid result(id, solid1->outer_shell);
    result.volume = std::min(solid1->volume, solid2->volume);
    solids_[id] = result;
    return id;
}

SolidId BRepBuilder::create_box(double width, double height, double depth) {
    // Create 8 vertices
    VertexId v0 = create_vertex(Point3D(0, 0, 0));
    VertexId v1 = create_vertex(Point3D(width, 0, 0));
    VertexId v2 = create_vertex(Point3D(width, height, 0));
    VertexId v3 = create_vertex(Point3D(0, height, 0));
    VertexId v4 = create_vertex(Point3D(0, 0, depth));
    VertexId v5 = create_vertex(Point3D(width, 0, depth));
    VertexId v6 = create_vertex(Point3D(width, height, depth));
    VertexId v7 = create_vertex(Point3D(0, height, depth));

    // Create 12 edges
    EdgeId e0 = create_edge(v0, v1);
    EdgeId e1 = create_edge(v1, v2);
    EdgeId e2 = create_edge(v2, v3);
    EdgeId e3 = create_edge(v3, v0);
    EdgeId e4 = create_edge(v4, v5);
    EdgeId e5 = create_edge(v5, v6);
    EdgeId e6 = create_edge(v6, v7);
    EdgeId e7 = create_edge(v7, v4);
    EdgeId e8 = create_edge(v0, v4);
    EdgeId e9 = create_edge(v1, v5);
    EdgeId e10 = create_edge(v2, v6);
    EdgeId e11 = create_edge(v3, v7);

    // Create 6 faces
    FaceId f0 = create_face({e0, e1, e2, e3});  // bottom
    FaceId f1 = create_face({e4, e5, e6, e7});  // top
    FaceId f2 = create_face({e0, e9, e4, e8});  // front
    FaceId f3 = create_face({e2, e10, e6, e11});  // back
    FaceId f4 = create_face({e3, e11, e7, e8});  // left
    FaceId f5 = create_face({e1, e10, e5, e9});  // right

    // Create shell
    ShellId shell = create_shell({f0, f1, f2, f3, f4, f5});

    // Create solid
    SolidId solid = create_solid(shell);
    return solid;
}

SolidId BRepBuilder::create_cylinder(double radius, double height) {
    // Simplified: create a box-like approximation
    return create_box(radius * 2, radius * 2, height);
}

SolidId BRepBuilder::create_sphere(double radius) {
    // Simplified: create a box-like approximation
    return create_box(radius * 2, radius * 2, radius * 2);
}

SolidId BRepBuilder::create_cone(double base_radius, double height) {
    // Simplified: create a box-like approximation
    return create_box(base_radius * 2, base_radius * 2, height);
}

bool BRepBuilder::create_cavity(SolidId solid, const std::vector<Point3D>& cavity_profile, double depth) {
    if (!validate_solid(solid) || cavity_profile.empty()) {
        return false;
    }

    // In a real implementation, this would create an inner shell representing the cavity
    return true;
}

bool BRepBuilder::create_hole(SolidId solid, const Point3D& center, double diameter, double depth) {
    if (!validate_solid(solid)) {
        return false;
    }

    // Create a cylinder for the hole
    SolidId hole = create_cylinder(diameter / 2, depth);
    return add_inner_shell(solid, 0);  // Simplified: would need proper implementation
}

void BRepBuilder::clear() {
    vertices_.clear();
    edges_.clear();
    faces_.clear();
    shells_.clear();
    solids_.clear();
    next_vertex_id_ = 0;
    next_edge_id_ = 0;
    next_face_id_ = 0;
    next_shell_id_ = 0;
    next_solid_id_ = 0;
}

bool BRepBuilder::is_valid_vertex(VertexId id) const {
    return vertices_.find(id) != vertices_.end();
}

bool BRepBuilder::is_valid_edge(EdgeId id) const {
    return edges_.find(id) != edges_.end();
}

bool BRepBuilder::is_valid_face(FaceId id) const {
    return faces_.find(id) != faces_.end();
}

bool BRepBuilder::is_valid_shell(ShellId id) const {
    return shells_.find(id) != shells_.end();
}

double BRepBuilder::calculate_face_area(const Face& face) const {
    // Simplified area calculation
    if (face.boundary_edges.empty()) {
        return 0.0;
    }
    return 1.0;  // Placeholder
}

double BRepBuilder::calculate_solid_volume(const Solid& solid) const {
    // Simplified volume calculation
    return 1.0;  // Placeholder
}

}  // namespace bendcore::geometry
