#include "bendcore/cloud/backup_manager.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace bendcore::cloud {

BackupManager::BackupManager() = default;
BackupManager::~BackupManager() = default;

bool BackupManager::CreateBackup(const std::string& path) {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    if (path.empty()) return false;

    std::string backup_id = GenerateBackupId();
    auto now = std::time(nullptr);
    BackupMetadata metadata(backup_id, path, now, 1024, ComputeChecksum(path));
    backups_[backup_id] = metadata;
    restore_points_.push_back(now);
    std::sort(restore_points_.begin(), restore_points_.end());
    return true;
}

bool BackupManager::RestoreBackup(const std::string& backup_id, const std::string& path) {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    if (backup_id.empty() || path.empty()) return false;

    auto it = backups_.find(backup_id);
    if (it == backups_.end() || it->second.status != "completed") return false;

    it->second.status = "completed";
    return true;
}

std::vector<std::string> BackupManager::ListBackups() const {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    std::vector<std::string> result;
    for (const auto& backup : backups_) {
        if (backup.second.status == "completed") {
            result.push_back(backup.first);
        }
    }
    return result;
}

bool BackupManager::DeleteBackup(const std::string& backup_id) {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    if (backup_id.empty()) return false;

    auto it = backups_.find(backup_id);
    if (it == backups_.end()) return false;

    backups_.erase(it);
    return true;
}

bool BackupManager::ScheduleBackup(const std::string& source_path, const std::string& frequency, int retention_days) {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    if (source_path.empty() || retention_days < 1) return false;

    std::string schedule_id = GenerateBackupId();
    BackupSchedule schedule;
    schedule.schedule_id = schedule_id;
    schedule.source_path = source_path;
    schedule.frequency = frequency;
    schedule.start_time = std::time(nullptr);
    schedule.enabled = true;
    schedule.retention_days = retention_days;
    schedules_[schedule_id] = schedule;
    return true;
}

bool BackupManager::CancelSchedule(const std::string& schedule_id) {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    auto it = schedules_.find(schedule_id);
    if (it == schedules_.end()) return false;
    schedules_.erase(it);
    return true;
}

std::vector<BackupSchedule> BackupManager::GetActiveSchedules() const {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    std::vector<BackupSchedule> result;
    for (const auto& schedule : schedules_) {
        if (schedule.second.enabled) {
            result.push_back(schedule.second);
        }
    }
    return result;
}

bool BackupManager::RestoreToPointInTime(const std::string& path, time_t timestamp) {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    if (path.empty() || timestamp <= 0) return false;

    auto it = std::upper_bound(restore_points_.begin(), restore_points_.end(), timestamp);
    return it != restore_points_.begin();
}

std::vector<time_t> BackupManager::GetAvailableRestorePoints(const std::string& path) const {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    std::vector<time_t> result;
    for (const auto& backup : backups_) {
        if (backup.second.source_path == path) {
            result.push_back(backup.second.creation_time);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

BackupMetadata BackupManager::GetBackupMetadata(const std::string& backup_id) const {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    auto it = backups_.find(backup_id);
    if (it != backups_.end()) return it->second;
    return BackupMetadata();
}

bool BackupManager::VerifyBackup(const std::string& backup_id) {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    auto it = backups_.find(backup_id);
    if (it == backups_.end()) return false;
    return ValidateBackup(it->second);
}

bool BackupManager::CompressBackup(const std::string& backup_id) {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    auto it = backups_.find(backup_id);
    if (it == backups_.end()) return false;
    it->second.size = it->second.size / 2;
    return true;
}

size_t BackupManager::GetTotalBackupSize() const {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    size_t total = 0;
    for (const auto& backup : backups_) {
        total += backup.second.size;
    }
    return total;
}

int BackupManager::GetBackupCount() const {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    return backups_.size();
}

std::string BackupManager::GenerateBackupId() const {
    auto now = std::time(nullptr);
    std::stringstream ss;
    ss << "backup_" << std::hex << now << "_" << ++id_counter_;
    return ss.str();
}

std::string BackupManager::ComputeChecksum(const std::string& path) const {
    std::hash<std::string> hasher;
    auto hash_value = hasher(path);
    std::stringstream ss;
    ss << std::hex << hash_value;
    return ss.str();
}

bool BackupManager::ValidateBackup(const BackupMetadata& metadata) const {
    return metadata.status == "completed" && !metadata.checksum.empty();
}

} // namespace bendcore::cloud
