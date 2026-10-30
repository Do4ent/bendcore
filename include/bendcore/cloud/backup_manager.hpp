#pragma once
#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <memory>
#include <mutex>

namespace bendcore::cloud {

struct BackupMetadata {
    std::string backup_id;
    std::string source_path;
    time_t creation_time;
    size_t size;
    std::string checksum;
    std::string status;
    std::vector<std::string> tags;

    BackupMetadata() : creation_time(0), size(0), status("pending") {}
    BackupMetadata(const std::string& id, const std::string& path, time_t t, size_t s, const std::string& cs)
        : backup_id(id), source_path(path), creation_time(t), size(s), checksum(cs), status("completed") {}
};

struct BackupSchedule {
    std::string schedule_id;
    std::string source_path;
    std::string frequency;
    time_t start_time;
    bool enabled;
    int retention_days;

    BackupSchedule() : start_time(0), enabled(false), retention_days(30) {}
};

class CloudBackup {
public:
    virtual bool CreateBackup(const std::string& path) = 0;
    virtual bool RestoreBackup(const std::string& backup_id, const std::string& path) = 0;
    virtual std::vector<std::string> ListBackups() const = 0;
    virtual bool DeleteBackup(const std::string& backup_id) = 0;
    virtual ~CloudBackup() = default;
};

class BackupManager : public CloudBackup {
public:
    BackupManager();
    ~BackupManager() override;

    bool CreateBackup(const std::string& path) override;
    bool RestoreBackup(const std::string& backup_id, const std::string& path) override;
    std::vector<std::string> ListBackups() const override;
    bool DeleteBackup(const std::string& backup_id) override;

    bool ScheduleBackup(const std::string& source_path, const std::string& frequency, int retention_days);
    bool CancelSchedule(const std::string& schedule_id);
    std::vector<BackupSchedule> GetActiveSchedules() const;

    bool RestoreToPointInTime(const std::string& path, time_t timestamp);
    std::vector<time_t> GetAvailableRestorePoints(const std::string& path) const;

    BackupMetadata GetBackupMetadata(const std::string& backup_id) const;
    bool VerifyBackup(const std::string& backup_id);
    bool CompressBackup(const std::string& backup_id);

    size_t GetTotalBackupSize() const;
    int GetBackupCount() const;

private:
    mutable std::mutex backup_mutex_;
    std::map<std::string, BackupMetadata> backups_;
    std::map<std::string, BackupSchedule> schedules_;
    std::vector<time_t> restore_points_;
    mutable int id_counter_ = 0;

    std::string GenerateBackupId() const;
    std::string ComputeChecksum(const std::string& path) const;
    bool ValidateBackup(const BackupMetadata& metadata) const;
};

}
