#ifndef BENDCORE_PLUGINS_PLUGIN_MARKETPLACE_HPP
#define BENDCORE_PLUGINS_PLUGIN_MARKETPLACE_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <chrono>
#include <shared_mutex>

namespace bendcore::plugins {

/**
 * Plugin rating entry
 */
struct Rating {
  std::string reviewer_id;
  int score;                           // 1-5 stars
  std::string comment;
  long timestamp_ms;
};

/**
 * Marketplace plugin listing
 */
struct MarketplacePlugin {
  std::string id;
  std::string name;
  std::string description;
  std::string version;
  std::string author;
  std::string homepage_url;
  std::string repository_url;
  std::string license;
  long published_date;
  long updated_date;
  int download_count = 0;
  double average_rating = 0.0;
  std::vector<std::string> tags;
  std::vector<std::string> compatible_versions;
  std::vector<Rating> ratings;
  bool is_verified = false;
  bool is_featured = false;
};

/**
 * Plugin version information
 */
struct PluginVersion {
  std::string version;
  long release_date;
  std::string changelog;
  std::string download_url;
  int download_count = 0;
  bool is_stable = true;
  std::vector<std::string> requirements;
};

/**
 * Plugin dependency for marketplace
 */
struct MarketplaceDependency {
  std::string plugin_id;
  std::string min_version;
  std::string max_version;
  bool is_optional;
};

/**
 * Plugin marketplace - Plugin discovery, versioning, and distribution
 * 
 * Provides plugin listing, version management, ratings, and dependency
 * resolution for a plugin marketplace system.
 */
class PluginMarketplace {
 public:
  /**
   * Create marketplace instance
   */
  PluginMarketplace();

  /**
   * Destructor
   */
  ~PluginMarketplace();

  // Plugin Listing Operations

  /**
   * Publish plugin to marketplace
   * @param plugin Plugin listing to publish
   * @return true if successful
   */
  bool publish_plugin(const MarketplacePlugin& plugin);

  /**
   * Unpublish plugin from marketplace
   * @param plugin_id Plugin identifier to unpublish
   * @return true if successful
   */
  bool unpublish_plugin(const std::string& plugin_id);

  /**
   * Get plugin listing by ID
   * @param plugin_id Plugin identifier
   * @return Plugin listing if found
   */
  std::optional<MarketplacePlugin> get_plugin(const std::string& plugin_id) const;

  /**
   * Search plugins by query
   * @param query Search query string
   * @return Vector of matching plugins
   */
  std::vector<MarketplacePlugin> search_plugins(const std::string& query) const;

  /**
   * Get all published plugins
   * @return Vector of all plugins
   */
  std::vector<MarketplacePlugin> get_all_plugins() const;

  /**
   * Get featured plugins
   * @param limit Maximum number of results
   * @return Vector of featured plugins
   */
  std::vector<MarketplacePlugin> get_featured_plugins(int limit = 10) const;

  // Version Management

  /**
   * Publish plugin version
   * @param plugin_id Plugin identifier
   * @param version Version information
   * @return true if successful
   */
  bool publish_version(const std::string& plugin_id, const PluginVersion& version);

  /**
   * Get plugin version
   * @param plugin_id Plugin identifier
   * @param version Version string
   * @return Version info if found
   */
  std::optional<PluginVersion> get_version(const std::string& plugin_id,
                                           const std::string& version) const;

  /**
   * Get all versions of plugin
   * @param plugin_id Plugin identifier
   * @return Vector of versions
   */
  std::vector<PluginVersion> get_all_versions(const std::string& plugin_id) const;

  /**
   * Get latest stable version
   * @param plugin_id Plugin identifier
   * @return Latest version info if found
   */
  std::optional<PluginVersion> get_latest_version(const std::string& plugin_id) const;

  // Rating and Review System

  /**
   * Add rating to plugin
   * @param plugin_id Plugin identifier
   * @param rating Rating entry
   * @return true if successful
   */
  bool add_rating(const std::string& plugin_id, const Rating& rating);

  /**
   * Get average rating for plugin
   * @param plugin_id Plugin identifier
   * @return Average rating (0.0-5.0)
   */
  double get_average_rating(const std::string& plugin_id) const;

  /**
   * Get all ratings for plugin
   * @param plugin_id Plugin identifier
   * @return Vector of ratings
   */
  std::vector<Rating> get_ratings(const std::string& plugin_id) const;

  /**
   * Get rating count
   * @param plugin_id Plugin identifier
   * @return Number of ratings
   */
  int get_rating_count(const std::string& plugin_id) const;

  // Dependency Resolution

  /**
   * Resolve plugin dependencies
   * @param plugin_id Plugin identifier
   * @param version Version string
   * @return Vector of required dependencies
   */
  std::vector<MarketplaceDependency> resolve_dependencies(
      const std::string& plugin_id,
      const std::string& version) const;

  /**
   * Check if version satisfies dependency
   * @param plugin_id Plugin identifier
   * @param min_version Minimum required version
   * @param max_version Maximum required version (empty = no limit)
   * @return true if version exists and is in range
   */
  bool satisfies_dependency(const std::string& plugin_id,
                           const std::string& min_version,
                           const std::string& max_version) const;

  // Download and Statistics

  /**
   * Increment download count
   * @param plugin_id Plugin identifier
   * @param version Version string (empty = any version)
   */
  void increment_download(const std::string& plugin_id,
                         const std::string& version = "");

  /**
   * Get total downloads for plugin
   * @param plugin_id Plugin identifier
   * @return Download count
   */
  int get_download_count(const std::string& plugin_id) const;

  /**
   * Get version download count
   * @param plugin_id Plugin identifier
   * @param version Version string
   * @return Download count for version
   */
  int get_version_download_count(const std::string& plugin_id,
                                 const std::string& version) const;

  // Marketplace Management

  /**
   * Set plugin as featured
   * @param plugin_id Plugin identifier
   * @param featured Whether to feature the plugin
   */
  void set_featured(const std::string& plugin_id, bool featured);

  /**
   * Verify plugin authenticity
   * @param plugin_id Plugin identifier
   * @param verified Verification status
   */
  void set_verified(const std::string& plugin_id, bool verified);

  /**
   * Get marketplace statistics
   * @return Map of statistics
   */
  std::map<std::string, int> get_statistics() const;

  /**
   * Clear all marketplace data
   */
  void clear();

 private:
  mutable std::shared_mutex marketplace_mutex_;
  std::map<std::string, MarketplacePlugin> plugins_;
  std::map<std::string, std::vector<PluginVersion>> versions_;
  std::map<std::string, std::vector<MarketplaceDependency>> dependencies_;
};

} // namespace bendcore::plugins

#endif // BENDCORE_PLUGINS_PLUGIN_MARKETPLACE_HPP
