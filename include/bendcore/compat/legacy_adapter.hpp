#pragma once

#include "../document/cad_document.hpp"
#include <string>
#include <vector>

namespace BendCore::CadCore {

/**
 * @brief Legacy format version
 */
enum class LegacyVersion {
    V1_0,
    V1_5,
    V2_0,
    UNKNOWN
};

/**
 * @brief Adapter for legacy CAD file formats
 */
class LegacyAdapter {
public:
    LegacyAdapter() noexcept = default;

    /**
     * @brief Detect legacy format version
     */
    static LegacyVersion detectVersion(const std::vector<uint8_t>& data) noexcept;

    /**
     * @brief Convert legacy format to current
     */
    bool convertFromLegacy(CadDocument& doc,
                          const std::vector<uint8_t>& legacyData,
                          LegacyVersion version) noexcept;

    /**
     * @brief Convert current format to legacy
     */
    std::vector<uint8_t> convertToLegacy(const CadDocument& doc,
                                         LegacyVersion version) const noexcept;

    /**
     * @brief Check if format is supported
     */
    static bool isSupportedVersion(LegacyVersion version) noexcept;

    /**
     * @brief Get version string
     */
    static std::string getVersionString(LegacyVersion version) noexcept;

private:
    bool convertFromV1(CadDocument& doc, const std::vector<uint8_t>& data) noexcept;
    bool convertFromV1_5(CadDocument& doc, const std::vector<uint8_t>& data) noexcept;
    bool convertFromV2(CadDocument& doc, const std::vector<uint8_t>& data) noexcept;
};

} // namespace BendCore::CadCore
