#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <cstdint>
#include <set>

namespace bendcore::cloud {

// User presence information
struct UserPresence {
    std::string userId;
    std::string userName;
    std::string userEmail;
    bool isActive = true;
    int64_t lastActivity = 0;
    std::string currentAction;  // "viewing", "editing", "idle"
    std::string cursorPosition;  // x,y coordinates
    std::string selectedObject;
};

// Lock information for conflict prevention
struct EntityLock {
    std::string lockId;
    std::string objectId;
    std::string userId;
    std::string userName;
    int64_t acquiredAt = 0;
    int64_t expiresAt = 0;  // Lock timeout
    std::string lockType;  // "exclusive", "shared"
};

// Real-time notification
struct Notification {
    std::string notificationId;
    std::string userId;
    std::string fromUser;
    std::string type;  // "comment", "mention", "change", "lock", "share"
    std::string message;
    std::string objectId;
    int64_t timestamp = 0;
    bool isRead = false;
};

// Change notification for real-time sync
struct ChangeNotification {
    std::string changeId;
    std::string objectId;
    std::string userId;
    std::string changeType;  // "created", "modified", "deleted"
    int64_t timestamp = 0;
    std::map<std::string, std::string> changeData;
};

// Real-time collaboration manager
class RealtimeCollaborationManager {
public:
    explicit RealtimeCollaborationManager();
    ~RealtimeCollaborationManager();

    // User presence management
    bool registerUserPresence(
        const std::string& userId,
        const std::string& userName,
        const std::string& userEmail
    );

    bool updateUserActivity(
        const std::string& userId,
        const std::string& action,
        const std::string& objectId = ""
    );

    bool removeUserPresence(const std::string& userId);

    std::optional<UserPresence> getUserPresence(const std::string& userId) const;

    std::vector<UserPresence> getActiveUsers() const;

    int getActiveUserCount() const;

    // Lock management for conflict prevention
    bool acquireLock(
        const std::string& objectId,
        const std::string& userId,
        const std::string& userName,
        int lockTimeoutSeconds = 300
    );

    bool releaseLock(
        const std::string& objectId,
        const std::string& userId
    );

    bool isLocked(const std::string& objectId) const;

    std::optional<EntityLock> getLock(const std::string& objectId) const;

    bool isLockedByUser(
        const std::string& objectId,
        const std::string& userId
    ) const;

    std::vector<EntityLock> getActiveLocks() const;

    // Notification system
    uint64_t createNotification(
        const std::string& userId,
        const std::string& fromUser,
        const std::string& type,
        const std::string& message,
        const std::string& objectId = ""
    );

    std::vector<Notification> getUserNotifications(
        const std::string& userId,
        bool unreadOnly = false
    ) const;

    bool markNotificationAsRead(uint64_t notificationId);

    bool deleteNotification(uint64_t notificationId);

    int getUnreadNotificationCount(const std::string& userId) const;

    // Real-time change broadcasting
    void broadcastChange(
        const std::string& objectId,
        const std::string& userId,
        const std::string& changeType,
        const std::map<std::string, std::string>& changeData
    );

    std::vector<ChangeNotification> getChanges(
        const std::string& objectId,
        int64_t sinceTimestamp = 0
    ) const;

    // Conflict detection
    struct ConflictDetection {
        bool hasConflict = false;
        std::string conflictType;  // "concurrent_edit", "lock_violation"
        std::string user1;
        std::string user2;
        std::string objectId;
        std::vector<std::string> affectedProperties;
    };

    ConflictDetection detectConflict(
        const std::string& objectId,
        const std::string& userId
    ) const;

    // Session management
    struct SessionInfo {
        std::string sessionId;
        std::string projectId;
        int64_t startedAt = 0;
        std::vector<std::string> participants;
        int changeCount = 0;
    };

    SessionInfo createSession(
        const std::string& projectId,
        const std::string& userId
    );

    bool addParticipantToSession(
        const std::string& sessionId,
        const std::string& userId
    );

    bool endSession(const std::string& sessionId);

    std::optional<SessionInfo> getSessionInfo(const std::string& sessionId) const;

    std::vector<SessionInfo> getActiveSessions() const;

    // Metrics
    struct CollaborationMetrics {
        int totalActiveUsers = 0;
        int totalActiveLocks = 0;
        int totalPendingNotifications = 0;
        int totalConflicts = 0;
        int averageLatencyMs = 0;
    };

    CollaborationMetrics getMetrics() const;

private:
    std::vector<UserPresence> activeUsers;
    std::vector<EntityLock> locks;
    std::vector<Notification> notifications;
    std::vector<ChangeNotification> changeHistory;
    std::map<std::string, SessionInfo> sessions;
    uint64_t nextNotificationId = 1;
    uint64_t nextLockId = 1;
};

}  // namespace bendcore::cloud
