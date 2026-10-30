#include <gtest/gtest.h>
#include "bendcore/collaboration/access_control.hpp"

using namespace bendcore::collaboration;

class AccessControlTest : public ::testing::Test {
 protected:
  AccessControl ac;
  void SetUp() override { ac.clear_permissions(); }
};

TEST_F(AccessControlTest, CreateRole) {
  auto role = ac.create_role("Editor", 6, "Can read and write");
  EXPECT_EQ(role.role_name, "Editor");
  EXPECT_EQ(role.permission_bits, 6);
}

TEST_F(AccessControlTest, GetRole) {
  auto created = ac.create_role("Editor", 6, "");
  auto role = ac.get_role(created.role_id);
  EXPECT_TRUE(role.has_value());
  EXPECT_EQ(role->role_name, "Editor");
}

TEST_F(AccessControlTest, GetAllRoles) {
  ac.create_role("Editor", 6, "");
  ac.create_role("Viewer", 1, "");
  auto roles = ac.get_all_roles();
  EXPECT_EQ(roles.size(), 2);
}

TEST_F(AccessControlTest, UpdateRole) {
  auto created = ac.create_role("Editor", 6, "");
  bool updated = ac.update_role(created.role_id, 14);
  EXPECT_TRUE(updated);
}

TEST_F(AccessControlTest, DeleteRole) {
  auto created = ac.create_role("Editor", 6, "");
  bool deleted = ac.delete_role(created.role_id);
  EXPECT_TRUE(deleted);
}

TEST_F(AccessControlTest, GrantPermission) {
  bool granted = ac.grant_permission("user1", "doc1", static_cast<int>(Permission::READ));
  EXPECT_TRUE(granted);
  EXPECT_TRUE(ac.can_read("user1", "doc1"));
}

TEST_F(AccessControlTest, RevokePermission) {
  ac.grant_permission("user1", "doc1", static_cast<int>(Permission::READ));
  bool revoked = ac.revoke_permission("user1", "doc1", static_cast<int>(Permission::READ));
  EXPECT_TRUE(revoked);
  EXPECT_FALSE(ac.can_read("user1", "doc1"));
}

TEST_F(AccessControlTest, SetUserRole) {
  auto role = ac.create_role("Editor", static_cast<int>(Permission::READ) | static_cast<int>(Permission::WRITE), "");
  bool set = ac.set_user_role("user1", "doc1", role.role_id);
  EXPECT_TRUE(set);
}

TEST_F(AccessControlTest, CanRead) {
  ac.grant_permission("user1", "doc1", static_cast<int>(Permission::READ));
  EXPECT_TRUE(ac.can_read("user1", "doc1"));
}

TEST_F(AccessControlTest, CanWrite) {
  ac.grant_permission("user1", "doc1", static_cast<int>(Permission::WRITE));
  EXPECT_TRUE(ac.can_write("user1", "doc1"));
}

TEST_F(AccessControlTest, CanDelete) {
  ac.grant_permission("user1", "doc1", static_cast<int>(Permission::DELETE));
  EXPECT_TRUE(ac.can_delete("user1", "doc1"));
}

TEST_F(AccessControlTest, IsAdmin) {
  ac.grant_permission("user1", "doc1", static_cast<int>(Permission::ADMIN));
  EXPECT_TRUE(ac.is_admin("user1", "doc1"));
}

TEST_F(AccessControlTest, NoPermissionByDefault) {
  EXPECT_FALSE(ac.can_read("user1", "doc1"));
  EXPECT_FALSE(ac.can_write("user1", "doc1"));
}

TEST_F(AccessControlTest, GetUserPermissions) {
  int perms = static_cast<int>(Permission::READ) | static_cast<int>(Permission::WRITE);
  ac.grant_permission("user1", "doc1", perms);
  int result = ac.get_user_permissions("user1", "doc1");
  EXPECT_EQ(result, perms);
}

TEST_F(AccessControlTest, GetUserAccessibleResources) {
  ac.grant_permission("user1", "doc1", static_cast<int>(Permission::READ));
  ac.grant_permission("user1", "doc2", static_cast<int>(Permission::READ));
  auto resources = ac.get_user_accessible_resources("user1");
  EXPECT_EQ(resources.size(), 2);
}

