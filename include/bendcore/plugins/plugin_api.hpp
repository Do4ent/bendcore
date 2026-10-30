#ifndef BENDCORE_PLUGINS_PLUGIN_API_HPP
#define BENDCORE_PLUGINS_PLUGIN_API_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <optional>
#include <atomic>
#include <shared_mutex>
#include <stdexcept>

namespace bendcore::plugins {

/**
 * Plugin metadata and version information
 */
struct PluginMetadata {
  std::string id;                      // Unique plugin identifier
  std::string name;                    // Display name
  std::string version;                 // Semantic version
  std::string description;             // Plugin description
  std::string author;                  // Plugin author
  std::string license;                 // License identifier
  std::vector<std::string> keywords;   // Search keywords
  std::string repository_url;          // Repository URL
  bool requires_restart = false;       // Requires application restart
};

/**
 * Plugin capability descriptor
 */
struct CapabilityDescriptor {
  std::string name;                    // Capability name
  std::string type;                    // Type identifier
  std::vector<std::string> features;   // Feature list
  std::map<std::string, std::string> properties; // Additional properties
};

/**
 * Plugin state enumeration
 */
enum class PluginState {
  UNINITIALIZED,      // Plugin not yet initialized
  LOADING,            // Plugin is loading
  LOADED,             // Plugin loaded successfully
  ACTIVE,             // Plugin is active
  SUSPENDED,          // Plugin is suspended
  ERROR,              // Plugin encountered error
  UNLOADING,          // Plugin is unloading
  UNLOADED            // Plugin unloaded
};

/**
 * Plugin priority for execution order
 */
enum class PluginPriority {
  CRITICAL = 0,       // Execute before all others
  HIGH = 1,
  NORMAL = 2,
  LOW = 3,
  DEFERRED = 4        // Execute after normal plugins
};

/**
 * Plugin dependency specification
 */
struct PluginDependency {
  std::string plugin_id;               // Required plugin ID
  std::string min_version;             // Minimum required version
  bool optional = false;               // Whether dependency is optional
};

/**
 * Plugin configuration entry
 */
struct ConfigEntry {
  std::string key;
  std::string value;
  std::string description;
  bool is_secret = false;              // Indicates sensitive data
};

/**
 * Plugin event data
 */
struct PluginEvent {
  std::string event_type;              // Type of event
  std::string plugin_id;               // Plugin that triggered event
  std::map<std::string, std::string> data; // Event data
  long timestamp_ms = 0;               // Timestamp in milliseconds
};

/**
 * Callback type for plugin events
 */
using PluginEventCallback = std::function<void(const PluginEvent&)>;

/**
 * IPlugin interface - Base interface for all plugins
 * 
 * Plugins implement this interface to provide functionality
 * and lifecycle management hooks.
 */
class IPlugin {
 public:
  virtual ~IPlugin() = default;

  /**
   * Get plugin metadata
   * @return Plugin metadata
   */
  virtual PluginMetadata get_metadata() const = 0;

  /**
   * Initialize the plugin
   * @param context Plugin context (opaque pointer to host context)
   * @return true if initialization succeeded
   */
  virtual bool initialize(void* context) = 0;

  /**
   * Activate the plugin
   * @return true if activation succeeded
   */
  virtual bool activate() = 0;

  /**
   * Deactivate the plugin
   * @return true if deactivation succeeded
   */
  virtual bool deactivate() = 0;

  /**
   * Shutdown the plugin
   * @return true if shutdown succeeded
   */
  virtual bool shutdown() = 0;

  /**
   * Get list of capabilities provided by this plugin
   * @return Vector of capability descriptors
   */
  virtual std::vector<CapabilityDescriptor> get_capabilities() const = 0;

  /**
   * Get plugin dependencies
   * @return Vector of plugin dependencies
   */
  virtual std::vector<PluginDependency> get_dependencies() const = 0;

  /**
   * Get current plugin state
   * @return Current PluginState
   */
  virtual PluginState get_state() const = 0;

