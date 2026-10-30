#include "bendcore/hardening/crash_recovery.hpp"
#include <chrono>
#include <algorithm>
#include <numeric>

namespace bendcore::hardening {

// CheckpointManager
CheckpointManager::CheckpointManager() : next_id_(1) {}

uint64_t CheckpointManager::createCheckpoint(CheckpointType type,
                                              const std::string& data,
                                              const std::string& label) {
    uint64_t id = next_id_++;
    Checkpoint cp;
    cp.checkpoint_id = id;
    cp.type = type;
    cp.data = data;
    cp.label = label;
    cp.is_valid = true;
    // Simple checksum: sum of char values
    uint64_t csum = 0;
    for (unsigned char c : data) csum += c;
    csum += id;
    cp.checksum = csum ? csum : 1;
    cp.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    checkpoints_.push_back(cp);
    return id;
}

Checkpoint* CheckpointManager::getCheckpoint(uint64_t id) {
    for (auto& cp : checkpoints_) {
        if (cp.checkpoint_id == id) return &cp;
    }
    return nullptr;
}

Checkpoint* CheckpointManager::getLatestCheckpoint() {
    if (checkpoints_.empty()) return nullptr;
    return &checkpoints_.back();
}

int CheckpointManager::getCheckpointCount() const {
    return static_cast<int>(checkpoints_.size());
}

bool CheckpointManager::validateCheckpoint(uint64_t id) {
    auto* cp = getCheckpoint(id);
    if (!cp) return false;
    return cp->is_valid;
}

bool CheckpointManager::deleteCheckpoint(uint64_t id) {
    for (auto it = checkpoints_.begin(); it != checkpoints_.end(); ++it) {
        if (it->checkpoint_id == id) {
            checkpoints_.erase(it);
            return true;
        }
    }
    return false;
}

// CrashRecoveryManager
CrashRecoveryManager::CrashRecoveryManager()
    : shutting_down_(false), recovery_attempts_(0), last_crash_time_(0) {
    metadata_ = {0, 0, "", false};
}

bool CrashRecoveryManager::isShuttingDown() const { return shutting_down_; }

void CrashRecoveryManager::initializeGracefulShutdown() {
    shutting_down_ = true;
}

void CrashRecoveryManager::requestShutdown() {
    shutting_down_ = true;
}

void CrashRecoveryManager::waitForShutdown(int timeout_ms) {
    // Non-blocking stub
    (void)timeout_ms;
}

int CrashRecoveryManager::getRecoveryAttempts() const { return recovery_attempts_; }

CheckpointManager& CrashRecoveryManager::getCheckpointManager() {
    return checkpoint_manager_;
}

RecoveryLevel CrashRecoveryManager::attemptRecovery() {
    recovery_attempts_++;
    last_crash_time_ = std::chrono::system_clock::now().time_since_epoch().count();
    metadata_.recovery_attempts = recovery_attempts_;
    metadata_.last_crash_time = last_crash_time_;
    metadata_.recovery_successful = true;

    if (checkpoint_manager_.getCheckpointCount() > 0) {
        return RecoveryLevel::FULL_RECOVERY;
    }
    return RecoveryLevel::PARTIAL;
}

int64_t CrashRecoveryManager::getLastCrashTime() const { return last_crash_time_; }

RecoveryMetadata* CrashRecoveryManager::getRecoveryMetadata() { return &metadata_; }

// SnapshotManager
SnapshotManager::SnapshotManager() {}

int SnapshotManager::getSnapshotCount() const {
    return static_cast<int>(snapshots_.size());
}

bool SnapshotManager::createSnapshot(const std::string& name) {
    snapshots_.push_back(name);
    return true;
}

void SnapshotManager::clearOldSnapshots(int keep_count) {
    while (static_cast<int>(snapshots_.size()) > keep_count) {
        snapshots_.erase(snapshots_.begin());
    }
}

}  // namespace bendcore::hardening
