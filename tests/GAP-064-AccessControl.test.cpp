// GAP-064: AccessControl (33 tests) - Permissions, roles, audit
#include <cassert>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <set>

namespace bendcore::collaboration {

enum class Role { VIEWER, EDITOR, OWNER };
enum class Permission { READ, WRITE, DELETE, ADMIN };

struct AccessGrant {
    int user_id;
    Role role;
    std::set<Permission> permissions;
};

struct AuditLog {
    int log_id;
    int user_id;
    std::string action;
    int entity_id;
    long timestamp;
    bool success;
};

class AccessControl {
public:
    void grantAccess(int user_id, int project_id, Role role) {
        AccessGrant grant = {user_id, role, getPermissionsForRole(role)};
        project_access_[project_id][user_id] = grant;
    }
    
    bool hasPermission(int user_id, int project_id, Permission perm) {
        auto it = project_access_.find(project_id);
        if (it == project_access_.end()) return false;
        
        auto it2 = it->second.find(user_id);
        if (it2 == it->second.end()) return false;
        
        return it2->second.permissions.find(perm) != it2->second.permissions.end();
    }
    
    Role getUserRole(int user_id, int project_id) {
        auto it = project_access_.find(project_id);
        if (it == project_access_.end()) return Role::VIEWER;
        
        auto it2 = it->second.find(user_id);
        if (it2 == it->second.end()) return Role::VIEWER;
        
        return it2->second.role;
    }
    
    void revokeAccess(int user_id, int project_id) {
        auto it = project_access_.find(project_id);
        if (it != project_access_.end()) {
            it->second.erase(user_id);
        }
    }
    
    size_t getProjectUserCount(int project_id) {
        auto it = project_access_.find(project_id);
        if (it == project_access_.end()) return 0;
        return it->second.size();
    }
    
    int logAuditEvent(int user_id, const std::string& action, int entity_id, bool success) {
        int lid = next_log_id_++;
        audit_logs_.push_back({lid, user_id, action, entity_id, 0, success});
        return lid;
    }
    
    AuditLog* getAuditLog(int id) {
        for (auto& log : audit_logs_) {
            if (log.log_id == id) return &log;
        }
        return nullptr;
    }
    
    size_t getAuditLogCount() const { return audit_logs_.size(); }
    
    size_t getFailedActionCount() const {
        int count = 0;
        for (const auto& log : audit_logs_) {
            if (!log.success) count++;
        }
        return count;
    }
    
private:
    std::map<int, std::map<int, AccessGrant>> project_access_;  // project_id -> user_id -> grant
    std::vector<AuditLog> audit_logs_;
    int next_log_id_ = 1;
    
    std::set<Permission> getPermissionsForRole(Role role) {
        switch(role) {
            case Role::VIEWER:
                return {Permission::READ};
            case Role::EDITOR:
                return {Permission::READ, Permission::WRITE};
            case Role::OWNER:
                return {Permission::READ, Permission::WRITE, Permission::DELETE, Permission::ADMIN};
            default:
                return {};
        }
    }
};

} // namespace bendcore::collaboration

using namespace bendcore::collaboration;

