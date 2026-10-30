#include "bendcore/plugins/plugin_api.hpp"
#include <algorithm>
#include <chrono>
#include <mutex>

namespace bendcore::plugins {

// PluginRegistry implementation
PluginRegistry& PluginRegistry::instance() {
  static PluginRegistry registry;
  return registry;
}

bool PluginRegistry::register_plugin(std::shared_ptr<IPlugin> plugin) {
  if (!plugin) return false;
  
  auto metadata = plugin->get_metadata();
  if (metadata.id.empty()) return false;
  
  std::unique_lock<std::shared_mutex> lock(registry_mutex_);
  
  if (plugins_.find(metadata.id) != plugins_.end()) {
    return false; // Plugin already registered
  }
  
  plugins_[metadata.id] = plugin;
  
  // Emit registration event
  PluginEvent event{
    "plugin.registered",
    metadata.id,
    {{"name", metadata.name}, {"version", metadata.version}},
    std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()
  };
  
  for (auto& callback : event_callbacks_) {
    callback(event);
  }
  
  return true;
}

bool PluginRegistry::unregister_plugin(const std::string& plugin_id) {
  std::unique_lock<std::shared_mutex> lock(registry_mutex_);
  
  auto it = plugins_.find(plugin_id);
  if (it == plugins_.end()) {
    return false;
  }
  
  plugins_.erase(it);
  
  // Emit unregistration event
  PluginEvent event{
    "plugin.unregistered",
    plugin_id,
    {},
    std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()
  };
  
  for (auto& callback : event_callbacks_) {
    callback(event);
  }
  
  return true;
}

std::shared_ptr<IPlugin> PluginRegistry::get_plugin(const std::string& plugin_id) const {
  std::shared_lock<std::shared_mutex> lock(registry_mutex_);
  
  auto it = plugins_.find(plugin_id);
  if (it != plugins_.end()) {
    return it->second;
  }
  return nullptr;
}

std::vector<std::shared_ptr<IPlugin>> PluginRegistry::get_all_plugins() const {
  std::shared_lock<std::shared_mutex> lock(registry_mutex_);
  
  std::vector<std::shared_ptr<IPlugin>> result;
  for (const auto& pair : plugins_) {
    result.push_back(pair.second);
  }
  return result;
}

std::vector<std::shared_ptr<IPlugin>> PluginRegistry::get_plugins_by_capability(
    const std::string& capability_name) const {
  std::shared_lock<std::shared_mutex> lock(registry_mutex_);
  
  std::vector<std::shared_ptr<IPlugin>> result;
  for (const auto& pair : plugins_) {
    auto capabilities = pair.second->get_capabilities();
    for (const auto& cap : capabilities) {
      if (cap.name == capability_name) {
        result.push_back(pair.second);
        break;
      }
    }
  }
  return result;
}

size_t PluginRegistry::get_plugin_count() const {
  std::shared_lock<std::shared_mutex> lock(registry_mutex_);
  return plugins_.size();
}

bool PluginRegistry::has_plugin(const std::string& plugin_id) const {
  std::shared_lock<std::shared_mutex> lock(registry_mutex_);
  return plugins_.find(plugin_id) != plugins_.end();
}

std::vector<std::shared_ptr<IPlugin>> PluginRegistry::get_plugins_by_state(PluginState state) const {
  std::shared_lock<std::shared_mutex> lock(registry_mutex_);
  
  std::vector<std::shared_ptr<IPlugin>> result;
  for (const auto& pair : plugins_) {
    if (pair.second->get_state() == state) {
      result.push_back(pair.second);
    }
  }
  return result;
}

int PluginRegistry::activate_all() {
  std::shared_lock<std::shared_mutex> lock(registry_mutex_);
  
  int activated = 0;
  for (auto& pair : plugins_) {
    if (pair.second->activate()) {
      activated++;
    }
  }
  return activated;
}

int PluginRegistry::deactivate_all() {
  std::shared_lock<std::shared_mutex> lock(registry_mutex_);
  
  int deactivated = 0;
  for (auto& pair : plugins_) {
    if (pair.second->deactivate()) {
      deactivated++;
    }
  }
  return deactivated;
}

void PluginRegistry::register_event_callback(PluginEventCallback callback) {
  std::unique_lock<std::shared_mutex> lock(registry_mutex_);
  event_callbacks_.push_back(callback);
}

void PluginRegistry::emit_event(const PluginEvent& event) {
  std::shared_lock<std::shared_mutex> lock(registry_mutex_);
  for (auto& callback : event_callbacks_) {
    callback(event);
  }
}

void PluginRegistry::clear() {
  std::unique_lock<std::shared_mutex> lock(registry_mutex_);
  plugins_.clear();
  event_callbacks_.clear();
}

} // namespace bendcore::plugins
