#include "bendcore/plugins/plugin_loader.hpp"
#include <dlfcn.h>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <sstream>

namespace fs = std::filesystem;
using namespace std::chrono;

namespace bendcore::plugins {

PluginLoader::PluginLoader() : plugin_directory_(".") {}

PluginLoader::~PluginLoader() {
    UnloadAll();
}

std::string PluginLoader::LoadPlugin(const std::string& plugin_path, bool auto_init) {
    // Execute pre-load callbacks
    for (const auto& callback : pre_load_callbacks_) {
        if (!callback(plugin_path)) {
            last_error_ = "Pre-load callback rejected plugin: " + plugin_path;
            return "";
        }
    }

    auto start_time = high_resolution_clock::now();

    if (!fs::exists(plugin_path)) {
        last_error_ = "Plugin file not found: " + plugin_path;
        return "";
    }

    void* handle = dlopen(plugin_path.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        last_error_ = std::string("Failed to load plugin: ") + dlerror();
        return "";
    }

    // Clear existing errors
    dlerror();

    // Get factory function
    typedef IPlugin* (*PluginFactory)();
    PluginFactory factory = reinterpret_cast<PluginFactory>(dlsym(handle, "CreatePlugin"));

    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        dlclose(handle);
        last_error_ = std::string("Cannot load symbol CreatePlugin: ") + dlsym_error;
        return "";
    }

    if (!factory) {
        dlclose(handle);
        last_error_ = "Factory function is null";
        return "";
    }

    // Create plugin instance
    std::shared_ptr<IPlugin> plugin(factory());
    if (!plugin) {
        dlclose(handle);
        last_error_ = "Failed to create plugin instance";
        return "";
    }

    // Validate plugin
    if (!plugin->Validate()) {
        dlclose(handle);
        last_error_ = "Plugin validation failed";
        return "";
    }

    auto metadata = plugin->GetMetadata();
    std::string plugin_id = metadata.id;

    // Check if plugin already loaded
    if (plugins_.find(plugin_id) != plugins_.end()) {
        dlclose(handle);
        last_error_ = "Plugin already loaded: " + plugin_id;
        return "";
    }

    // Verify API version compatibility
    if (!VersionValidator::IsCompatible(PLUGIN_API_VERSION, plugin->GetRequiredAPIVersion())) {
        dlclose(handle);
        last_error_ = "Plugin API version incompatible";
        return "";
    }

    auto load_time = high_resolution_clock::now();

    // Store plugin
    PluginEntry entry{
        .instance = plugin,
        .path = plugin_path,
        .handle = handle,
        .metadata = metadata,
        .load_time = load_time
    };

    plugins_[plugin_id] = entry;

    // Initialize if requested
    if (auto_init) {
        if (!InitializePlugin(plugin_id)) {
            UnloadPlugin(plugin_id);
            return "";
        }
    }

    // Execute post-load callbacks
    for (const auto& callback : post_load_callbacks_) {
        callback(plugin_id, true);
    }

    return plugin_id;
}

bool PluginLoader::UnloadPlugin(const std::string& plugin_id) {
    auto it = plugins_.find(plugin_id);
    if (it == plugins_.end()) {
        last_error_ = "Plugin not found: " + plugin_id;
        return false;
    }

    auto& entry = it->second;

    // Shutdown plugin
    if (entry.instance->GetStatus() != PluginStatus::UNLOADED) {
        if (!entry.instance->Shutdown()) {
            last_error_ = "Failed to shutdown plugin: " + plugin_id;
            return false;
        }
    }

    // Close library handle
    if (entry.handle && dlclose(entry.handle) != 0) {
        last_error_ = std::string("Failed to close plugin library: ") + dlerror();
        // Don't fail here, continue with cleanup
    }

    plugins_.erase(it);
    return true;
}

bool PluginLoader::UnloadAll() {
    bool all_success = true;
    std::vector<std::string> plugin_ids;

    // Get all plugin IDs first (avoid iterator invalidation)
    for (const auto& [id, _] : plugins_) {
        plugin_ids.push_back(id);
    }

    // Unload in reverse dependency order
    for (auto it = plugin_ids.rbegin(); it != plugin_ids.rend(); ++it) {
        if (!UnloadPlugin(*it)) {
            all_success = false;
        }
    }

    return all_success;
}

