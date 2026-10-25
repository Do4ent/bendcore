#include "bendcore/collaboration.hpp"

#include <algorithm>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace bendcore::collaboration {

// DocumentVersion implementation
std::string DocumentVersion::toString() const {
    std::ostringstream oss;
    oss << major << "." << minor << "." << patch;
    return oss.str();
}

// ChangeTracker implementation
ChangeTracker::ChangeTracker() = default;
ChangeTracker::~ChangeTracker() = default;

uint64_t ChangeTracker::recordChange(
    const std::string& userId,
    const std::string& userName,
    ChangeType type,
    const std::string& objectId,
    const std::string& objectName,
    const std::string& description,
    const std::string& beforeValue,
    const std::string& afterValue
) {
    Change change;
    change.id = nextChangeId++;
    change.userId = userId;
    change.userName = userName;
    change.type = type;
    change.objectId = objectId;
    change.objectName = objectName;
    change.description = description;
    change.beforeValue = beforeValue;
    change.afterValue = afterValue;
    change.timestamp = std::time(nullptr);
    change.isRolledBack = false;

    changes.push_back(change);
    return change.id;
}

std::vector<Change> ChangeTracker::getChangeHistory(
    std::optional<std::string> objectId,
    std::optional<std::string> userId,
    int64_t fromTimestamp,
    int64_t toTimestamp
) const {
    std::vector<Change> result;

    for (const auto& change : changes) {
        // Проверяем фильтры
        if (objectId.has_value() && change.objectId != objectId.value()) {
            continue;
        }
        if (userId.has_value() && change.userId != userId.value()) {
            continue;
        }
        if (fromTimestamp > 0 && change.timestamp < fromTimestamp) {
            continue;
        }
        if (toTimestamp > 0 && change.timestamp > toTimestamp) {
            continue;
        }
        if (change.isRolledBack) {
            continue;
        }

        result.push_back(change);
    }

    // Сортируем по времени (новые первыми)
    std::sort(result.begin(), result.end(),
        [](const Change& a, const Change& b) {
            return a.timestamp > b.timestamp;
        }
    );

    return result;
}

bool ChangeTracker::rollbackChange(uint64_t changeId, [[maybe_unused]] const std::string& userId) {
    for (auto& change : changes) {
        if (change.id == changeId && !change.isRolledBack) {
            change.isRolledBack = true;
            return true;
        }
    }
    return false;
}

std::vector<Change> ChangeTracker::getChangesByUser(
    const std::string& userId,
    int64_t fromTimestamp,
    int64_t toTimestamp
) const {
    return getChangeHistory(
        std::nullopt,
        userId,
        fromTimestamp,
        toTimestamp
    );
}

ChangeTracker::ChangeStats ChangeTracker::getChangeStats() const {
    ChangeStats stats;
    stats.totalChanges = changes.size();

    for (const auto& change : changes) {
        if (!change.isRolledBack) {
            int typeIdx = static_cast<int>(change.type);
            if (typeIdx >= 0 && typeIdx < 11) {
                stats.changesByType[typeIdx]++;
            }

            if (stats.changesByUser.find(change.userId) == stats.changesByUser.end()) {
                stats.changesByUser[change.userId] = 0;
            }
            stats.changesByUser[change.userId]++;

            if (stats.oldestTimestamp == 0 || change.timestamp < stats.oldestTimestamp) {
                stats.oldestTimestamp = change.timestamp;
            }
            if (change.timestamp > stats.newestTimestamp) {
                stats.newestTimestamp = change.timestamp;
            }
        }
    }

    return stats;
}

void ChangeTracker::purgeOldChanges(int64_t keepAfterTimestamp) {
    auto it = std::remove_if(changes.begin(), changes.end(),
        [keepAfterTimestamp](const Change& c) {
            return c.timestamp < keepAfterTimestamp;
        }
    );
    changes.erase(it, changes.end());
}

// VersionManager implementation
VersionManager::VersionManager() = default;
VersionManager::~VersionManager() = default;

