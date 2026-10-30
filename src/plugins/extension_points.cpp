#include "bendcore/plugins/extension_points.hpp"
#include <algorithm>
#include <mutex>

namespace bendcore::plugins {

ExtensionRegistry& ExtensionRegistry::instance() {
  static ExtensionRegistry registry;
  return registry;
}

bool ExtensionRegistry::register_extension_point(const ExtensionPoint& point) {
  std::unique_lock<std::shared_mutex> lock(registry_mutex_);
  if (extension_points_.find(point.id) != extension_points_.end()) {
    return false;
  }
  extension_points_[point.id] = point;
  return true;
}

bool ExtensionRegistry::unregister_extension_point(const std::string& point_id) {
  std::unique_lock<std::shared_mutex> lock(registry_mutex_);
  auto it = extension_points_.find(point_id);
  if (it == extension_points_.end()) return false;
  extension_points_.erase(it);
  return true;
}

std::optional<ExtensionPoint> ExtensionRegistry::get_extension_point(const std::string& point_id) const {
  std::shared_lock<std::shared_mutex> lock(registry_mutex_);
  auto it = extension_points_.find(point_id);
  if (it != extension_points_.end()) return it->second;
  return std::nullopt;
}

std::vector<ExtensionPoint> ExtensionRegistry::get_all_extension_points() const {
  std::shared_lock<std::shared_mutex> lock(registry_mutex_);
  std::vector<ExtensionPoint> result;
  for (const auto& pair : extension_points_) {
    result.push_back(pair.second);
  }
  return result;
}

bool ExtensionRegistry::has_extension_point(const std::string& point_id) const {
  std::shared_lock<std::shared_mutex> lock(registry_mutex_);
  return extension_points_.find(point_id) != extension_points_.end();
}

bool ExtensionRegistry::register_extension(const Extension& extension) {
  std::unique_lock<std::shared_mutex> lock(registry_mutex_);
  
  if (extension_points_.find(extension.extension_point_id) == extension_points_.end()) {
    return false; // Extension point must exist
  }
  
  if (extensions_.find(extension.id) != extensions_.end()) {
    return false; // Already registered
  }
  
  extensions_[extension.id] = extension;
  return true;
}

bool ExtensionRegistry::unregister_extension(const std::string& extension_id) {
  std::unique_lock<std::shared_mutex> lock(registry_mutex_);
  auto it = extensions_.find(extension_id);
  if (it == extensions_.end()) return false;
  extensions_.erase(it);
  return true;
}

std::optional<Extension> ExtensionRegistry::get_extension(const std::string& extension_id) const {
  std::shared_lock<std::shared_mutex> lock(registry_mutex_);
  auto it = extensions_.find(extension_id);
  if (it != extensions_.end()) return it->second;
  return std::nullopt;
}

std::vector<Extension> ExtensionRegistry::get_extensions_for_point(const std::string& point_id) const {
  std::shared_lock<std::shared_mutex> lock(registry_mutex_);
  std::vector<Extension> result;
  for (const auto& pair : extensions_) {
    if (pair.second.extension_point_id == point_id) {
      result.push_back(pair.second);
    }
  }
  return result;
}

std::vector<Extension> ExtensionRegistry::get_extensions_by_plugin(const std::string& plugin_id) const {
  std::shared_lock<std::shared_mutex> lock(registry_mutex_);
  std::vector<Extension> result;
  for (const auto& pair : extensions_) {
    if (pair.second.plugin_id == plugin_id) {
      result.push_back(pair.second);
    }
  }
  return result;
}

int ExtensionRegistry::get_extension_count(const std::string& point_id) const {
  std::shared_lock<std::shared_mutex> lock(registry_mutex_);
  int count = 0;
  for (const auto& pair : extensions_) {
    if (pair.second.extension_point_id == point_id) {
      count++;
    }
  }
  return count;
}

void ExtensionRegistry::enable_extension(const std::string& extension_id) {
  std::unique_lock<std::shared_mutex> lock(registry_mutex_);
  auto it = extensions_.find(extension_id);
  if (it != extensions_.end()) {
    it->second.is_enabled = true;
  }
}

void ExtensionRegistry::disable_extension(const std::string& extension_id) {
  std::unique_lock<std::shared_mutex> lock(registry_mutex_);
  auto it = extensions_.find(extension_id);
  if (it != extensions_.end()) {
    it->second.is_enabled = false;
  }
}

bool ExtensionRegistry::is_extension_enabled(const std::string& extension_id) const {
  std::shared_lock<std::shared_mutex> lock(registry_mutex_);
  auto it = extensions_.find(extension_id);
  if (it != extensions_.end()) return it->second.is_enabled;
  return false;
}

std::vector<Extension> ExtensionRegistry::get_sorted_extensions(const std::string& point_id) const {
  std::shared_lock<std::shared_mutex> lock(registry_mutex_);
  std::vector<Extension> result;
  for (const auto& pair : extensions_) {
    if (pair.second.extension_point_id == point_id && pair.second.is_enabled) {
      result.push_back(pair.second);
    }
  }
  std::sort(result.begin(), result.end(), 
            [](const Extension& a, const Extension& b) { return a.priority > b.priority; });
  return result;
}

void ExtensionRegistry::clear() {
  std::unique_lock<std::shared_mutex> lock(registry_mutex_);
  extension_points_.clear();
  extensions_.clear();
}

} // namespace bendcore::plugins
