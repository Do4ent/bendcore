#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <map>
#include "../include/bendcore/cloud/data_sync.hpp"

class DataSyncTest : public ::testing::Test {
protected:
    std::unique_ptr<bendcore::cloud::DataSync> data_sync;

    void SetUp() override {
        data_sync = std::make_unique<bendcore::cloud::DataSync>();
    }
};

TEST_F(DataSyncTest, InitializeSync) {
    EXPECT_NE(data_sync.get(), nullptr);
}

TEST_F(DataSyncTest, SyncDataUpstream) {
    std::string local_data = "test_data_v1";
    auto sync_result = data_sync->syncUpstream(local_data);
    EXPECT_TRUE(sync_result);
}

TEST_F(DataSyncTest, SyncDataDownstream) {
    std::string remote_data = "remote_v1";
    auto sync_result = data_sync->syncDownstream(remote_data);
    EXPECT_TRUE(sync_result);
}

TEST_F(DataSyncTest, TrackChanges) {
    data_sync->trackChange("field1", "old_value", "new_value");
    auto changes = data_sync->getChanges();
    EXPECT_FALSE(changes.empty());
}

TEST_F(DataSyncTest, ConflictDetection) {
    std::vector<bendcore::cloud::SyncChange> local_changes;
    std::vector<bendcore::cloud::SyncChange> remote_changes;

    local_changes.push_back({"field1", "v1", "v2"});
    remote_changes.push_back({"field1", "v1", "v3"});

    auto conflicts = data_sync->detectConflicts(local_changes, remote_changes);
    EXPECT_FALSE(conflicts.empty());
}

TEST_F(DataSyncTest, ResolvConflict) {
    bendcore::cloud::SyncConflict conflict{"field1", "v2", "v3"};
    auto resolved = data_sync->resolveConflict(conflict, bendcore::cloud::ResolutionStrategy::LOCAL_WINS);
    EXPECT_TRUE(resolved);
}

TEST_F(DataSyncTest, BiDirectionalSync) {
    std::string local_state = "state1";
    std::string remote_state = "state1";

    auto local_sync = data_sync->syncUpstream(local_state);
    auto remote_sync = data_sync->syncDownstream(remote_state);

    EXPECT_TRUE(local_sync && remote_sync);
}

TEST_F(DataSyncTest, SyncProgress) {
    data_sync->startSync();
    auto progress = data_sync->getProgress();
    EXPECT_GE(progress, 0.0f);
    EXPECT_LE(progress, 1.0f);
}

TEST_F(DataSyncTest, SyncCompletion) {
    data_sync->startSync();
    data_sync->completeSync();
    auto progress = data_sync->getProgress();
    EXPECT_EQ(progress, 1.0f);
}

TEST_F(DataSyncTest, SyncRetry) {
    bool retry_success = data_sync->retrySync(3);
    EXPECT_TRUE(retry_success);
}

TEST_F(DataSyncTest, SyncTimeout) {
    data_sync->setSyncTimeout(5000);
    EXPECT_EQ(data_sync->getSyncTimeout(), 5000);
}

TEST_F(DataSyncTest, SyncNotification) {
    bool notified = false;
    data_sync->onSyncComplete([&notified]() { notified = true; });
    data_sync->completeSync();
    EXPECT_TRUE(notified);
}

TEST_F(DataSyncTest, BatchSync) {
    std::vector<std::string> batch_data{"data1", "data2", "data3"};
    auto batch_result = data_sync->batchSync(batch_data);
    EXPECT_TRUE(batch_result);
}

TEST_F(DataSyncTest, SelectiveSyncFields) {
    std::vector<std::string> fields{"field1", "field2", "field3"};
    auto selective_sync = data_sync->selectiveSync(fields);
    EXPECT_TRUE(selective_sync);
}

TEST_F(DataSyncTest, SyncScheduling) {
    data_sync->scheduleSync(std::chrono::seconds(60));
    EXPECT_TRUE(data_sync->isSyncScheduled());
}

