#include "bendcore/persistence/atomic_save.hpp"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace BendCore::CadCore {

AtomicSave::AtomicSave(const std::string& filePath) noexcept
    : filePath_(filePath), tempFilePath_(filePath + ".tmp") {}

AtomicSave::~AtomicSave() noexcept {
    if (inProgress_) {
        rollback();
    }
}

bool AtomicSave::begin() noexcept {
    if (inProgress_) return false;

    // Create backup of existing file if it exists
    if (fs::exists(filePath_)) {
        try {
            std::string backupPath = getBackupFilePath();
            fs::copy_file(filePath_, backupPath, fs::copy_options::overwrite_existing);
        } catch (...) {
            return false;
        }
    }

    inProgress_ = true;
    return true;
}

bool AtomicSave::write(const uint8_t* data, size_t size) noexcept {
    if (!inProgress_) return false;

    std::ofstream file(tempFilePath_, std::ios::binary | std::ios::app);
    if (!file.is_open()) return false;

    file.write(reinterpret_cast<const char*>(data), size);
    return file.good();
}

bool AtomicSave::commit() noexcept {
    if (!inProgress_) return false;

    try {
        // Atomic rename (on most filesystems)
        fs::rename(tempFilePath_, filePath_);
        inProgress_ = false;
        return true;
    } catch (...) {
        return false;
    }
}

void AtomicSave::rollback() noexcept {
    inProgress_ = false;
    try {
        if (fs::exists(tempFilePath_)) {
            fs::remove(tempFilePath_);
        }
    } catch (...) {
    }
}

std::string AtomicSave::getTempFilePath() const noexcept {
    return filePath_ + ".tmp";
}

std::string AtomicSave::getBackupFilePath() const noexcept {
    return filePath_ + ".backup";
}

} // namespace BendCore::CadCore
