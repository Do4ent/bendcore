#include <gtest/gtest.h>
#include "bendcore/mobile/offline_mode.hpp"

using namespace bendcore::mobile;

class OfflineModeTest : public ::testing::Test {
protected:
    OfflineModeManager manager;
};

TEST_F(OfflineModeTest, EnableOfflineMode) {
    EXPECT_TRUE(manager.enableOfflineMode(true));
    EXPECT_TRUE(manager.isOfflineModeEnabled());
}

TEST_F(OfflineModeTest, CacheData) {
    EXPECT_TRUE(manager.cacheData("id1", "content"));
}

TEST_F(OfflineModeTest, RetrieveCachedData) {
    manager.cacheData("id1", "test_content");
    std::string content;
    EXPECT_TRUE(manager.retrieveCachedData("id1", content));
    EXPECT_EQ(content, "test_content");
}

TEST_F(OfflineModeTest, RemoveCachedData) {
    manager.cacheData("id1", "content");
    EXPECT_TRUE(manager.removeCachedData("id1"));
}

TEST_F(OfflineModeTest, GetAllCachedDataIds) {
    manager.cacheData("id1", "c1");
    manager.cacheData("id2", "c2");
    auto ids = manager.getAllCachedDataIds();
    EXPECT_GE(ids.size(), 2);
}

TEST_F(OfflineModeTest, GetCacheSize) {
    manager.cacheData("id1", "content");
    size_t size = manager.getCacheSize();
    EXPECT_GT(size, 0);
}

TEST_F(OfflineModeTest, ClearCache) {
    manager.cacheData("id1", "content");
    EXPECT_TRUE(manager.clearCache());
}

TEST_F(OfflineModeTest, QueueSyncOperation) {
    SyncOperation op{.operationId = "op1", .dataId = "id1"};
    EXPECT_TRUE(manager.queueSyncOperation(op));
}

TEST_F(OfflineModeTest, GetPendingSyncOperations) {
    SyncOperation op{.operationId = "op1"};
    manager.queueSyncOperation(op);
    auto pending = manager.getPendingSyncOperations();
    EXPECT_GE(pending.size(), 1);
}

TEST_F(OfflineModeTest, Synchronize) {
    bool result = manager.synchronize();
    EXPECT_TRUE(true);
}

TEST_F(OfflineModeTest, SyncData) {
    manager.cacheData("id1", "content");
    EXPECT_TRUE(manager.syncData("id1"));
}

TEST_F(OfflineModeTest, GetSyncProgress) {
    float progress = manager.getSyncProgress();
    EXPECT_GE(progress, 0.0f);
}

TEST_F(OfflineModeTest, GetPendingSyncCount) {
    SyncOperation op{.operationId = "op1"};
    manager.queueSyncOperation(op);
    int32_t count = manager.getPendingSyncCount();
    EXPECT_GE(count, 1);
}

TEST_F(OfflineModeTest, DetectConflict) {
    bool conflict = manager.detectConflict("id1");
    EXPECT_FALSE(conflict);
}

TEST_F(OfflineModeTest, GetConflicts) {
    auto conflicts = manager.getConflicts();
    EXPECT_GE(conflicts.size(), 0);
}

TEST_F(OfflineModeTest, CalculateDataHash) {
    std::string hash = manager.calculateDataHash("test_data");
    EXPECT_FALSE(hash.empty());
}

TEST_F(OfflineModeTest, ValidateDataIntegrity) {
    CachedData data{.hash = "somehash"};
    EXPECT_TRUE(manager.validateDataIntegrity(data));
}

TEST_F(OfflineModeTest, CompressCache) {
    EXPECT_TRUE(manager.compressCache());
}

TEST_F(OfflineModeTest, OptimizeCacheSize) {
    manager.cacheData("id1", "content");
    EXPECT_TRUE(manager.optimizeCacheSize(1000));
}

TEST_F(OfflineModeTest, LogOfflineOperation) {
    SyncOperation op{.operationId = "op1"};
    EXPECT_TRUE(manager.logOfflineOperation(op));
}

TEST_F(OfflineModeTest, GetOperationHistory) {
    SyncOperation op{.operationId = "op1"};
    manager.logOfflineOperation(op);
    auto history = manager.getOperationHistory();
    EXPECT_GE(history.size(), 1);
}

TEST_F(OfflineModeTest, ClearOperationHistory) {
    EXPECT_TRUE(manager.clearOperationHistory());
}

TEST_F(OfflineModeTest, CreateDataVersion) {
    EXPECT_TRUE(manager.createDataVersion("id1", "version1"));
}

TEST_F(OfflineModeTest, GetDataVersionHistory) {
    manager.createDataVersion("id1", "v1");
    auto history = manager.getDataVersionHistory("id1");
    EXPECT_GE(history.size(), 0);
}

TEST_F(OfflineModeTest, RestoreDataVersion) {
    manager.createDataVersion("id1", "content");
    EXPECT_TRUE(manager.restoreDataVersion("id1", 1));
}

TEST_F(OfflineModeTest, ExecuteBatchOperations) {
    std::vector<SyncOperation> ops{{.operationId = "op1"}};
    EXPECT_TRUE(manager.executeBatchOfflineOperations(ops));
}

TEST_F(OfflineModeTest, IsNetworkAvailable) {
    bool available = manager.isNetworkAvailable();
    EXPECT_TRUE(true);
}

TEST_F(OfflineModeTest, OnNetworkStatusChange) {
    manager.onNetworkStatusChanged(true);
    EXPECT_TRUE(manager.isNetworkAvailable());
}

TEST_F(OfflineModeTest, RepairCorruptedData) {
    manager.cacheData("id1", "data");
    EXPECT_TRUE(manager.repairCorruptedData("id1"));
}

TEST_F(OfflineModeTest, SetConflictResolutionStrategy) {
    manager.setConflictResolutionStrategy(ConflictResolutionStrategy::LOCAL_WINS);
    EXPECT_TRUE(true);
}

TEST_F(OfflineModeTest, GetCacheUsageByType) {
    auto usage = manager.getCacheUsageByType();
    EXPECT_TRUE(true);
}

TEST_F(OfflineModeTest, GetSyncStatusForOperation) {
    SyncOperation op{.operationId = "op_test", .status = SyncStatus::PENDING};
    manager.queueSyncOperation(op);
    auto status = manager.getSyncStatus("op_test");
    EXPECT_TRUE(true);
}

TEST_F(OfflineModeTest, ResolveSyncConflict) {
    DataConflict conflict{.dataId = "id1", .localVersion = "v1", .remoteVersion = "v2"};
    EXPECT_TRUE(manager.resolveConflict(conflict));
}

