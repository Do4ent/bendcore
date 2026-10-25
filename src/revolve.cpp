#include "bendcore/features/revolve.hpp"
#include <cmath>

namespace BendCore::CadCore {

RevolveFeature::RevolveFeature(const EntityId& id, const std::string& name)
    : id_(id), name_(name) {}

bool RevolveFeature::validate() const noexcept {
    // Profile sketch must be defined
    if (config_.profile_sketch == EntityId()) {
        return false;
    }

    // Axis direction must be non-zero
    double axis_length = config_.axis_direction.magnitude();
    if (axis_length < 1e-6) {
        return false;
    }

    // Revolution angle must be positive and reasonable
    if (config_.angle < 0.001 || config_.angle > 360.0) {
        return false;
    }

    // If bidirectional, angle must be <= 180
    if (config_.bidirectional && config_.angle > 180.0) {
        return false;
    }

    // If thin wall, thickness must be positive
    if (config_.thin_wall && config_.thickness <= 0.0) {
        return false;
    }

    // Tolerance should be positive
    if (config_.tolerance <= 0.0) {
        return false;
    }

    return true;
}

} // namespace BendCore::CadCore