DocumentVersion VersionManager::createVersion(
    const std::string& description,
    const std::string& createdBy,
    const std::vector<uint64_t>& changeIds
) {
    // Увеличиваем номер версии
    currentPatch++;
    if (currentPatch > 99) {
        currentPatch = 0;
        currentMinor++;
        if (currentMinor > 99) {
            currentMinor = 0;
            currentMajor++;
        }
    }

    DocumentVersion version;
    version.major = currentMajor;
    version.minor = currentMinor;
    version.patch = currentPatch;
    version.description = description;
    version.createdBy = createdBy;
    version.timestamp = std::time(nullptr);
    version.changeIds = changeIds;

    // Упрощённый хеш (в реальности SHA-1)
    std::ostringstream oss;
    oss << std::hex << std::time(nullptr) << changeIds.size();
    version.hash = oss.str();

    versions.push_back(version);
    return version;
}

DocumentVersion VersionManager::getCurrentVersion() const {
    if (versions.empty()) {
        DocumentVersion v;
        v.major = 1;
        v.minor = 0;
        v.patch = 0;
        return v;
    }
    return versions.back();
}

std::optional<DocumentVersion> VersionManager::getVersion(const std::string& versionString) const {
    for (const auto& v : versions) {
        if (v.toString() == versionString) {
            return v;
        }
    }
    return std::nullopt;
}

std::vector<DocumentVersion> VersionManager::getVersionHistory(int limit) const {
    std::vector<DocumentVersion> result;

    int count = 0;
    for (auto it = versions.rbegin(); it != versions.rend() && count < limit; ++it, ++count) {
        result.push_back(*it);
    }

    return result;
}

bool VersionManager::revertToVersion(const std::string& versionString, [[maybe_unused]] const std::string& userId) {
    auto it = std::find_if(versions.begin(), versions.end(),
        [&versionString](const DocumentVersion& v) {
            return v.toString() == versionString;
        }
    );

    if (it != versions.end()) {
        // В реальной системе здесь выполнялось бы откачивание
        return true;
    }

    return false;
}

VersionManager::VersionDiff VersionManager::compareVersions(
    const std::string& version1,
    const std::string& version2
) const {
    VersionDiff diff;

    auto v1 = getVersion(version1);
    auto v2 = getVersion(version2);

    if (v1.has_value() && v2.has_value()) {
        diff.totalDifferences = std::max(v1->changeIds.size(), v2->changeIds.size());
    }

    return diff;
}

void VersionManager::tagVersion(const std::string& versionString, const std::string& tagName) {
    tags[tagName] = versionString;
}

std::optional<DocumentVersion> VersionManager::getVersionByTag(const std::string& tagName) const {
    auto it = tags.find(tagName);
    if (it != tags.end()) {
        return getVersion(it->second);
    }
    return std::nullopt;
}

std::map<std::string, std::string> VersionManager::getAllTags() const {
    return tags;
}

// CommentManager implementation
CommentManager::CommentManager() = default;
CommentManager::~CommentManager() = default;

uint64_t CommentManager::addComment(
    const std::string& userId,
    const std::string& userName,
    const std::string& userEmail,
    const std::string& objectId,
    const std::string& text,
    std::optional<uint64_t> parentId
) {
    Comment comment;
    comment.id = nextCommentId++;
    comment.userId = userId;
    comment.userName = userName;
    comment.userEmail = userEmail;
    comment.objectId = objectId;
    comment.text = text;
    comment.timestamp = std::time(nullptr);
    comment.parentId = parentId;
    comment.isResolved = false;

    comments.push_back(comment);

    // Добавляем ID комментария в список ответов родителя
    if (parentId.has_value()) {
        for (auto& c : comments) {
            if (c.id == parentId.value()) {
                c.replies.push_back(comment.id);
                break;
            }
        }
    }

    return comment.id;
}

std::vector<Comment> CommentManager::getComments(const std::string& objectId) const {
    std::vector<Comment> result;

    for (const auto& c : comments) {
        if (c.objectId == objectId && !c.parentId.has_value()) {
            result.push_back(c);
        }
    }

    // Сортируем по времени (старые первыми)
    std::sort(result.begin(), result.end(),
        [](const Comment& a, const Comment& b) {
            return a.timestamp < b.timestamp;
        }
    );

    return result;
}

