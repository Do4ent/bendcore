#pragma once
#include <string>
#include <functional>
#include <vector>
#include <map>
#include <ctime>
#include <memory>
#include <mutex>
#include <queue>

namespace bendcore::cloud {

enum class JobStatus {
    PENDING, RUNNING, COMPLETED, FAILED, CANCELLED
};

struct ComputationJob {
    std::string job_id;
    std::string code;
    JobStatus status;
    std::string result;
    time_t submission_time;
    time_t completion_time;
    std::string error_message;
    int retry_count;
    int priority;
    std::map<std::string, std::string> parameters;

    ComputationJob()
        : status(JobStatus::PENDING), submission_time(0), completion_time(0), retry_count(0), priority(5) {}
};

struct ResourceAllocation {
    std::string allocation_id;
    int cpu_cores;
    int memory_gb;
    std::string gpu_type;
    int gpu_count;

    ResourceAllocation() : cpu_cores(1), memory_gb(1), gpu_count(0) {}
};

class CloudComputing {
public:
    virtual bool SubmitJob(const std::string& code) = 0;
    virtual bool WaitForResult(const std::string& job_id) = 0;
    virtual std::string GetResult(const std::string& job_id) = 0;
    virtual ~CloudComputing() = default;
};

class CloudComputingScheduler : public CloudComputing {
public:
    CloudComputingScheduler();
    ~CloudComputingScheduler() override;

    bool SubmitJob(const std::string& code) override;
    bool WaitForResult(const std::string& job_id) override;
    std::string GetResult(const std::string& job_id) override;

    bool SubmitJobWithPriority(const std::string& code, int priority);
    bool SubmitJobWithResources(const std::string& code, const ResourceAllocation& resources);
    bool CancelJob(const std::string& job_id);
    ComputationJob GetJobStatus(const std::string& job_id) const;

    std::vector<ComputationJob> GetQueuedJobs() const;
    std::vector<ComputationJob> GetRunningJobs() const;
    int GetQueueLength() const;

    bool AllocateResources(const std::string& job_id, const ResourceAllocation& resources);
    ResourceAllocation GetAllocatedResources(const std::string& job_id) const;
    bool ReleaseResources(const std::string& job_id);

    bool RetryJob(const std::string& job_id);
    std::vector<ComputationJob> GetFailedJobs() const;

    std::vector<std::string> SubmitBatchJobs(const std::vector<std::string>& codes);
    std::map<std::string, std::string> WaitForBatchResults(const std::vector<std::string>& job_ids);

    int GetTotalJobsProcessed() const;
    double GetAverageJobDuration() const;
    double GetJobSuccessRate() const;

private:
    mutable std::mutex scheduler_mutex_;
    std::map<std::string, ComputationJob> jobs_;
    std::queue<ComputationJob> job_queue_;
    std::map<std::string, ResourceAllocation> resource_allocations_;
    int total_jobs_processed_;

    std::string GenerateJobId() const;
    bool ProcessJob(const ComputationJob& job);
    void UpdateJobStatus(const std::string& job_id, JobStatus status);
};

}
