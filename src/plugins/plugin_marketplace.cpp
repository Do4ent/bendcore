#include "bendcore/plugins/plugin_marketplace.hpp"
#include <algorithm>

namespace bendcore::plugins {

PluginMarketplace& PluginMarketplace::GetInstance() {
    static PluginMarketplace instance;
    return instance;
}

PluginMarketplace::PluginMarketplace() = default;

bool PluginMarketplace::RegisterPlugin(const PluginMetadata& metadata) {
    if (plugins_.find(metadata.id) != plugins_.end()) {
        return false;
    }
    plugins_[metadata.id] = MarketplaceEntry{.metadata = metadata};
    return true;
}

std::vector<PluginMetadata> PluginMarketplace::SearchPlugins(const std::string& query) {
    std::vector<PluginMetadata> result;
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);

    for (const auto& [_, entry] : plugins_) {
        std::string lower_name = entry.metadata.name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

        if (lower_name.find(lower_query) != std::string::npos) {
            result.push_back(entry.metadata);
        }
    }
    return result;
}

const PluginMetadata* PluginMarketplace::GetPluginInfo(const std::string& plugin_id) const {
    auto it = plugins_.find(plugin_id);
    if (it != plugins_.end()) {
        return &it->second.metadata;
    }
    return nullptr;
}

std::vector<PluginMetadata> PluginMarketplace::GetAllPlugins() const {
    std::vector<PluginMetadata> result;
    for (const auto& [_, entry] : plugins_) {
        result.push_back(entry.metadata);
    }
    return result;
}

bool PluginMarketplace::RatePlugin(const std::string& plugin_id, int rating) {
    auto it = plugins_.find(plugin_id);
    if (it == plugins_.end()) {
        return false;
    }

    if (rating < 1 || rating > 5) {
        return false;
    }

    auto& entry = it->second;
    double current_total = entry.rating * entry.review_count;
    entry.review_count++;
    entry.rating = (current_total + rating) / entry.review_count;
    return true;
}

double PluginMarketplace::GetPluginRating(const std::string& plugin_id) const {
    auto it = plugins_.find(plugin_id);
    if (it != plugins_.end()) {
        return it->second.rating;
    }
    return 0.0;
}

bool PluginMarketplace::InstallPlugin(const std::string& plugin_id, const std::string& url) {
    auto it = plugins_.find(plugin_id);
    if (it == plugins_.end()) {
        return false;
    }

    it->second.install_url = url;
    it->second.download_count++;
    return true;
}

bool PluginMarketplace::UninstallPlugin(const std::string& plugin_id) {
    return plugins_.erase(plugin_id) > 0;
}

int PluginMarketplace::GetDownloadCount(const std::string& plugin_id) const {
    auto it = plugins_.find(plugin_id);
    if (it != plugins_.end()) {
        return it->second.download_count;
    }
    return 0;
}

std::vector<PluginMetadata> PluginMarketplace::GetPluginsByCategory(const std::string& category) const {
    std::vector<PluginMetadata> result;
    // Note: metadata doesn't have category field, but we can add tag matching
    return result;
}

void PluginMarketplace::Clear() {
    plugins_.clear();
}

} // namespace bendcore::plugins
