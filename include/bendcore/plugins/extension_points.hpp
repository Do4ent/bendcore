#pragma once

#include "plugin_api.hpp"
#include <map>
#include <set>
#include <vector>
#include <functional>
#include <memory>

namespace bendcore::plugins {

/**
 * Extension point definition
 * Defines a place where plugins can hook into application functionality
 */
struct ExtensionPoint {
    std::string id;                    // Unique identifier
    std::string name;                  // Human-readable name
    std::string description;           // What this extension point does
    std::string category;              // Category (e.g., "rendering", "analysis")
    int priority;                      // Priority for execution order
    std::vector<std::string> required_params;  // Required parameters
    std::vector<std::string> optional_params;  // Optional parameters
    std::string version;               // API version
    bool is_active;                    // Whether this extension point is active
};

/**
 * Extension implementation by a plugin
 */
struct Extension {
    std::string id;                    // Extension ID
    std::string plugin_id;             // ID of plugin providing this extension
    std::string extension_point;       // Extension point ID
    std::string name;                  // Extension name
    std::function<bool(const std::map<std::string, std::any>&)> callback;
    int priority;                      // Execution priority
    bool enabled;                      // Whether this extension is enabled
};

/**
 * Extension point registry
 * Manages extension points and their implementations
 */
class ExtensionPointRegistry {
public:
    static ExtensionPointRegistry& GetInstance();

    /**
     * Register an extension point
     */
    bool RegisterExtensionPoint(const ExtensionPoint& point);

    /**
     * Unregister an extension point
     */
    bool UnregisterExtensionPoint(const std::string& point_id);

    /**
     * Get extension point by ID
     */
    const ExtensionPoint* GetExtensionPoint(const std::string& point_id) const;

    /**
     * Get all extension points
     */
    std::vector<ExtensionPoint> GetAllExtensionPoints() const;

    /**
     * Register an extension implementation
     */
    bool RegisterExtension(const Extension& extension);

    /**
     * Unregister an extension
     */
    bool UnregisterExtension(const std::string& extension_id);

    /**
     * Get extensions for a specific extension point
     */
    std::vector<Extension> GetExtensionsForPoint(const std::string& point_id) const;

    /**
     * Execute all extensions for a point
     */
    bool ExecuteExtensions(const std::string& point_id,
                          const std::map<std::string, std::any>& params);

    /**
     * Check if an extension point exists
     */
    bool HasExtensionPoint(const std::string& point_id) const;

    /**
     * Check if an extension exists
     */
    bool HasExtension(const std::string& extension_id) const;

    /**
     * Enable/disable extension
     */
    bool SetExtensionEnabled(const std::string& extension_id, bool enabled);

    /**
     * Get extension by ID
     */
    const Extension* GetExtension(const std::string& extension_id) const;

    /**
     * Get extensions by plugin
     */
    std::vector<Extension> GetExtensionsByPlugin(const std::string& plugin_id) const;

    /**
     * Remove all extensions from a plugin
     */
    bool RemovePluginExtensions(const std::string& plugin_id);

    /**
     * Clear all extensions and extension points
     */
    void Clear();

    /**
     * Get extension count
     */
    size_t GetExtensionCount() const;

    /**
     * Get extension point count
     */
    size_t GetExtensionPointCount() const;

private:
    ExtensionPointRegistry();
    std::map<std::string, ExtensionPoint> extension_points_;
    std::map<std::string, Extension> extensions_;
};

/**
 * Extension point categories
 */
namespace ExtensionCategories {
    constexpr const char* RENDERING = "rendering";
    constexpr const char* ANALYSIS = "analysis";
    constexpr const char* FILE_FORMAT = "file_format";
    constexpr const char* GEOMETRY = "geometry";
    constexpr const char* SIMULATION = "simulation";
    constexpr const char* EXPORT = "export";
    constexpr const char* IMPORT = "import";
    constexpr const char* TOOL = "tool";
    constexpr const char* UI = "ui";
    constexpr const char* DOCUMENT = "document";
}

/**
 * Built-in extension points
 */
namespace BuiltInExtensionPoints {
    constexpr const char* ON_DOCUMENT_OPEN = "on.document.open";
    constexpr const char* ON_DOCUMENT_SAVE = "on.document.save";
    constexpr const char* ON_DOCUMENT_CLOSE = "on.document.close";
    constexpr const char* ON_GEOMETRY_CREATED = "on.geometry.created";
    constexpr const char* ON_GEOMETRY_MODIFIED = "on.geometry.modified";
    constexpr const char* ON_RENDER_START = "on.render.start";
    constexpr const char* ON_RENDER_COMPLETE = "on.render.complete";
    constexpr const char* GET_EXPORT_FORMATS = "get.export.formats";
    constexpr const char* GET_IMPORT_FORMATS = "get.import.formats";
    constexpr const char* PERFORM_ANALYSIS = "perform.analysis";
    constexpr const char* CUSTOMIZE_UI = "customize.ui";
}

/**
 * Extension manager for plugin-provided extensions
 */
class ExtensionManager {
public:
    ExtensionManager();

    /**
     * Register all extension points
     */
    void RegisterAllExtensionPoints();

    /**
     * Register a custom extension point
     */
    bool RegisterCustomExtensionPoint(const ExtensionPoint& point);

    /**
     * Register extension from plugin
     */
    bool RegisterPluginExtension(const std::string& plugin_id,
                                const std::string& extension_point,
                                const std::string& extension_name,
                                std::function<bool(const std::map<std::string, std::any>&)> callback);

    /**
     * Unregister plugin extensions
     */
    bool UnregisterPluginExtensions(const std::string& plugin_id);

    /**
     * Execute extensions
     */
    bool ExecuteExtensions(const std::string& extension_point,
                          const std::map<std::string, std::any>& params = {});

    /**
     * Get available extension points
     */
    std::vector<std::string> GetAvailableExtensionPoints() const;

    /**
     * Get registered extensions for a point
     */
    int GetExtensionCountForPoint(const std::string& point_id) const;

    /**
     * Get summary of all extensions
     */
    std::map<std::string, int> GetExtensionSummary() const;

private:
    ExtensionPointRegistry& registry_;
};

} // namespace bendcore::plugins
