#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

namespace bendcore::mobile {

// ==============================================================================
// OFFLINE MODE - GAP-082
// ==============================================================================
// Offline synchronization, local caching, conflict resolution, data integrity

enum class SyncStatus {
    SYNCED,
    PENDING,
    SYNCING,
    CONFLICT,
    FAILED
};

enum class ConflictResolutionStrategy {
    LAST_WRITE_WINS,
    LOCAL_WINS,
    REMOTE_WINS,
    MANUAL_MERGE
};

struct CachedData {
    std::string dataId;
    std::string content;
    int64_t timestamp;
    std::string hash;
    SyncStatus syncStatus;
    int32_t version;
};

struct SyncOperation {
    std::string operationId;
    std::string dataId;
    std::string operation;  // "create", "update", "delete"
    std::string payload;
    int64_t timestamp;
    SyncStatus status;
    int32_t retryCount;
};

struct DataConflict {
    std::string dataId;
    std::string localVersion;
    std::string remoteVersion;
    int64_t localTimestamp;
    int64_t remoteTimestamp;
    ConflictResolutionStrategy strategy;
};

class OfflineModeManager {
public:
    OfflineModeManager();
    ~OfflineModeManager();

    // Offline mode control
    bool enableOfflineMode(bool enable);
    bool isOfflineModeEnabled() const;
    bool isNetworkAvailable() const;
    void onNetworkStatusChanged(bool available);

    // Data caching
    bool cacheData(const std::string& dataId, const std::string& content);
    bool retrieveCachedData(const std::string& dataId, std::string& outContent);
    bool removeCachedData(const std::string& dataId);
    std::vector<std::string> getAllCachedDataIds();
    size_t getCacheSize() const;
    bool clearCache();

    // Sync queue management
    bool queueSyncOperation(const SyncOperation& operation);
    std::vector<SyncOperation> getPendingSyncOperations();
    bool processSyncQueue();
    SyncStatus getSyncStatus(const std::string& operationId);

    // Synchronization
    bool synchronize();
    bool syncData(const std::string& dataId);
    float getSyncProgress() const;
    int32_t getPendingSyncCount() const;

    // Conflict detection and resolution
    bool detectConflict(const std::string& dataId);
    std::vector<DataConflict> getConflicts();
    bool resolveConflict(const DataConflict& conflict);
    void setConflictResolutionStrategy(ConflictResolutionStrategy strategy);

    // Data integrity
    std::string calculateDataHash(const std::string& data);
    bool validateDataIntegrity(const CachedData& data);
    bool repairCorruptedData(const std::string& dataId);

    // Cache optimization
    bool compressCache();
    bool optimizeCacheSize(size_t maxSizeBytes);
    std::map<std::string, size_t> getCacheUsageByType();

    // Offline operations log
    bool logOfflineOperation(const SyncOperation& operation);
    std::vector<SyncOperation> getOperationHistory();
    bool clearOperationHistory();

    // Offline data versioning
    bool createDataVersion(const std::string& dataId, const std::string& content);
    std::vector<CachedData> getDataVersionHistory(const std::string& dataId);
    bool restoreDataVersion(const std::string& dataId, int32_t version);

    // Batch operations
    bool executeBatchOfflineOperations(const std::vector<SyncOperation>& operations);

private:
    std::map<std::string, CachedData> cache;
    std::vector<SyncOperation> syncQueue;
    std::vector<DataConflict> conflicts;
    bool offlineModeEnabled;
    bool networkAvailable;
    ConflictResolutionStrategy conflictStrategy;
    float syncProgress;
};

}  // namespace bendcore::mobile