int main() {
    int passed = 0, failed = 0;
    
    #define TEST(name, cond) { \
        if (cond) { passed++; } else { \
            printf("FAIL: %s\n", name); failed++; \
        } \
    }
    
    // Test 1-5: Access granting
    {
        AccessControl ac;
        
        ac.grantAccess(1, 1, Role::EDITOR);
        TEST("T1: Access granted", ac.hasPermission(1, 1, Permission::READ));
        TEST("T2: Read permission", ac.hasPermission(1, 1, Permission::READ));
        TEST("T3: Write permission", ac.hasPermission(1, 1, Permission::WRITE));
        TEST("T4: Role set to EDITOR", ac.getUserRole(1, 1) == Role::EDITOR);
        TEST("T5: Project has user", ac.getProjectUserCount(1) == 1);
    }
    
    // Test 6-10: Role-based permissions
    {
        AccessControl ac;
        
        ac.grantAccess(1, 1, Role::VIEWER);
        ac.grantAccess(2, 1, Role::EDITOR);
        ac.grantAccess(3, 1, Role::OWNER);
        
        TEST("T6: Viewer can read", ac.hasPermission(1, 1, Permission::READ));
        TEST("T7: Viewer cannot write", !ac.hasPermission(1, 1, Permission::WRITE));
        
        TEST("T8: Editor can read", ac.hasPermission(2, 1, Permission::READ));
        TEST("T9: Editor can write", ac.hasPermission(2, 1, Permission::WRITE));
        TEST("T10: Editor cannot delete", !ac.hasPermission(2, 1, Permission::DELETE));
    }
    
    // Test 11-15: Owner permissions
    {
        AccessControl ac;
        
        ac.grantAccess(1, 1, Role::OWNER);
        
        TEST("T11: Owner can read", ac.hasPermission(1, 1, Permission::READ));
        TEST("T12: Owner can write", ac.hasPermission(1, 1, Permission::WRITE));
        TEST("T13: Owner can delete", ac.hasPermission(1, 1, Permission::DELETE));
        TEST("T14: Owner can admin", ac.hasPermission(1, 1, Permission::ADMIN));
        TEST("T15: Role is OWNER", ac.getUserRole(1, 1) == Role::OWNER);
    }
    
    // Test 16-20: Multiple users in project
    {
        AccessControl ac;
        
        ac.grantAccess(1, 1, Role::OWNER);
        ac.grantAccess(2, 1, Role::EDITOR);
        ac.grantAccess(3, 1, Role::VIEWER);
        ac.grantAccess(4, 1, Role::EDITOR);
        
        TEST("T16: Four users", ac.getProjectUserCount(1) == 4);
        
        TEST("T17: User 1 is owner", ac.getUserRole(1, 1) == Role::OWNER);
        TEST("T18: User 2 is editor", ac.getUserRole(2, 1) == Role::EDITOR);
        TEST("T19: User 3 is viewer", ac.getUserRole(3, 1) == Role::VIEWER);
        TEST("T20: User 4 is editor", ac.getUserRole(4, 1) == Role::EDITOR);
    }
    
    // Test 21-25: Access revocation
    {
        AccessControl ac;
        
        ac.grantAccess(1, 1, Role::EDITOR);
        TEST("T21: User has access", ac.hasPermission(1, 1, Permission::READ));
        TEST("T22: One user in project", ac.getProjectUserCount(1) == 1);
        
        ac.revokeAccess(1, 1);
        TEST("T23: User no longer has read", !ac.hasPermission(1, 1, Permission::READ));
        TEST("T24: No users in project", ac.getProjectUserCount(1) == 0);
        
        ac.grantAccess(1, 1, Role::VIEWER);
        TEST("T25: Can regrant access", ac.hasPermission(1, 1, Permission::READ));
    }
    
    // Test 26-30: Audit logging
    {
        AccessControl ac;
        
        int lid1 = ac.logAuditEvent(1, "read_entity", 5, true);
        TEST("T26: Audit logged", lid1 > 0);
        
        auto* log = ac.getAuditLog(lid1);
        TEST("T27: Log retrieved", log != nullptr);
        TEST("T28: User ID correct", log->user_id == 1);
        TEST("T29: Action correct", log->action == "read_entity");
        TEST("T30: Success flag set", log->success);
    }
    
    // Test 31-33: Audit trail
    {
        AccessControl ac;
        
        ac.logAuditEvent(1, "read", 1, true);
        ac.logAuditEvent(2, "write", 2, true);
        ac.logAuditEvent(3, "delete", 3, false);
        ac.logAuditEvent(1, "admin", 4, true);
        ac.logAuditEvent(2, "read", 5, false);
        
        TEST("T31: Five audit logs", ac.getAuditLogCount() == 5);
        TEST("T32: Two failed actions", ac.getFailedActionCount() == 2);
        TEST("T33: Audit logging working", ac.getAuditLogCount() > 0);
    }
    
    printf("\n=== GAP-064: AccessControl ===\n");
    printf("Passed: %d/33\n", passed);
    printf("Failed: %d/33\n", failed);
    return (failed == 0) ? 0 : 1;
}
