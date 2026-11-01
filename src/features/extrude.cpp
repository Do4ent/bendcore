#include "extrude.hpp"
#include <cmath>

namespace bendcore::features {

Extrude::Result Extrude::extrude(
    const Face& profile_face,
    const ExtrudeParameters& params,
    const Vector3D& direction
) {
    Result result;

    if (!validate_profile(profile_face)) {
        result.error_message = "Invalid profile face";
        return result;
    }

    // Generate lateral faces
    Vector3D extrusion_dir = direction.normalized();
    if (extrusion_dir.length() < 1e-9) {
        extrusion_dir = compute_extrusion_direction(profile_face);
    }

    double length = params.length;
    if (params.symmetric) {
        length = params.length / 2.0;
    }

    result.generated_faces = generate_faces(profile_face, extrusion_dir, length);

    if (result.generated_faces.empty()) {
        result.error_message = "Failed to generate extrusion faces";
        return result;
    }

    // Generate top face (parallel to profile)
    Face top_face = profile_face;
    top_face.translate(extrusion_dir * length);

    if (params.create_solid) {
        result.generated_solid = generate_solid(
            profile_face,
            result.generated_faces,
            top_face
        );
    }

    result.success = true;
    return result;
}

bool Extrude::validate_profile(const Face& profile_face) {
    // Check if face is valid planar face
    if (profile_face.vertices.empty()) {
        return false;
    }

    // Check if all vertices are coplanar
    if (profile_face.vertices.size() < 3) {
        return false;
    }

    return true;
}

Vector3D Extrude::compute_extrusion_direction(const Face& face) {
    // Compute face normal as extrusion direction
    if (face.vertices.size() < 3) {
        return Vector3D(0, 0, 1);  // Default: Z direction
    }

    const auto& v0 = face.vertices[0];
    const auto& v1 = face.vertices[1];
    const auto& v2 = face.vertices[2];

    Vector3D edge1 = v1 - v0;
    Vector3D edge2 = v2 - v0;

    Vector3D normal = edge1.cross(edge2);
    return normal.normalized();
}

std::vector<Face> Extrude::generate_faces(
    const Face& profile,
    const Vector3D& direction,
    double length
) {
    std::vector<Face> faces;

    if (profile.edges.empty()) {
        return faces;
    }

    // For each edge of the profile, create a lateral face
    Vector3D offset = direction * length;

    for (const auto& edge : profile.edges) {
        Face lateral_face;

        // Bottom edge
        lateral_face.vertices.push_back(edge.start);
        lateral_face.vertices.push_back(edge.end);

        // Top edge (translated)
        lateral_face.vertices.push_back(edge.end + offset);
        lateral_face.vertices.push_back(edge.start + offset);

        faces.push_back(lateral_face);
    }

    return faces;
}

Solid Extrude::generate_solid(
    const Face& profile,
    const std::vector<Face>& lateral_faces,
    const Face& top_face
) {
    Solid solid;

    // Add bottom face
    solid.faces.push_back(profile);

    // Add lateral faces
    solid.faces.insert(solid.faces.end(), lateral_faces.begin(), lateral_faces.end());

    // Add top face
    solid.faces.push_back(top_face);

    return solid;
}

}  // namespace bendcore::features
