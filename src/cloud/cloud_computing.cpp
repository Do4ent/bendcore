#include "bendcore/cloud/cloud_computing.hpp"
#include <algorithm>
#include <chrono>
#include <sstream>

namespace bendcore::cloud {

CloudComputingScheduler::CloudComputingScheduler()
    : total_jobs_processed_(0) {}

CloudComputingScheduler::~CloudComputingScheduler() = default;

bool CloudComputingScheduler::SubmitJob(const std::string& code) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    if (code.empty()) return false;

    ComputationJob job;
    job.job_id = GenerateJobId();
    job.code = code;
    job.status = JobStatus::PENDING;
    job.submission_time = std::time(nullptr);
    job.priority = 5;

    jobs_[job.job_id] = job;
    job_queue_.push(job);
    return true;
}

bool CloudComputingScheduler::WaitForResult(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    auto it = jobs_.find(job_id);
    if (it == jobs_.end()) return false;

    it->second.status = JobStatus::RUNNING;
    it->second.status = JobStatus::COMPLETED;
    it->second.completion_time = std::time(nullptr);
    it->second.result = "computation_result_" + job_id;
    total_jobs_processed_++;
    return true;
}

std::string CloudComputingScheduler::GetResult(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    auto it = jobs_.find(job_id);
    if (it != jobs_.end() && it->second.status == JobStatus::COMPLETED) {
        return it->second.result;
    }
    return "";
}

bool CloudComputingScheduler::SubmitJobWithPriority(const std::string& code, int priority) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    if (code.empty() || priority < 0 || priority > 10) return false;

    ComputationJob job;
    job.job_id = GenerateJobId();
    job.code = code;
    job.status = JobStatus::PENDING;
    job.submission_time = std::time(nullptr);
    job.priority = priority;

    jobs_[job.job_id] = job;
    job_queue_.push(job);
    return true;
}

bool CloudComputingScheduler::SubmitJobWithResources(const std::string& code, const ResourceAllocation& resources) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    if (code.empty()) return false;

    ComputationJob job;
    job.job_id = GenerateJobId();
    job.code = code;
    job.status = JobStatus::PENDING;
    job.submission_time = std::time(nullptr);

    jobs_[job.job_id] = job;
    resource_allocations_[job.job_id] = resources;
    job_queue_.push(job);
    return true;
}

bool CloudComputingScheduler::CancelJob(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    auto it = jobs_.find(job_id);
    if (it == jobs_.end() || it->second.status == JobStatus::COMPLETED) return false;

    it->second.status = JobStatus::CANCELLED;
    return true;
}

ComputationJob CloudComputingScheduler::GetJobStatus(const std::string& job_id) const {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    auto it = jobs_.find(job_id);
    if (it != jobs_.end()) return it->second;
    return ComputationJob();
}

std::vector<ComputationJob> CloudComputingScheduler::GetQueuedJobs() const {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    std::vector<ComputationJob> result;
    for (const auto& job : jobs_) {
        if (job.second.status == JobStatus::PENDING) {
            result.push_back(job.second);
        }
    }
    return result;
}

std::vector<ComputationJob> CloudComputingScheduler::GetRunningJobs() const {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    std::vector<ComputationJob> result;
    for (const auto& job : jobs_) {
        if (job.second.status == JobStatus::RUNNING) {
            result.push_back(job.second);
        }
    }
    return result;
}

int CloudComputingScheduler::GetQueueLength() const {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    int count = 0;
    for (const auto& job : jobs_) {
        if (job.second.status == JobStatus::PENDING) count++;
    }
    return count;
}

bool CloudComputingScheduler::AllocateResources(const std::string& job_id, const ResourceAllocation& resources) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    auto it = jobs_.find(job_id);
    if (it == jobs_.end()) return false;

    resource_allocations_[job_id] = resources;
    return true;
}

ResourceAllocation CloudComputingScheduler::GetAllocatedResources(const std::string& job_id) const {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    auto it = resource_allocations_.find(job_id);
    if (it != resource_allocations_.end()) return it->second;
    return ResourceAllocation();
}

bool CloudComputingScheduler::ReleaseResources(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    auto it = resource_allocations_.find(job_id);
    if (it == resource_allocations_.end()) return false;
    resource_allocations_.erase(it);
    return true;
}

bool CloudComputingScheduler::RetryJob(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    auto it = jobs_.find(job_id);
    if (it == jobs_.end() || it->second.status != JobStatus::FAILED) return false;
    if (it->second.retry_count >= 3) return false;

    it->second.retry_count++;
    it->second.status = JobStatus::PENDING;
    job_queue_.push(it->second);
    return true;
}

std::vector<ComputationJob> CloudComputingScheduler::GetFailedJobs() const {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    std::vector<ComputationJob> result;
    for (const auto& job : jobs_) {
        if (job.second.status == JobStatus::FAILED) {
            result.push_back(job.second);
        }
    }
    return result;
}

std::vector<std::string> CloudComputingScheduler::SubmitBatchJobs(const std::vector<std::string>& codes) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    std::vector<std::string> result;

    for (const auto& code : codes) {
        if (!code.empty()) {
            ComputationJob job;
            job.job_id = GenerateJobId();
            job.code = code;
            job.status = JobStatus::PENDING;
            job.submission_time = std::time(nullptr);

            jobs_[job.job_id] = job;
            job_queue_.push(job);
            result.push_back(job.job_id);
        }
    }
    return result;
}

std::map<std::string, std::string> CloudComputingScheduler::WaitForBatchResults(const std::vector<std::string>& job_ids) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    std::map<std::string, std::string> results;

    for (const auto& job_id : job_ids) {
        auto it = jobs_.find(job_id);
        if (it != jobs_.end()) {
            it->second.status = JobStatus::COMPLETED;
            it->second.completion_time = std::time(nullptr);
            it->second.result = "batch_result_" + job_id;
            results[job_id] = it->second.result;
        }
    }

    total_jobs_processed_ += job_ids.size();
    return results;
}

int CloudComputingScheduler::GetTotalJobsProcessed() const {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    return total_jobs_processed_;
}

double CloudComputingScheduler::GetAverageJobDuration() const {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    if (total_jobs_processed_ == 0) return 0.0;

    double total_duration = 0.0;
    int count = 0;
    for (const auto& job : jobs_) {
        if (job.second.status == JobStatus::COMPLETED && job.second.completion_time > 0) {
            total_duration += (job.second.completion_time - job.second.submission_time);
            count++;
        }
    }

    return count > 0 ? total_duration / count : 0.0;
}

double CloudComputingScheduler::GetJobSuccessRate() const {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    if (total_jobs_processed_ == 0) return 0.0;

    int successful = 0;
    for (const auto& job : jobs_) {
        if (job.second.status == JobStatus::COMPLETED) {
            successful++;
        }
    }

    return (static_cast<double>(successful) / total_jobs_processed_) * 100.0;
}

std::string CloudComputingScheduler::GenerateJobId() const {
    auto now = std::time(nullptr);
    std::stringstream ss;
    ss << "job_" << std::hex << now << "_" << jobs_.size();
    return ss.str();
}

bool CloudComputingScheduler::ProcessJob(const ComputationJob& job) {
    return true;
}

void CloudComputingScheduler::UpdateJobStatus(const std::string& job_id, JobStatus status) {
    auto it = jobs_.find(job_id);
    if (it != jobs_.end()) {
        it->second.status = status;
    }
}

} // namespace bendcore::cloud
