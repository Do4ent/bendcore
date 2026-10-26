#include "bendcore/cloud/user_management.hpp"

#include <algorithm>
#include <ctime>

namespace bendcore::cloud {

UserManagementSystem::UserManagementSystem() {
    // Initialize default role definitions
    RoleDefinition adminRole;
    adminRole.role = UserRole::Admin;
    adminRole.roleName = "Administrator";
    adminRole.description = "Full access";
    adminRole.permissions = {"read", "write", "delete", "share", "admin"};
    adminRole.hierarchyLevel = 4;
    roleDefinitions[UserRole::Admin] = adminRole;

    RoleDefinition editorRole;
    editorRole.role = UserRole::Editor;
    editorRole.roleName = "Editor";
    editorRole.description = "Edit documents";
    editorRole.permissions = {"read", "write", "share"};
    editorRole.hierarchyLevel = 2;
    roleDefinitions[UserRole::Editor] = editorRole;

    RoleDefinition viewerRole;
    viewerRole.role = UserRole::Viewer;
    viewerRole.roleName = "Viewer";
    viewerRole.description = "View only";
    viewerRole.permissions = {"read"};
    viewerRole.hierarchyLevel = 0;
    roleDefinitions[UserRole::Viewer] = viewerRole;
}

UserManagementSystem::~UserManagementSystem() = default;

bool UserManagementSystem::createUser(
    const std::string& email,
    const std::string& firstName,
    const std::string& lastName,
    UserRole role
) {
    if (email.empty() || firstName.empty()) {
        return false;
    }

    // Check if user already exists
    if (getUserByEmail(email).has_value()) {
        return false;
    }

    User user;
    user.userId = "user_" + std::to_string(users.size() + 1);
    user.email = email;
    user.firstName = firstName;
    user.lastName = lastName;
    user.displayName = firstName + " " + lastName;
    user.role = role;
    user.status = UserStatus::Active;
    user.createdAt = std::time(nullptr);
    user.lastLogin = 0;

    users.push_back(user);
    return true;
}

std::optional<User> UserManagementSystem::getUser(const std::string& userId) const {
    auto it = std::find_if(users.begin(), users.end(),
        [&userId](const User& u) { return u.userId == userId; });

    if (it != users.end()) {
        return *it;
    }

    return std::nullopt;
}

std::optional<User> UserManagementSystem::getUserByEmail(const std::string& email) const {
    auto it = std::find_if(users.begin(), users.end(),
        [&email](const User& u) { return u.email == email; });

    if (it != users.end()) {
        return *it;
    }

    return std::nullopt;
}

bool UserManagementSystem::updateUser(const User& user) {
    auto it = std::find_if(users.begin(), users.end(),
        [&user](const User& u) { return u.userId == user.userId; });

    if (it != users.end()) {
        *it = user;
        return true;
    }

    return false;
}

bool UserManagementSystem::deleteUser(const std::string& userId) {
    auto it = std::find_if(users.begin(), users.end(),
        [&userId](const User& u) { return u.userId == userId; });

    if (it != users.end()) {
        users.erase(it);
        return true;
    }

    return false;
}

std::vector<User> UserManagementSystem::getAllUsers() const {
    return users;
}

bool UserManagementSystem::activateUser(const std::string& userId) {
    auto user = getUser(userId);

    if (user.has_value()) {
        User updated = user.value();
        updated.status = UserStatus::Active;
        return updateUser(updated);
    }

    return false;
}

bool UserManagementSystem::deactivateUser(const std::string& userId) {
    auto user = getUser(userId);

    if (user.has_value()) {
        User updated = user.value();
        updated.status = UserStatus::Inactive;
        return updateUser(updated);
    }

    return false;
}

bool UserManagementSystem::suspendUser(const std::string& userId, const std::string& reason) {
    auto user = getUser(userId);

    if (user.has_value()) {
        User updated = user.value();
        updated.status = UserStatus::Suspended;
        updated.preferences["suspension_reason"] = reason;
        return updateUser(updated);
    }

    return false;
}

bool UserManagementSystem::unsuspendUser(const std::string& userId) {
    auto user = getUser(userId);

    if (user.has_value()) {
        User updated = user.value();
        updated.status = UserStatus::Active;
        return updateUser(updated);
    }

    return false;
}

bool UserManagementSystem::assignRole(
    const std::string& userId,
    UserRole role
) {
    auto user = getUser(userId);

    if (user.has_value()) {
        User updated = user.value();
        updated.role = role;
        return updateUser(updated);
    }

    return false;
}

UserRole UserManagementSystem::getUserRole(const std::string& userId) const {
    auto user = getUser(userId);

    if (user.has_value()) {
        return user.value().role;
    }

    return UserRole::Viewer;
}

std::vector<User> UserManagementSystem::getUsersByRole(UserRole role) const {
    std::vector<User> result;

    for (const auto& user : users) {
        if (user.role == role) {
            result.push_back(user);
        }
    }

    return result;
}

bool UserManagementSystem::grantPermission(
    const std::string& projectId,
    const std::string& userId,
    UserRole role,
    const std::set<std::string>& actions
) {
    // Check if permission already exists
    auto perm_it = std::find_if(permissions.begin(), permissions.end(),
        [&projectId, &userId](const Permission& p) {
            return p.projectId == projectId && p.userId == userId;
        });

    Permission permission;
    permission.permissionId = "perm_" + std::to_string(permissions.size() + 1);
    permission.projectId = projectId;
    permission.userId = userId;
    permission.role = role;
    permission.grantedActions = actions;
    permission.grantedAt = std::time(nullptr);

    if (perm_it != permissions.end()) {
        *perm_it = permission;
    } else {
        permissions.push_back(permission);
    }

    return true;
}

bool UserManagementSystem::revokePermission(
    const std::string& projectId,
    const std::string& userId
) {
    auto it = std::find_if(permissions.begin(), permissions.end(),
        [&projectId, &userId](const Permission& p) {
            return p.projectId == projectId && p.userId == userId;
        });

    if (it != permissions.end()) {
        permissions.erase(it);
        return true;
    }

    return false;
}

std::optional<Permission> UserManagementSystem::getPermission(
    const std::string& projectId,
    const std::string& userId
) const {
    auto it = std::find_if(permissions.begin(), permissions.end(),
        [&projectId, &userId](const Permission& p) {
            return p.projectId == projectId && p.userId == userId;
        });

    if (it != permissions.end()) {
        return *it;
    }

    return std::nullopt;
}

std::vector<Permission> UserManagementSystem::getProjectPermissions(
    const std::string& projectId
) const {
    std::vector<Permission> result;

    for (const auto& perm : permissions) {
        if (perm.projectId == projectId) {
            result.push_back(perm);
        }
    }

    return result;
}

std::vector<Permission> UserManagementSystem::getUserPermissions(
    const std::string& userId
) const {
    std::vector<Permission> result;

    for (const auto& perm : permissions) {
        if (perm.userId == userId) {
            result.push_back(perm);
        }
    }

    return result;
}

bool UserManagementSystem::hasPermission(
    const std::string& userId,
    const std::string& projectId,
    const std::string& action
) const {
    auto perm = getPermission(projectId, userId);

    if (!perm.has_value()) {
        return false;
    }

    return perm.value().grantedActions.find(action) != perm.value().grantedActions.end();
}

bool UserManagementSystem::hasRole(
    const std::string& userId,
    UserRole requiredRole
) const {
    auto user = getUser(userId);

    if (!user.has_value()) {
        return false;
    }

    auto required_role_def = getRoleDefinition(requiredRole);
    auto user_role_def = getRoleDefinition(user.value().role);

    if (!required_role_def.has_value() || !user_role_def.has_value()) {
        return false;
    }

    return user_role_def.value().hierarchyLevel >= required_role_def.value().hierarchyLevel;
}

void UserManagementSystem::defineRole(
    UserRole role,
    const std::string& roleName,
    const std::string& description,
    const std::set<std::string>& permissions_set,
    int hierarchyLevel
) {
    RoleDefinition roleDef;
    roleDef.role = role;
    roleDef.roleName = roleName;
    roleDef.description = description;
    roleDef.permissions = permissions_set;
    roleDef.hierarchyLevel = hierarchyLevel;

    roleDefinitions[role] = roleDef;
}

std::optional<RoleDefinition> UserManagementSystem::getRoleDefinition(UserRole role) const {
    auto it = roleDefinitions.find(role);

    if (it != roleDefinitions.end()) {
        return it->second;
    }

    return std::nullopt;
}

std::vector<RoleDefinition> UserManagementSystem::getAllRoleDefinitions() const {
    std::vector<RoleDefinition> result;

    for (const auto& pair : roleDefinitions) {
        result.push_back(pair.second);
    }

    return result;
}

bool UserManagementSystem::createTeam(
    const std::string& teamName,
    const std::string& owner,
    const std::string& description
) {
    if (teamName.empty() || owner.empty()) {
        return false;
    }

    Team team;
    team.teamId = "team_" + std::to_string(teams.size() + 1);
    team.teamName = teamName;
    team.owner = owner;
    team.description = description;
    team.createdAt = std::time(nullptr);
    team.members.push_back(owner);

    teams.push_back(team);
    return true;
}

bool UserManagementSystem::addTeamMember(
    const std::string& teamId,
    const std::string& userId
) {
    auto it = std::find_if(teams.begin(), teams.end(),
        [&teamId](const Team& t) { return t.teamId == teamId; });

    if (it != teams.end()) {
        auto member_it = std::find(it->members.begin(), it->members.end(), userId);

        if (member_it == it->members.end()) {
            it->members.push_back(userId);
            return true;
        }
    }

    return false;
}

bool UserManagementSystem::removeTeamMember(
    const std::string& teamId,
    const std::string& userId
) {
    auto it = std::find_if(teams.begin(), teams.end(),
        [&teamId](const Team& t) { return t.teamId == teamId; });

    if (it != teams.end()) {
        auto member_it = std::find(it->members.begin(), it->members.end(), userId);

        if (member_it != it->members.end()) {
            it->members.erase(member_it);
            return true;
        }
    }

    return false;
}

std::optional<UserManagementSystem::Team> UserManagementSystem::getTeam(
    const std::string& teamId
) const {
    auto it = std::find_if(teams.begin(), teams.end(),
        [&teamId](const Team& t) { return t.teamId == teamId; });

    if (it != teams.end()) {
        return *it;
    }

    return std::nullopt;
}

std::vector<UserManagementSystem::Team> UserManagementSystem::getUserTeams(
    const std::string& userId
) const {
    std::vector<Team> result;

    for (const auto& team : teams) {
        auto member_it = std::find(team.members.begin(), team.members.end(), userId);

        if (member_it != team.members.end()) {
            result.push_back(team);
        }
    }

    return result;
}

void UserManagementSystem::logAuditEntry(
    const std::string& userId,
    const std::string& action,
    const std::string& resourceId,
    const std::string& resourceType,
    bool success,
    const std::string& details
) {
    AuditEntry entry;
    entry.entryId = nextAuditEntryId++;
    entry.userId = userId;
    entry.action = action;
    entry.resourceId = resourceId;
    entry.resourceType = resourceType;
    entry.timestamp = std::time(nullptr);
    entry.details = details;
    entry.success = success;

    auditLog.push_back(entry);
}

std::vector<UserManagementSystem::AuditEntry> UserManagementSystem::getAuditLog(
    std::optional<std::string> userId,
    std::optional<std::string> action,
    int limit
) const {
    std::vector<AuditEntry> result;

    for (const auto& entry : auditLog) {
        if (userId.has_value() && entry.userId != userId.value()) {
            continue;
        }

        if (action.has_value() && entry.action != action.value()) {
            continue;
        }

        result.push_back(entry);
    }

    std::sort(result.begin(), result.end(),
        [](const AuditEntry& a, const AuditEntry& b) {
            return a.timestamp > b.timestamp;
        });

    if (limit > 0 && result.size() > static_cast<size_t>(limit)) {
        result.resize(limit);
    }

    return result;
}

bool UserManagementSystem::setUserPreference(
    const std::string& userId,
    const std::string& key,
    const std::string& value
) {
    auto user = getUser(userId);

    if (user.has_value()) {
        User updated = user.value();
        updated.preferences[key] = value;
        return updateUser(updated);
    }

    return false;
}

std::optional<std::string> UserManagementSystem::getUserPreference(
    const std::string& userId,
    const std::string& key
) const {
    auto user = getUser(userId);

    if (user.has_value()) {
        auto pref_it = user.value().preferences.find(key);

        if (pref_it != user.value().preferences.end()) {
            return pref_it->second;
        }
    }

    return std::nullopt;
}

UserManagementSystem::UserStatistics UserManagementSystem::getStatistics() const {
    UserStatistics stats;
    stats.totalUsers = users.size();

    for (const auto& user : users) {
        if (user.status == UserStatus::Active) {
            stats.activeUsers++;
        }

        if (user.role == UserRole::Admin) {
            stats.adminUsers++;
        } else if (user.role == UserRole::Editor) {
            stats.editorUsers++;
        } else if (user.role == UserRole::Viewer) {
            stats.viewerUsers++;
        }
    }

    stats.totalTeams = teams.size();

    if (!teams.empty()) {
        int totalMembers = 0;

        for (const auto& team : teams) {
            totalMembers += team.members.size();
        }

        stats.avgUsersPerTeam = totalMembers / teams.size();
    }

    return stats;
}

}  // namespace bendcore::cloud
