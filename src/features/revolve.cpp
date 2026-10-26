#include "bendcore/features/revolve.hpp"
#include <algorithm>
#include <cmath>

namespace BendCore::CadCore {

RevolveFeature::RevolveFeature(const EntityId& id, const std::string& name)
    : id_(id), name_(name) {
    config_.profile_sketch = EntityId();
    config_.axis_type = AxisType::Z;
    config_.axis_origin = Vector3D(0.0, 0.0, 0.0);
    config_.axis_direction = Vector3D(0.0, 0.0, 1.0);
    config_.angle = 360.0;
    config_.bidirectional = false;
    config_.thin_wall = false;
    config_.thickness = 0.0;
    config_.tolerance = 0.01;
}

bool RevolveFeature::validate() const noexcept {
    // Check profile is defined
    if (config_.profile_sketch == EntityId()) {
        return false;
    }

    // Check angle is positive and valid
    if (config_.angle <= 0.0 || config_.angle > 360.0) {
        return false;
    }

    // Check tolerance is positive
    if (config_.tolerance <= 0.0) {
        return false;
    }

    // Check axis direction is normalized (approximate check)
    double magnitude = std::sqrt(
        config_.axis_direction.x * config_.axis_direction.x +
        config_.axis_direction.y * config_.axis_direction.y +
        config_.axis_direction.z * config_.axis_direction.z
    );
    if (magnitude < 0.99 || magnitude > 1.01) {
        return false;
    }

    // Check thin wall parameters
    if (config_.thin_wall) {
        if (config_.thickness <= 0.0) {
            return false;
        }
    }

    // If bidirectional, angle must be less than 360
    if (config_.bidirectional && std::abs(config_.angle - 360.0) < 0.001) {
        return false;
    }

    // Check axis origin is finite
    if (!std::isfinite(config_.axis_origin.x) ||
        !std::isfinite(config_.axis_origin.y) ||
        !std::isfinite(config_.axis_origin.z)) {
        return false;
    }

    return true;
}

} // namespace BendCore::CadCore
