#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace bendcore::hardening {

// ==============================================================================
// CRASH RECOVERY - GAP-089
// ==============================================================================

enum class CheckpointType {
    FULL,
    INCREMENTAL,
    SNAPSHOT
};

enum class RecoveryLevel {
    NONE,
    PARTIAL,
    FULL_RECOVERY
};

struct Checkpoint {
    uint64_t checkpoint_id;
    CheckpointType type;
    std::string data;
    std::string label;
    bool is_valid;
    uint64_t checksum;
    int64_t timestamp;
};

struct RecoveryMetadata {
    int recovery_attempts;
    int64_t last_crash_time;
    std::string last_checkpoint_id;
    bool recovery_successful;
};

class CheckpointManager {
public:
    CheckpointManager();
    uint64_t createCheckpoint(CheckpointType type, const std::string& data,
                              const std::string& label = "");
    Checkpoint* getCheckpoint(uint64_t id);
    Checkpoint* getLatestCheckpoint();
    int getCheckpointCount() const;
    bool validateCheckpoint(uint64_t id);
    bool deleteCheckpoint(uint64_t id);
private:
    std::vector<Checkpoint> checkpoints_;
    uint64_t next_id_;
};

class CrashRecoveryManager {
public:
    CrashRecoveryManager();
    bool isShuttingDown() const;
    void initializeGracefulShutdown();
    void requestShutdown();
    void waitForShutdown(int timeout_ms);
    int getRecoveryAttempts() const;
    CheckpointManager& getCheckpointManager();
    RecoveryLevel attemptRecovery();
    int64_t getLastCrashTime() const;
    RecoveryMetadata* getRecoveryMetadata();
private:
    bool shutting_down_;
    int recovery_attempts_;
    int64_t last_crash_time_;
    RecoveryMetadata metadata_;
    CheckpointManager checkpoint_manager_;
};

class SnapshotManager {
public:
    SnapshotManager();
    int getSnapshotCount() const;
    bool createSnapshot(const std::string& name);
    void clearOldSnapshots(int keep_count);
private:
    std::vector<std::string> snapshots_;
};

}  // namespace bendcore::hardening

namespace bendcore {
    namespace production = hardening;
}
