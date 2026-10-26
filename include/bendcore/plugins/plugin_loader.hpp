#pragma once

#include "plugin_api.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>

namespace bendcore::plugins {

/**
 * Plugin loading and lifecycle management
 * Handles loading, initialization, and unloading of plugins
 */
class PluginLoader {
public:
    PluginLoader();
    ~PluginLoader();

    // Delete copy operations
    PluginLoader(const PluginLoader&) = delete;
    PluginLoader& operator=(const PluginLoader&) = delete;

    /**
     * Load a plugin from a shared library
     * @param plugin_path: Path to plugin .so/.dll file
     * @param auto_init: Automatically initialize plugin after loading
     * @return: Plugin ID if successful, empty string on failure
     */
    std::string LoadPlugin(const std::string& plugin_path, bool auto_init = true);

    /**
     * Unload a plugin by ID
     * @param plugin_id: Plugin identifier
     * @return: true if successful
     */
    bool UnloadPlugin(const std::string& plugin_id);

    /**
     * Unload all plugins
     */
    bool UnloadAll();

    /**
     * Get loaded plugin by ID
     */
    std::shared_ptr<IPlugin> GetPlugin(const std::string& plugin_id) const;

    /**
     * Get all loaded plugins
     */
    std::vector<std::shared_ptr<IPlugin>> GetAllPlugins() const;

    /**
     * Get all plugin infos with status
     */
    std::vector<PluginInfo> GetLoadedPluginsInfo() const;

    /**
     * Check if plugin is loaded
     */
    bool IsPluginLoaded(const std::string& plugin_id) const;

    /**
     * Initialize a loaded plugin
     */
    bool InitializePlugin(const std::string& plugin_id);

    /**
     * Shutdown a loaded plugin
     */
    bool ShutdownPlugin(const std::string& plugin_id);

    /**
     * Enable/disable a plugin
     */
    bool SetPluginEnabled(const std::string& plugin_id, bool enabled);

    /**
     * Get plugin status
     */
    PluginStatus GetPluginStatus(const std::string& plugin_id) const;

    /**
     * Load plugins from directory
     * @param directory: Directory containing plugin files
     * @param pattern: File pattern to match (e.g., "*.so")
     * @return: Number of plugins successfully loaded
     */
    int LoadPluginsFromDirectory(const std::string& directory, const std::string& pattern = "*.so");

    /**
     * Get directory containing plugins
     */
    const std::string& GetPluginDirectory() const;

    /**
     * Set plugin search directory
     */
    void SetPluginDirectory(const std::string& directory);

    /**
     * Validate plugin dependencies
     * @return: true if all dependencies are satisfiable
     */
    bool ValidateDependencies();

    /**
     * Resolve plugin loading order based on dependencies
     * @return: Ordered list of plugin IDs for initialization
     */
    std::vector<std::string> ResolveDependencyOrder();

    /**
     * Register pre-load callback
     */
    void RegisterPreLoadCallback(std::function<bool(const std::string&)> callback);

    /**
     * Register post-load callback
     */
    void RegisterPostLoadCallback(std::function<void(const std::string&, bool)> callback);

    /**
     * Get plugin load time
     */
    double GetPluginLoadTime(const std::string& plugin_id) const;

    /**
     * Get total number of loaded plugins
     */
    size_t GetPluginCount() const;

    /**
     * Get plugin by name
     */
    std::shared_ptr<IPlugin> GetPluginByName(const std::string& name) const;

    /**
     * Get plugins by type
     */
    std::vector<std::shared_ptr<IPlugin>> GetPluginsByType(PluginType type) const;

    /**
     * Clear all plugins and reset state
     */
    void Clear();

    /**
     * Get last error message
     */
    const std::string& GetLastError() const;

    /**
     * Check if plugin satisfies version requirement
     */
    bool VerifyPluginVersion(const std::string& plugin_id, const std::string& required_version) const;

private:
    struct PluginEntry {
        std::shared_ptr<IPlugin> instance;
        std::string path;
        void* handle;  // DLL/SO handle
        PluginMetadata metadata;
        std::chrono::high_resolution_clock::time_point load_time;
    };

    std::map<std::string, PluginEntry> plugins_;
    std::string plugin_directory_;
    std::string last_error_;
    std::vector<std::function<bool(const std::string&)>> pre_load_callbacks_;
    std::vector<std::function<void(const std::string&, bool)>> post_load_callbacks_;

    bool LoadPluginFromLibrary(const std::string& path);
    std::string ExtractPluginId(const std::shared_ptr<IPlugin>& plugin) const;
    bool CheckDependency(const std::string& plugin_id, const std::string& dependency);
};

/**
 * Plugin loader manager for handling multiple loader instances
 */
class PluginLoaderManager {
public:
    static PluginLoaderManager& GetInstance();

    /**
     * Create a new loader context
     */
    PluginLoader& CreateLoader(const std::string& context_name);

    /**
     * Get existing loader by context name
     */
    PluginLoader* GetLoader(const std::string& context_name);

    /**
     * Remove loader by context name
     */
    bool RemoveLoader(const std::string& context_name);

    /**
     * Get all active loaders
     */
    std::vector<std::string> GetAllContexts() const;

private:
    PluginLoaderManager();
    std::map<std::string, std::unique_ptr<PluginLoader>> loaders_;
};

} // namespace bendcore::plugins
