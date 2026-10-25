#include "bendcore/compat/legacy_adapter.hpp"

namespace BendCore::CadCore {

LegacyVersion LegacyAdapter::detectVersion(const std::vector<uint8_t>& data) noexcept {
    if (data.empty()) return LegacyVersion::UNKNOWN;

    // Simple version detection based on first bytes
    if (data.size() >= 4) {
        uint32_t magic = *reinterpret_cast<const uint32_t*>(data.data());

        if (magic == 0x00000001) return LegacyVersion::V1_0;
        if (magic == 0x00000015) return LegacyVersion::V1_5;
        if (magic == 0x00000020) return LegacyVersion::V2_0;
    }

    return LegacyVersion::UNKNOWN;
}

bool LegacyAdapter::convertFromLegacy(CadDocument& doc,
                                      const std::vector<uint8_t>& legacyData,
                                      LegacyVersion version) noexcept {
    switch (version) {
        case LegacyVersion::V1_0:
            return convertFromV1(doc, legacyData);
        case LegacyVersion::V1_5:
            return convertFromV1_5(doc, legacyData);
        case LegacyVersion::V2_0:
            return convertFromV2(doc, legacyData);
        case LegacyVersion::UNKNOWN:
            return false;
    }
    return false;
}

std::vector<uint8_t> LegacyAdapter::convertToLegacy(const CadDocument& doc,
                                                     LegacyVersion version) const noexcept {
    (void)doc;
    std::vector<uint8_t> result;

    // Write version marker
    switch (version) {
        case LegacyVersion::V1_0:
            result.push_back(0x01);
            break;
        case LegacyVersion::V1_5:
            result.push_back(0x15);
            break;
        case LegacyVersion::V2_0:
            result.push_back(0x20);
            break;
        case LegacyVersion::UNKNOWN:
            break;
    }

    return result;
}

bool LegacyAdapter::isSupportedVersion(LegacyVersion version) noexcept {
    return version == LegacyVersion::V1_0 ||
           version == LegacyVersion::V1_5 ||
           version == LegacyVersion::V2_0;
}

std::string LegacyAdapter::getVersionString(LegacyVersion version) noexcept {
    switch (version) {
        case LegacyVersion::V1_0:
            return "1.0";
        case LegacyVersion::V1_5:
            return "1.5";
        case LegacyVersion::V2_0:
            return "2.0";
        case LegacyVersion::UNKNOWN:
            return "Unknown";
    }
    return "Unknown";
}

bool LegacyAdapter::convertFromV1(CadDocument& doc, const std::vector<uint8_t>& data) noexcept {
    (void)doc;
    return !data.empty();
}

bool LegacyAdapter::convertFromV1_5(CadDocument& doc, const std::vector<uint8_t>& data) noexcept {
    (void)doc;
    return !data.empty();
}

bool LegacyAdapter::convertFromV2(CadDocument& doc, const std::vector<uint8_t>& data) noexcept {
    (void)doc;
    return !data.empty();
}

} // namespace BendCore::CadCore
