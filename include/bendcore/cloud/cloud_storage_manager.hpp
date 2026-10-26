#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <cstdint>
#include <chrono>

namespace bendcore::cloud {

// Cloud storage types
enum class StorageType {
    Local,      // Local filesystem
    Cloud,      // Cloud provider (AWS S3, Azure Blob, Google Cloud Storage)
    Hybrid      // Both local and cloud
};

// Project metadata
struct ProjectMetadata {
    std::string projectId;
    std::string projectName;
    std::string description;
    std::string owner;
    std::vector<std::string> collaborators;
    int64_t createdAt = 0;
    int64_t modifiedAt = 0;
    int64_t sizeBytes = 0;
    bool isPublic = false;
    std::string visibilityLevel;  // "private", "team", "public"
    std::map<std::string, std::string> tags;
    std::string lastModifiedBy;
};

// Version information for cloud storage
struct VersionInfo {
    std::string versionId;
    std::string projectId;
    int64_t timestamp = 0;
    std::string author;
    std::string message;
    int64_t sizeBytes = 0;
    std::string hash;      // Content hash
    bool isLatest = false;
};

// Backup information
struct BackupInfo {
    std::string backupId;
    std::string projectId;
    int64_t timestamp = 0;
    std::string backupType;  // "incremental", "full"
    int64_t sizeBytes = 0;
    bool isRestored = false;
};

// Cloud storage manager
class CloudStorageManager {
public:
    explicit CloudStorageManager(StorageType type = StorageType::Hybrid);
    ~CloudStorageManager();

    // Project operations
    bool uploadProject(
        const std::string& projectId,
        const std::string& projectName,
        const std::string& filePath,
        const std::string& owner
    );

    bool downloadProject(
        const std::string& projectId,
        const std::string& outputPath
    );

    bool deleteProject(const std::string& projectId);

    bool projectExists(const std::string& projectId) const;

    // Versioning
    bool saveVersion(
        const std::string& projectId,
        const std::string& filePath,
        const std::string& message,
        const std::string& author
    );

    std::optional<VersionInfo> getLatestVersion(const std::string& projectId) const;

    std::vector<VersionInfo> getVersionHistory(
        const std::string& projectId,
        int limit = 50
    ) const;

    bool restoreVersion(
        const std::string& projectId,
        const std::string& versionId,
        const std::string& outputPath
    );

    // Backup operations
    bool createBackup(
        const std::string& projectId,
        const std::string& filePath,
        const std::string& backupType = "incremental"
    );

    std::optional<BackupInfo> getLatestBackup(const std::string& projectId) const;

    std::vector<BackupInfo> getBackupHistory(
        const std::string& projectId,
        int limit = 20
    ) const;

    bool restoreBackup(
        const std::string& projectId,
        const std::string& backupId,
        const std::string& outputPath
    );

    // Project metadata
    std::optional<ProjectMetadata> getProjectMetadata(const std::string& projectId) const;

    bool updateProjectMetadata(
        const std::string& projectId,
        const ProjectMetadata& metadata
    );

    // List operations
    std::vector<ProjectMetadata> listProjects(
        const std::string& owner,
        std::optional<std::string> filterTag = {}
    ) const;

    // Search
    std::vector<ProjectMetadata> searchProjects(
        const std::string& searchQuery,
        int limit = 20
    ) const;

    // Storage info
    struct StorageInfo {
        int64_t totalSizeBytes = 0;
        int64_t usedSizeBytes = 0;
        int projectCount = 0;
        int versionCount = 0;
        int backupCount = 0;
    };

    StorageInfo getStorageInfo(const std::string& owner) const;

private:
    StorageType storageType;
    std::vector<ProjectMetadata> projects;
    std::vector<VersionInfo> versions;
    std::vector<BackupInfo> backups;
    std::map<std::string, std::vector<VersionInfo>> projectVersions;
    std::map<std::string, std::vector<BackupInfo>> projectBackups;
};

}  // namespace bendcore::cloud
