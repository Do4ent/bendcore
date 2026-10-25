#include "bendcore/persistence/recovery.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace fs = std::filesystem;

namespace BendCore::CadCore {

RecoverySystem::RecoverySystem(const std::string& recoveryDir) noexcept
    : recoveryDir_(recoveryDir) {
    if (!fs::exists(recoveryDir_)) {
        fs::create_directories(recoveryDir_);
    }
}

bool RecoverySystem::createCheckpoint(const CadDocument& doc) noexcept {
    RecoveryPoint point;
    point.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    point.filePath = recoveryDir_ + "/" + std::to_string(point.timestamp) + ".recovery";
    point.docRevision = doc.getRevision();
    point.isValid = true;

    std::ofstream file(point.filePath, std::ios::binary);
    if (!file.is_open()) return false;

    // Write header
    uint64_t magic = 0xCADC0DE5;
    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    file.write(reinterpret_cast<const char*>(&point.docRevision), sizeof(point.docRevision));

    if (!file.good()) return false;

    checkpoints_.push_back(point);
    return true;
}

std::vector<RecoveryPoint> RecoverySystem::getRecoveryPoints() const noexcept {
    std::vector<RecoveryPoint> points;

    if (!fs::exists(recoveryDir_)) return points;

    for (const auto& entry : fs::directory_iterator(recoveryDir_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".recovery") {
            RecoveryPoint point;
            point.filePath = entry.path().string();
            point.fileSize = fs::file_size(entry.path());
            point.isValid = true;
            points.push_back(point);
        }
    }

    return points;
}

bool RecoverySystem::recoverFrom(CadDocument& doc, const RecoveryPoint& point) noexcept {
    (void)doc;
    std::ifstream file(point.filePath, std::ios::binary);
    if (!file.is_open()) return false;

    // Read header
    uint64_t magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));

    if (magic != 0xCADC0DE5) return false;

    uint64_t revision;
    file.read(reinterpret_cast<char*>(&revision), sizeof(revision));

    return file.good();
}

bool RecoverySystem::deleteRecoveryPoint(const RecoveryPoint& point) noexcept {
    try {
        if (fs::exists(point.filePath)) {
            fs::remove(point.filePath);
            return true;
        }
    } catch (...) {
        return false;
    }
    return false;
}

bool RecoverySystem::cleanOldRecoveryPoints(uint64_t maxAgeSeconds) noexcept {
    try {
        for (const auto& entry : fs::directory_iterator(recoveryDir_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".recovery") {
                auto lastWrite = fs::last_write_time(entry.path());
                auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::file_clock::now() - lastWrite
                ).count();

                if (static_cast<uint64_t>(duration) > maxAgeSeconds * 1000000000ULL) {
                    fs::remove(entry.path());
                }
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool RecoverySystem::needsRecovery() const noexcept {
    if (!fs::exists(recoveryDir_)) return false;

    for (const auto& entry : fs::directory_iterator(recoveryDir_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".recovery") {
            return true;
        }
    }

    return false;
}

} // namespace BendCore::CadCore
