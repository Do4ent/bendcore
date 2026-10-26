#include "bendcore/cloud/cloud_storage_manager.hpp"

#include <algorithm>
#include <ctime>

namespace bendcore::cloud {

CloudStorageManager::CloudStorageManager(StorageType type)
    : storageType(type) {
}

CloudStorageManager::~CloudStorageManager() = default;

bool CloudStorageManager::uploadProject(
    const std::string& projectId,
    const std::string& projectName,
    const std::string& filePath,
    const std::string& owner
) {
    if (projectId.empty() || projectName.empty() || owner.empty()) {
        return false;
    }

    ProjectMetadata metadata;
    metadata.projectId = projectId;
    metadata.projectName = projectName;
    metadata.owner = owner;
    metadata.createdAt = std::time(nullptr);
    metadata.modifiedAt = metadata.createdAt;
    metadata.sizeBytes = filePath.length();  // Simplified
    metadata.isPublic = false;
    metadata.visibilityLevel = "private";
    metadata.lastModifiedBy = owner;

    projects.push_back(metadata);
    return true;
}

bool CloudStorageManager::downloadProject(
    const std::string& projectId,
    const std::string& outputPath
) {
    auto it = std::find_if(projects.begin(), projects.end(),
        [&projectId](const ProjectMetadata& m) { return m.projectId == projectId; });

    return it != projects.end();
}

bool CloudStorageManager::deleteProject(const std::string& projectId) {
    auto it = std::find_if(projects.begin(), projects.end(),
        [&projectId](const ProjectMetadata& m) { return m.projectId == projectId; });

    if (it != projects.end()) {
        projects.erase(it);
        return true;
    }
    return false;
}

bool CloudStorageManager::projectExists(const std::string& projectId) const {
    return std::any_of(projects.begin(), projects.end(),
        [&projectId](const ProjectMetadata& m) { return m.projectId == projectId; });
}

bool CloudStorageManager::saveVersion(
    const std::string& projectId,
    const std::string& filePath,
    const std::string& message,
    const std::string& author
) {
    if (!projectExists(projectId)) {
        return false;
    }

    VersionInfo version;
    version.versionId = projectId + "_v" + std::to_string(versions.size() + 1);
    version.projectId = projectId;
    version.timestamp = std::time(nullptr);
    version.author = author;
    version.message = message;
    version.sizeBytes = filePath.length();
    version.isLatest = true;

    // Mark previous versions as not latest
    for (auto& v : projectVersions[projectId]) {
        v.isLatest = false;
    }

    versions.push_back(version);
    projectVersions[projectId].push_back(version);
    return true;
}

std::optional<VersionInfo> CloudStorageManager::getLatestVersion(
    const std::string& projectId
) const {
    auto it = projectVersions.find(projectId);
    if (it != projectVersions.end() && !it->second.empty()) {
        for (const auto& v : it->second) {
            if (v.isLatest) {
                return v;
            }
        }
    }
    return std::nullopt;
}

std::vector<VersionInfo> CloudStorageManager::getVersionHistory(
    const std::string& projectId,
    int limit
) const {
    auto it = projectVersions.find(projectId);
    if (it == projectVersions.end()) {
        return {};
    }

    auto versions_copy = it->second;
    std::sort(versions_copy.begin(), versions_copy.end(),
        [](const VersionInfo& a, const VersionInfo& b) {
            return a.timestamp > b.timestamp;
        });

    if (limit > 0 && versions_copy.size() > static_cast<size_t>(limit)) {
        versions_copy.resize(limit);
    }

    return versions_copy;
}

bool CloudStorageManager::restoreVersion(
    const std::string& projectId,
    const std::string& versionId,
    const std::string& outputPath
) {
    auto it = projectVersions.find(projectId);
    if (it == projectVersions.end()) {
        return false;
    }

    auto ver_it = std::find_if(it->second.begin(), it->second.end(),
        [&versionId](const VersionInfo& v) { return v.versionId == versionId; });

    return ver_it != it->second.end();
}

bool CloudStorageManager::createBackup(
    const std::string& projectId,
    const std::string& filePath,
    const std::string& backupType
) {
    if (!projectExists(projectId)) {
        return false;
    }

    BackupInfo backup;
    backup.backupId = projectId + "_bak_" + std::to_string(backups.size() + 1);
    backup.projectId = projectId;
    backup.timestamp = std::time(nullptr);
    backup.backupType = backupType;
    backup.sizeBytes = filePath.length();
    backup.isRestored = false;

    backups.push_back(backup);
    projectBackups[projectId].push_back(backup);
    return true;
}

std::optional<BackupInfo> CloudStorageManager::getLatestBackup(
    const std::string& projectId
) const {
    auto it = projectBackups.find(projectId);
    if (it != projectBackups.end() && !it->second.empty()) {
        return it->second.back();
    }
    return std::nullopt;
}

std::vector<BackupInfo> CloudStorageManager::getBackupHistory(
    const std::string& projectId,
    int limit
) const {
    auto it = projectBackups.find(projectId);
    if (it == projectBackups.end()) {
        return {};
    }

    auto backups_copy = it->second;
    std::sort(backups_copy.begin(), backups_copy.end(),
        [](const BackupInfo& a, const BackupInfo& b) {
            return a.timestamp > b.timestamp;
        });

    if (limit > 0 && backups_copy.size() > static_cast<size_t>(limit)) {
        backups_copy.resize(limit);
    }

    return backups_copy;
}

bool CloudStorageManager::restoreBackup(
    const std::string& projectId,
    const std::string& backupId,
    const std::string& outputPath
) {
    auto it = projectBackups.find(projectId);
    if (it == projectBackups.end()) {
        return false;
    }

    auto bak_it = std::find_if(it->second.begin(), it->second.end(),
        [&backupId](const BackupInfo& b) { return b.backupId == backupId; });

    return bak_it != it->second.end();
}

std::optional<ProjectMetadata> CloudStorageManager::getProjectMetadata(
    const std::string& projectId
) const {
    auto it = std::find_if(projects.begin(), projects.end(),
        [&projectId](const ProjectMetadata& m) { return m.projectId == projectId; });

    if (it != projects.end()) {
        return *it;
    }
    return std::nullopt;
}

bool CloudStorageManager::updateProjectMetadata(
    const std::string& projectId,
    const ProjectMetadata& metadata
) {
    auto it = std::find_if(projects.begin(), projects.end(),
        [&projectId](const ProjectMetadata& m) { return m.projectId == projectId; });

    if (it != projects.end()) {
        *it = metadata;
        it->modifiedAt = std::time(nullptr);
        return true;
    }
    return false;
}

std::vector<ProjectMetadata> CloudStorageManager::listProjects(
    const std::string& owner,
    std::optional<std::string> filterTag
) const {
    std::vector<ProjectMetadata> result;

    for (const auto& project : projects) {
        if (project.owner != owner) {
            continue;
        }

        if (filterTag.has_value()) {
            auto tag_it = project.tags.find(filterTag.value());
            if (tag_it == project.tags.end()) {
                continue;
            }
        }

        result.push_back(project);
    }

    std::sort(result.begin(), result.end(),
        [](const ProjectMetadata& a, const ProjectMetadata& b) {
            return a.modifiedAt > b.modifiedAt;
        });

    return result;
}

std::vector<ProjectMetadata> CloudStorageManager::searchProjects(
    const std::string& searchQuery,
    int limit
) const {
    std::vector<ProjectMetadata> result;

    for (const auto& project : projects) {
        if (project.projectName.find(searchQuery) != std::string::npos ||
            project.description.find(searchQuery) != std::string::npos) {
            result.push_back(project);
        }
    }

    if (limit > 0 && result.size() > static_cast<size_t>(limit)) {
        result.resize(limit);
    }

    return result;
}

CloudStorageManager::StorageInfo CloudStorageManager::getStorageInfo(
    const std::string& owner
) const {
    StorageInfo info;

    for (const auto& project : projects) {
        if (project.owner == owner) {
            info.usedSizeBytes += project.sizeBytes;
            info.projectCount++;
        }
    }

    for (const auto& version : versions) {
        info.versionCount++;
    }

    for (const auto& backup : backups) {
        info.backupCount++;
    }

    info.totalSizeBytes = 1099511627776;  // 1 TB
    return info;
}

}  // namespace bendcore::cloud
