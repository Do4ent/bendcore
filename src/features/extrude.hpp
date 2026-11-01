#pragma once

#include <vector>
#include <memory>
#include <optional>
#include "bendcore/geometry_kernel.hpp"

namespace bendcore::features {

struct ExtrudeParameters {
    double length = 10.0;           // Extrusion length
    bool symmetric = false;          // Extrude in both directions
    double taper_angle = 0.0;        // Taper angle in degrees
    bool create_solid = true;        // Create solid or shell
};

class Extrude {
public:
    struct Result {
        bool success = false;
        std::string error_message;
        std::vector<Face> generated_faces;
        Solid generated_solid;
    };

    static Result extrude(
        const Face& profile_face,
        const ExtrudeParameters& params,
        const Vector3D& direction = Vector3D(0, 0, 1)
    );

    static bool validate_profile(const Face& profile_face);
    static Vector3D compute_extrusion_direction(const Face& face);
    static std::vector<Face> generate_faces(
        const Face& profile,
        const Vector3D& direction,
        double length
    );
    static Solid generate_solid(
        const Face& profile,
        const std::vector<Face>& lateral_faces,
        const Face& top_face
    );
};

}  // namespace bendcore::features