std::vector<Comment> CommentManager::getCommentThread(uint64_t parentId) const {
    std::vector<Comment> result;

    for (const auto& c : comments) {
        if ((c.id == parentId) || (c.parentId.has_value() && c.parentId.value() == parentId)) {
            result.push_back(c);
        }
    }

    // Сортируем по времени
    std::sort(result.begin(), result.end(),
        [](const Comment& a, const Comment& b) {
            return a.timestamp < b.timestamp;
        }
    );

    return result;
}

std::vector<Comment> CommentManager::getCommentsByUser(const std::string& userId) const {
    std::vector<Comment> result;

    for (const auto& c : comments) {
        if (c.userId == userId) {
            result.push_back(c);
        }
    }

    // Сортируем по времени
    std::sort(result.begin(), result.end(),
        [](const Comment& a, const Comment& b) {
            return a.timestamp > b.timestamp;
        }
    );

    return result;
}

bool CommentManager::editComment(uint64_t commentId, const std::string& newText) {
    for (auto& c : comments) {
        if (c.id == commentId) {
            c.text = newText;
            return true;
        }
    }
    return false;
}

bool CommentManager::deleteComment(uint64_t commentId) {
    auto it = std::find_if(comments.begin(), comments.end(),
        [commentId](const Comment& c) { return c.id == commentId; }
    );

    if (it != comments.end()) {
        comments.erase(it);
        return true;
    }
    return false;
}

bool CommentManager::resolveComment(uint64_t commentId) {
    for (auto& c : comments) {
        if (c.id == commentId) {
            c.isResolved = true;
            return true;
        }
    }
    return false;
}

std::vector<Comment> CommentManager::getUnresolvedComments() const {
    std::vector<Comment> result;

    for (const auto& c : comments) {
        if (!c.isResolved && !c.parentId.has_value()) {
            result.push_back(c);
        }
    }

    return result;
}

bool CommentManager::attachFile(uint64_t commentId, const std::string& filePath) {
    for (auto& c : comments) {
        if (c.id == commentId) {
            c.attachmentPath = filePath;
            return true;
        }
    }
    return false;
}

CommentManager::CommentStats CommentManager::getCommentStats() const {
    CommentStats stats;
    stats.totalComments = comments.size();
    stats.unresolvedComments = 0;

    for (const auto& c : comments) {
        if (!c.isResolved && !c.parentId.has_value()) {
            stats.unresolvedComments++;
        }

        if (stats.commentsByObject.find(c.objectId) == stats.commentsByObject.end()) {
            stats.commentsByObject[c.objectId] = 0;
        }
        stats.commentsByObject[c.objectId]++;
    }

    return stats;
}

// AccessManager implementation
AccessManager::AccessManager() = default;
AccessManager::~AccessManager() = default;

void AccessManager::addUser(
    const std::string& userId,
    const std::string& userName,
    const std::string& email,
    AccessLevel accessLevel
) {
    UserInfo user;
    user.userId = userId;
    user.userName = userName;
    user.email = email;
    user.accessLevel = accessLevel;
    user.isActive = true;
    user.lastSeenTimestamp = std::time(nullptr);

    users.push_back(user);
}

std::optional<UserInfo> AccessManager::getUserInfo(const std::string& userId) const {
    for (const auto& u : users) {
        if (u.userId == userId) {
            return u;
        }
    }
    return std::nullopt;
}

bool AccessManager::setAccessLevel(const std::string& userId, AccessLevel accessLevel) {
    for (auto& u : users) {
        if (u.userId == userId) {
            u.accessLevel = accessLevel;
            return true;
        }
    }
    return false;
}