  /**
   * Configure plugin with configuration entries
   * @param config Vector of configuration entries
   * @return true if configuration succeeded
   */
  virtual bool configure(const std::vector<ConfigEntry>& config) = 0;

  /**
   * Get plugin configuration
   * @return Current configuration
   */
  virtual std::vector<ConfigEntry> get_configuration() const = 0;

  /**
   * Check if plugin is compatible with given version
   * @param api_version API version to check
   * @return true if compatible
   */
  virtual bool is_compatible(const std::string& api_version) const = 0;

  /**
   * Execute plugin command
   * @param command Command name
   * @param parameters Command parameters
   * @return Command result as string
   */
  virtual std::string execute_command(const std::string& command,
                                      const std::map<std::string, std::string>& parameters) = 0;

  /**
   * Get plugin diagnostics
   * @return Diagnostic information
   */
  virtual std::map<std::string, std::string> get_diagnostics() const = 0;
};

/**
 * Plugin registry - Manages plugin registration and lifecycle
 * 
 * Thread-safe singleton that manages all plugins in the system.
 * Provides methods for registering, loading, activating, and
 * querying plugin information.
 */
class PluginRegistry {
 public:
  /**
   * Get singleton instance
   * @return Reference to PluginRegistry instance
   */
  static PluginRegistry& instance();

  /**
   * Register a plugin with the registry
   * @param plugin Shared pointer to plugin implementation
   * @return true if registration succeeded
   */
  bool register_plugin(std::shared_ptr<IPlugin> plugin);

  /**
   * Unregister a plugin
   * @param plugin_id Plugin identifier to unregister
   * @return true if unregistration succeeded
   */
  bool unregister_plugin(const std::string& plugin_id);

  /**
   * Get registered plugin by ID
   * @param plugin_id Plugin identifier
   * @return Shared pointer to plugin, or nullptr if not found
   */
  std::shared_ptr<IPlugin> get_plugin(const std::string& plugin_id) const;

  /**
   * Get all registered plugins
   * @return Vector of all registered plugins
   */
  std::vector<std::shared_ptr<IPlugin>> get_all_plugins() const;

  /**
   * Get plugins by capability
   * @param capability_name Capability name to search for
   * @return Vector of plugins providing the capability
   */
  std::vector<std::shared_ptr<IPlugin>> get_plugins_by_capability(
      const std::string& capability_name) const;

  /**
   * Get plugin count
   * @return Number of registered plugins
   */
  size_t get_plugin_count() const;

  /**
   * Check if plugin is registered
   * @param plugin_id Plugin identifier
   * @return true if plugin is registered
   */
  bool has_plugin(const std::string& plugin_id) const;

  /**
   * Get plugins by state
   * @param state Plugin state to filter by
   * @return Vector of plugins in specified state
   */
  std::vector<std::shared_ptr<IPlugin>> get_plugins_by_state(PluginState state) const;

  /**
   * Activate all registered plugins
   * @return Number of successfully activated plugins
   */
  int activate_all();

  /**
   * Deactivate all registered plugins
   * @return Number of successfully deactivated plugins
   */
  int deactivate_all();

  /**
   * Register event callback for plugin events
   * @param callback Function to call on events
   */
  void register_event_callback(PluginEventCallback callback);

  /**
   * Emit a plugin event
   * @param event Event to emit
   */
  void emit_event(const PluginEvent& event);

  /**
   * Clear all plugins
   */
  void clear();

 private:
  PluginRegistry() = default;
  ~PluginRegistry() = default;

  mutable std::shared_mutex registry_mutex_;
  std::map<std::string, std::shared_ptr<IPlugin>> plugins_;
  std::vector<PluginEventCallback> event_callbacks_;
};

/**
 * Plugin exception - Thrown for plugin-related errors
 */
class PluginException : public std::runtime_error {
 public:
  explicit PluginException(const std::string& message)
      : std::runtime_error(message) {}
};

} // namespace bendcore::plugins

#endif // BENDCORE_PLUGINS_PLUGIN_API_HPP
