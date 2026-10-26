#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <any>

namespace bendcore::plugins {

/**
 * Plugin API Version - Semantic versioning
 * MAJOR.MINOR.PATCH
 */
constexpr const char* PLUGIN_API_VERSION = "1.0.0";

/**
 * Plugin Status enumeration
 */
enum class PluginStatus {
    UNLOADED = 0,
    LOADING = 1,
    LOADED = 2,
    INITIALIZING = 3,
    ACTIVE = 4,
    DISABLED = 5,
    ERROR = 6,
    UNLOADING = 7
};

/**
 * Plugin Type classification
 */
enum class PluginType {
    RENDERER = 0,
    ANALYZER = 1,
    EXPORTER = 2,
    IMPORTER = 3,
    UTILITY = 4,
    EXTENSION = 5
};

/**
 * Plugin Priority for initialization order
 */
enum class PluginPriority {
    CRITICAL = 0,      // Must load before others
    HIGH = 1,
    NORMAL = 2,
    LOW = 3
};

/**
 * Permission flag for plugins
 */
enum class PluginPermission {
    READ_DOCUMENTS = 1 << 0,
    WRITE_DOCUMENTS = 1 << 1,
    NETWORK_ACCESS = 1 << 2,
    FILE_SYSTEM = 1 << 3,
    RENDERING = 1 << 4,
    GEOMETRY_ACCESS = 1 << 5,
    USER_INPUT = 1 << 6,
    ANALYTICS = 1 << 7,
    ALL_PERMISSIONS = (1 << 8) - 1
};

/**
 * Plugin metadata information
 */
struct PluginMetadata {
    std::string id;                    // Unique plugin identifier
    std::string name;                  // Human-readable name
    std::string version;               // Semantic version
    std::string author;                // Author/organization
    std::string description;           // Plugin description
    std::string license;               // License type (MIT, Apache, etc.)
    std::vector<std::string> tags;     // Searchable tags
    std::string documentation_url;     // URL to documentation
    std::string repository_url;        // Source repository
    uint32_t permissions;              // Required permissions (bitfield)
};

/**
 * Plugin Info structure with runtime information
 */
struct PluginInfo {
    PluginMetadata metadata;
    PluginType type;
    PluginPriority priority;
    PluginStatus status;
    std::string entry_point;           // Main entry point
    std::vector<std::string> dependencies;  // Required plugins
    std::vector<std::string> provides;      // Services provided
    uint64_t memory_usage;             // Current memory usage
    double load_time_ms;               // Load time in milliseconds
};

/**
 * Hook callback type
 */
using HookCallback = std::function<bool(const std::string&, const std::map<std::string, std::any>&)>;

/**
 * Plugin base interface
 * All plugins must implement this interface
 */
class IPlugin {
public:
    virtual ~IPlugin() = default;

    /**
     * Get plugin metadata
     */
    virtual PluginMetadata GetMetadata() const = 0;

    /**
     * Initialize plugin
     * Called when plugin is loaded
     */
    virtual bool Initialize() = 0;

    /**
     * Shutdown plugin
     * Called before plugin is unloaded
     */
    virtual bool Shutdown() = 0;

    /**
     * Check if plugin is enabled
     */
    virtual bool IsEnabled() const = 0;

    /**
     * Enable/disable plugin
     */
    virtual void SetEnabled(bool enabled) = 0;

    /**
     * Get current status
     */
    virtual PluginStatus GetStatus() const = 0;

    /**
     * Register hook callback
     * @param hook_name: Unique hook name
     * @param callback: Function to execute
     * @return: Hook registration ID
     */
    virtual std::string RegisterHook(const std::string& hook_name,
                                     HookCallback callback) = 0;

    /**
     * Unregister hook by ID
     */
    virtual bool UnregisterHook(const std::string& hook_id) = 0;

    /**
     * Get list of registered hooks
     */
    virtual std::vector<std::string> GetRegisteredHooks() const = 0;

    /**
     * Execute a command
     * @param command: Command name
     * @param args: Command arguments
     * @return: Command result
     */
    virtual std::string ExecuteCommand(const std::string& command,
                                      const std::map<std::string, std::string>& args) = 0;

    /**
     * Get plugin configuration
     */
    virtual std::map<std::string, std::any> GetConfiguration() const = 0;

    /**
     * Set plugin configuration
     */
    virtual bool SetConfiguration(const std::map<std::string, std::any>& config) = 0;

    /**
     * Get version
     */
    virtual std::string GetVersion() const = 0;

    /**
     * Get required API version
     */
    virtual std::string GetRequiredAPIVersion() const = 0;

    /**
     * Validate plugin integrity
     */
    virtual bool Validate() const = 0;
};

/**
 * Plugin interface factory
 * Used to create plugin instances
 */
typedef IPlugin* (*PluginFactory)();

/**
 * Plugin version compatibility check
 */
class VersionValidator {
public:
    /**
     * Check if two versions are compatible
     * @param required: Required version (e.g., "1.0.0")
     * @param actual: Actual version (e.g., "1.0.1")
     * @return: true if versions are compatible
     */
    static bool IsCompatible(const std::string& required, const std::string& actual);

    /**
     * Parse version string into major, minor, patch
     */
    static bool ParseVersion(const std::string& version,
                            int& major, int& minor, int& patch);

    /**
     * Compare two versions
     * @return: -1 if v1 < v2, 0 if equal, 1 if v1 > v2
     */
    static int CompareVersions(const std::string& v1, const std::string& v2);
};

/**
 * Plugin API context
 * Provides access to BendCore functionality from plugins
 */
class PluginContext {
public:
    /**
     * Get application version
     */
    virtual std::string GetApplicationVersion() const = 0;

    /**
     * Get application data directory
     */
    virtual std::string GetApplicationDataDir() const = 0;

    /**
     * Get plugin data directory
     */
    virtual std::string GetPluginDataDir(const std::string& plugin_id) const = 0;

    /**
     * Log message from plugin
     */
    virtual void Log(const std::string& message, const std::string& level = "INFO") = 0;

    /**
     * Check if another plugin is available
     */
    virtual bool IsPluginAvailable(const std::string& plugin_id) const = 0;

    /**
     * Request interface from another plugin
     */
    virtual void* GetPluginInterface(const std::string& plugin_id,
                                    const std::string& interface_name) = 0;

    virtual ~PluginContext() = default;
};

/**
 * Global hook executor for inter-plugin communication
 */
class HookExecutor {
public:
    /**
     * Execute all registered hooks for a given name
     */
    static bool ExecuteHooks(const std::string& hook_name,
                            const std::map<std::string, std::any>& data);

    /**
     * Register global hook
     */
    static void RegisterGlobalHook(const std::string& hook_name, HookCallback callback);

    /**
     * Unregister global hook
     */
    static void UnregisterGlobalHook(const std::string& hook_name);
};

} // namespace bendcore::plugins
