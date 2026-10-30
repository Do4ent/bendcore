#include <gtest/gtest.h>
#include "bendcore/cloud/backup_manager.hpp"

using namespace bendcore::cloud;

class BackupTest : public ::testing::Test {
protected:
    BackupManager mgr;
};

// 31 tests for GAP-072
TEST_F(BackupTest, CreateBackup) { EXPECT_TRUE(mgr.CreateBackup("/data/f")); }
TEST_F(BackupTest, CreateEmpty) { EXPECT_FALSE(mgr.CreateBackup("")); }
TEST_F(BackupTest, CreateMultiple) { EXPECT_TRUE(mgr.CreateBackup("/data/f1")); EXPECT_TRUE(mgr.CreateBackup("/data/f2")); }
TEST_F(BackupTest, ListEmpty) { auto b = mgr.ListBackups(); EXPECT_EQ(b.size(), 0); }
TEST_F(BackupTest, ListAfterCreate) { mgr.CreateBackup("/data/f"); auto b = mgr.ListBackups(); EXPECT_GT(b.size(), 0); }
TEST_F(BackupTest, RestoreValid) { mgr.CreateBackup("/data/f"); auto b = mgr.ListBackups(); if (!b.empty()) EXPECT_TRUE(mgr.RestoreBackup(b[0], "/restore/f")); }
TEST_F(BackupTest, RestoreEmptyId) { EXPECT_FALSE(mgr.RestoreBackup("", "/restore")); }
TEST_F(BackupTest, DeleteValid) { mgr.CreateBackup("/data/f"); auto b = mgr.ListBackups(); if (!b.empty()) EXPECT_TRUE(mgr.DeleteBackup(b[0])); }
TEST_F(BackupTest, DeleteEmpty) { EXPECT_FALSE(mgr.DeleteBackup("")); }
TEST_F(BackupTest, ScheduleDaily) { EXPECT_TRUE(mgr.ScheduleBackup("/data/f", "daily", 30)); }
TEST_F(BackupTest, ScheduleWeekly) { EXPECT_TRUE(mgr.ScheduleBackup("/data/f", "weekly", 30)); }
TEST_F(BackupTest, ScheduleInvalid) { EXPECT_FALSE(mgr.ScheduleBackup("", "daily", 30)); }
TEST_F(BackupTest, GetSchedules) { mgr.ScheduleBackup("/d/f1", "daily", 30); mgr.ScheduleBackup("/d/f2", "weekly", 30); auto s = mgr.GetActiveSchedules(); EXPECT_GE(s.size(), 2); }
TEST_F(BackupTest, CancelSchedule) { mgr.ScheduleBackup("/d/f", "daily", 30); auto s = mgr.GetActiveSchedules(); if (!s.empty()) EXPECT_TRUE(mgr.CancelSchedule(s[0].schedule_id)); }
TEST_F(BackupTest, RestorePIT) { mgr.CreateBackup("/data/f"); EXPECT_TRUE(mgr.RestoreToPointInTime("/data/f", std::time(nullptr))); }
TEST_F(BackupTest, GetRestorePoints) { mgr.CreateBackup("/data/f"); mgr.CreateBackup("/data/f"); auto rp = mgr.GetAvailableRestorePoints("/data/f"); EXPECT_GT(rp.size(), 0); }
TEST_F(BackupTest, GetMetadata) { mgr.CreateBackup("/data/f"); auto b = mgr.ListBackups(); if (!b.empty()) { auto m = mgr.GetBackupMetadata(b[0]); EXPECT_GT(m.creation_time, 0); } }
TEST_F(BackupTest, VerifyBackup) { mgr.CreateBackup("/data/f"); auto b = mgr.ListBackups(); if (!b.empty()) EXPECT_TRUE(mgr.VerifyBackup(b[0])); }
TEST_F(BackupTest, CompressBackup) { mgr.CreateBackup("/data/f"); auto b = mgr.ListBackups(); if (!b.empty()) EXPECT_TRUE(mgr.CompressBackup(b[0])); }
TEST_F(BackupTest, GetSize) { mgr.CreateBackup("/data/f"); auto sz = mgr.GetTotalBackupSize(); EXPECT_GT(sz, 0); }
TEST_F(BackupTest, GetCount) { mgr.CreateBackup("/data/f1"); mgr.CreateBackup("/data/f2"); EXPECT_EQ(mgr.GetBackupCount(), 2); }
TEST_F(BackupTest, StressManyBackups) { for (int i=0; i<15; i++) EXPECT_TRUE(mgr.CreateBackup("/data/f"+std::to_string(i))); }
TEST_F(BackupTest, StressSchedules) { for (int i=0; i<10; i++) EXPECT_TRUE(mgr.ScheduleBackup("/d/f"+std::to_string(i), "daily", 30)); }
TEST_F(BackupTest, ScheduleHourly) { EXPECT_TRUE(mgr.ScheduleBackup("/data/f", "hourly", 30)); }
TEST_F(BackupTest, ScheduleMonthly) { EXPECT_TRUE(mgr.ScheduleBackup("/data/f", "monthly", 30)); }
TEST_F(BackupTest, CountEmpty) { EXPECT_EQ(mgr.GetBackupCount(), 0); }
TEST_F(BackupTest, DeleteMultiple) { mgr.CreateBackup("/d/f1"); mgr.CreateBackup("/d/f2"); auto b = mgr.ListBackups(); if (b.size() > 0) { int before = b.size(); mgr.DeleteBackup(b[0]); auto after = mgr.ListBackups(); EXPECT_LT(after.size(), before); } }
TEST_F(BackupTest, RestorePITInvalid) { EXPECT_FALSE(mgr.RestoreToPointInTime("/d/f", -1)); }
TEST_F(BackupTest, MetadataInvalid) { auto m = mgr.GetBackupMetadata("nonexistent"); EXPECT_EQ(m.creation_time, 0); }
TEST_F(BackupTest, VerifyInvalid) { EXPECT_FALSE(mgr.VerifyBackup("nonexistent")); }
TEST_F(BackupTest, CompressInvalid) { EXPECT_FALSE(mgr.CompressBackup("nonexistent")); }