std::shared_ptr<IPlugin> PluginLoader::GetPlugin(const std::string& plugin_id) const {
    auto it = plugins_.find(plugin_id);
    if (it != plugins_.end()) {
        return it->second.instance;
    }
    return nullptr;
}

std::vector<std::shared_ptr<IPlugin>> PluginLoader::GetAllPlugins() const {
    std::vector<std::shared_ptr<IPlugin>> result;
    for (const auto& [_, entry] : plugins_) {
        result.push_back(entry.instance);
    }
    return result;
}

std::vector<PluginInfo> PluginLoader::GetLoadedPluginsInfo() const {
    std::vector<PluginInfo> result;
    for (const auto& [_, entry] : plugins_) {
        PluginInfo info{
            .metadata = entry.metadata,
            .type = PluginType::UTILITY,  // Default type
            .priority = PluginPriority::NORMAL,
            .status = entry.instance->GetStatus(),
            .entry_point = "",
            .dependencies = {},
            .provides = {},
            .memory_usage = 0,
            .load_time_ms = 0.0
        };
        result.push_back(info);
    }
    return result;
}

bool PluginLoader::IsPluginLoaded(const std::string& plugin_id) const {
    return plugins_.find(plugin_id) != plugins_.end();
}

bool PluginLoader::InitializePlugin(const std::string& plugin_id) {
    auto it = plugins_.find(plugin_id);
    if (it == plugins_.end()) {
        last_error_ = "Plugin not found: " + plugin_id;
        return false;
    }

    auto& entry = it->second;
    if (!entry.instance->Initialize()) {
        last_error_ = "Failed to initialize plugin: " + plugin_id;
        return false;
    }

    return true;
}

bool PluginLoader::ShutdownPlugin(const std::string& plugin_id) {
    auto it = plugins_.find(plugin_id);
    if (it == plugins_.end()) {
        last_error_ = "Plugin not found: " + plugin_id;
        return false;
    }

    auto& entry = it->second;
    if (!entry.instance->Shutdown()) {
        last_error_ = "Failed to shutdown plugin: " + plugin_id;
        return false;
    }

    return true;
}

bool PluginLoader::SetPluginEnabled(const std::string& plugin_id, bool enabled) {
    auto it = plugins_.find(plugin_id);
    if (it == plugins_.end()) {
        last_error_ = "Plugin not found: " + plugin_id;
        return false;
    }

    it->second.instance->SetEnabled(enabled);
    return true;
}

PluginStatus PluginLoader::GetPluginStatus(const std::string& plugin_id) const {
    auto it = plugins_.find(plugin_id);
    if (it != plugins_.end()) {
        return it->second.instance->GetStatus();
    }
    return PluginStatus::UNLOADED;
}

int PluginLoader::LoadPluginsFromDirectory(const std::string& directory, const std::string& pattern) {
    if (!fs::exists(directory)) {
        last_error_ = "Directory not found: " + directory;
        return 0;
    }

    int loaded_count = 0;
    std::string glob_pattern = pattern;

    // Convert *.so to regex
    if (glob_pattern == "*.so") {
        glob_pattern = ".+\\.so$";
    }

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (filename.find(".so") != std::string::npos) {
                if (LoadPlugin(entry.path().string(), true) != "") {
                    loaded_count++;
                }
            }
        }
    }

    return loaded_count;
}

const std::string& PluginLoader::GetPluginDirectory() const {
    return plugin_directory_;
}

void PluginLoader::SetPluginDirectory(const std::string& directory) {
    plugin_directory_ = directory;
}

bool PluginLoader::ValidateDependencies() {
    for (const auto& [plugin_id, entry] : plugins_) {
        for (const auto& dep : entry.metadata.dependencies) {
            if (!IsPluginLoaded(dep)) {
                last_error_ = "Unmet dependency: " + plugin_id + " requires " + dep;
                return false;
            }
        }
    }
    return true;
}

