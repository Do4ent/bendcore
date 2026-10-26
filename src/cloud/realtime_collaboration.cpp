#include "bendcore/cloud/realtime_collaboration.hpp"

#include <algorithm>
#include <ctime>

namespace bendcore::cloud {

RealtimeCollaborationManager::RealtimeCollaborationManager() = default;
RealtimeCollaborationManager::~RealtimeCollaborationManager() = default;

bool RealtimeCollaborationManager::registerUserPresence(
    const std::string& userId,
    const std::string& userName,
    const std::string& userEmail
) {
    if (userId.empty() || userName.empty()) {
        return false;
    }

    // Check if user already registered
    auto it = std::find_if(activeUsers.begin(), activeUsers.end(),
        [&userId](const UserPresence& u) { return u.userId == userId; });

    if (it != activeUsers.end()) {
        it->lastActivity = std::time(nullptr);
        return true;
    }

    UserPresence presence;
    presence.userId = userId;
    presence.userName = userName;
    presence.userEmail = userEmail;
    presence.isActive = true;
    presence.lastActivity = std::time(nullptr);
    presence.currentAction = "idle";

    activeUsers.push_back(presence);
    return true;
}

bool RealtimeCollaborationManager::updateUserActivity(
    const std::string& userId,
    const std::string& action,
    const std::string& objectId
) {
    auto it = std::find_if(activeUsers.begin(), activeUsers.end(),
        [&userId](const UserPresence& u) { return u.userId == userId; });

    if (it != activeUsers.end()) {
        it->currentAction = action;
        it->selectedObject = objectId;
        it->lastActivity = std::time(nullptr);
        return true;
    }

    return false;
}

bool RealtimeCollaborationManager::removeUserPresence(const std::string& userId) {
    auto it = std::find_if(activeUsers.begin(), activeUsers.end(),
        [&userId](const UserPresence& u) { return u.userId == userId; });

    if (it != activeUsers.end()) {
        activeUsers.erase(it);
        return true;
    }

    return false;
}

std::optional<UserPresence> RealtimeCollaborationManager::getUserPresence(
    const std::string& userId
) const {
    auto it = std::find_if(activeUsers.begin(), activeUsers.end(),
        [&userId](const UserPresence& u) { return u.userId == userId; });

    if (it != activeUsers.end()) {
        return *it;
    }

    return std::nullopt;
}

std::vector<UserPresence> RealtimeCollaborationManager::getActiveUsers() const {
    return activeUsers;
}

int RealtimeCollaborationManager::getActiveUserCount() const {
    return static_cast<int>(activeUsers.size());
}

bool RealtimeCollaborationManager::acquireLock(
    const std::string& objectId,
    const std::string& userId,
    const std::string& userName,
    int lockTimeoutSeconds
) {
    if (objectId.empty() || userId.empty()) {
        return false;
    }

    // Check if object is already locked by another user
    for (const auto& lock : locks) {
        if (lock.objectId == objectId && lock.userId != userId) {
            return false;  // Already locked
        }
    }

    EntityLock newLock;
    newLock.lockId = "lock_" + std::to_string(nextLockId++);
    newLock.objectId = objectId;
    newLock.userId = userId;
    newLock.userName = userName;
    newLock.acquiredAt = std::time(nullptr);
    newLock.expiresAt = newLock.acquiredAt + lockTimeoutSeconds;
    newLock.lockType = "exclusive";

    locks.push_back(newLock);
    return true;
}

bool RealtimeCollaborationManager::releaseLock(
    const std::string& objectId,
    const std::string& userId
) {
    auto it = std::find_if(locks.begin(), locks.end(),
        [&objectId, &userId](const EntityLock& l) {
            return l.objectId == objectId && l.userId == userId;
        });

    if (it != locks.end()) {
        locks.erase(it);
        return true;
    }

    return false;
}

bool RealtimeCollaborationManager::isLocked(const std::string& objectId) const {
    int64_t now = std::time(nullptr);

    for (const auto& lock : locks) {
        if (lock.objectId == objectId && lock.expiresAt > now) {
            return true;
        }
    }

    return false;
}

std::optional<EntityLock> RealtimeCollaborationManager::getLock(
    const std::string& objectId
) const {
    int64_t now = std::time(nullptr);

    for (const auto& lock : locks) {
        if (lock.objectId == objectId && lock.expiresAt > now) {
            return lock;
        }
    }

    return std::nullopt;
}

bool RealtimeCollaborationManager::isLockedByUser(
    const std::string& objectId,
    const std::string& userId
) const {
    int64_t now = std::time(nullptr);

    for (const auto& lock : locks) {
        if (lock.objectId == objectId && lock.userId == userId && lock.expiresAt > now) {
            return true;
        }
    }

    return false;
}

std::vector<EntityLock> RealtimeCollaborationManager::getActiveLocks() const {
    int64_t now = std::time(nullptr);
    std::vector<EntityLock> activeLocks;

    for (const auto& lock : locks) {
        if (lock.expiresAt > now) {
            activeLocks.push_back(lock);
        }
    }

    return activeLocks;
}

uint64_t RealtimeCollaborationManager::createNotification(
    const std::string& userId,
    const std::string& fromUser,
    const std::string& type,
    const std::string& message,
    const std::string& objectId
) {
    Notification notification;
    notification.notificationId = "notif_" + std::to_string(nextNotificationId);
    notification.userId = userId;
    notification.fromUser = fromUser;
    notification.type = type;
    notification.message = message;
    notification.objectId = objectId;
    notification.timestamp = std::time(nullptr);
    notification.isRead = false;

    notifications.push_back(notification);
    return nextNotificationId++;
}

std::vector<Notification> RealtimeCollaborationManager::getUserNotifications(
    const std::string& userId,
    bool unreadOnly
) const {
    std::vector<Notification> result;

    for (const auto& notif : notifications) {
        if (notif.userId != userId) {
            continue;
        }

        if (unreadOnly && notif.isRead) {
            continue;
        }

        result.push_back(notif);
    }

    std::sort(result.begin(), result.end(),
        [](const Notification& a, const Notification& b) {
            return a.timestamp > b.timestamp;
        });

    return result;
}

bool RealtimeCollaborationManager::markNotificationAsRead(uint64_t notificationId) {
    for (auto& notif : notifications) {
        if (notif.notificationId == "notif_" + std::to_string(notificationId)) {
            notif.isRead = true;
            return true;
        }
    }

    return false;
}

bool RealtimeCollaborationManager::deleteNotification(uint64_t notificationId) {
    auto it = std::find_if(notifications.begin(), notifications.end(),
        [notificationId](const Notification& n) {
            return n.notificationId == "notif_" + std::to_string(notificationId);
        });

    if (it != notifications.end()) {
        notifications.erase(it);
        return true;
    }

    return false;
}

int RealtimeCollaborationManager::getUnreadNotificationCount(
    const std::string& userId
) const {
    int count = 0;

    for (const auto& notif : notifications) {
        if (notif.userId == userId && !notif.isRead) {
            count++;
        }
    }

    return count;
}

void RealtimeCollaborationManager::broadcastChange(
    const std::string& objectId,
    const std::string& userId,
    const std::string& changeType,
    const std::map<std::string, std::string>& changeData
) {
    ChangeNotification change;
    change.changeId = "change_" + std::to_string(changeHistory.size() + 1);
    change.objectId = objectId;
    change.userId = userId;
    change.changeType = changeType;
    change.timestamp = std::time(nullptr);
    change.changeData = changeData;

    changeHistory.push_back(change);
}

std::vector<ChangeNotification> RealtimeCollaborationManager::getChanges(
    const std::string& objectId,
    int64_t sinceTimestamp
) const {
    std::vector<ChangeNotification> result;

    for (const auto& change : changeHistory) {
        if (change.objectId != objectId) {
            continue;
        }

        if (sinceTimestamp > 0 && change.timestamp < sinceTimestamp) {
            continue;
        }

        result.push_back(change);
    }

    std::sort(result.begin(), result.end(),
        [](const ChangeNotification& a, const ChangeNotification& b) {
            return a.timestamp > b.timestamp;
        });

    return result;
}

RealtimeCollaborationManager::ConflictDetection RealtimeCollaborationManager::detectConflict(
    const std::string& objectId,
    const std::string& userId
) const {
    ConflictDetection detection;
    detection.hasConflict = false;

    // Check for locks by other users
    int64_t now = std::time(nullptr);

    for (const auto& lock : locks) {
        if (lock.objectId == objectId && lock.userId != userId && lock.expiresAt > now) {
            detection.hasConflict = true;
            detection.conflictType = "lock_violation";
            detection.user1 = userId;
            detection.user2 = lock.userId;
            detection.objectId = objectId;
            return detection;
        }
    }

    return detection;
}

RealtimeCollaborationManager::SessionInfo RealtimeCollaborationManager::createSession(
    const std::string& projectId,
    const std::string& userId
) {
    SessionInfo session;
    session.sessionId = "session_" + std::to_string(sessions.size() + 1);
    session.projectId = projectId;
    session.startedAt = std::time(nullptr);
    session.participants.push_back(userId);
    session.changeCount = 0;

    sessions[session.sessionId] = session;
    return session;
}

bool RealtimeCollaborationManager::addParticipantToSession(
    const std::string& sessionId,
    const std::string& userId
) {
    auto it = sessions.find(sessionId);

    if (it == sessions.end()) {
        return false;
    }

    auto& participants = it->second.participants;

    if (std::find(participants.begin(), participants.end(), userId) == participants.end()) {
        participants.push_back(userId);
        return true;
    }

    return false;
}

bool RealtimeCollaborationManager::endSession(const std::string& sessionId) {
    auto it = sessions.find(sessionId);

    if (it != sessions.end()) {
        sessions.erase(it);
        return true;
    }

    return false;
}

std::optional<RealtimeCollaborationManager::SessionInfo> RealtimeCollaborationManager::getSessionInfo(
    const std::string& sessionId
) const {
    auto it = sessions.find(sessionId);

    if (it != sessions.end()) {
        return it->second;
    }

    return std::nullopt;
}

std::vector<RealtimeCollaborationManager::SessionInfo> RealtimeCollaborationManager::getActiveSessions() const {
    std::vector<SessionInfo> result;

    for (const auto& pair : sessions) {
        result.push_back(pair.second);
    }

    return result;
}

RealtimeCollaborationManager::CollaborationMetrics RealtimeCollaborationManager::getMetrics() const {
    CollaborationMetrics metrics;
    metrics.totalActiveUsers = getActiveUserCount();
    metrics.totalActiveLocks = getActiveLocks().size();

    for (const auto& notif : notifications) {
        if (!notif.isRead) {
            metrics.totalPendingNotifications++;
        }
    }

    metrics.totalConflicts = 0;
    for (const auto& lock : locks) {
        int64_t now = std::time(nullptr);
        if (lock.expiresAt <= now) {
            metrics.totalConflicts++;
        }
    }

    return metrics;
}

}  // namespace bendcore::cloud
