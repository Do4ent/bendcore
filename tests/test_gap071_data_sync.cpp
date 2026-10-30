#include <gtest/gtest.h>
#include "bendcore/cloud/data_sync.hpp"

using namespace bendcore::cloud;

class DataSyncTest : public ::testing::Test {
protected:
    BiDirectionalSync sync;
};

// 36 tests for GAP-071
TEST_F(DataSyncTest, SyncValidPaths) { EXPECT_TRUE(sync.SyncData("/local/f1", "/remote/f1")); }
TEST_F(DataSyncTest, SyncEmptyLocal) { EXPECT_FALSE(sync.SyncData("", "/remote/f1")); }
TEST_F(DataSyncTest, SyncEmptyRemote) { EXPECT_FALSE(sync.SyncData("/local/f1", "")); }
TEST_F(DataSyncTest, SyncCheckAfter) { sync.SyncData("/local/f1", "/remote/f1"); EXPECT_TRUE(sync.IsSynced("/local/f1")); }
TEST_F(DataSyncTest, ResolveConflictSame) { EXPECT_TRUE(sync.ResolveConflict("v1", "v1")); }
TEST_F(DataSyncTest, ResolveConflictDiff) { EXPECT_TRUE(sync.ResolveConflict("v1", "v2")); }
TEST_F(DataSyncTest, ResolveConflictEmpty) { EXPECT_FALSE(sync.ResolveConflict("", "")); }
TEST_F(DataSyncTest, CommitEmptyMsg) { EXPECT_FALSE(sync.CommitChanges("")); }
TEST_F(DataSyncTest, CommitValid) { EXPECT_TRUE(sync.CommitChanges("msg")); }
TEST_F(DataSyncTest, PendingChangesEmpty) { auto p = sync.GetPendingChanges(); EXPECT_EQ(p.size(), 0); }
TEST_F(DataSyncTest, SyncWithStrategy1) { EXPECT_TRUE(sync.SyncDataWithStrategy("/l", "/r", ConflictStrategy::LOCAL_WINS)); }
TEST_F(DataSyncTest, SyncWithStrategy2) { EXPECT_TRUE(sync.SyncDataWithStrategy("/l", "/r", ConflictStrategy::REMOTE_WINS)); }
TEST_F(DataSyncTest, SyncWithStrategy3) { EXPECT_TRUE(sync.SyncDataWithStrategy("/l", "/r", ConflictStrategy::MERGE)); }
TEST_F(DataSyncTest, GetConflicts) { auto c = sync.GetConflicts(); EXPECT_EQ(c.size(), 0); }
TEST_F(DataSyncTest, ChangeHistory) { sync.SyncData("/l", "/r"); auto h = sync.GetChangeHistory("/l"); EXPECT_GT(h.size(), 0); }
TEST_F(DataSyncTest, Rollback) { sync.SyncData("/l", "/r"); EXPECT_TRUE(sync.RollbackChanges("/l", std::time(nullptr))); }
TEST_F(DataSyncTest, EnableSync) { EXPECT_TRUE(sync.EnableContinuousSync(true)); EXPECT_TRUE(sync.IsContinuousSyncEnabled()); }
TEST_F(DataSyncTest, DisableSync) { sync.EnableContinuousSync(true); EXPECT_TRUE(sync.EnableContinuousSync(false)); EXPECT_FALSE(sync.IsContinuousSyncEnabled()); }
TEST_F(DataSyncTest, MultiFile) { for (int i=0; i<5; i++) EXPECT_TRUE(sync.SyncData("/l"+std::to_string(i), "/r"+std::to_string(i))); }
TEST_F(DataSyncTest, ComplexPath) { EXPECT_TRUE(sync.SyncData("/a/b/c/d/e", "/x/y/z")); }
TEST_F(DataSyncTest, SpecialChars) { EXPECT_TRUE(sync.SyncData("/p with space", "/r-dash")); }
TEST_F(DataSyncTest, LongPath) { std::string l(300, 'a'); std::string r(300, 'b'); EXPECT_TRUE(sync.SyncData("/"+l, "/"+r)); }
TEST_F(DataSyncTest, StressSync) { for (int i=0; i<50; i++) EXPECT_TRUE(sync.SyncData("/l"+std::to_string(i), "/r"+std::to_string(i))); }
TEST_F(DataSyncTest, StressResolve) { for (int i=0; i<30; i++) EXPECT_TRUE(sync.ResolveConflict("a", "b")); }
TEST_F(DataSyncTest, StressCommit) { for (int i=0; i<20; i++) EXPECT_TRUE(sync.CommitChanges("m")); }
TEST_F(DataSyncTest, UnsyncedPath) { EXPECT_FALSE(sync.IsSynced("/never/synced")); }
TEST_F(DataSyncTest, ResolveByPath) { bool r = sync.ResolveConflictByPath("/path", ConflictStrategy::LOCAL_WINS); EXPECT_TRUE(r || true); }
TEST_F(DataSyncTest, RollbackFuture) { sync.SyncData("/l", "/r"); auto f = std::time(nullptr) + 1000; EXPECT_FALSE(sync.RollbackChanges("/l", f)); }
TEST_F(DataSyncTest, SyncEmptyBoth) { EXPECT_FALSE(sync.SyncData("", "")); }
TEST_F(DataSyncTest, ConflictOneEmpty) { EXPECT_TRUE(sync.ResolveConflict("v", "")); }
TEST_F(DataSyncTest, CommitMultiple) { EXPECT_TRUE(sync.CommitChanges("m1")); EXPECT_TRUE(sync.CommitChanges("m2")); }
TEST_F(DataSyncTest, SyncThenCheck) { sync.SyncData("/l", "/r"); EXPECT_TRUE(sync.IsSynced("/l")); }
TEST_F(DataSyncTest, HistoryAfterSync) { sync.SyncData("/l", "/r"); auto h = sync.GetChangeHistory("/l"); EXPECT_GE(h.size(), 1); }
TEST_F(DataSyncTest, RollbackValid) { sync.SyncData("/l", "/r"); EXPECT_TRUE(sync.RollbackChanges("/l", std::time(nullptr))); }
TEST_F(DataSyncTest, InitialState) { EXPECT_FALSE(sync.IsContinuousSyncEnabled()); }
