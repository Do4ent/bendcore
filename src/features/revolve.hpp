#pragma once

#include <vector>
#include <memory>
#include <optional>
#include "bendcore/geometry_kernel.hpp"

namespace bendcore::features {

struct RevolveParameters {
    double angle = 360.0;           // Revolution angle in degrees
    int num_segments = 12;          // Number of segments for revolution
    bool create_solid = true;       // Create solid or shell
};

class Revolve {
public:
    struct Result {
        bool success = false;
        std::string error_message;
        std::vector<Face> generated_faces;
        Solid generated_solid;
    };

    static Result revolve(
        const Face& profile_face,
        const Point3D& axis_origin,
        const Vector3D& axis_direction,
        const RevolveParameters& params
    );

    static bool validate_profile(const Face& profile_face);
    static bool validate_axis(const Vector3D& axis_direction);
    static std::vector<Face> generate_revolution_faces(
        const Face& profile,
        const Point3D& axis_origin,
        const Vector3D& axis_direction,
        double total_angle,
        int num_segments
    );
    static Solid generate_solid(
        const std::vector<Face>& revolution_faces,
        const Face& profile,
        double total_angle
    );
};

}  // namespace bendcore::features
