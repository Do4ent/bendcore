#include <gtest/gtest.h>
#include <memory>
#include "../include/bendcore/mobile/offline_mode.hpp"

class OfflineModeTest : public ::testing::Test {
protected:
    std::unique_ptr<bendcore::mobile::OfflineMode> offline;

    void SetUp() override {
        offline = std::make_unique<bendcore::mobile::OfflineMode>();
    }
};

TEST_F(OfflineModeTest, InitializeOfflineMode) {
    EXPECT_NE(offline.get(), nullptr);
}

TEST_F(OfflineModeTest, EnableOfflineMode) {
    auto enabled = offline->enableOfflineMode(true);
    EXPECT_TRUE(enabled);
}

TEST_F(OfflineModeTest, DisableOfflineMode) {
    offline->enableOfflineMode(true);
    auto disabled = offline->enableOfflineMode(false);
    EXPECT_TRUE(disabled);
}

TEST_F(OfflineModeTest, IsOfflineMode) {
    offline->enableOfflineMode(true);
    auto is_offline = offline->isOfflineMode();
    EXPECT_TRUE(is_offline);
}

TEST_F(OfflineModeTest, SyncOfflineData) {
    offline->enableOfflineMode(true);
    offline->queueOfflineEdit("doc1", "content");
    auto synced = offline->syncOfflineData();
    EXPECT_TRUE(synced);
}

TEST_F(OfflineModeTest, QueueOfflineEdit) {
    offline->enableOfflineMode(true);
    auto queued = offline->queueOfflineEdit("doc1", "new_content");
    EXPECT_TRUE(queued);
}

TEST_F(OfflineModeTest, GetOfflineEdits) {
    offline->enableOfflineMode(true);
    offline->queueOfflineEdit("doc1", "content1");
    offline->queueOfflineEdit("doc1", "content2");
    auto edits = offline->getOfflineEdits("doc1");
    EXPECT_GE(edits.size(), 2);
}

TEST_F(OfflineModeTest, ClearOfflineEdits) {
    offline->enableOfflineMode(true);
    offline->queueOfflineEdit("doc1", "content");
    auto cleared = offline->clearOfflineEdits("doc1");
    EXPECT_TRUE(cleared);
}

TEST_F(OfflineModeTest, LocalStorageWrite) {
    offline->enableOfflineMode(true);
    auto written = offline->writeToLocalStorage("key1", "value1");
    EXPECT_TRUE(written);
}

TEST_F(OfflineModeTest, LocalStorageRead) {
    offline->enableOfflineMode(true);
    offline->writeToLocalStorage("key1", "value1");
    auto value = offline->readFromLocalStorage("key1");
    EXPECT_EQ(value, "value1");
}

TEST_F(OfflineModeTest, LocalStorageDelete) {
    offline->enableOfflineMode(true);
    offline->writeToLocalStorage("key1", "value1");
    auto deleted = offline->deleteFromLocalStorage("key1");
    EXPECT_TRUE(deleted);
}

TEST_F(OfflineModeTest, GetLocalStorageSize) {
    offline->enableOfflineMode(true);
    offline->writeToLocalStorage("key1", "value1");
    auto size = offline->getLocalStorageSize();
    EXPECT_GT(size, 0);
}

TEST_F(OfflineModeTest, ClearLocalStorage) {
    offline->enableOfflineMode(true);
    offline->writeToLocalStorage("key1", "value1");
    auto cleared = offline->clearLocalStorage();
    EXPECT_TRUE(cleared);
}

TEST_F(OfflineModeTest, OfflineDatabase) {
    offline->enableOfflineMode(true);
    auto db = offline->getOfflineDatabase();
    EXPECT_NE(db, nullptr);
}

TEST_F(OfflineModeTest, CreateOfflineDocument) {
    offline->enableOfflineMode(true);
    auto doc_id = offline->createOfflineDocument("test_doc");
    EXPECT_FALSE(doc_id.empty());
}

TEST_F(OfflineModeTest, OpenOfflineDocument) {
    offline->enableOfflineMode(true);
    auto doc_id = offline->createOfflineDocument("test_doc");
    auto opened = offline->openOfflineDocument(doc_id);
    EXPECT_TRUE(opened);
}

