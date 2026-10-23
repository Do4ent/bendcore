/**
 * BendCore Settings Manager
 * Persistent storage and management of application settings
 * File: ~/.bendcore/settings.json
 */

#pragma once

#include <string>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>

namespace bendcore {

using json = nlohmann::json;

/**
 * Settings Manager: Handles persistent application settings
 */
class SettingsManager {
 public:
  static SettingsManager& instance() {
    static SettingsManager manager;
    return manager;
  }

  /**
   * Load settings from disk
   */
  bool load();

  /**
   * Save settings to disk
   */
  bool save();

  /**
   * Get a setting value
   */
  json get(const std::string& key) const;

  /**
   * Set a setting value
   */
  void set(const std::string& key, const json& value);

  /**
   * Check if a setting exists
   */
  bool has(const std::string& key) const;

  /**
   * Get all settings
   */
  json get_all() const;

  /**
   * Reset to default settings
   */
  void reset_to_defaults();

  /**
   * Get settings file path
   */
  std::string get_settings_path() const;

  /**
   * Get toolbar buttons
   */
  std::vector<std::string> get_toolbar_buttons() const;

  /**
   * Set toolbar buttons
   */
  void set_toolbar_buttons(const std::vector<std::string>& buttons);

  /**
   * Get current theme name
   */
  std::string get_current_theme() const;

  /**
   * Set current theme
   */
  void set_current_theme(const std::string& theme_name);

  /**
   * Get current profile
   */
  std::string get_current_profile() const;

  /**
   * Set current profile
   */
  void set_current_profile(const std::string& profile_name);

  /**
   * Get keyboard shortcut
   */
  std::string get_shortcut(const std::string& command_id) const;

  /**
   * Set keyboard shortcut
   */
  void set_shortcut(const std::string& command_id, const std::string& key_sequence);

  /**
   * Export settings as JSON string
   */
  std::string export_settings() const;

  /**
   * Import settings from JSON string
   */
  bool import_settings(const std::string& json_string);

 private:
  SettingsManager();
  ~SettingsManager();

  // No copy or move
  SettingsManager(const SettingsManager&) = delete;
  SettingsManager& operator=(const SettingsManager&) = delete;

  /**
   * Create default settings structure
   */
  void create_defaults();

  /**
   * Migrate settings from older versions
   */
  void migrate_settings();

  /**
   * Ensure settings directory exists
   */
  void ensure_settings_directory();

  /**
   * Load settings from file
   */
  json load_from_file();

  /**
   * Save settings to file
   */
  bool save_to_file();

  json settings_;
  std::string settings_path_;
  bool dirty_ = false;
};

}  // namespace bendcore
