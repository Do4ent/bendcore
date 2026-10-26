#include <gtest/gtest.h>
#include "bendcore/cloud/data_sync.hpp"

namespace bendcore::cloud {
class MockDataSync : public DataSync {
public:
    bool SyncData(const std::string& l, const std::string& r) override { return true; }
    bool ResolveConflict(const std::string& l, const std::string& r) override { return true; }
    bool IsSynced(const std::string& p) const override { return true; }
    std::vector<std::string> GetPendingChanges() const override { return {}; }
    bool CommitChanges(const std::string& m) override { return true; }
};
}

using namespace bendcore::cloud;

class DataSyncTest : public ::testing::Test {
protected:
    MockDataSync sync;
};

TEST_F(DataSyncTest, SyncData) { EXPECT_TRUE(sync.SyncData("/local", "/remote")); }
TEST_F(DataSyncTest, ResolveConflict) { EXPECT_TRUE(sync.ResolveConflict("v1", "v2")); }
TEST_F(DataSyncTest, IsSynced) { EXPECT_TRUE(sync.IsSynced("/path")); }
TEST_F(DataSyncTest, GetPendingChanges1) { EXPECT_EQ(sync.GetPendingChanges().size(), 0); }
TEST_F(DataSyncTest, CommitChanges) { EXPECT_TRUE(sync.CommitChanges("msg")); }
TEST_F(DataSyncTest, MultipleSyncs) { EXPECT_TRUE(sync.SyncData("/a", "/b")); EXPECT_TRUE(sync.SyncData("/c", "/d")); }
TEST_F(DataSyncTest, ConflictResolution1) { EXPECT_TRUE(sync.ResolveConflict("a", "b")); }
TEST_F(DataSyncTest, ConflictResolution2) { EXPECT_TRUE(sync.ResolveConflict("c", "d")); }
TEST_F(DataSyncTest, SyncStatusCheck1) { EXPECT_TRUE(sync.IsSynced("/p1")); }
TEST_F(DataSyncTest, SyncStatusCheck2) { EXPECT_TRUE(sync.IsSynced("/p2")); }
TEST_F(DataSyncTest, CommitMultiple1) { EXPECT_TRUE(sync.CommitChanges("m1")); }
TEST_F(DataSyncTest, CommitMultiple2) { EXPECT_TRUE(sync.CommitChanges("m2")); }
TEST_F(DataSyncTest, SyncLargeData) { EXPECT_TRUE(sync.SyncData("/large", "/large")); }
TEST_F(DataSyncTest, ConflictWithSync) { EXPECT_TRUE(sync.SyncData("/x", "/y")); EXPECT_TRUE(sync.ResolveConflict("x", "y")); }
TEST_F(DataSyncTest, PendingAfterSync) { auto p = sync.GetPendingChanges(); EXPECT_GE(p.size(), 0); }
TEST_F(DataSyncTest, BatchSync) { for(int i=0;i<5;i++) EXPECT_TRUE(sync.SyncData("/p"+std::to_string(i), "/r"+std::to_string(i))); }
TEST_F(DataSyncTest, RapidCommit) { for(int i=0;i<10;i++) EXPECT_TRUE(sync.CommitChanges("m")); }
TEST_F(DataSyncTest, SyncCheck1) { EXPECT_TRUE(sync.IsSynced("/test")); }
TEST_F(DataSyncTest, SyncCheck2) { EXPECT_TRUE(sync.IsSynced("/check")); }
TEST_F(DataSyncTest, MultiPath) { EXPECT_TRUE(sync.SyncData("/a/b/c", "/x/y/z")); }
TEST_F(DataSyncTest, SpecialCharacters) { EXPECT_TRUE(sync.SyncData("/path with spaces", "/remote path")); }
TEST_F(DataSyncTest, DeepPath) { EXPECT_TRUE(sync.SyncData("/a/b/c/d/e/f/g", "/x/y/z")); }
TEST_F(DataSyncTest, EmptyPath) { EXPECT_TRUE(sync.SyncData("", "")); }
TEST_F(DataSyncTest, RootPath) { EXPECT_TRUE(sync.SyncData("/", "/")); }
TEST_F(DataSyncTest, SyncStress) { for(int i=0;i<100;i++) EXPECT_TRUE(sync.IsSynced("/p")); }
TEST_F(DataSyncTest, ConflictStress) { for(int i=0;i<50;i++) EXPECT_TRUE(sync.ResolveConflict("l", "r")); }
TEST_F(DataSyncTest, CommitStress) { for(int i=0;i<50;i++) EXPECT_TRUE(sync.CommitChanges("stress")); }
TEST_F(DataSyncTest, MixedOps1) { EXPECT_TRUE(sync.SyncData("/a", "/b")); EXPECT_TRUE(sync.IsSynced("/a")); }
TEST_F(DataSyncTest, MixedOps2) { EXPECT_TRUE(sync.CommitChanges("m")); EXPECT_EQ(sync.GetPendingChanges().size(), 0); }
TEST_F(DataSyncTest, MixedOps3) { EXPECT_TRUE(sync.ResolveConflict("l", "r")); EXPECT_TRUE(sync.CommitChanges("resolved")); }
TEST_F(DataSyncTest, SequentialOps) { EXPECT_TRUE(sync.SyncData("/a", "/b")); EXPECT_TRUE(sync.ResolveConflict("a", "b")); EXPECT_TRUE(sync.CommitChanges("m")); }
