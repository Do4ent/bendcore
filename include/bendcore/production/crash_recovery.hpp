#ifndef BENDCORE_PRODUCTION_CRASH_RECOVERY_HPP
#define BENDCORE_PRODUCTION_CRASH_RECOVERY_HPP

#include <string>
#include <memory>
#include <map>
#include <optional>
#include <chrono>
#include <functional>

namespace bendcore::production {

/**
 * Checkpoint state information
 */
struct CheckpointState {
  std::string id;
  std::string name;
  std::chrono::system_clock::time_point timestamp;
  std::map<std::string, std::string> data;
  size_t size_bytes;
  std::string hash;
  bool compressed;
  int version;
};

/**
 * Recovery checkpoint manager
 */
class CheckpointManager {
 public:
  CheckpointManager(const std::string& checkpoint_dir);
  ~CheckpointManager();

  // Create checkpoint
  std::string create_checkpoint(const std::string& name);

  // Store data in checkpoint
  bool store_checkpoint_data(const std::string& checkpoint_id,
                            const std::string& key,
                            const std::string& value);

  // Retrieve checkpoint
  std::optional<CheckpointState> get_checkpoint(const std::string& checkpoint_id);

  // List all checkpoints
  std::vector<CheckpointState> list_checkpoints() const;

  // Delete checkpoint
  bool delete_checkpoint(const std::string& checkpoint_id);

  // Cleanup old checkpoints
  int cleanup_old_checkpoints(int max_age_hours);

 private:
  std::string checkpoint_dir_;
  mutable std::shared_mutex mutex_;
  std::map<std::string, CheckpointState> checkpoints_;
};

/**
 * Recovery manager for graceful shutdown and restart
 */
class RecoveryManager {
 public:
  using ShutdownHook = std::function<void()>;
  using RecoveryHook = std::function<void(const CheckpointState&)>;

  RecoveryManager();
  ~RecoveryManager();

  // Register shutdown hook
  void register_shutdown_hook(ShutdownHook hook);

  // Register recovery hook
  void register_recovery_hook(RecoveryHook hook);

  // Perform graceful shutdown
  void graceful_shutdown();

  // Attempt recovery from checkpoint
  bool recover_from_checkpoint(const std::string& checkpoint_id);

  // Check if recovery is needed
  bool needs_recovery() const;

  // Get last recovery state
  std::optional<CheckpointState> get_last_recovery_state() const;

 private:
  std::vector<ShutdownHook> shutdown_hooks_;
  std::vector<RecoveryHook> recovery_hooks_;
  std::optional<CheckpointState> last_recovery_state_;
  bool recovery_in_progress_ = false;
  mutable std::shared_mutex mutex_;
};

/**
 * Process state snapshotter
 */
class StateSnapshotter {
 public:
  StateSnapshotter();
  ~StateSnapshotter() = default;

  // Snapshot current process state
  std::string snapshot_state();

  // Restore from snapshot
  bool restore_from_snapshot(const std::string& snapshot_data);

  // Get memory state
  std::map<std::string, size_t> get_memory_state() const;

 private:
  std::map<std::string, std::string> last_snapshot_;
};

} // namespace bendcore::production

#endif // BENDCORE_PRODUCTION_CRASH_RECOVERY_HPP
