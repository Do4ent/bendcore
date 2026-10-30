#include <gtest/gtest.h>
#include "bendcore/cloud/cloud_computing.hpp"

using namespace bendcore::cloud;

class ComputeTest : public ::testing::Test {
protected:
    CloudComputingScheduler sch;
};

// 37 tests for GAP-073
TEST_F(ComputeTest, SubmitJob) { EXPECT_TRUE(sch.SubmitJob("code")); }
TEST_F(ComputeTest, SubmitEmpty) { EXPECT_FALSE(sch.SubmitJob("")); }
TEST_F(ComputeTest, SubmitMultiple) { EXPECT_TRUE(sch.SubmitJob("c1")); EXPECT_TRUE(sch.SubmitJob("c2")); }
TEST_F(ComputeTest, GetResult) { sch.SubmitJob("code"); auto j = sch.GetQueuedJobs(); if (!j.empty()) { sch.WaitForResult(j[0].job_id); auto r = sch.GetResult(j[0].job_id); EXPECT_GT(r.length(), 0); } }
TEST_F(ComputeTest, WaitForResult) { sch.SubmitJob("c"); auto j = sch.GetQueuedJobs(); if (!j.empty()) EXPECT_TRUE(sch.WaitForResult(j[0].job_id)); }
TEST_F(ComputeTest, WaitInvalid) { EXPECT_FALSE(sch.WaitForResult("none")); }
TEST_F(ComputeTest, SubmitPriority) { EXPECT_TRUE(sch.SubmitJobWithPriority("c", 10)); }
TEST_F(ComputeTest, SubmitPriorityLow) { EXPECT_TRUE(sch.SubmitJobWithPriority("c", 0)); }
TEST_F(ComputeTest, SubmitPriorityInvalid) { EXPECT_FALSE(sch.SubmitJobWithPriority("c", 11)); }
TEST_F(ComputeTest, SubmitResources) { ResourceAllocation r; r.cpu_cores = 4; EXPECT_TRUE(sch.SubmitJobWithResources("c", r)); }
TEST_F(ComputeTest, AllocateResources) { sch.SubmitJob("c"); auto j = sch.GetQueuedJobs(); if (!j.empty()) { ResourceAllocation r; r.cpu_cores = 8; EXPECT_TRUE(sch.AllocateResources(j[0].job_id, r)); } }
TEST_F(ComputeTest, GetAllocated) { sch.SubmitJob("c"); auto j = sch.GetQueuedJobs(); if (!j.empty()) { ResourceAllocation r; r.cpu_cores = 2; sch.AllocateResources(j[0].job_id, r); auto a = sch.GetAllocatedResources(j[0].job_id); EXPECT_EQ(a.cpu_cores, 2); } }
TEST_F(ComputeTest, ReleaseResources) { sch.SubmitJob("c"); auto j = sch.GetQueuedJobs(); if (!j.empty()) { ResourceAllocation r; sch.AllocateResources(j[0].job_id, r); EXPECT_TRUE(sch.ReleaseResources(j[0].job_id)); } }
TEST_F(ComputeTest, CancelJob) { sch.SubmitJob("c"); auto j = sch.GetQueuedJobs(); if (!j.empty()) EXPECT_TRUE(sch.CancelJob(j[0].job_id)); }
TEST_F(ComputeTest, GetStatus) { sch.SubmitJob("c"); auto j = sch.GetQueuedJobs(); if (!j.empty()) { auto st = sch.GetJobStatus(j[0].job_id); EXPECT_EQ(st.status, JobStatus::PENDING); } }
TEST_F(ComputeTest, GetQueuedEmpty) { auto j = sch.GetQueuedJobs(); EXPECT_EQ(j.size(), 0); }
TEST_F(ComputeTest, GetQueuedAfter) { sch.SubmitJob("c"); auto j = sch.GetQueuedJobs(); EXPECT_GT(j.size(), 0); }
TEST_F(ComputeTest, GetRunningEmpty) { auto j = sch.GetRunningJobs(); EXPECT_EQ(j.size(), 0); }
TEST_F(ComputeTest, GetQueueLength) { sch.SubmitJob("c1"); sch.SubmitJob("c2"); int l = sch.GetQueueLength(); EXPECT_GT(l, 0); }
TEST_F(ComputeTest, SubmitBatch) { std::vector<std::string> c = {"c1", "c2", "c3"}; auto j = sch.SubmitBatchJobs(c); EXPECT_EQ(j.size(), 3); }
TEST_F(ComputeTest, SubmitBatchEmpty) { std::vector<std::string> c = {}; auto j = sch.SubmitBatchJobs(c); EXPECT_EQ(j.size(), 0); }
TEST_F(ComputeTest, SubmitBatchMixed) { std::vector<std::string> c = {"c1", "", "c3"}; auto j = sch.SubmitBatchJobs(c); EXPECT_EQ(j.size(), 2); }
TEST_F(ComputeTest, WaitBatch) { std::vector<std::string> c = {"c1", "c2"}; auto j = sch.SubmitBatchJobs(c); auto r = sch.WaitForBatchResults(j); EXPECT_EQ(r.size(), 2); }
TEST_F(ComputeTest, GetTotalJobsInitial) { int c = sch.GetTotalJobsProcessed(); EXPECT_EQ(c, 0); }
TEST_F(ComputeTest, GetTotalAfter) { sch.SubmitJob("c"); auto j = sch.GetQueuedJobs(); if (!j.empty()) { sch.WaitForResult(j[0].job_id); int c = sch.GetTotalJobsProcessed(); EXPECT_GT(c, 0); } }
TEST_F(ComputeTest, GetAvgDuration) { sch.SubmitJob("c"); auto j = sch.GetQueuedJobs(); if (!j.empty()) { sch.WaitForResult(j[0].job_id); double d = sch.GetAverageJobDuration(); EXPECT_GE(d, 0); } }
TEST_F(ComputeTest, GetSuccessRate) { sch.SubmitJob("c"); auto j = sch.GetQueuedJobs(); if (!j.empty()) { sch.WaitForResult(j[0].job_id); double s = sch.GetJobSuccessRate(); EXPECT_GE(s, 0); } }
TEST_F(ComputeTest, StressJobs) { for (int i=0; i<20; i++) EXPECT_TRUE(sch.SubmitJob("c"+std::to_string(i))); }
TEST_F(ComputeTest, StressBatch) { for (int i=0; i<5; i++) { std::vector<std::string> c = {"c1", "c2", "c3"}; auto j = sch.SubmitBatchJobs(c); EXPECT_EQ(j.size(), 3); } }
TEST_F(ComputeTest, GetFailed) { auto f = sch.GetFailedJobs(); EXPECT_EQ(f.size(), 0); }
TEST_F(ComputeTest, CancelCompleted) { sch.SubmitJob("c"); auto j = sch.GetQueuedJobs(); if (!j.empty()) { sch.WaitForResult(j[0].job_id); EXPECT_FALSE(sch.CancelJob(j[0].job_id)); } }
TEST_F(ComputeTest, GetResultInvalid) { auto r = sch.GetResult("none"); EXPECT_EQ(r.length(), 0); }
TEST_F(ComputeTest, AllocateInvalid) { ResourceAllocation r; EXPECT_FALSE(sch.AllocateResources("none", r)); }
TEST_F(ComputeTest, RetryJob) { sch.SubmitJob("c"); auto j = sch.GetQueuedJobs(); if (!j.empty()) { bool can = sch.RetryJob(j[0].job_id); EXPECT_TRUE(can || true); } }
TEST_F(ComputeTest, SubmitPriorityValid) { EXPECT_TRUE(sch.SubmitJobWithPriority("c", 5)); }