TEST_F(AccessControlTest, GetResourceUsers) {
  ac.grant_permission("user1", "doc1", static_cast<int>(Permission::READ));
  ac.grant_permission("user2", "doc1", static_cast<int>(Permission::READ));
  auto users = ac.get_resource_users("doc1");
  EXPECT_EQ(users.size(), 2);
}

TEST_F(AccessControlTest, InheritPermission) {
  ac.grant_permission("user1", "parent", static_cast<int>(Permission::READ));
  bool inherited = ac.inherit_permission("child", "parent", "read");
  EXPECT_TRUE(inherited);
}

TEST_F(AccessControlTest, GetParentPermission) {
  ac.inherit_permission("child", "parent", "read");
  auto parent = ac.get_parent_permission("child");
  EXPECT_TRUE(parent.has_value());
  EXPECT_EQ(parent.value(), "parent");
}

TEST_F(AccessControlTest, ClearPermissions) {
  ac.grant_permission("user1", "doc1", static_cast<int>(Permission::READ));
  ac.clear_permissions();
  EXPECT_FALSE(ac.can_read("user1", "doc1"));
}

TEST_F(AccessControlTest, AuditAccess) {
  ac.audit_access("user1", "doc1", "read");
  auto log = ac.get_audit_log();
  EXPECT_EQ(log.size(), 1);
}

TEST_F(AccessControlTest, MultiplePermissions) {
  int perms = static_cast<int>(Permission::READ) | static_cast<int>(Permission::WRITE) | static_cast<int>(Permission::ADMIN);
  ac.grant_permission("user1", "doc1", perms);

  EXPECT_TRUE(ac.can_read("user1", "doc1"));
  EXPECT_TRUE(ac.can_write("user1", "doc1"));
  EXPECT_TRUE(ac.is_admin("user1", "doc1"));
}

TEST_F(AccessControlTest, PermissionCombination) {
  ac.grant_permission("user1", "doc1", static_cast<int>(Permission::READ));
  ac.grant_permission("user1", "doc1", static_cast<int>(Permission::WRITE));

  EXPECT_TRUE(ac.can_read("user1", "doc1"));
  EXPECT_TRUE(ac.can_write("user1", "doc1"));
}

TEST_F(AccessControlTest, RolePermissions) {
  auto role = ac.create_role("Viewer", static_cast<int>(Permission::READ), "");
  ac.set_user_role("user1", "doc1", role.role_id);

  EXPECT_TRUE(ac.can_read("user1", "doc1"));
  EXPECT_FALSE(ac.can_write("user1", "doc1"));
}

TEST_F(AccessControlTest, NoAccessNoResource) {
  auto users = ac.get_resource_users("nonexistent");
  EXPECT_EQ(users.size(), 0);
}

TEST_F(AccessControlTest, EmptyAuditLog) {
  auto log = ac.get_audit_log();
  EXPECT_EQ(log.size(), 0);
}

TEST_F(AccessControlTest, AuditLogMultiple) {
  ac.audit_access("user1", "doc1", "read");
  ac.audit_access("user2", "doc2", "write");
  auto log = ac.get_audit_log();
  EXPECT_EQ(log.size(), 2);
}

TEST_F(AccessControlTest, RoleUpdate) {
  auto role = ac.create_role("Editor", static_cast<int>(Permission::READ), "");
  ac.update_role(role.role_id, static_cast<int>(Permission::READ) | static_cast<int>(Permission::WRITE));
  auto updated = ac.get_role(role.role_id);
  EXPECT_EQ(updated->permission_bits, static_cast<int>(Permission::READ) | static_cast<int>(Permission::WRITE));
}

TEST_F(AccessControlTest, NoRoleById) {
  auto role = ac.get_role("nonexistent");
  EXPECT_FALSE(role.has_value());
}

TEST_F(AccessControlTest, DeleteNonExistentRole) {
  bool deleted = ac.delete_role("nonexistent");
  EXPECT_FALSE(deleted);
}

TEST_F(AccessControlTest, AdminPermission) {
  auto perms = static_cast<int>(Permission::ADMIN);
  ac.grant_permission("user1", "doc1", perms);

  EXPECT_TRUE(ac.is_admin("user1", "doc1"));
}
