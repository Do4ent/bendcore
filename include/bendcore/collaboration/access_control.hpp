#ifndef BENDCORE_COLLABORATION_ACCESS_CONTROL_HPP
#define BENDCORE_COLLABORATION_ACCESS_CONTROL_HPP

#include <string>
#include <vector>
#include <set>
#include <map>
#include <memory>
#include <optional>

namespace bendcore::collaboration {

enum class Permission {
  NONE = 0,
  READ = 1,
  WRITE = 2,
  ADMIN = 4,
  DELETE = 8
};

struct Role {
  std::string role_id;
  std::string role_name;
  int permission_bits = 0;
  std::string description;
};

struct UserPermission {
  std::string user_id;
  std::string resource_id;
  int permission_bits = 0;
  std::string role;
};

class AccessControl {
 public:
  AccessControl();
  ~AccessControl();

  Role create_role(const std::string& name, int permissions, const std::string& description);
  std::optional<Role> get_role(const std::string& role_id) const;
  std::vector<Role> get_all_roles() const;
  bool update_role(const std::string& role_id, int permissions);
  bool delete_role(const std::string& role_id);

  bool grant_permission(const std::string& user_id, const std::string& resource_id, int permission);
  bool revoke_permission(const std::string& user_id, const std::string& resource_id, int permission);
  bool set_user_role(const std::string& user_id, const std::string& resource_id, const std::string& role_id);

  bool has_permission(const std::string& user_id, const std::string& resource_id, Permission perm) const;
  bool can_read(const std::string& user_id, const std::string& resource_id) const;
  bool can_write(const std::string& user_id, const std::string& resource_id) const;
  bool can_delete(const std::string& user_id, const std::string& resource_id) const;
  bool is_admin(const std::string& user_id, const std::string& resource_id) const;

  int get_user_permissions(const std::string& user_id, const std::string& resource_id) const;
  std::vector<std::string> get_user_accessible_resources(const std::string& user_id) const;
  std::vector<std::string> get_resource_users(const std::string& resource_id) const;

  bool inherit_permission(const std::string& child_id, const std::string& parent_id, const std::string& permission_type);
  std::optional<std::string> get_parent_permission(const std::string& resource_id) const;

  void clear_permissions();
  void audit_access(const std::string& user_id, const std::string& resource_id, const std::string& action);
  std::vector<std::string> get_audit_log() const;

 private:
  std::map<std::string, Role> roles_;
  std::map<std::string, UserPermission> user_permissions_;
  std::map<std::string, std::map<std::string, int>> resource_permissions_;
  std::map<std::string, std::string> permission_inheritance_;
  std::vector<std::string> audit_log_;

  std::string generate_role_id() const;
};

} // namespace bendcore::collaboration

#endif // BENDCORE_COLLABORATION_ACCESS_CONTROL_HPP
