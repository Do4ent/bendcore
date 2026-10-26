#include "bendcore/plugins/plugin_api.hpp"
#include <sstream>
#include <algorithm>
#include <regex>

namespace bendcore::plugins {

namespace {
    std::map<std::string, std::vector<HookCallback>> g_global_hooks;
}

// VersionValidator implementation
bool VersionValidator::IsCompatible(const std::string& required, const std::string& actual) {
    int req_major, req_minor, req_patch;
    int act_major, act_minor, act_patch;

    if (!ParseVersion(required, req_major, req_minor, req_patch)) {
        return false;
    }
    if (!ParseVersion(actual, act_major, act_minor, act_patch)) {
        return false;
    }

    // Check major version - must be same
    if (req_major != act_major) {
        return false;
    }

    // Check minor version - actual should be >= required
    if (act_minor < req_minor) {
        return false;
    }

    // If minor versions are same, patch should be >=
    if (act_minor == req_minor && act_patch < req_patch) {
        return false;
    }

    return true;
}

bool VersionValidator::ParseVersion(const std::string& version,
                                   int& major, int& minor, int& patch) {
    std::regex version_regex(R"((\d+)\.(\d+)\.(\d+))");
    std::smatch match;

    if (!std::regex_match(version, match, version_regex)) {
        return false;
    }

    try {
        major = std::stoi(match[1].str());
        minor = std::stoi(match[2].str());
        patch = std::stoi(match[3].str());
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

int VersionValidator::CompareVersions(const std::string& v1, const std::string& v2) {
    int v1_major, v1_minor, v1_patch;
    int v2_major, v2_minor, v2_patch;

    if (!ParseVersion(v1, v1_major, v1_minor, v1_patch) ||
        !ParseVersion(v2, v2_major, v2_minor, v2_patch)) {
        return 0;
    }

    if (v1_major != v2_major) {
        return v1_major < v2_major ? -1 : 1;
    }
    if (v1_minor != v2_minor) {
        return v1_minor < v2_minor ? -1 : 1;
    }
    if (v1_patch != v2_patch) {
        return v1_patch < v2_patch ? -1 : 1;
    }

    return 0;
}

// HookExecutor implementation
bool HookExecutor::ExecuteHooks(const std::string& hook_name,
                               const std::map<std::string, std::any>& data) {
    auto it = g_global_hooks.find(hook_name);
    if (it == g_global_hooks.end()) {
        return true;  // No hooks registered is not an error
    }

    bool all_success = true;
    for (const auto& callback : it->second) {
        try {
            if (!callback(hook_name, data)) {
                all_success = false;
            }
        } catch (const std::exception& e) {
            all_success = false;
        }
    }

    return all_success;
}

void HookExecutor::RegisterGlobalHook(const std::string& hook_name, HookCallback callback) {
    g_global_hooks[hook_name].push_back(callback);
}

void HookExecutor::UnregisterGlobalHook(const std::string& hook_name) {
    auto it = g_global_hooks.find(hook_name);
    if (it != g_global_hooks.end()) {
        g_global_hooks.erase(it);
    }
}

} // namespace bendcore::plugins
