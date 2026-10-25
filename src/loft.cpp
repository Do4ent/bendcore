#include "bendcore/features/loft.hpp"

namespace BendCore::CadCore {

LoftFeature::LoftFeature(const EntityId& id, const std::string& name)
    : id_(id), name_(name) {}

bool LoftFeature::validate() const noexcept {
    // Need at least 2 profiles
    if (config_.profiles.size() < 2) {
        return false;
    }

    // All profiles must have valid sketch IDs
    for (const auto& profile : config_.profiles) {
        if (profile.sketch_id == EntityId()) {
            return false;
        }
    }

    // Blend parameters should be between 0 and 1
    for (size_t i = 0; i < config_.profiles.size(); ++i) {
        const auto& profile = config_.profiles[i];
        if (profile.blend_parameter < 0.0 || profile.blend_parameter > 1.0) {
            return false;
        }

        // For non-closed loft, first should be 0 and last should be 1
        if (!config_.closed) {
            if (i == 0 && profile.blend_parameter != 0.0) {
                return false;
            }
            if (i == config_.profiles.size() - 1 &&
                profile.blend_parameter != 1.0) {
                return false;
            }
        }
    }

    // Tolerance should be positive
    if (config_.tolerance <= 0.0) {
        return false;
    }

    return true;
}

} // namespace BendCore::CadCore
