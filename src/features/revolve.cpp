#include "revolve.hpp"
#include <cmath>

namespace bendcore::features {

Revolve::Result Revolve::revolve(
    const Face& profile_face,
    const Point3D& axis_origin,
    const Vector3D& axis_direction,
    const RevolveParameters& params
) {
    Result result;

    if (!validate_profile(profile_face)) {
        result.error_message = "Invalid profile face";
        return result;
    }

    if (!validate_axis(axis_direction)) {
        result.error_message = "Invalid revolution axis";
        return result;
    }

    // Convert angle to radians
    double total_angle = params.angle * M_PI / 180.0;

    result.generated_faces = generate_revolution_faces(
        profile_face,
        axis_origin,
        axis_direction.normalized(),
        total_angle,
        params.num_segments
    );

    if (result.generated_faces.empty()) {
        result.error_message = "Failed to generate revolution faces";
        return result;
    }

    if (params.create_solid) {
        result.generated_solid = generate_solid(
            result.generated_faces,
            profile_face,
            total_angle
        );
    }

    result.success = true;
    return result;
}

bool Revolve::validate_profile(const Face& profile_face) {
    return !profile_face.vertices.empty() && profile_face.vertices.size() >= 2;
}

bool Revolve::validate_axis(const Vector3D& axis_direction) {
    return axis_direction.length() > 1e-9;
}

std::vector<Face> Revolve::generate_revolution_faces(
    const Face& profile,
    const Point3D& axis_origin,
    const Vector3D& axis_direction,
    double total_angle,
    int num_segments
) {
    std::vector<Face> faces;

    if (num_segments < 3) num_segments = 3;
    if (num_segments > 128) num_segments = 128;

    double angle_step = total_angle / num_segments;

    for (int i = 0; i < num_segments; ++i) {
        double angle1 = i * angle_step;
        double angle2 = (i + 1) * angle_step;

        Face segment_face;

        for (const auto& vertex : profile.vertices) {
            Vector3D from_axis = vertex - axis_origin;
            
            double cost1 = std::cos(angle1);
            double sint1 = std::sin(angle1);
            
            Point3D v1 = vertex;
            
            double cost2 = std::cos(angle2);
            double sint2 = std::sin(angle2);
            
            Point3D v2 = vertex;

            segment_face.vertices.push_back(v1);
        }

        if (!segment_face.vertices.empty()) {
            faces.push_back(segment_face);
        }
    }

    return faces;
}

Solid Revolve::generate_solid(
    const std::vector<Face>& revolution_faces,
    const Face& profile,
    double total_angle
) {
    Solid solid;

    solid.faces.insert(solid.faces.end(), revolution_faces.begin(), revolution_faces.end());
    solid.faces.push_back(profile);

    return solid;
}

}
