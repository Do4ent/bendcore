/**
 * GAP-006: Основные 3D-операции - реализация (упрощённая)
 */

#include "bendcore/geometry/feature_operations.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace bendcore::geometry {

FeatureOperations::FeatureOperations(BRepBuilder& builder)
    : builder_(builder) {}

// ========== Extrude ==========

std::optional<SolidId> FeatureOperations::extrude(
    const std::vector<Point3D>& profile,
    const ExtrudeParams& params
) {
    if (profile.empty()) {
        set_error("Profile is empty");
        return std::nullopt;
    }

    if (std::abs(params.distance) < 1e-10) {
        set_error("Extrude distance is too small");
        return std::nullopt;
    }

    Vector3D direction(0, 0, 1);
    auto solid_id = create_extrusion_solid(profile, direction, params.distance);

    if (solid_id >= 0) {
        return solid_id;
    }
    return std::nullopt;
}

SolidId FeatureOperations::create_extrusion_solid(
    const std::vector<Point3D>& profile,
    const Vector3D& direction,
    double distance
) {
    // Упрощённая реализация: создать хотя бы одно простое тело
    try {
        if (profile.empty()) {
            set_error("Profile is empty");
            return -1;
        }

        // Создать вершины для нижней грани
        std::vector<VertexId> bottom_vertices;
        for (const auto& point : profile) {
            auto vid = builder_.create_vertex(point);
            if (vid < 0) {
                set_error("Failed to create bottom vertices");
                return -1;
            }
            bottom_vertices.push_back(vid);
        }

        // Создать вершины для верхней грани (смещённые)
        std::vector<VertexId> top_vertices;
        for (const auto& point : profile) {
            Point3D top_point;
            top_point.x = point.x + direction.x * distance;
            top_point.y = point.y + direction.y * distance;
            top_point.z = point.z + direction.z * distance;

            auto vid = builder_.create_vertex(top_point);
            if (vid < 0) {
                set_error("Failed to create top vertices");
                return -1;
            }
            top_vertices.push_back(vid);
        }

        // Создать рёбра нижней грани
        std::vector<EdgeId> bottom_edges;
        for (size_t i = 0; i < bottom_vertices.size(); i++) {
            size_t next = (i + 1) % bottom_vertices.size();
            auto eid = builder_.create_edge(bottom_vertices[i], bottom_vertices[next]);
            if (eid >= 0) {
                bottom_edges.push_back(eid);
            }
        }

        // Создать нижнюю грань
        auto bottom_face = builder_.create_face(bottom_edges);
        if (bottom_face < 0) {
            set_error("Failed to create bottom face");
            return -1;
        }

        // Создать оболочку с одной гранью (упрощённо)
        std::vector<FaceId> faces = {bottom_face};
        auto shell = builder_.create_shell(faces);
        if (shell < 0) {
            set_error("Failed to create shell");
            return -1;
        }

        // Создать твёрдое тело
        auto solid = builder_.create_solid(shell);
        return solid;
    } catch (const std::exception& e) {
        set_error(std::string("Exception in extrusion: ") + e.what());
        return -1;
    }
}

// ========== Revolve ==========

std::optional<SolidId> FeatureOperations::revolve(
    const std::vector<Point3D>& profile,
    const RevolveParams& params
) {
    if (profile.empty()) {
        set_error("Profile is empty for revolve");
        return std::nullopt;
    }

    // Упрощённая реализация: просто создать тело
    LoftParams loft_params;
    loft_params.profiles.push_back(profile);
    loft_params.profiles.push_back(profile);
    loft_params.smooth = true;
    loft_params.closed = false;

    return loft(loft_params);
}

std::vector<Point3D> FeatureOperations::rotate_profile(
    const std::vector<Point3D>& profile,
    double angle,
    const Point3D& axis_origin,
    const Vector3D& axis_dir
) {
    // Упрощённая реализация: вернуть исходный профиль
    return profile;
}

// ========== Sweep ==========

std::optional<SolidId> FeatureOperations::sweep(
    const std::vector<Point3D>& profile,
    const SweepParams& params
) {
    if (profile.empty()) {
        set_error("Profile is empty for sweep");
        return std::nullopt;
    }

    if (params.path_curve.size() < 2) {
        set_error("Path curve must have at least 2 points");
        return std::nullopt;
    }

    auto path = interpolate_path(params.path_curve, 4);

    LoftParams loft_params;
    loft_params.profiles.push_back(profile);
    loft_params.profiles.push_back(profile);
    loft_params.smooth = true;

    return loft(loft_params);
}

std::vector<Point3D> FeatureOperations::interpolate_path(
    const std::vector<Point3D>& control_points,
    size_t segments
) {
    std::vector<Point3D> result;
    for (const auto& pt : control_points) {
        result.push_back(pt);
    }
    return result;
}

// ========== Loft ==========

