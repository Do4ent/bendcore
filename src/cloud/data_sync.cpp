#include "bendcore/cloud/data_sync.hpp"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace bendcore::cloud {

BiDirectionalSync::BiDirectionalSync()
    : continuous_sync_enabled_(false) {}

BiDirectionalSync::~BiDirectionalSync() = default;

bool BiDirectionalSync::SyncData(const std::string& local_path, const std::string& remote_path) {
    std::lock_guard<std::mutex> lock(sync_mutex_);

    if (local_path.empty() || remote_path.empty()) {
        return false;
    }

    auto now = std::time(nullptr);
    std::string hash = ComputeHash(local_path + remote_path);
    SyncChange change(local_path, "modify", now, hash);

    auto it = synced_items_.find(local_path);
    if (it != synced_items_.end()) {
        synced_items_[local_path] = change;
    } else {
        synced_items_[local_path] = change;
    }

    change_history_[local_path].push_back(change);

    auto pending_it = std::find_if(pending_changes_.begin(), pending_changes_.end(),
                                    [&local_path](const SyncChange& c) { return c.path == local_path; });
    if (pending_it != pending_changes_.end()) {
        pending_changes_.erase(pending_it);
    }

    return true;
}

bool BiDirectionalSync::ResolveConflict(const std::string& local_version, const std::string& remote_version) {
    std::lock_guard<std::mutex> lock(sync_mutex_);

    if (local_version.empty() && remote_version.empty()) {
        return false;
    }

    if (local_version == remote_version) {
        return true;
    }

    auto now = std::time(nullptr);
    SyncConflict conflict("automatic", local_version, remote_version, now, now, ConflictStrategy::MERGE);

    return ApplyConflictResolution(conflict);
}

bool BiDirectionalSync::IsSynced(const std::string& path) const {
    std::lock_guard<std::mutex> lock(sync_mutex_);
    return synced_items_.find(path) != synced_items_.end();
}

std::vector<std::string> BiDirectionalSync::GetPendingChanges() const {
    std::lock_guard<std::mutex> lock(sync_mutex_);
    std::vector<std::string> result;
    for (const auto& change : pending_changes_) {
        result.push_back(change.path);
    }
    return result;
}

bool BiDirectionalSync::CommitChanges(const std::string& message) {
    std::lock_guard<std::mutex> lock(sync_mutex_);

    if (message.empty()) {
        return false;
    }

    for (const auto& change : pending_changes_) {
        synced_items_[change.path] = change;
    }

    pending_changes_.clear();

    return true;
}

bool BiDirectionalSync::SyncDataWithStrategy(const std::string& local_path, const std::string& remote_path,
                                            ConflictStrategy strategy) {
    std::lock_guard<std::mutex> lock(sync_mutex_);

    if (local_path.empty() || remote_path.empty()) {
        return false;
    }

    auto now = std::time(nullptr);
    std::string hash = ComputeHash(local_path + remote_path);
    SyncChange change(local_path, "modify", now, hash);

    synced_items_[local_path] = change;
    change_history_[local_path].push_back(change);

    return true;
}

std::vector<SyncConflict> BiDirectionalSync::GetConflicts() const {
    std::lock_guard<std::mutex> lock(sync_mutex_);
    return conflicts_;
}

bool BiDirectionalSync::ResolveConflictByPath(const std::string& path, ConflictStrategy strategy) {
    std::lock_guard<std::mutex> lock(sync_mutex_);

    auto it = std::find_if(conflicts_.begin(), conflicts_.end(),
                           [&path](const SyncConflict& c) { return c.path == path; });

    if (it != conflicts_.end()) {
        it->resolution_strategy = strategy;
        ApplyConflictResolution(*it);
        conflicts_.erase(it);
        return true;
    }

    return false;
}

std::vector<SyncChange> BiDirectionalSync::GetChangeHistory(const std::string& path) const {
    std::lock_guard<std::mutex> lock(sync_mutex_);
    auto it = change_history_.find(path);
    if (it != change_history_.end()) {
        return it->second;
    }
    return {};
}

bool BiDirectionalSync::RollbackChanges(const std::string& path, time_t timestamp) {
    std::lock_guard<std::mutex> lock(sync_mutex_);

    if (path.empty()) {
        return false;
    }

    auto it = change_history_.find(path);
    if (it == change_history_.end()) {
        return false;
    }

    auto& history = it->second;
    for (auto rit = history.rbegin(); rit != history.rend(); ++rit) {
        if (rit->timestamp <= timestamp) {
            synced_items_[path] = *rit;
            return true;
        }
    }

    return false;
}

bool BiDirectionalSync::EnableContinuousSync(bool enabled) {
    std::lock_guard<std::mutex> lock(sync_mutex_);
    continuous_sync_enabled_ = enabled;
    return true;
}

bool BiDirectionalSync::IsContinuousSyncEnabled() const {
    std::lock_guard<std::mutex> lock(sync_mutex_);
    return continuous_sync_enabled_;
}

bool BiDirectionalSync::DetectConflict(const std::string& local_version, const std::string& remote_version) {
    return local_version != remote_version;
}

bool BiDirectionalSync::ApplyConflictResolution(const SyncConflict& conflict) {
    switch (conflict.resolution_strategy) {
        case ConflictStrategy::LOCAL_WINS:
        case ConflictStrategy::REMOTE_WINS:
        case ConflictStrategy::MERGE:
            return true;
        case ConflictStrategy::MANUAL:
            conflicts_.push_back(conflict);
            return false;
    }
    return false;
}

std::string BiDirectionalSync::ComputeHash(const std::string& data) const {
    std::hash<std::string> hasher;
    auto hash_value = hasher(data);
    std::stringstream ss;
    ss << std::hex << hash_value;
    return ss.str();
}

} // namespace bendcore::cloud
