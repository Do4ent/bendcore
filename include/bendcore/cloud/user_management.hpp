#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>
#include <set>

namespace bendcore::cloud {

// User roles
enum class UserRole {
    Admin,
    ProjectOwner,
    Editor,
    Commenter,
    Viewer
};

// User status
enum class UserStatus {
    Active,
    Inactive,
    Suspended,
    Deleted
};

// User information
struct User {
    std::string userId;
    std::string email;
    std::string firstName;
    std::string lastName;
    std::string displayName;
    UserRole role = UserRole::Viewer;
    UserStatus status = UserStatus::Active;
    int64_t createdAt = 0;
    int64_t lastLogin = 0;
    std::string profileImageUrl;
    std::string bio;
    std::map<std::string, std::string> preferences;
};

// Permission definition
struct Permission {
    std::string permissionId;
    std::string projectId;
    std::string userId;
    UserRole role = UserRole::Viewer;
    std::set<std::string> grantedActions;  // "read", "write", "delete", "share", "admin"
    int64_t grantedAt = 0;
    std::optional<int64_t> expiresAt;
};

// Role-based access control (RBAC) definition
struct RoleDefinition {
    UserRole role;
    std::string roleName;
    std::string description;
    std::set<std::string> permissions;  // Action names
    int hierarchyLevel = 0;  // 0=viewer, 1=commenter, 2=editor, 3=owner, 4=admin
};

// User management system
class UserManagementSystem {
public:
    explicit UserManagementSystem();
    ~UserManagementSystem();

    // User CRUD operations
    bool createUser(
        const std::string& email,
        const std::string& firstName,
        const std::string& lastName,
        UserRole role = UserRole::Viewer
    );

    std::optional<User> getUser(const std::string& userId) const;

    std::optional<User> getUserByEmail(const std::string& email) const;

    bool updateUser(const User& user);

    bool deleteUser(const std::string& userId);

    std::vector<User> getAllUsers() const;

    // User status management
    bool activateUser(const std::string& userId);

    bool deactivateUser(const std::string& userId);

    bool suspendUser(const std::string& userId, const std::string& reason);

    bool unsuspendUser(const std::string& userId);

    // Role management
    bool assignRole(
        const std::string& userId,
        UserRole role
    );

    UserRole getUserRole(const std::string& userId) const;

    std::vector<User> getUsersByRole(UserRole role) const;

    // Permission management
    bool grantPermission(
        const std::string& projectId,
        const std::string& userId,
        UserRole role,
        const std::set<std::string>& actions
    );

    bool revokePermission(
        const std::string& projectId,
        const std::string& userId
    );

    std::optional<Permission> getPermission(
        const std::string& projectId,
        const std::string& userId
    ) const;

    std::vector<Permission> getProjectPermissions(const std::string& projectId) const;

    std::vector<Permission> getUserPermissions(const std::string& userId) const;

    // Permission checking
    bool hasPermission(
        const std::string& userId,
        const std::string& projectId,
        const std::string& action
    ) const;

    bool hasRole(
        const std::string& userId,
        UserRole requiredRole
    ) const;

    // RBAC role definitions
    void defineRole(
        UserRole role,
        const std::string& roleName,
        const std::string& description,
        const std::set<std::string>& permissions,
        int hierarchyLevel
    );

    std::optional<RoleDefinition> getRoleDefinition(UserRole role) const;

    std::vector<RoleDefinition> getAllRoleDefinitions() const;

    // Team management
    struct Team {
        std::string teamId;
        std::string teamName;
        std::string owner;
        std::vector<std::string> members;
        int64_t createdAt = 0;
        std::string description;
    };

    bool createTeam(
        const std::string& teamName,
        const std::string& owner,
        const std::string& description = ""
    );

    bool addTeamMember(
        const std::string& teamId,
        const std::string& userId
    );

    bool removeTeamMember(
        const std::string& teamId,
        const std::string& userId
    );

    std::optional<Team> getTeam(const std::string& teamId) const;

    std::vector<Team> getUserTeams(const std::string& userId) const;

    // Audit trail
    struct AuditEntry {
        uint64_t entryId = 0;
        std::string userId;
        std::string action;
        std::string resourceId;
        std::string resourceType;
        int64_t timestamp = 0;
        std::string details;
        bool success = true;
    };

    void logAuditEntry(
        const std::string& userId,
        const std::string& action,
        const std::string& resourceId,
        const std::string& resourceType,
        bool success = true,
        const std::string& details = ""
    );

    std::vector<AuditEntry> getAuditLog(
        std::optional<std::string> userId = {},
        std::optional<std::string> action = {},
        int limit = 100
    ) const;

    // Preferences
    bool setUserPreference(
        const std::string& userId,
        const std::string& key,
        const std::string& value
    );

    std::optional<std::string> getUserPreference(
        const std::string& userId,
        const std::string& key
    ) const;

    // Statistics
    struct UserStatistics {
        int totalUsers = 0;
        int activeUsers = 0;
        int adminUsers = 0;
        int editorUsers = 0;
        int viewerUsers = 0;
        int totalTeams = 0;
        int avgUsersPerTeam = 0;
    };

    UserStatistics getStatistics() const;

private:
    std::vector<User> users;
    std::vector<Permission> permissions;
    std::vector<Team> teams;
    std::vector<AuditEntry> auditLog;
    std::map<UserRole, RoleDefinition> roleDefinitions;
    uint64_t nextAuditEntryId = 1;
};

}  // namespace bendcore::cloud
