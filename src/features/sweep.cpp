#include "bendcore/features/sweep.hpp"
#include <cmath>
#include <algorithm>

namespace BendCore::CadCore {

SweepFeature::SweepFeature(const EntityId& id, const std::string& name)
    : id_(id), name_(name) {
    config_.profile_sketch = EntityId();
    config_.path_entity = EntityId();
    config_.scale_start = 1.0;
    config_.scale_end = 1.0;
    config_.twist_angle = 0.0;
    config_.normal_direction = Vector3D(0.0, 0.0, 1.0);
    config_.keep_profile_normal = true;
    config_.closed_profile = false;
    config_.tolerance = 0.01;
}

bool SweepFeature::validate() const noexcept {
    // Check profile is defined
    if (config_.profile_sketch == EntityId()) {
        return false;
    }

    // Check path is defined
    if (config_.path_entity == EntityId()) {
        return false;
    }

    // Check scale values are positive
    if (config_.scale_start <= 0.0 || config_.scale_end <= 0.0) {
        return false;
    }

    // Check tolerance is positive
    if (config_.tolerance <= 0.0) {
        return false;
    }

    // Check normal direction is normalized (approximate)
    double magnitude = std::sqrt(
        config_.normal_direction.x * config_.normal_direction.x +
        config_.normal_direction.y * config_.normal_direction.y +
        config_.normal_direction.z * config_.normal_direction.z
    );
    if (magnitude < 0.99 || magnitude > 1.01) {
        return false;
    }

    // Twist angle is unbounded but should be finite
    if (!std::isfinite(config_.twist_angle)) {
        return false;
    }

    return true;
}

} // namespace BendCore::CadCore
