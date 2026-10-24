// DWFx Parser - Complete Implementation
// Phase 27/28: DWF/DWFx Import with W3D Mesh Integration
// Points 1-4: W3D Linking, Geometry Rounding, Port Detection, Mesh Sampling

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

namespace bendcore::import::dwfx {

// ========== Point 1: W3D Structure and Linking ==========

struct W3DTag {
    uint32_t tag_index;
    std::string segment_name;
    std::string library_include;
    std::vector<uint32_t> mesh_indices;
};

struct W3DInstance {
    uint32_t instance_id;
    std::string name;
    W3DTag tag;
    std::vector<float> transform_matrix;  // 4x4 transformation
    bool visible;
};

struct W3DGeometry {
    std::vector<float> vertices;
    std::vector<uint32_t> triangles;
    std::vector<float> normals;
};

// ========== Point 2: Geometry Rounding ==========

struct GeometryRoundingOptions {
    double length_tolerance = 1.0;       // mm
    double angle_tolerance = 0.5;        // degrees
    double diameter_tolerance = 0.1;     // mm
    double max_cumulative_error = 0.5;   // mm
};

struct RoundedSegment {
    double length;
    double angle_bend;
    double angle_rotation;
    double diameter;
    bool valid;
    std::string error_message;
};

// ========== Point 3: Automatic Port Detection ==========

struct PortCandidate {
    std::string feature_id;
    double diameter;
    double coaxiality_error;  // mm
    double angular_alignment; // degrees
    int priority;              // 0 = best match
};

struct DetectedPort {
    std::string port_id;
    uint32_t segment_index;
    std::vector<float> position;
    std::vector<float> direction;
    double diameter;
    double coaxiality_error;
    bool valid;
};

// ========== Point 4: Mesh-derived Tube Sampling ==========

struct TubeSample {
    double position_along_axis;
    double diameter;
    std::vector<float> cross_section_vertices;
    bool valid;
};

struct MeshTubeSamples {
    std::vector<TubeSample> samples;
    std::vector<float> centerline_points;
    double mean_diameter;
    double diameter_variance;
    bool is_tubular_geometry;
    std::string validation_status;  // OK | insufficient_data | mesh_not_tubular
};

// ========== DWFx Parser Main Interface ==========

class DWFxParser {
public:
    explicit DWFxParser(const std::string& dwfx_path);
    ~DWFxParser() = default;

    // Point 1: W3D Linking
    bool parseW3DStructure();
    std::vector<W3DInstance> getInstances() const;
    W3DGeometry getMeshGeometry(uint32_t mesh_index) const;

    // Point 2: Geometry Rounding
    RoundedSegment roundGeometry(
        double length,
        double angle_bend,
        const GeometryRoundingOptions& options
    ) const;

    // Point 3: Port Detection
    std::vector<PortCandidate> detectPortCandidates(
        const std::vector<float>& tube_endpoint,
        double diameter,
        const std::vector<W3DGeometry>& environment_features
    ) const;

    // Point 4: Mesh Sampling
    MeshTubeSamples sampleTubeFromMesh(
        const W3DGeometry& mesh,
        double section_interval = 10.0  // mm
    ) const;

private:
    std::string dwfx_path_;
    std::vector<W3DInstance> instances_;
    std::map<uint32_t, W3DGeometry> mesh_cache_;

    // BC-001: ZIP extraction support
    std::string extracted_temp_dir_;
    std::vector<char> content_xml_data_;
    std::vector<char> descriptor_list_data_;
    std::vector<std::string> w3d_files_;

    bool extractZipContents();
    bool parseContentXml();
    bool parseW3DOpCodes(const std::string& w3d_data);

    std::vector<float> computePrincipalAxis(const std::vector<float>& vertices) const;
    double computeDiameterAtSection(
        const std::vector<float>& vertices,
        const std::vector<float>& section_center,
        double tolerance
    ) const;
};

// ========== Utility Functions ==========

namespace util {
    // S-A-B path simplification
    RoundedSegment simplifyPath(
        double length,
        double angle_bend,
        double angle_rotation,
        const GeometryRoundingOptions& options
    );

    // Coaxiality matching
    double computeCoaxialityError(
        const std::vector<float>& axis1_direction,
        const std::vector<float>& axis2_direction,
        const std::vector<float>& offset
    );

    // Mesh validation
    bool validateMeshGeometry(const W3DGeometry& mesh);

    // Transform matrix utilities
    void applyTransform(
        std::vector<float>& vertices,
        const std::vector<float>& transform_matrix
    );
}

}  // namespace bendcore::import::dwfx
