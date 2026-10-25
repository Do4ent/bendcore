#include "bendcore/features/sweep.hpp"

namespace BendCore::CadCore {

SweepFeature::SweepFeature(const EntityId& id, const std::string& name)
    : id_(id), name_(name) {}

bool SweepFeature::validate() const noexcept {
    // Profile sketch must be defined
    if (config_.profile_sketch == EntityId()) {
        return false;
    }

    // Path entity must be defined
    if (config_.path_entity == EntityId()) {
        return false;
    }

    // Scale factors should be positive
    if (config_.scale_start <= 0.0 || config_.scale_end <= 0.0) {
        return false;
    }

    // Twist angle should be valid (typically -360 to 360 degrees)
    if (config_.twist_angle < -360.0 || config_.twist_angle > 360.0) {
        return false;
    }

    // Normal direction should be non-zero
    if (config_.normal_direction.magnitude() < 1e-6) {
        return false;
    }

    // Tolerance should be positive
    if (config_.tolerance <= 0.0) {
        return false;
    }

    return true;
}

} // namespace BendCore::CadCore
