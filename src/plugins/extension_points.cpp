#include "bendcore/plugins/extension_points.hpp"
#include <algorithm>

namespace bendcore::plugins {

// ExtensionPointRegistry implementation
ExtensionPointRegistry& ExtensionPointRegistry::GetInstance() {
    static ExtensionPointRegistry instance;
    return instance;
}

ExtensionPointRegistry::ExtensionPointRegistry() = default;

bool ExtensionPointRegistry::RegisterExtensionPoint(const ExtensionPoint& point) {
    if (extension_points_.find(point.id) != extension_points_.end()) {
        return false;  // Already exists
    }
    extension_points_[point.id] = point;
    return true;
}

bool ExtensionPointRegistry::UnregisterExtensionPoint(const std::string& point_id) {
    return extension_points_.erase(point_id) > 0;
}

const ExtensionPoint* ExtensionPointRegistry::GetExtensionPoint(const std::string& point_id) const {
    auto it = extension_points_.find(point_id);
    if (it != extension_points_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<ExtensionPoint> ExtensionPointRegistry::GetAllExtensionPoints() const {
    std::vector<ExtensionPoint> result;
    for (const auto& [_, point] : extension_points_) {
        result.push_back(point);
    }
    return result;
}

bool ExtensionPointRegistry::RegisterExtension(const Extension& extension) {
    if (extensions_.find(extension.id) != extensions_.end()) {
        return false;  // Already exists
    }

    // Check if extension point exists
    if (extension_points_.find(extension.extension_point) == extension_points_.end()) {
        return false;  // Extension point not found
    }

    extensions_[extension.id] = extension;
    return true;
}

bool ExtensionPointRegistry::UnregisterExtension(const std::string& extension_id) {
    return extensions_.erase(extension_id) > 0;
}

std::vector<Extension> ExtensionPointRegistry::GetExtensionsForPoint(const std::string& point_id) const {
    std::vector<Extension> result;
    for (const auto& [_, ext] : extensions_) {
        if (ext.extension_point == point_id && ext.enabled) {
            result.push_back(ext);
        }
    }
    // Sort by priority
    std::sort(result.begin(), result.end(), [](const Extension& a, const Extension& b) {
        return a.priority < b.priority;
    });
    return result;
}

bool ExtensionPointRegistry::ExecuteExtensions(const std::string& point_id,
                                              const std::map<std::string, std::any>& params) {
    auto extensions = GetExtensionsForPoint(point_id);

    bool all_success = true;
    for (const auto& ext : extensions) {
        try {
            if (!ext.callback(params)) {
                all_success = false;
            }
        } catch (const std::exception&) {
            all_success = false;
        }
    }
    return all_success;
}

bool ExtensionPointRegistry::HasExtensionPoint(const std::string& point_id) const {
    return extension_points_.find(point_id) != extension_points_.end();
}

bool ExtensionPointRegistry::HasExtension(const std::string& extension_id) const {
    return extensions_.find(extension_id) != extensions_.end();
}

bool ExtensionPointRegistry::SetExtensionEnabled(const std::string& extension_id, bool enabled) {
    auto it = extensions_.find(extension_id);
    if (it == extensions_.end()) {
        return false;
    }
    it->second.enabled = enabled;
    return true;
}

const Extension* ExtensionPointRegistry::GetExtension(const std::string& extension_id) const {
    auto it = extensions_.find(extension_id);
    if (it != extensions_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<Extension> ExtensionPointRegistry::GetExtensionsByPlugin(const std::string& plugin_id) const {
    std::vector<Extension> result;
    for (const auto& [_, ext] : extensions_) {
        if (ext.plugin_id == plugin_id) {
            result.push_back(ext);
        }
    }
    return result;
}

bool ExtensionPointRegistry::RemovePluginExtensions(const std::string& plugin_id) {
    auto extensions = GetExtensionsByPlugin(plugin_id);
    bool all_success = true;
    for (const auto& ext : extensions) {
        if (!UnregisterExtension(ext.id)) {
            all_success = false;
        }
    }
    return all_success;
}

void ExtensionPointRegistry::Clear() {
    extension_points_.clear();
    extensions_.clear();
}

size_t ExtensionPointRegistry::GetExtensionCount() const {
    return extensions_.size();
}

size_t ExtensionPointRegistry::GetExtensionPointCount() const {
    return extension_points_.size();
}

// ExtensionManager implementation
ExtensionManager::ExtensionManager() : registry_(ExtensionPointRegistry::GetInstance()) {
    RegisterAllExtensionPoints();
}

void ExtensionManager::RegisterAllExtensionPoints() {
    // Register built-in extension points
    registry_.RegisterExtensionPoint({
        .id = BuiltInExtensionPoints::ON_DOCUMENT_OPEN,
        .name = "On Document Open",
        .description = "Called when a document is opened",
        .category = ExtensionCategories::DOCUMENT,
        .priority = 0,
        .required_params = {},
        .optional_params = {"document_path", "document_id"},
        .version = "1.0.0",
        .is_active = true
    });

    registry_.RegisterExtensionPoint({
        .id = BuiltInExtensionPoints::ON_DOCUMENT_SAVE,
        .name = "On Document Save",
        .description = "Called when a document is saved",
        .category = ExtensionCategories::DOCUMENT,
        .priority = 0,
        .required_params = {},
        .optional_params = {"document_path"},
        .version = "1.0.0",
        .is_active = true
    });

    registry_.RegisterExtensionPoint({
        .id = BuiltInExtensionPoints::ON_DOCUMENT_CLOSE,
        .name = "On Document Close",
        .description = "Called when a document is closed",
        .category = ExtensionCategories::DOCUMENT,
        .priority = 0,
        .required_params = {},
        .optional_params = {"document_id"},
        .version = "1.0.0",
        .is_active = true
    });

    registry_.RegisterExtensionPoint({
        .id = BuiltInExtensionPoints::ON_GEOMETRY_CREATED,
        .name = "On Geometry Created",
        .description = "Called when geometry is created",
        .category = ExtensionCategories::GEOMETRY,
        .priority = 0,
        .required_params = {"geometry_id"},
        .optional_params = {"geometry_type", "bounds"},
        .version = "1.0.0",
        .is_active = true
    });

    registry_.RegisterExtensionPoint({
        .id = BuiltInExtensionPoints::ON_GEOMETRY_MODIFIED,
        .name = "On Geometry Modified",
        .description = "Called when geometry is modified",
        .category = ExtensionCategories::GEOMETRY,
        .priority = 0,
        .required_params = {"geometry_id"},
        .optional_params = {"modification_type"},
        .version = "1.0.0",
        .is_active = true
    });

    registry_.RegisterExtensionPoint({
        .id = BuiltInExtensionPoints::ON_RENDER_START,
        .name = "On Render Start",
        .description = "Called when rendering starts",
        .category = ExtensionCategories::RENDERING,
        .priority = 0,
        .required_params = {},
        .optional_params = {"viewport", "quality"},
        .version = "1.0.0",
        .is_active = true
    });

    registry_.RegisterExtensionPoint({
        .id = BuiltInExtensionPoints::ON_RENDER_COMPLETE,
        .name = "On Render Complete",
        .description = "Called when rendering completes",
        .category = ExtensionCategories::RENDERING,
        .priority = 0,
        .required_params = {},
        .optional_params = {"render_time"},
        .version = "1.0.0",
        .is_active = true
    });

    registry_.RegisterExtensionPoint({
        .id = BuiltInExtensionPoints::GET_EXPORT_FORMATS,
        .name = "Get Export Formats",
        .description = "Get available export formats",
        .category = ExtensionCategories::EXPORT,
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    });

    registry_.RegisterExtensionPoint({
        .id = BuiltInExtensionPoints::GET_IMPORT_FORMATS,
        .name = "Get Import Formats",
        .description = "Get available import formats",
        .category = ExtensionCategories::IMPORT,
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    });

    registry_.RegisterExtensionPoint({
        .id = BuiltInExtensionPoints::PERFORM_ANALYSIS,
        .name = "Perform Analysis",
        .description = "Perform analysis on geometry",
        .category = ExtensionCategories::ANALYSIS,
        .priority = 0,
        .required_params = {"geometry_id"},
        .optional_params = {"analysis_type", "parameters"},
        .version = "1.0.0",
        .is_active = true
    });

    registry_.RegisterExtensionPoint({
        .id = BuiltInExtensionPoints::CUSTOMIZE_UI,
        .name = "Customize UI",
        .description = "Customize the user interface",
        .category = ExtensionCategories::UI,
        .priority = 0,
        .required_params = {},
        .optional_params = {"ui_section", "widget_type"},
        .version = "1.0.0",
        .is_active = true
    });
}

bool ExtensionManager::RegisterCustomExtensionPoint(const ExtensionPoint& point) {
    return registry_.RegisterExtensionPoint(point);
}

bool ExtensionManager::RegisterPluginExtension(const std::string& plugin_id,
                                             const std::string& extension_point,
                                             const std::string& extension_name,
                                             std::function<bool(const std::map<std::string, std::any>&)> callback) {
    std::string ext_id = plugin_id + "." + extension_name;
    Extension ext{
        .id = ext_id,
        .plugin_id = plugin_id,
        .extension_point = extension_point,
        .name = extension_name,
        .callback = callback,
        .priority = 100,  // Default priority
        .enabled = true
    };

    return registry_.RegisterExtension(ext);
}

bool ExtensionManager::UnregisterPluginExtensions(const std::string& plugin_id) {
    return registry_.RemovePluginExtensions(plugin_id);
}

bool ExtensionManager::ExecuteExtensions(const std::string& extension_point,
                                        const std::map<std::string, std::any>& params) {
    return registry_.ExecuteExtensions(extension_point, params);
}

std::vector<std::string> ExtensionManager::GetAvailableExtensionPoints() const {
    std::vector<std::string> result;
    for (const auto& point : registry_.GetAllExtensionPoints()) {
        result.push_back(point.id);
    }
    return result;
}

int ExtensionManager::GetExtensionCountForPoint(const std::string& point_id) const {
    return registry_.GetExtensionsForPoint(point_id).size();
}

std::map<std::string, int> ExtensionManager::GetExtensionSummary() const {
    std::map<std::string, int> summary;
    for (const auto& point : registry_.GetAllExtensionPoints()) {
        summary[point.id] = registry_.GetExtensionsForPoint(point.id).size();
    }
    return summary;
}

} // namespace bendcore::plugins
