#include "bendcore/features/loft.hpp"
#include <cmath>
#include <algorithm>

namespace BendCore::CadCore {

LoftFeature::LoftFeature(const EntityId& id, const std::string& name)
    : id_(id), name_(name) {
    config_.ruled_surface = false;
    config_.smooth_transition = true;
    config_.tolerance = 0.01;
    config_.closed = false;
}

bool LoftFeature::validate() const noexcept {
    // Check minimum profile count
    if (config_.profiles.size() < 2) {
        return false;
    }

    // Check that all profiles have valid sketch IDs
    for (const auto& profile : config_.profiles) {
        if (profile.sketch_id == EntityId()) {
            return false;
        }
    }

    // Check blend parameters are in valid range
    for (const auto& profile : config_.profiles) {
        if (profile.blend_parameter < 0.0 || profile.blend_parameter > 1.0) {
            return false;
        }
    }

    // Check tolerance is positive
    if (config_.tolerance <= 0.0) {
        return false;
    }

    // Check that profiles are properly ordered
    if (config_.profiles.size() > 1) {
        for (size_t i = 1; i < config_.profiles.size(); ++i) {
            if (config_.profiles[i].blend_parameter <= config_.profiles[i-1].blend_parameter) {
                return false;
            }
        }
    }

    return true;
}

} // namespace BendCore::CadCore
