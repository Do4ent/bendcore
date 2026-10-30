#ifndef BENDCORE_PLUGINS_PLUGIN_LOADER_HPP
#define BENDCORE_PLUGINS_PLUGIN_LOADER_HPP

#include "plugin_api.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <optional>
#include <shared_mutex>

namespace bendcore::plugins {

/**
 * Plugin load strategy
 */
enum class LoadStrategy {
  EAGER,      // Load immediately upon discovery
  LAZY,       // Load on first use
  MANUAL,     // Require explicit load call
  CONDITIONAL // Load based on runtime conditions
};

/**
 * Plugin library metadata
 */
struct PluginLibrary {
  std::string id;
  std::string path;
  std::string version;
  long load_time_ms = 0;
  bool is_loaded = false;
  std::vector<std::string> provided_plugins;
};

/**
 * Plugin load result
 */
struct LoadResult {
  bool success;
  std::string plugin_id;
  std::string error_message;
  long duration_ms;
};

/**
 * Plugin loader - Manages dynamic loading of plugins
 * 
 * Handles discovery, loading, and lifecycle management of plugins
 * from various sources (files, packages, etc.)
 */
class PluginLoader {
 public:
  /**
   * Create plugin loader instance
   */
  PluginLoader();

  /**
   * Destructor
   */
  ~PluginLoader();

  /**
   * Discover plugins in directory
   * @param directory Path to search for plugins
   * @return Vector of discovered plugin libraries
   */
  std::vector<PluginLibrary> discover_plugins(const std::string& directory);

  /**
   * Load plugin from path
   * @param library_path Path to plugin library
   * @return LoadResult with success status
   */
  LoadResult load_plugin(const std::string& library_path);

  /**
   * Load plugin by ID
   * @param plugin_id Plugin identifier to load
   * @return LoadResult with success status
   */
  LoadResult load_plugin_by_id(const std::string& plugin_id);

  /**
   * Unload plugin
   * @param plugin_id Plugin identifier to unload
   * @return true if unload succeeded
   */
  bool unload_plugin(const std::string& plugin_id);

  /**
   * Get loaded plugin
   * @param plugin_id Plugin identifier
   * @return Shared pointer to plugin, or nullptr if not loaded
   */
  std::shared_ptr<IPlugin> get_loaded_plugin(const std::string& plugin_id) const;

  /**
   * Get all loaded plugins
   * @return Vector of loaded plugins
   */
  std::vector<std::shared_ptr<IPlugin>> get_loaded_plugins() const;

  /**
   * Set plugin directory
   * @param directory Directory path
   */
  void set_plugin_directory(const std::string& directory);

  /**
   * Get plugin directory
   * @return Current plugin directory
   */
  std::string get_plugin_directory() const;

  /**
   * Load all discovered plugins
   * @return Number of successfully loaded plugins
   */
  int load_all_discovered();

  /**
   * Reload plugin (unload and reload)
   * @param plugin_id Plugin to reload
   * @return true if reload succeeded
   */
  bool reload_plugin(const std::string& plugin_id);

  /**
   * Get load strategy for plugin
   * @param plugin_id Plugin identifier
   * @return Load strategy for the plugin
   */
  LoadStrategy get_load_strategy(const std::string& plugin_id) const;

  /**
   * Set load strategy for plugin
   * @param plugin_id Plugin identifier
   * @param strategy Load strategy
   */
  void set_load_strategy(const std::string& plugin_id, LoadStrategy strategy);

  /**
   * Get library info
   * @param library_id Library identifier
   * @return Library metadata if found
   */
  std::optional<PluginLibrary> get_library_info(const std::string& library_id) const;

  /**
   * Get load statistics
   * @return Map of statistics
   */
  std::map<std::string, long> get_load_statistics() const;

  /**
   * Register load callback
   * @param callback Function to call on load events
   */
  using LoadCallback = std::function<void(const LoadResult&)>;
  void register_load_callback(LoadCallback callback);

  /**
   * Clear all loaded plugins
   */
  void clear();

 private:
  mutable std::shared_mutex loader_mutex_;
  std::map<std::string, std::shared_ptr<IPlugin>> loaded_plugins_;
  std::map<std::string, PluginLibrary> libraries_;
  std::map<std::string, LoadStrategy> load_strategies_;
  std::vector<LoadCallback> load_callbacks_;
  std::string plugin_directory_;
};

} // namespace bendcore::plugins

#endif // BENDCORE_PLUGINS_PLUGIN_LOADER_HPP
