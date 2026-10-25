#pragma once

#include "../document/cad_document.hpp"
#include <string>
#include <memory>

namespace BendCore::CadCore {

/**
 * @brief Atomic save operation for crash safety
 */
class AtomicSave {
public:
    AtomicSave(const std::string& filePath) noexcept;
    ~AtomicSave() noexcept;

    /**
     * @brief Begin atomic save operation
     */
    bool begin() noexcept;

    /**
     * @brief Write data to temporary file
     */
    bool write(const uint8_t* data, size_t size) noexcept;

    /**
     * @brief Commit atomic save (rename temp to real)
     */
    bool commit() noexcept;

    /**
     * @brief Rollback atomic save (delete temp)
     */
    void rollback() noexcept;

    /**
     * @brief Check if save is in progress
     */
    bool isInProgress() const noexcept { return inProgress_; }

    /**
     * @brief Get original file path
     */
    const std::string& getFilePath() const noexcept { return filePath_; }

    /**
     * @brief Get temporary file path
     */
    std::string getTempFilePath() const noexcept;

    /**
     * @brief Get backup file path
     */
    std::string getBackupFilePath() const noexcept;

private:
    std::string filePath_;
    bool inProgress_ = false;
    std::string tempFilePath_;
};

} // namespace BendCore::CadCore
