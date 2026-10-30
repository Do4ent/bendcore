#pragma once
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <any>
#include <ctime>
#include <memory>
#include <queue>
#include <mutex>
#include <set>

namespace bendcore::cloud {

// Conflict resolution strategy types
enum class ConflictStrategy {
    LOCAL_WINS,      // Keep local version
    REMOTE_WINS,     // Keep remote version
    MERGE,           // Attempt automatic merge
    MANUAL           // Require manual resolution
};

// Sync change type
struct SyncChange {
    std::string path;
    std::string type;  // "create", "modify", "delete"
    time_t timestamp;
    std::string hash;

    SyncChange() : timestamp(0) {}
    SyncChange(const std::string& p, const std::string& t, time_t ts, const std::string& h)
        : path(p), type(t), timestamp(ts), hash(h) {}
};

// Conflict information
struct SyncConflict {
    std::string path;
    std::string local_version;
    std::string remote_version;
    time_t local_timestamp;
    time_t remote_timestamp;
    ConflictStrategy resolution_strategy;

    SyncConflict(const std::string& p, const std::string& lv, const std::string& rv,
                 time_t lt, time_t rt, ConflictStrategy cs)
        : path(p), local_version(lv), remote_version(rv),
          local_timestamp(lt), remote_timestamp(rt), resolution_strategy(cs) {}
};

/**
 * Abstract base class for data synchronization
 */
class DataSync {
public:
    virtual ~DataSync() = default;
    virtual bool SyncData(const std::string& local_path, const std::string& remote_path) = 0;
    virtual bool ResolveConflict(const std::string& local_version, const std::string& remote_version) = 0;
    virtual bool IsSynced(const std::string& path) const = 0;
    virtual std::vector<std::string> GetPendingChanges() const = 0;
    virtual bool CommitChanges(const std::string& message) = 0;
};

/**
 * Bi-directional data synchronization implementation with conflict resolution
 */
class BiDirectionalSync : public DataSync {
public:
    BiDirectionalSync();
    ~BiDirectionalSync() override;

    bool SyncData(const std::string& local_path, const std::string& remote_path) override;
    bool ResolveConflict(const std::string& local_version, const std::string& remote_version) override;
    bool IsSynced(const std::string& path) const override;
    std::vector<std::string> GetPendingChanges() const override;
    bool CommitChanges(const std::string& message) override;

    // Additional methods for advanced sync operations
    bool SyncDataWithStrategy(const std::string& local_path, const std::string& remote_path,
                             ConflictStrategy strategy);
    std::vector<SyncConflict> GetConflicts() const;
    bool ResolveConflictByPath(const std::string& path, ConflictStrategy strategy);
    std::vector<SyncChange> GetChangeHistory(const std::string& path) const;
    bool RollbackChanges(const std::string& path, time_t timestamp);
    bool EnableContinuousSync(bool enabled);
    bool IsContinuousSyncEnabled() const;

private:
    mutable std::mutex sync_mutex_;
    std::map<std::string, SyncChange> synced_items_;
    std::vector<SyncChange> pending_changes_;
    std::vector<SyncConflict> conflicts_;
    std::map<std::string, std::vector<SyncChange>> change_history_;
    bool continuous_sync_enabled_;

    bool DetectConflict(const std::string& local_version, const std::string& remote_version);
    bool ApplyConflictResolution(const SyncConflict& conflict);
    std::string ComputeHash(const std::string& data) const;
};

} // namespace bendcore::cloud
