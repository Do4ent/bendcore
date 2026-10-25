#pragma once

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "bendcore/geometry/geometry_kernel.hpp"

namespace bendcore::geometry {

/**
 * CAD-CORE-014: BRepBuilder
 * B-Rep (Boundary Representation) конструктор для создания тел
 * Поддерживает топологию: Vertex, Edge, Face, Shell, Solid
 */

using VertexId = int;
using EdgeId = int;
using FaceId = int;
using ShellId = int;
using SolidId = int;

/**
 * Vertex — вершина в B-Rep топологии
 */
struct Vertex {
    VertexId id = -1;
    Point3D point;

    Vertex() = default;
    Vertex(VertexId id, const Point3D& point) : id(id), point(point) {}

    [[nodiscard]] std::string to_string() const;
};

/**
 * Edge — ребро в B-Rep топологии
 */
struct Edge {
    EdgeId id = -1;
    VertexId start_vertex = -1;
    VertexId end_vertex = -1;
    std::vector<Point3D> control_points;  // Для кривых (NURBS, линия и т.д.)
    double length = 0.0;

    Edge() = default;
    Edge(EdgeId id, VertexId start, VertexId end) : id(id), start_vertex(start), end_vertex(end) {}
    [[nodiscard]] std::string to_string() const;
};

/**
 * Face — грань в B-Rep топологии
 */
struct Face {
    FaceId id = -1;
    std::vector<EdgeId> boundary_edges;  // Внешний контур
    std::vector<std::vector<EdgeId>> holes;  // Отверстия
    Vector3D normal;  // Нормаль к грани
    double area = 0.0;

    Face() = default;
    Face(FaceId id, const std::vector<EdgeId>& boundary) : id(id), boundary_edges(boundary) {}
    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] bool has_holes() const { return !holes.empty(); }
};

/**
 * Shell — оболочка (набор связных граней)
 */
struct Shell {
    ShellId id = -1;
    std::vector<FaceId> faces;
    bool is_closed = false;

    Shell() = default;
    Shell(ShellId id, const std::vector<FaceId>& faces) : id(id), faces(faces) {}
    [[nodiscard]] std::string to_string() const;
};

/**
 * Solid — твердое тело (одна внешняя оболочка + опциональные внутренние оболочки для полостей)
 */
struct Solid {
    SolidId id = -1;
    ShellId outer_shell = -1;
    std::vector<ShellId> inner_shells;  // Для полостей и отверстий
    double volume = 0.0;

    Solid() = default;
    Solid(SolidId id, ShellId outer_shell) : id(id), outer_shell(outer_shell) {}
    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] bool is_valid() const { return outer_shell >= 0; }
    [[nodiscard]] int euler_characteristic() const;
};

/**
 * BRepBuilder — конструктор B-Rep тел
 */
class BRepBuilder {
public:
    BRepBuilder(double tolerance = DEFAULT_TOLERANCE);

    // Создание топологических элементов
    [[nodiscard]] VertexId create_vertex(const Point3D& point);
    [[nodiscard]] EdgeId create_edge(VertexId start, VertexId end);
    [[nodiscard]] EdgeId create_edge_with_curve(VertexId start, VertexId end,
                                                const std::vector<Point3D>& control_points);
    [[nodiscard]] FaceId create_face(const std::vector<EdgeId>& boundary);
    [[nodiscard]] FaceId create_face_with_holes(const std::vector<EdgeId>& boundary,
                                               const std::vector<std::vector<EdgeId>>& holes);
    [[nodiscard]] ShellId create_shell(const std::vector<FaceId>& faces);
    [[nodiscard]] SolidId create_solid(ShellId outer_shell);

    // Добавление внутренних оболочек (полостей)
    bool add_inner_shell(SolidId solid, ShellId inner_shell);

    // Получение элементов
    [[nodiscard]] std::optional<Vertex> get_vertex(VertexId id) const;
    [[nodiscard]] std::optional<Edge> get_edge(EdgeId id) const;
    [[nodiscard]] std::optional<Face> get_face(FaceId id) const;
    [[nodiscard]] std::optional<Shell> get_shell(ShellId id) const;
    [[nodiscard]] std::optional<Solid> get_solid(SolidId id) const;

    // Валидация
    [[nodiscard]] bool validate_solid(SolidId solid) const;
    [[nodiscard]] bool is_solid_closed(SolidId solid) const;
    [[nodiscard]] int euler_characteristic(SolidId solid) const;

    // Boolean операции
    [[nodiscard]] std::optional<SolidId> boolean_union(SolidId s1, SolidId s2);
    [[nodiscard]] std::optional<SolidId> boolean_difference(SolidId s1, SolidId s2);
    [[nodiscard]] std::optional<SolidId> boolean_intersection(SolidId s1, SolidId s2);

    // Создание стандартных форм
    [[nodiscard]] SolidId create_box(double width, double height, double depth);
    [[nodiscard]] SolidId create_cylinder(double radius, double height);
    [[nodiscard]] SolidId create_sphere(double radius);
    [[nodiscard]] SolidId create_cone(double base_radius, double height);

    // Создание полостей и отверстий
    bool create_cavity(SolidId solid, const std::vector<Point3D>& cavity_profile, double depth);
    bool create_hole(SolidId solid, const Point3D& center, double diameter, double depth);

    // Статистика
    [[nodiscard]] int vertex_count() const { return vertices_.size(); }
    [[nodiscard]] int edge_count() const { return edges_.size(); }
    [[nodiscard]] int face_count() const { return faces_.size(); }
    [[nodiscard]] int shell_count() const { return shells_.size(); }
    [[nodiscard]] int solid_count() const { return solids_.size(); }

    // Очистка
    void clear();

private:
    double tolerance_;
    std::map<VertexId, Vertex> vertices_;
    std::map<EdgeId, Edge> edges_;
    std::map<FaceId, Face> faces_;
    std::map<ShellId, Shell> shells_;
    std::map<SolidId, Solid> solids_;

    int next_vertex_id_ = 0;
    int next_edge_id_ = 0;
    int next_face_id_ = 0;
    int next_shell_id_ = 0;
    int next_solid_id_ = 0;

    // Вспомогательные методы
    [[nodiscard]] bool is_valid_vertex(VertexId id) const;
    [[nodiscard]] bool is_valid_edge(EdgeId id) const;
    [[nodiscard]] bool is_valid_face(FaceId id) const;
    [[nodiscard]] bool is_valid_shell(ShellId id) const;
    [[nodiscard]] double calculate_face_area(const Face& face) const;
    [[nodiscard]] double calculate_solid_volume(const Solid& solid) const;
};

}  // namespace bendcore::geometry