std::vector<std::string> PluginLoader::ResolveDependencyOrder() {
    std::vector<std::string> result;
    std::set<std::string> visited;
    std::set<std::string> visiting;

    std::function<bool(const std::string&)> visit = [&](const std::string& id) -> bool {
        if (visited.find(id) != visited.end()) {
            return true;  // Already visited
        }
        if (visiting.find(id) != visiting.end()) {
            return false;  // Cycle detected
        }

        visiting.insert(id);

        auto it = plugins_.find(id);
        if (it != plugins_.end()) {
            for (const auto& dep : it->second.metadata.dependencies) {
                if (!visit(dep)) {
                    return false;  // Cycle or missing dependency
                }
            }
        }

        visiting.erase(id);
        visited.insert(id);
        result.push_back(id);
        return true;
    };

    for (const auto& [id, _] : plugins_) {
        if (!visit(id)) {
            last_error_ = "Circular dependency or missing dependency detected";
            return {};
        }
    }

    return result;
}

void PluginLoader::RegisterPreLoadCallback(std::function<bool(const std::string&)> callback) {
    pre_load_callbacks_.push_back(callback);
}

void PluginLoader::RegisterPostLoadCallback(std::function<void(const std::string&, bool)> callback) {
    post_load_callbacks_.push_back(callback);
}

double PluginLoader::GetPluginLoadTime(const std::string& plugin_id) const {
    auto it = plugins_.find(plugin_id);
    if (it != plugins_.end()) {
        auto now = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(now - it->second.load_time);
        return duration.count();
    }
    return 0.0;
}

size_t PluginLoader::GetPluginCount() const {
    return plugins_.size();
}

std::shared_ptr<IPlugin> PluginLoader::GetPluginByName(const std::string& name) const {
    for (const auto& [_, entry] : plugins_) {
        if (entry.metadata.name == name) {
            return entry.instance;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<IPlugin>> PluginLoader::GetPluginsByType(PluginType type) const {
    std::vector<std::shared_ptr<IPlugin>> result;
    // Note: We're not storing type in the entry, so this is a placeholder
    return result;
}

void PluginLoader::Clear() {
    UnloadAll();
    plugins_.clear();
    last_error_.clear();
}

const std::string& PluginLoader::GetLastError() const {
    return last_error_;
}

bool PluginLoader::VerifyPluginVersion(const std::string& plugin_id, const std::string& required_version) const {
    auto it = plugins_.find(plugin_id);
    if (it == plugins_.end()) {
        return false;
    }

    return VersionValidator::IsCompatible(required_version, it->second.metadata.version);
}

bool PluginLoader::LoadPluginFromLibrary(const std::string& path) {
    return LoadPlugin(path, false) != "";
}

std::string PluginLoader::ExtractPluginId(const std::shared_ptr<IPlugin>& plugin) const {
    if (plugin) {
        return plugin->GetMetadata().id;
    }
    return "";
}

bool PluginLoader::CheckDependency(const std::string& plugin_id, const std::string& dependency) {
    return IsPluginLoaded(dependency);
}

// PluginLoaderManager implementation
PluginLoaderManager& PluginLoaderManager::GetInstance() {
    static PluginLoaderManager instance;
    return instance;
}

PluginLoaderManager::PluginLoaderManager() = default;

PluginLoader& PluginLoaderManager::CreateLoader(const std::string& context_name) {
    loaders_[context_name] = std::make_unique<PluginLoader>();
    return *loaders_[context_name];
}

PluginLoader* PluginLoaderManager::GetLoader(const std::string& context_name) {
    auto it = loaders_.find(context_name);
    if (it != loaders_.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool PluginLoaderManager::RemoveLoader(const std::string& context_name) {
    return loaders_.erase(context_name) > 0;
}

std::vector<std::string> PluginLoaderManager::GetAllContexts() const {
    std::vector<std::string> result;
    for (const auto& [name, _] : loaders_) {
        result.push_back(name);
    }
    return result;
}

} // namespace bendcore::plugins
