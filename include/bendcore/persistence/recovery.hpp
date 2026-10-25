#pragma once

#include "../document/cad_document.hpp"
#include <string>
#include <vector>
#include <memory>

namespace BendCore::CadCore {

/**
 * @brief Recovery point metadata
 */
struct RecoveryPoint {
    uint64_t timestamp = 0;
    std::string filePath;
    uint64_t fileSize = 0;
    uint64_t docRevision = 0;
    bool isValid = false;
};

/**
 * @brief Recovery system for crash recovery
 */
class RecoverySystem {
public:
    RecoverySystem(const std::string& recoveryDir) noexcept;

    /**
     * @brief Create recovery checkpoint
     */
    bool createCheckpoint(const CadDocument& doc) noexcept;

    /**
     * @brief Get available recovery points
     */
    std::vector<RecoveryPoint> getRecoveryPoints() const noexcept;

    /**
     * @brief Recover from checkpoint
     */
    bool recoverFrom(CadDocument& doc, const RecoveryPoint& point) noexcept;

    /**
     * @brief Delete recovery point
     */
    bool deleteRecoveryPoint(const RecoveryPoint& point) noexcept;

    /**
     * @brief Clean old recovery points
     */
    bool cleanOldRecoveryPoints(uint64_t maxAgeSeconds = 86400) noexcept;

    /**
     * @brief Get recovery directory
     */
    const std::string& getRecoveryDir() const noexcept { return recoveryDir_; }

    /**
     * @brief Check if recovery is needed
     */
    bool needsRecovery() const noexcept;

private:
    std::string recoveryDir_;
    std::vector<RecoveryPoint> checkpoints_;
};

} // namespace BendCore::CadCore