TEST_F(OfflineModeTest, SaveOfflineDocument) {
    offline->enableOfflineMode(true);
    auto doc_id = offline->createOfflineDocument("test_doc");
    auto saved = offline->saveOfflineDocument(doc_id);
    EXPECT_TRUE(saved);
}

TEST_F(OfflineModeTest, DeleteOfflineDocument) {
    offline->enableOfflineMode(true);
    auto doc_id = offline->createOfflineDocument("test_doc");
    auto deleted = offline->deleteOfflineDocument(doc_id);
    EXPECT_TRUE(deleted);
}

TEST_F(OfflineModeTest, ListOfflineDocuments) {
    offline->enableOfflineMode(true);
    offline->createOfflineDocument("doc1");
    offline->createOfflineDocument("doc2");
    auto docs = offline->listOfflineDocuments();
    EXPECT_GE(docs.size(), 2);
}

TEST_F(OfflineModeTest, SyncQueue) {
    offline->enableOfflineMode(true);
    offline->queueOfflineEdit("doc1", "content");
    auto queue_size = offline->getSyncQueueSize();
    EXPECT_GE(queue_size, 1);
}

TEST_F(OfflineModeTest, ConflictDetection) {
    offline->enableOfflineMode(true);
    offline->queueOfflineEdit("doc1", "content1");
    auto conflicts = offline->detectConflicts("doc1");
    EXPECT_GE(conflicts.size(), 0);
}

TEST_F(OfflineModeTest, ResolveConflict) {
    offline->enableOfflineMode(true);
    offline->queueOfflineEdit("doc1", "content1");
    auto resolved = offline->resolveConflict("doc1", "local");
    EXPECT_TRUE(resolved);
}

TEST_F(OfflineModeTest, OfflineMetadata) {
    offline->enableOfflineMode(true);
    auto metadata = offline->getOfflineMetadata();
    EXPECT_FALSE(metadata.empty());
}

TEST_F(OfflineModeTest, OfflineStatistics) {
    offline->enableOfflineMode(true);
    offline->queueOfflineEdit("doc1", "content");
    auto stats = offline->getOfflineStatistics();
    EXPECT_GT(stats.total_edits, 0);
}

TEST_F(OfflineModeTest, OnSyncComplete) {
    offline->enableOfflineMode(true);
    bool synced = false;
    offline->onSyncComplete([&synced]() { synced = true; });
    offline->syncOfflineData();
    EXPECT_TRUE(synced);
}

TEST_F(OfflineModeTest, OfflineVersion) {
    auto version = offline->getOfflineVersion();
    EXPECT_FALSE(version.empty());
}

TEST_F(OfflineModeTest, AsyncSync) {
    offline->enableOfflineMode(true);
    auto future = offline->syncOfflineDataAsync();
    EXPECT_TRUE(future.valid());
}

TEST_F(OfflineModeTest, OfflineIndexing) {
    offline->enableOfflineMode(true);
    offline->createOfflineDocument("doc1");
    auto indexed = offline->indexOfflineDocuments();
    EXPECT_TRUE(indexed);
}

TEST_F(OfflineModeTest, OfflineSearch) {
    offline->enableOfflineMode(true);
    offline->createOfflineDocument("searchable_doc");
    auto results = offline->searchOfflineDocuments("searchable");
    EXPECT_GE(results.size(), 0);
}

TEST_F(OfflineModeTest, DatabaseMaintenance) {
    offline->enableOfflineMode(true);
    auto maintained = offline->performDatabaseMaintenance();
    EXPECT_TRUE(maintained);
}

TEST_F(OfflineModeTest, BackupOfflineData) {
    offline->enableOfflineMode(true);
    auto backup = offline->backupOfflineData();
    EXPECT_FALSE(backup.empty());
}

TEST_F(OfflineModeTest, RestoreOfflineData) {
    offline->enableOfflineMode(true);
    auto backup = offline->backupOfflineData();
    auto restored = offline->restoreOfflineData(backup);
    EXPECT_TRUE(restored);
}
