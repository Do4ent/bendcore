#ifndef BENDCORE_PLUGINS_EXTENSION_POINTS_HPP
#define BENDCORE_PLUGINS_EXTENSION_POINTS_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <optional>
#include <shared_mutex>

namespace bendcore::plugins {

/**
 * Extension point interface
 */
struct ExtensionPoint {
  std::string id;                      // Unique extension point ID
  std::string name;                    // Display name
  std::string description;             // Description
  std::string version;                 // API version
  std::vector<std::string> capabilities; // Supported capabilities
  bool is_experimental = false;        // Experimental API flag
};

/**
 * Extension data
 */
struct Extension {
  std::string id;
  std::string extension_point_id;
  std::string name;
  std::string plugin_id;
  std::string version;
  std::map<std::string, std::string> properties;
  int priority = 0;
  bool is_enabled = true;
};

/**
 * Extension registry and management
 */
class ExtensionRegistry {
 public:
  static ExtensionRegistry& instance();

  bool register_extension_point(const ExtensionPoint& point);
  bool unregister_extension_point(const std::string& point_id);
  std::optional<ExtensionPoint> get_extension_point(const std::string& point_id) const;
  std::vector<ExtensionPoint> get_all_extension_points() const;
  bool has_extension_point(const std::string& point_id) const;

  bool register_extension(const Extension& extension);
  bool unregister_extension(const std::string& extension_id);
  std::optional<Extension> get_extension(const std::string& extension_id) const;
  std::vector<Extension> get_extensions_for_point(const std::string& point_id) const;
  std::vector<Extension> get_extensions_by_plugin(const std::string& plugin_id) const;
  
  int get_extension_count(const std::string& point_id) const;
  void enable_extension(const std::string& extension_id);
  void disable_extension(const std::string& extension_id);
  bool is_extension_enabled(const std::string& extension_id) const;
  
  std::vector<Extension> get_sorted_extensions(const std::string& point_id) const;
  void clear();

 private:
  ExtensionRegistry() = default;
  ~ExtensionRegistry() = default;

  mutable std::shared_mutex registry_mutex_;
  std::map<std::string, ExtensionPoint> extension_points_;
  std::map<std::string, Extension> extensions_;
};

} // namespace bendcore::plugins

#endif // BENDCORE_PLUGINS_EXTENSION_POINTS_HPP
