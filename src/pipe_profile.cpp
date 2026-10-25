#include "bendcore/pipe_profile.h"

namespace bendcore::pipe {

std::vector<PipeProfile> PipeProfileLibrary::getDIN2391Profiles() {
    std::vector<PipeProfile> profiles;
    // DIN 2391 seamless steel tubes - common hydraulic/pneumatic sizes
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "DIN2391-6x1", 6.0, 4.0, 1.0, 1000, "steel"));
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "DIN2391-8x1", 8.0, 6.0, 1.0, 1000, "steel"));
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "DIN2391-10x1.5", 10.0, 7.0, 1.5, 1000, "steel"));
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "DIN2391-12x1.5", 12.0, 9.0, 1.5, 1000, "steel"));
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "DIN2391-16x2", 16.0, 12.0, 2.0, 1000, "steel"));
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "DIN2391-20x2", 20.0, 16.0, 2.0, 1000, "steel"));
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "DIN2391-25x2.5", 25.0, 20.0, 2.5, 1000, "steel"));
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "DIN2391-32x2.5", 32.0, 27.0, 2.5, 1000, "steel"));
    return profiles;
}

std::vector<PipeProfile> PipeProfileLibrary::getISOProfiles() {
    std::vector<PipeProfile> profiles;
    // ISO 6149 metric ports - common in ISO 11582/ISO 6162 hydraulic systems
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "ISO-M6x1", 6.0, 4.2, 0.9, 1000, "steel"));
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "ISO-M8x1.5", 8.0, 5.0, 1.5, 1000, "steel"));
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "ISO-M10x1.5", 10.0, 7.0, 1.5, 1000, "steel"));
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "ISO-M12x1.75", 12.0, 8.5, 1.75, 1000, "steel"));
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "ISO-M16x2", 16.0, 12.0, 2.0, 1000, "steel"));
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "ISO-M20x2.5", 20.0, 15.0, 2.5, 1000, "steel"));
    return profiles;
}

std::vector<PipeProfile> PipeProfileLibrary::getGOSTProfiles() {
    std::vector<PipeProfile> profiles;
    // GOST 8732 seamless steel pipes
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "GOST8732-10x1", 10.0, 8.0, 1.0, 1000, "steel"));
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "GOST8732-12x1.5", 12.0, 9.0, 1.5, 1000, "steel"));
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "GOST8732-14x1.5", 14.0, 11.0, 1.5, 1000, "steel"));
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "GOST8732-16x2", 16.0, 12.0, 2.0, 1000, "steel"));
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "GOST8732-20x2", 20.0, 16.0, 2.0, 1000, "steel"));
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "GOST8732-25x2.5", 25.0, 20.0, 2.5, 1000, "steel"));
    profiles.push_back(PipeProfile(ProfileType::CIRCULAR, "GOST8732-32x2.5", 32.0, 27.0, 2.5, 1000, "steel"));
    return profiles;
}

std::optional<PipeProfile> PipeProfileLibrary::findProfile(const std::string& designation) {
    auto din = getDIN2391Profiles();
    auto iso = getISOProfiles();
    auto gost = getGOSTProfiles();

    for (const auto& p : din) {
        if (p.designation == designation) return p;
    }
    for (const auto& p : iso) {
        if (p.designation == designation) return p;
    }
    for (const auto& p : gost) {
        if (p.designation == designation) return p;
    }

    return std::nullopt;
}

} // namespace bendcore::pipe