TEST_F(DataSyncTest, SyncCancellation) {
    data_sync->startSync();
    data_sync->cancelSync();
    auto active = data_sync->isSyncActive();
    EXPECT_FALSE(active);
}

TEST_F(DataSyncTest, SyncStatistics) {
    data_sync->syncUpstream("test");
    auto stats = data_sync->getStatistics();
    EXPECT_GT(stats.total_syncs, 0);
}

TEST_F(DataSyncTest, DeltaSyncOptimization) {
    auto uses_delta = data_sync->useDeltaSync();
    EXPECT_TRUE(uses_delta);
}

TEST_F(DataSyncTest, CompressionSupport) {
    bool compression_enabled = data_sync->enableCompression(true);
    EXPECT_TRUE(compression_enabled);
}

TEST_F(DataSyncTest, EncryptedSync) {
    bool encryption_enabled = data_sync->enableEncryption(true);
    EXPECT_TRUE(encryption_enabled);
}

TEST_F(DataSyncTest, SyncMultipleRegions) {
    std::vector<std::string> regions{"us-east", "eu-west", "ap-south"};
    auto region_sync = data_sync->syncToRegions(regions);
    EXPECT_TRUE(region_sync);
}

TEST_F(DataSyncTest, RollingSync) {
    for (int i = 0; i < 5; i++) {
        auto sync = data_sync->syncUpstream("batch_" + std::to_string(i));
        EXPECT_TRUE(sync);
    }
}

TEST_F(DataSyncTest, SyncErrorRecovery) {
    data_sync->startSync();
    data_sync->simulateError();
    auto recovered = data_sync->recoverFromError();
    EXPECT_TRUE(recovered);
}

TEST_F(DataSyncTest, SyncMetrics) {
    data_sync->syncUpstream("test");
    auto metrics = data_sync->getMetrics();
    EXPECT_GT(metrics.bytes_synced, 0);
}

TEST_F(DataSyncTest, SyncVersion) {
    auto version = data_sync->getSyncVersion();
    EXPECT_EQ(version, "1.0");
}

TEST_F(DataSyncTest, AsyncSync) {
    auto future_sync = data_sync->asyncSync("test_data");
    EXPECT_TRUE(future_sync.valid());
}

TEST_F(DataSyncTest, SyncQueueManagement) {
    data_sync->queueSync("item1");
    data_sync->queueSync("item2");
    auto queue_size = data_sync->getQueueSize();
    EXPECT_EQ(queue_size, 2);
}

TEST_F(DataSyncTest, SyncPriority) {
    data_sync->queueSyncWithPriority("critical", 10);
    auto first = data_sync->getNextSyncItem();
    EXPECT_EQ(first, "critical");
}

TEST_F(DataSyncTest, SyncDeduplication) {
    data_sync->queueSync("item1");
    data_sync->queueSync("item1");
    auto queue_size = data_sync->getQueueSize();
    EXPECT_EQ(queue_size, 1);
}

TEST_F(DataSyncTest, SyncRollback) {
    data_sync->syncUpstream("version1");
    data_sync->syncUpstream("version2");
    auto rollback_success = data_sync->rollbackToVersion("version1");
    EXPECT_TRUE(rollback_success);
}

TEST_F(DataSyncTest, SyncAuditLog) {
    data_sync->syncUpstream("test");
    auto log = data_sync->getAuditLog();
    EXPECT_FALSE(log.empty());
}

TEST_F(DataSyncTest, OfflineQueueing) {
    data_sync->enableOfflineMode(true);
    auto queued = data_sync->queueOfflineSync("offline_data");
    EXPECT_TRUE(queued);
}

TEST_F(DataSyncTest, SyncResumeAfterDisconnect) {
    data_sync->simulateDisconnect();
    auto resumed = data_sync->resumeSync();
    EXPECT_TRUE(resumed);
}

TEST_F(DataSyncTest, SyncMemoryEfficiency) {
    size_t initial_memory = data_sync->getMemoryUsage();
    data_sync->syncUpstream("large_dataset");
    size_t final_memory = data_sync->getMemoryUsage();
    EXPECT_LT(final_memory, initial_memory * 2);
}
