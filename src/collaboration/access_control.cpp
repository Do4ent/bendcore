#include "bendcore/collaboration/access_control.hpp"
#include <uuid/uuid.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>

namespace bendcore::collaboration {

AccessControl::AccessControl() = default;
AccessControl::~AccessControl() = default;

Role AccessControl::create_role(const std::string& name, int permissions, const std::string& description) {
  Role role;
  role.role_id = generate_role_id();
  role.role_name = name;
  role.permission_bits = permissions;
  role.description = description;

  roles_[role.role_id] = role;
  return role;
}

std::optional<Role> AccessControl::get_role(const std::string& role_id) const {
  auto it = roles_.find(role_id);
  if (it != roles_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<Role> AccessControl::get_all_roles() const {
  std::vector<Role> result;
  for (const auto& pair : roles_) {
    result.push_back(pair.second);
  }
  return result;
}

bool AccessControl::update_role(const std::string& role_id, int permissions) {
  auto it = roles_.find(role_id);
  if (it == roles_.end()) {
    return false;
  }
  it->second.permission_bits = permissions;
  return true;
}

bool AccessControl::delete_role(const std::string& role_id) {
  return roles_.erase(role_id) > 0;
}

bool AccessControl::grant_permission(const std::string& user_id, const std::string& resource_id, int permission) {
  std::string key = user_id + ":" + resource_id;
  user_permissions_[key].user_id = user_id;
  user_permissions_[key].resource_id = resource_id;
  user_permissions_[key].permission_bits |= permission;
  resource_permissions_[resource_id][user_id] |= permission;
  return true;
}

bool AccessControl::revoke_permission(const std::string& user_id, const std::string& resource_id, int permission) {
  std::string key = user_id + ":" + resource_id;
  auto it = user_permissions_.find(key);
  if (it == user_permissions_.end()) {
    return false;
  }
  it->second.permission_bits &= ~permission;
  resource_permissions_[resource_id][user_id] &= ~permission;
  return true;
}

bool AccessControl::set_user_role(const std::string& user_id, const std::string& resource_id, const std::string& role_id) {
  auto role = get_role(role_id);
  if (!role) {
    return false;
  }

  std::string key = user_id + ":" + resource_id;
  user_permissions_[key].user_id = user_id;
  user_permissions_[key].resource_id = resource_id;
  user_permissions_[key].role = role_id;
  user_permissions_[key].permission_bits = role->permission_bits;
  resource_permissions_[resource_id][user_id] = role->permission_bits;

  return true;
}

bool AccessControl::has_permission(const std::string& user_id, const std::string& resource_id, Permission perm) const {
  std::string key = user_id + ":" + resource_id;
  auto it = user_permissions_.find(key);
  if (it != user_permissions_.end()) {
    return (it->second.permission_bits & static_cast<int>(perm)) != 0;
  }

  // Check inherited permissions
  auto parent_it = permission_inheritance_.find(resource_id);
  if (parent_it != permission_inheritance_.end()) {
    return has_permission(user_id, parent_it->second, perm);
  }

  return false;
}

bool AccessControl::can_read(const std::string& user_id, const std::string& resource_id) const {
  return has_permission(user_id, resource_id, Permission::READ);
}

bool AccessControl::can_write(const std::string& user_id, const std::string& resource_id) const {
  return has_permission(user_id, resource_id, Permission::WRITE);
}

bool AccessControl::can_delete(const std::string& user_id, const std::string& resource_id) const {
  return has_permission(user_id, resource_id, Permission::DELETE);
}

bool AccessControl::is_admin(const std::string& user_id, const std::string& resource_id) const {
  return has_permission(user_id, resource_id, Permission::ADMIN);
}

int AccessControl::get_user_permissions(const std::string& user_id, const std::string& resource_id) const {
  std::string key = user_id + ":" + resource_id;
  auto it = user_permissions_.find(key);
  if (it != user_permissions_.end()) {
    return it->second.permission_bits;
  }
  return 0;
}

std::vector<std::string> AccessControl::get_user_accessible_resources(const std::string& user_id) const {
  std::set<std::string> resources;
  for (const auto& pair : user_permissions_) {
    if (pair.second.user_id == user_id && pair.second.permission_bits > 0) {
      resources.insert(pair.second.resource_id);
    }
  }
  return std::vector<std::string>(resources.begin(), resources.end());
}

std::vector<std::string> AccessControl::get_resource_users(const std::string& resource_id) const {
  std::set<std::string> users;
  auto it = resource_permissions_.find(resource_id);
  if (it != resource_permissions_.end()) {
    for (const auto& pair : it->second) {
      if (pair.second > 0) {
        users.insert(pair.first);
      }
    }
  }
  return std::vector<std::string>(users.begin(), users.end());
}

bool AccessControl::inherit_permission(const std::string& child_id, const std::string& parent_id, const std::string& permission_type) {
  permission_inheritance_[child_id] = parent_id;
  return true;
}

std::optional<std::string> AccessControl::get_parent_permission(const std::string& resource_id) const {
  auto it = permission_inheritance_.find(resource_id);
  if (it != permission_inheritance_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void AccessControl::clear_permissions() {
  roles_.clear();
  user_permissions_.clear();
  resource_permissions_.clear();
  permission_inheritance_.clear();
  audit_log_.clear();
}

void AccessControl::audit_access(const std::string& user_id, const std::string& resource_id, const std::string& action) {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << "[" << time_t << "] user=" << user_id << " resource=" << resource_id << " action=" << action;
  audit_log_.push_back(ss.str());
}

std::vector<std::string> AccessControl::get_audit_log() const {
  return audit_log_;
}

std::string AccessControl::generate_role_id() const {
  uuid_t uuid;
  uuid_generate(uuid);
  std::stringstream ss;
  for (int i = 0; i < 16; ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(uuid[i]);
  }
  return "role_" + ss.str();
}

} // namespace bendcore::collaboration
