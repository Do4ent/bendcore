#pragma once

#include "plugin_api.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace bendcore::plugins {

/**
 * Plugin marketplace for discovery and installation
 */
class PluginMarketplace {
public:
    static PluginMarketplace& GetInstance();

    /**
     * Register a plugin in the marketplace
     */
    bool RegisterPlugin(const PluginMetadata& metadata);

    /**
     * Search plugins by name or tag
     */
    std::vector<PluginMetadata> SearchPlugins(const std::string& query);

    /**
     * Get plugin by ID
     */
    const PluginMetadata* GetPluginInfo(const std::string& plugin_id) const;

    /**
     * Get all available plugins
     */
    std::vector<PluginMetadata> GetAllPlugins() const;

    /**
     * Rate a plugin (1-5 stars)
     */
    bool RatePlugin(const std::string& plugin_id, int rating);

    /**
     * Get plugin rating
     */
    double GetPluginRating(const std::string& plugin_id) const;

    /**
     * Install plugin from URL
     */
    bool InstallPlugin(const std::string& plugin_id, const std::string& url);

    /**
     * Uninstall plugin
     */
    bool UninstallPlugin(const std::string& plugin_id);

    /**
     * Get download count
     */
    int GetDownloadCount(const std::string& plugin_id) const;

    /**
     * Get plugins by category
     */
    std::vector<PluginMetadata> GetPluginsByCategory(const std::string& category) const;

    /**
     * Clear marketplace
     */
    void Clear();

private:
    PluginMarketplace();

    struct MarketplaceEntry {
        PluginMetadata metadata;
        double rating = 0.0;
        int download_count = 0;
        int review_count = 0;
        std::string install_url;
    };

    std::map<std::string, MarketplaceEntry> plugins_;
};

} // namespace bendcore::plugins
