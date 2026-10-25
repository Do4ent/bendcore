#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <cmath>

namespace bendcore::pipe {

enum class ProfileType {
    CIRCULAR,
    SQUARE,
    RECTANGULAR,
    OVAL,
    HEXAGONAL,
    CUSTOM
};

struct PipeProfile {
    ProfileType type;
    std::string designation;
    double outerDiameter;        // mm
    double innerDiameter;        // mm
    double wallThickness;        // mm
    double length;               // mm
    std::string material;        // steel, aluminum, copper, titanium

    PipeProfile() : type(ProfileType::CIRCULAR), outerDiameter(0),
                    innerDiameter(0), wallThickness(0), length(0) {}

    PipeProfile(ProfileType t, const std::string& des, double od,
                double id, double wt, double len, const std::string& mat)
        : type(t), designation(des), outerDiameter(od), innerDiameter(id),
          wallThickness(wt), length(len), material(mat) {}

    double getCrossSectionArea() const {
        if (type == ProfileType::CIRCULAR) {
            double ri = innerDiameter / 2.0;
            double ro = outerDiameter / 2.0;
            return M_PI * (ro * ro - ri * ri);
        }
        return 0.0; // Simplified
    }

    double getMomentOfInertia() const {
        if (type == ProfileType::CIRCULAR) {
            double ri = innerDiameter / 2.0;
            double ro = outerDiameter / 2.0;
            return M_PI / 64.0 * (ro * ro * ro * ro - ri * ri * ri * ri);
        }
        return 0.0;
    }

    double getSectionModulus() const {
        if (outerDiameter > 0) {
            return getMomentOfInertia() / (outerDiameter / 2.0);
        }
        return 0.0;
    }
};

// Standard DIN, ISO, GOST pipe profiles
class PipeProfileLibrary {
public:
    static std::vector<PipeProfile> getDIN2391Profiles();
    static std::vector<PipeProfile> getISOProfiles();
    static std::vector<PipeProfile> getGOSTProfiles();
    static std::optional<PipeProfile> findProfile(const std::string& designation);
};

} // namespace bendcore::pipe
