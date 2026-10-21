#include "bendcore/dwfx/import_coordinator.hpp"
#include "bendcore/dwfx/mesh_sampling.hpp"
#include "bendcore/dwfx/geometry_rounding.hpp"
#include "bendcore/dwfx/autoport_detection.hpp"

#include <cmath>
#include <algorithm>

namespace bendcore::dwfx {

namespace {

std::vector<PathSegment> samples_to_path_segments(const std::vector<TubeSample>& samples) {
    std::vector<PathSegment> segments;
    
    if (samples.size() < 2) return segments;
    
    double avg_diameter = 0.0;
    for (const auto& s : samples) {
        avg_diameter += s.diameter_mm;
    }
    avg_diameter /= static_cast<double>(samples.size());
    
    for (size_t i = 0; i < samples.size() - 1; ++i) {
        const auto& s1 = samples[i];
        const auto& s2 = samples[i + 1];
        
        const double dx = s2.position.x - s1.position.x;
        const double dy = s2.position.y - s1.position.y;
        const double dz = s2.position.z - s1.position.z;
        const double length = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        PathSegment seg;
        seg.kind = PathSegment::Straight;
        seg.length_mm = length;
        seg.radius_mm = 0.0;
        seg.angle_deg = 0.0;
        seg.rotation_deg = 0.0;
        segments.push_back(seg);
    }
    
    return segments;
}

TubeModel create_tube_model(
    const std::string& tube_id,
    const MeshSamplingResult& mesh_result,
    const RoundingResult& rounding_result,
    const std::vector<DetectedPort>& detected_ports,
    double od_mm,
    double wall_mm) {
    
    TubeModel model;
    model.id = tube_id;
    model.source = "dwfx";
    model.od = od_mm;
    model.wall = wall_mm;
    
    bendcore::Path path;
    for (const auto& seg : rounding_result.segments) {
        if (seg.radius_mm < 1e-6) {
            path.push_back(bendcore::Straight{seg.length_mm});
        } else {
            path.push_back(bendcore::Bend{
                seg.radius_mm,
                seg.angle_deg,
                seg.rotation_deg
            });
        }
    }
    
    if (!path.empty()) {
        model.path = path;
    }
    
    for (const auto& det_port : detected_ports) {
        bendcore::Port port;
        port.name = det_port.feature.feature_name;
        port.pos = {
            det_port.feature.axis_point.x,
            det_port.feature.axis_point.y,
            det_port.feature.axis_point.z
        };
        port.direction = {
            det_port.feature.axis_direction.x,
            det_port.feature.axis_direction.y,
            det_port.feature.axis_direction.z
        };
        port.od = det_port.feature.diameter_mm;
        
        model.ports.push_back(port);
    }
    
    return model;
}

}

ImportCoordinatorResult import_tube_from_mesh(
    const std::string& tube_id,
    const std::vector<Vec3d>& vertices,
    const std::vector<std::array<std::int32_t, 3>>& faces,
    const std::vector<Vec3d>& environment_features,
    double od_mm,
    double wall_mm,
    const ImportCoordinatorSettings& settings) {
    
    ImportCoordinatorResult result;
    result.success = false;
    
    MeshSamplingSettings mesh_settings;
    mesh_settings.radius_tolerance_rel = settings.mesh_radius_tolerance_rel;
    mesh_settings.radius_tolerance_mm = settings.mesh_radius_tolerance_mm;
    mesh_settings.min_samples = settings.mesh_min_samples;
    
    const auto mesh_result = sample_tube_from_mesh(vertices, faces, mesh_settings);
    result.mesh_sampling_status = mesh_result.validation_status;
    
    if (mesh_result.validation_status != "OK") {
        result.error_message = "Mesh sampling failed: " + mesh_result.validation_status;
        return result;
    }
    
    if (mesh_result.samples.empty()) {
        result.error_message = "No tube samples extracted from mesh";
        return result;
    }
    
    const auto path_segments = samples_to_path_segments(mesh_result.samples);
    
    RoundingSettings rounding_settings;
    rounding_settings.round_length_mm = settings.round_length_mm;
    rounding_settings.round_angle_deg = settings.round_angle_deg;
    rounding_settings.round_align_deg = settings.round_align_deg;
    rounding_settings.round_diameter_wall_mm = settings.round_diameter_wall_mm;
    rounding_settings.max_deviation_mm = settings.max_deviation_mm;
    
    const auto rounding_result = round_path(path_segments, od_mm, wall_mm, rounding_settings);
    result.geometry_rounding_status = rounding_result.exceeds_tolerance ? "exceeds_tolerance" : "OK";
    
    std::vector<DetectedPort> detected_ports;
    
    if (!environment_features.empty() && mesh_result.start_point && mesh_result.end_point) {
        std::vector<CylindricalFeature> env_features;
        for (size_t i = 0; i < environment_features.size(); ++i) {
            env_features.push_back({
                environment_features[i],
                {0, 0, 1},
                od_mm,
                false,
                "env_" + std::to_string(i)
            });
        }
        
        AutoportSettings autoport_settings;
        autoport_settings.linear_tolerance_mm = settings.port_linear_tolerance_mm;
        autoport_settings.angular_tolerance_deg = settings.port_angular_tolerance_deg;
        autoport_settings.od_tolerance_rel = settings.port_od_tolerance_rel;
        autoport_settings.od_tolerance_min_mm = settings.port_od_tolerance_min_mm;
        
        detected_ports = detect_ports(
            mesh_result.start_point.value(),
            mesh_result.end_point.value(),
            {0, 0, 1},
            od_mm,
            env_features,
            autoport_settings
        );
    }
    
    result.detected_ports_count = static_cast<int>(detected_ports.size());
    
    const auto tube_model = create_tube_model(
        tube_id,
        mesh_result,
        rounding_result,
        detected_ports,
        od_mm,
        wall_mm
    );
    
    result.tube_model = tube_model;
    result.success = true;
    
    return result;
}

}
