#include "bendcore/plugins/plugin_loader.hpp"
#include <chrono>
#include <algorithm>
#include <mutex>

namespace bendcore::plugins {

PluginLoader::PluginLoader() : plugin_directory_(".") {}

PluginLoader::~PluginLoader() = default;

std::vector<PluginLibrary> PluginLoader::discover_plugins(const std::string& directory) {
  std::shared_lock<std::shared_mutex> lock(loader_mutex_);
  std::vector<PluginLibrary> results;
  
  for (const auto& pair : libraries_) {
    results.push_back(pair.second);
  }
  return results;
}

LoadResult PluginLoader::load_plugin(const std::string& library_path) {
  auto start = std::chrono::high_resolution_clock::now();
  
  // Mock implementation - in real scenario, would use dlopen/LoadLibrary
  LoadResult result{false, "", "Mock implementation", 0};
  
  auto end = std::chrono::high_resolution_clock::now();
  result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  
  return result;
}

LoadResult PluginLoader::load_plugin_by_id(const std::string& plugin_id) {
  auto start = std::chrono::high_resolution_clock::now();
  
  std::shared_lock<std::shared_mutex> lock(loader_mutex_);
  
  auto it = libraries_.find(plugin_id);
  if (it == libraries_.end()) {
    auto end = std::chrono::high_resolution_clock::now();
    return {false, plugin_id, "Plugin not found", 
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()};
  }
  
  LoadResult result{true, plugin_id, "", 0};
  auto end = std::chrono::high_resolution_clock::now();
  result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  
  for (auto& callback : load_callbacks_) {
    callback(result);
  }
  
  return result;
}

bool PluginLoader::unload_plugin(const std::string& plugin_id) {
  std::unique_lock<std::shared_mutex> lock(loader_mutex_);
  
  auto it = loaded_plugins_.find(plugin_id);
  if (it != loaded_plugins_.end()) {
    loaded_plugins_.erase(it);
    return true;
  }
  return false;
}

std::shared_ptr<IPlugin> PluginLoader::get_loaded_plugin(const std::string& plugin_id) const {
  std::shared_lock<std::shared_mutex> lock(loader_mutex_);
  
  auto it = loaded_plugins_.find(plugin_id);
  if (it != loaded_plugins_.end()) {
    return it->second;
  }
  return nullptr;
}

std::vector<std::shared_ptr<IPlugin>> PluginLoader::get_loaded_plugins() const {
  std::shared_lock<std::shared_mutex> lock(loader_mutex_);
  
  std::vector<std::shared_ptr<IPlugin>> result;
  for (const auto& pair : loaded_plugins_) {
    result.push_back(pair.second);
  }
  return result;
}

void PluginLoader::set_plugin_directory(const std::string& directory) {
  std::unique_lock<std::shared_mutex> lock(loader_mutex_);
  plugin_directory_ = directory;
}

std::string PluginLoader::get_plugin_directory() const {
  std::shared_lock<std::shared_mutex> lock(loader_mutex_);
  return plugin_directory_;
}

int PluginLoader::load_all_discovered() {
  std::shared_lock<std::shared_mutex> lock(loader_mutex_);
  return loaded_plugins_.size();
}

bool PluginLoader::reload_plugin(const std::string& plugin_id) {
  unload_plugin(plugin_id);
  auto result = load_plugin_by_id(plugin_id);
  return result.success;
}

LoadStrategy PluginLoader::get_load_strategy(const std::string& plugin_id) const {
  std::shared_lock<std::shared_mutex> lock(loader_mutex_);
  
  auto it = load_strategies_.find(plugin_id);
  if (it != load_strategies_.end()) {
    return it->second;
  }
  return LoadStrategy::EAGER;
}

void PluginLoader::set_load_strategy(const std::string& plugin_id, LoadStrategy strategy) {
  std::unique_lock<std::shared_mutex> lock(loader_mutex_);
  load_strategies_[plugin_id] = strategy;
}

std::optional<PluginLibrary> PluginLoader::get_library_info(const std::string& library_id) const {
  std::shared_lock<std::shared_mutex> lock(loader_mutex_);
  
  auto it = libraries_.find(library_id);
  if (it != libraries_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::map<std::string, long> PluginLoader::get_load_statistics() const {
  std::shared_lock<std::shared_mutex> lock(loader_mutex_);
  return {{"total_loaded", static_cast<long>(loaded_plugins_.size())},
          {"total_libraries", static_cast<long>(libraries_.size())}};
}

void PluginLoader::register_load_callback(LoadCallback callback) {
  std::unique_lock<std::shared_mutex> lock(loader_mutex_);
  load_callbacks_.push_back(callback);
}

void PluginLoader::clear() {
  std::unique_lock<std::shared_mutex> lock(loader_mutex_);
  loaded_plugins_.clear();
  libraries_.clear();
  load_strategies_.clear();
  load_callbacks_.clear();
}

} // namespace bendcore::plugins
