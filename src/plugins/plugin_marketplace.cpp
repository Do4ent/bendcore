#include "bendcore/plugins/plugin_marketplace.hpp"
#include <algorithm>
#include <mutex>
#include <cmath>

namespace bendcore::plugins {

PluginMarketplace::PluginMarketplace() = default;

PluginMarketplace::~PluginMarketplace() = default;

bool PluginMarketplace::publish_plugin(const MarketplacePlugin& plugin) {
  std::unique_lock<std::shared_mutex> lock(marketplace_mutex_);
  if (plugins_.find(plugin.id) != plugins_.end()) return false;
  plugins_[plugin.id] = plugin;
  return true;
}

bool PluginMarketplace::unpublish_plugin(const std::string& plugin_id) {
  std::unique_lock<std::shared_mutex> lock(marketplace_mutex_);
  auto it = plugins_.find(plugin_id);
  if (it == plugins_.end()) return false;
  plugins_.erase(it);
  versions_.erase(plugin_id);
  dependencies_.erase(plugin_id);
  return true;
}

std::optional<MarketplacePlugin> PluginMarketplace::get_plugin(const std::string& plugin_id) const {
  std::shared_lock<std::shared_mutex> lock(marketplace_mutex_);
  auto it = plugins_.find(plugin_id);
  if (it != plugins_.end()) return it->second;
  return std::nullopt;
}

std::vector<MarketplacePlugin> PluginMarketplace::search_plugins(const std::string& query) const {
  std::shared_lock<std::shared_mutex> lock(marketplace_mutex_);
  std::vector<MarketplacePlugin> results;
  for (const auto& pair : plugins_) {
    if (pair.second.name.find(query) != std::string::npos ||
        pair.second.description.find(query) != std::string::npos) {
      results.push_back(pair.second);
    }
  }
  return results;
}

std::vector<MarketplacePlugin> PluginMarketplace::get_all_plugins() const {
  std::shared_lock<std::shared_mutex> lock(marketplace_mutex_);
  std::vector<MarketplacePlugin> results;
  for (const auto& pair : plugins_) {
    results.push_back(pair.second);
  }
  return results;
}

std::vector<MarketplacePlugin> PluginMarketplace::get_featured_plugins(int limit) const {
  std::shared_lock<std::shared_mutex> lock(marketplace_mutex_);
  std::vector<MarketplacePlugin> featured;
  for (const auto& pair : plugins_) {
    if (pair.second.is_featured) {
      featured.push_back(pair.second);
    }
  }
  std::sort(featured.begin(), featured.end(),
            [](const MarketplacePlugin& a, const MarketplacePlugin& b) {
              return a.download_count > b.download_count;
            });
  if (featured.size() > static_cast<size_t>(limit)) {
    featured.resize(limit);
  }
  return featured;
}

bool PluginMarketplace::publish_version(const std::string& plugin_id, const PluginVersion& version) {
  std::unique_lock<std::shared_mutex> lock(marketplace_mutex_);
  if (plugins_.find(plugin_id) == plugins_.end()) return false;
  versions_[plugin_id].push_back(version);
  return true;
}

std::optional<PluginVersion> PluginMarketplace::get_version(const std::string& plugin_id,
                                                            const std::string& version) const {
  std::shared_lock<std::shared_mutex> lock(marketplace_mutex_);
  auto it = versions_.find(plugin_id);
  if (it != versions_.end()) {
    for (const auto& v : it->second) {
      if (v.version == version) return v;
    }
  }
  return std::nullopt;
}

std::vector<PluginVersion> PluginMarketplace::get_all_versions(const std::string& plugin_id) const {
  std::shared_lock<std::shared_mutex> lock(marketplace_mutex_);
  auto it = versions_.find(plugin_id);
  if (it != versions_.end()) return it->second;
  return {};
}

std::optional<PluginVersion> PluginMarketplace::get_latest_version(const std::string& plugin_id) const {
  std::shared_lock<std::shared_mutex> lock(marketplace_mutex_);
  auto it = versions_.find(plugin_id);
  if (it != versions_.end() && !it->second.empty()) {
    auto latest = std::max_element(it->second.begin(), it->second.end(),
        [](const PluginVersion& a, const PluginVersion& b) {
          return a.release_date < b.release_date;
        });
    return *latest;
  }
  return std::nullopt;
}

bool PluginMarketplace::add_rating(const std::string& plugin_id, const Rating& rating) {
  std::unique_lock<std::shared_mutex> lock(marketplace_mutex_);
  auto it = plugins_.find(plugin_id);
  if (it == plugins_.end()) return false;
  
  it->second.ratings.push_back(rating);
  
  double sum = 0.0;
  for (const auto& r : it->second.ratings) {
    sum += r.score;
  }
  it->second.average_rating = sum / it->second.ratings.size();
  
  return true;
}

double PluginMarketplace::get_average_rating(const std::string& plugin_id) const {
  std::shared_lock<std::shared_mutex> lock(marketplace_mutex_);
  auto it = plugins_.find(plugin_id);
  if (it != plugins_.end()) return it->second.average_rating;
  return 0.0;
}

std::vector<Rating> PluginMarketplace::get_ratings(const std::string& plugin_id) const {
  std::shared_lock<std::shared_mutex> lock(marketplace_mutex_);
  auto it = plugins_.find(plugin_id);
  if (it != plugins_.end()) return it->second.ratings;
  return {};
}

int PluginMarketplace::get_rating_count(const std::string& plugin_id) const {
  std::shared_lock<std::shared_mutex> lock(marketplace_mutex_);
  auto it = plugins_.find(plugin_id);
  if (it != plugins_.end()) return it->second.ratings.size();
  return 0;
}

std::vector<MarketplaceDependency> PluginMarketplace::resolve_dependencies(
    const std::string& plugin_id,
    const std::string& version) const {
  std::shared_lock<std::shared_mutex> lock(marketplace_mutex_);
  auto it = dependencies_.find(plugin_id);
  if (it != dependencies_.end()) return it->second;
  return {};
}

bool PluginMarketplace::satisfies_dependency(const std::string& plugin_id,
                                            const std::string& min_version,
                                            const std::string& max_version) const {
  return get_version(plugin_id, min_version).has_value();
}

void PluginMarketplace::increment_download(const std::string& plugin_id,
                                          const std::string& version) {
  std::unique_lock<std::shared_mutex> lock(marketplace_mutex_);
  auto it = plugins_.find(plugin_id);
  if (it != plugins_.end()) {
    it->second.download_count++;
  }
  
  if (!version.empty()) {
    auto vit = versions_.find(plugin_id);
    if (vit != versions_.end()) {
      for (auto& v : vit->second) {
        if (v.version == version) {
          v.download_count++;
          break;
        }
      }
    }
  }
}

int PluginMarketplace::get_download_count(const std::string& plugin_id) const {
  std::shared_lock<std::shared_mutex> lock(marketplace_mutex_);
  auto it = plugins_.find(plugin_id);
  if (it != plugins_.end()) return it->second.download_count;
  return 0;
}

int PluginMarketplace::get_version_download_count(const std::string& plugin_id,
                                                 const std::string& version) const {
  std::shared_lock<std::shared_mutex> lock(marketplace_mutex_);
  auto it = versions_.find(plugin_id);
  if (it != versions_.end()) {
    for (const auto& v : it->second) {
      if (v.version == version) return v.download_count;
    }
  }
  return 0;
}

void PluginMarketplace::set_featured(const std::string& plugin_id, bool featured) {
  std::unique_lock<std::shared_mutex> lock(marketplace_mutex_);
  auto it = plugins_.find(plugin_id);
  if (it != plugins_.end()) {
    it->second.is_featured = featured;
  }
}

void PluginMarketplace::set_verified(const std::string& plugin_id, bool verified) {
  std::unique_lock<std::shared_mutex> lock(marketplace_mutex_);
  auto it = plugins_.find(plugin_id);
  if (it != plugins_.end()) {
    it->second.is_verified = verified;
  }
}

std::map<std::string, int> PluginMarketplace::get_statistics() const {
  std::shared_lock<std::shared_mutex> lock(marketplace_mutex_);
  int total_downloads = 0;
  for (const auto& pair : plugins_) {
    total_downloads += pair.second.download_count;
  }
  return {{"total_plugins", static_cast<int>(plugins_.size())},
          {"total_downloads", total_downloads},
          {"total_versions", static_cast<int>(versions_.size())}};
}

void PluginMarketplace::clear() {
  std::unique_lock<std::shared_mutex> lock(marketplace_mutex_);
  plugins_.clear();
  versions_.clear();
  dependencies_.clear();
}

} // namespace bendcore::plugins