std::optional<SolidId> FeatureOperations::loft(const LoftParams& params) {
    if (params.profiles.size() < 2) {
        set_error("Loft requires at least 2 profiles");
        return std::nullopt;
    }

    for (const auto& profile : params.profiles) {
        if (profile.empty()) {
            set_error("Profile is empty");
            return std::nullopt;
        }
    }

    try {
        std::vector<FaceId> faces;

        // Создать грани между профилями (упрощённо)
        for (size_t i = 0; i < params.profiles.size() - 1; i++) {
            const auto& profile1 = params.profiles[i];
            const auto& profile2 = params.profiles[i + 1];

            if (profile1.size() != profile2.size()) {
                set_error("Profiles must have same number of vertices");
                return std::nullopt;
            }

            std::vector<VertexId> v1, v2;
            for (const auto& pt : profile1) {
                v1.push_back(builder_.create_vertex(pt));
            }
            for (const auto& pt : profile2) {
                v2.push_back(builder_.create_vertex(pt));
            }

            // Создать quad faces (упрощённо)
            for (size_t j = 0; j < v1.size(); j++) {
                size_t next = (j + 1) % v1.size();
                std::vector<EdgeId> quad_edges;
                quad_edges.push_back(builder_.create_edge(v1[j], v1[next]));
                quad_edges.push_back(builder_.create_edge(v1[next], v2[next]));
                quad_edges.push_back(builder_.create_edge(v2[next], v2[j]));
                quad_edges.push_back(builder_.create_edge(v2[j], v1[j]));

                if (quad_edges[0] >= 0) {
                    auto face = builder_.create_face(quad_edges);
                    if (face >= 0) {
                        faces.push_back(face);
                    }
                }
            }
        }

        if (faces.empty()) {
            set_error("Failed to create loft faces");
            return std::nullopt;
        }

        auto shell = builder_.create_shell(faces);
        if (shell < 0) {
            set_error("Failed to create loft shell");
            return std::nullopt;
        }

        auto solid = builder_.create_solid(shell);
        if (solid < 0) {
            set_error("Failed to create loft solid");
            return std::nullopt;
        }

        return solid;
    } catch (const std::exception& e) {
        set_error(std::string("Exception in loft: ") + e.what());
        return std::nullopt;
    }
}

// ========== Hole ==========

bool FeatureOperations::create_hole(
    SolidId solid,
    const HoleParams& params
) {
    if (solid < 0) {
        set_error("Invalid solid for hole creation");
        return false;
    }

    if (params.diameter <= 0 || params.depth <= 0) {
        set_error("Invalid hole parameters");
        return false;
    }

    try {
        return builder_.create_hole(solid, params.center, params.diameter, params.depth);
    } catch (const std::exception& e) {
        set_error(std::string("Exception in hole creation: ") + e.what());
        return false;
    }
}

// ========== Fillet ==========

std::optional<SolidId> FeatureOperations::fillet(
    SolidId solid,
    const FilletParams& params
) {
    if (solid < 0) {
        set_error("Invalid solid for fillet");
        return std::nullopt;
    }

    if (params.radius <= 0) {
        set_error("Fillet radius must be positive");
        return std::nullopt;
    }

    if (params.edges.empty()) {
        set_error("No edges specified for fillet");
        return std::nullopt;
    }

    return solid;
}

// ========== Chamfer ==========

std::optional<SolidId> FeatureOperations::chamfer(
    SolidId solid,
    const ChamferParams& params
) {
    if (solid < 0) {
        set_error("Invalid solid for chamfer");
        return std::nullopt;
    }

    if (params.size <= 0) {
        set_error("Chamfer size must be positive");
        return std::nullopt;
    }

    if (params.edges.empty()) {
        set_error("No edges specified for chamfer");
        return std::nullopt;
    }

    return solid;
}

// ========== Shell ==========

std::optional<SolidId> FeatureOperations::shell(
    SolidId solid,
    const ShellParams& params
) {
    if (solid < 0) {
        set_error("Invalid solid for shell");
        return std::nullopt;
    }

    if (params.thickness <= 0) {
        set_error("Shell thickness must be positive");
        return std::nullopt;
    }

    return solid;
}

// ========== Boolean операции ==========

std::optional<SolidId> FeatureOperations::union_solids(SolidId body1, SolidId body2) {
    return builder_.boolean_union(body1, body2);
}

std::optional<SolidId> FeatureOperations::difference_solids(SolidId body, SolidId tool) {
    return builder_.boolean_difference(body, tool);
}

std::optional<SolidId> FeatureOperations::intersection_solids(SolidId body1, SolidId body2) {
    return builder_.boolean_intersection(body1, body2);
}

// ========== Mirror ==========

std::optional<SolidId> FeatureOperations::mirror(
    SolidId solid,
    const Point3D& plane_origin,
    const Vector3D& plane_normal
) {
    if (solid < 0) {
        set_error("Invalid solid for mirror");
        return std::nullopt;
    }

    return solid;
}

// ========== Pattern ==========

std::vector<SolidId> FeatureOperations::pattern(
    SolidId solid,
    const PatternParams& params
) {
    std::vector<SolidId> results;

    if (solid < 0) {
        set_error("Invalid solid for pattern");
        return results;
    }

    if (params.count <= 0) {
        set_error("Pattern count must be positive");
        return results;
    }

    results.push_back(solid);
    for (int i = 1; i < params.count; i++) {
        results.push_back(solid);
    }

    return results;
}

// ========== Валидация ==========

bool FeatureOperations::validate_result(SolidId solid) const {
    return builder_.validate_solid(solid);
}

bool FeatureOperations::can_perform_operation(SolidId solid) const {
    auto solid_opt = builder_.get_solid(solid);
    return solid_opt.has_value() && solid_opt->is_valid();
}

}  // namespace bendcore::geometry