bool AccessManager::hasPermission(
    const std::string& userId,
    const std::string& action
) const {
    auto user = getUserInfo(userId);
    if (!user.has_value() || !user->isActive) {
        return false;
    }

    // Проверяем права в зависимости от действия и уровня доступа
    if (action == "view") {
        return user->accessLevel >= AccessLevel::Viewer;
    }
    if (action == "comment") {
        return user->accessLevel >= AccessLevel::Commenter;
    }
    if (action == "edit") {
        return user->accessLevel >= AccessLevel::Editor;
    }
    if (action == "admin") {
        return user->accessLevel >= AccessLevel::Admin;
    }

    return false;
}

std::vector<UserInfo> AccessManager::getUsers(
    std::optional<AccessLevel> filterByLevel
) const {
    std::vector<UserInfo> result;

    for (const auto& u : users) {
        if (filterByLevel.has_value()) {
            if (u.accessLevel != filterByLevel.value()) {
                continue;
            }
        }
        result.push_back(u);
    }

    return result;
}

bool AccessManager::setUserActive(const std::string& userId, bool active) {
    for (auto& u : users) {
        if (u.userId == userId) {
            u.isActive = active;
            return true;
        }
    }
    return false;
}

void AccessManager::logActivity(
    const std::string& userId,
    const std::string& action,
    const std::string& objectId,
    const std::string& details
) {
    UserActivity activity;
    activity.userId = userId;
    activity.action = action;
    activity.objectId = objectId;
    activity.timestamp = std::time(nullptr);
    activity.details = details;

    activities.push_back(activity);
}

std::vector<UserActivity> AccessManager::getUserActivity(
    const std::string& userId,
    int64_t fromTimestamp,
    int64_t toTimestamp
) const {
    std::vector<UserActivity> result;

    for (const auto& a : activities) {
        if (a.userId != userId) continue;
        if (fromTimestamp > 0 && a.timestamp < fromTimestamp) continue;
        if (toTimestamp > 0 && a.timestamp > toTimestamp) continue;

        result.push_back(a);
    }

    // Сортируем по времени (новые первыми)
    std::sort(result.begin(), result.end(),
        [](const UserActivity& a, const UserActivity& b) {
            return a.timestamp > b.timestamp;
        }
    );

    return result;
}

std::vector<UserActivity> AccessManager::getObjectActivity(
    const std::string& objectId,
    int64_t fromTimestamp,
    int64_t toTimestamp
) const {
    std::vector<UserActivity> result;

    for (const auto& a : activities) {
        if (a.objectId != objectId) continue;
        if (fromTimestamp > 0 && a.timestamp < fromTimestamp) continue;
        if (toTimestamp > 0 && a.timestamp > toTimestamp) continue;

        result.push_back(a);
    }

    // Сортируем по времени
    std::sort(result.begin(), result.end(),
        [](const UserActivity& a, const UserActivity& b) {
            return a.timestamp > b.timestamp;
        }
    );

    return result;
}

// CollaborationManager implementation
CollaborationManager::CollaborationManager() = default;
CollaborationManager::~CollaborationManager() = default;

bool CollaborationManager::saveSession([[maybe_unused]] const std::string& sessionFile) const {
    // В реальной реализации здесь сохранялись бы все данные в файл
    return true;
}

bool CollaborationManager::loadSession([[maybe_unused]] const std::string& sessionFile) {
    // В реальной реализации здесь загружались бы все данные из файла
    return true;
}

std::vector<UserInfo> CollaborationManager::getActiveUsers() const {
    return accessManager.getUsers();
}

void CollaborationManager::reportConflict([[maybe_unused]] const ConflictInfo& conflict) {
    // В реальной реализации здесь сохранялась бы информация о конфликте
}

std::vector<CollaborationManager::ConflictInfo> CollaborationManager::getUnresolvedConflicts() const {
    // В реальной реализации здесь возвращались бы неразрешённые конфликты
    return std::vector<ConflictInfo>();
}

bool CollaborationManager::resolveConflict([[maybe_unused]] const std::string& objectId, [[maybe_unused]] const std::string& resolution) {
    // В реальной реализации здесь выполнялось бы разрешение конфликта
    return true;
}

}  // namespace bendcore::collaboration
