#include "bendcore/production/crash_recovery.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace bendcore::production {

CheckpointManager::CheckpointManager(const std::string& checkpoint_dir)
    : checkpoint_dir_(checkpoint_dir) {
  std::filesystem::create_directories(checkpoint_dir);
}

CheckpointManager::~CheckpointManager() {}

std::string CheckpointManager::create_checkpoint(const std::string& name) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << name << "_" << timestamp;
  std::string checkpoint_id = ss.str();

  CheckpointState state{
    .id = checkpoint_id,
    .name = name,
    .timestamp = now,
    .size_bytes = 0,
    .compressed = false,
    .version = 1
  };

  checkpoints_[checkpoint_id] = state;
  return checkpoint_id;
}

bool CheckpointManager::store_checkpoint_data(const std::string& checkpoint_id,
                                             const std::string& key,
                                             const std::string& value) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  auto it = checkpoints_.find(checkpoint_id);
  if (it == checkpoints_.end()) {
    return false;
  }
  it->second.data[key] = value;
  it->second.size_bytes += value.size();
  return true;
}

std::optional<CheckpointState> CheckpointManager::get_checkpoint(const std::string& checkpoint_id) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  auto it = checkpoints_.find(checkpoint_id);
  if (it != checkpoints_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<CheckpointState> CheckpointManager::list_checkpoints() const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  std::vector<CheckpointState> result;
  for (const auto& pair : checkpoints_) {
    result.push_back(pair.second);
  }
  return result;
}

bool CheckpointManager::delete_checkpoint(const std::string& checkpoint_id) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  return checkpoints_.erase(checkpoint_id) > 0;
}

int CheckpointManager::cleanup_old_checkpoints(int max_age_hours) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  auto now = std::chrono::system_clock::now();
  auto cutoff = now - std::chrono::hours(max_age_hours);
  int deleted = 0;

  for (auto it = checkpoints_.begin(); it != checkpoints_.end();) {
    if (it->second.timestamp < cutoff) {
      it = checkpoints_.erase(it);
      deleted++;
    } else {
      ++it;
    }
  }
  return deleted;
}

RecoveryManager::RecoveryManager() {}

void RecoveryManager::register_shutdown_hook(ShutdownHook hook) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  shutdown_hooks_.push_back(hook);
}

void RecoveryManager::register_recovery_hook(RecoveryHook hook) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  recovery_hooks_.push_back(hook);
}

void RecoveryManager::graceful_shutdown() {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  for (auto& hook : shutdown_hooks_) {
    try {
      hook();
    } catch (...) {
      // Log but continue with other hooks
    }
  }
}

bool RecoveryManager::recover_from_checkpoint(const std::string& checkpoint_id) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  recovery_in_progress_ = true;

  // Simulate recovery - in real implementation would load checkpoint
  CheckpointState state{
    .id = checkpoint_id,
    .name = "recovery",
    .timestamp = std::chrono::system_clock::now()
  };

  for (auto& hook : recovery_hooks_) {
    try {
      hook(state);
    } catch (...) {
      recovery_in_progress_ = false;
      return false;
    }
  }

  last_recovery_state_ = state;
  recovery_in_progress_ = false;
  return true;
}

bool RecoveryManager::needs_recovery() const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  return recovery_in_progress_;
}

std::optional<CheckpointState> RecoveryManager::get_last_recovery_state() const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  return last_recovery_state_;
}

StateSnapshotter::StateSnapshotter() {}

std::string StateSnapshotter::snapshot_state() {
  std::stringstream ss;
  ss << "snapshot_" << std::chrono::system_clock::now().time_since_epoch().count();
  return ss.str();
}

bool StateSnapshotter::restore_from_snapshot(const std::string& snapshot_data) {
  last_snapshot_["data"] = snapshot_data;
  return true;
}

std::map<std::string, size_t> StateSnapshotter::get_memory_state() const {
  return std::map<std::string, size_t>();
}

} // namespace bendcore::production
