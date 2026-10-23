/**
 * BendCore Settings Manager Implementation
 */

#include "settings_manager.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <cstdlib>

namespace fs = std::filesystem;

namespace bendcore {

/**
 * Get home directory path
 */
static std::string get_home_directory() {
#ifdef _WIN32
  const char* home = std::getenv("USERPROFILE");
  if (!home) {
    home = std::getenv("HOMEDRIVE");
  }
  return home ? home : "";
#else
  const char* home = std::getenv("HOME");
  return home ? home : "";
#endif
}

SettingsManager::SettingsManager() {
  std::string home = get_home_directory();
  if (home.empty()) {
    throw std::runtime_error("Cannot determine home directory");
  }

  settings_path_ = home + "/.bendcore/settings.json";
  ensure_settings_directory();
  create_defaults();
  load();
}

SettingsManager::~SettingsManager() {
  if (dirty_) {
    save();
  }
}

void SettingsManager::ensure_settings_directory() {
  try {
    fs::path settings_dir = settings_path_;
    settings_dir = settings_dir.parent_path();
    fs::create_directories(settings_dir);
  } catch (const std::exception& e) {
    std::cerr << "Error creating settings directory: " << e.what() << std::endl;
  }
}

void SettingsManager::create_defaults() {
  settings_ = json{
      {"version", "1.0.0"},
      {"currentProfile", "Default"},
      {"currentWorkspaceProfile", ""},
      {
          "toolbar",
          {
              {"buttons",
               {
                   "new-document",
                   "open-document",
                   "save-document",
                   "separator",
                   "undo",
                   "redo",
                   "separator",
                   "new-tube",
                   "new-sketch",
                   "new-component",
                   "separator",
                   "check-collisions",
                   "check-gaps",
               }},
              {"homeTabButtons",
               {
                   "new-document",
                   "open-document",
                   "save-document",
                   "print-document",
               }},
              {"assemblyTabButtons",
               {
                   "insert-component",
                   "new-port",
                   "trace-tube",
                   "add-support",
               }},
              {"drawingTabButtons",
               {
                   "new-sheet",
                   "create-view",
                   "insert-bend-table",
               }},
          }
      },
      {
          "theme",
          {
              {"name", "Light"},
              {"preset", "light"},
              {
                  "colors",
                  {
                      {"tube", "#1F6FC5"},
                      {"create", "#2E9E5B"},
                      {"delete", "#D93A3A"},
                      {"import", "#0E97A3"},
                      {"links", "#8A4FD8"},
                      {"edit", "#E8820C"},
                      {"update", "#F5B517"},
                      {"freeze", "#2D9CDB"},
                      {"auxiliary", "#C9920A"},
                      {"axes_x", "#FF0000"},
                      {"axes_y", "#00FF00"},
                      {"axes_z", "#0000FF"},
                      {"neutral", "#757575"},
                  }
              }
          }
      },
      {"profiles", json::object()},
      {"workspaceProfiles", json::object()},
      {"shortcuts", json::object()},
  };
}

void SettingsManager::migrate_settings() {
  // Handle version upgrades
  if (settings_.contains("version")) {
    std::string version = settings_["version"].get<std::string>();
    if (version == "1.0.0") {
      // Current version, no migration needed
    }
  } else {
    // Very old version or missing, upgrade to 1.0.0
    settings_["version"] = "1.0.0";
  }
}

json SettingsManager::load_from_file() {
  try {
    std::ifstream file(settings_path_);
    if (!file.is_open()) {
      return json::object();
    }

    json loaded;
    file >> loaded;
    return loaded;
  } catch (const std::exception& e) {
    std::cerr << "Error loading settings: " << e.what() << std::endl;
    return json::object();
  }
}

bool SettingsManager::load() {
  try {
    json loaded = load_from_file();
    if (!loaded.empty()) {
      settings_ = loaded;
      migrate_settings();
    }
    dirty_ = false;
    return true;
  } catch (const std::exception& e) {
    std::cerr << "Error loading settings: " << e.what() << std::endl;
    return false;
  }
}

bool SettingsManager::save_to_file() {
  try {
    std::ofstream file(settings_path_);
    if (!file.is_open()) {
      return false;
    }

    file << settings_.dump(2);  // Pretty-print with 2-space indent
    file.close();
    return true;
  } catch (const std::exception& e) {
    std::cerr << "Error saving settings: " << e.what() << std::endl;
    return false;
  }
}

bool SettingsManager::save() {
  bool result = save_to_file();
  if (result) {
    dirty_ = false;
  }
  return result;
}

json SettingsManager::get(const std::string& key) const {
  auto it = settings_.find(key);
  if (it != settings_.end()) {
    return *it;
  }
  return json::null();
}

void SettingsManager::set(const std::string& key, const json& value) {
  settings_[key] = value;
  dirty_ = true;
}

bool SettingsManager::has(const std::string& key) const {
  return settings_.contains(key);
}

json SettingsManager::get_all() const {
  return settings_;
}

void SettingsManager::reset_to_defaults() {
  create_defaults();
  dirty_ = true;
  save();
}

std::string SettingsManager::get_settings_path() const {
  return settings_path_;
}

std::vector<std::string> SettingsManager::get_toolbar_buttons() const {
  try {
    auto toolbar = settings_["toolbar"];
    if (toolbar.contains("buttons")) {
      return toolbar["buttons"].get<std::vector<std::string>>();
    }
  } catch (const std::exception& e) {
    std::cerr << "Error getting toolbar buttons: " << e.what() << std::endl;
  }
  return {};
}

void SettingsManager::set_toolbar_buttons(
    const std::vector<std::string>& buttons) {
  settings_["toolbar"]["buttons"] = buttons;
  dirty_ = true;
}

std::string SettingsManager::get_current_theme() const {
  try {
    if (settings_.contains("theme")) {
      auto theme = settings_["theme"];
      if (theme.contains("name")) {
        return theme["name"].get<std::string>();
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Error getting theme: " << e.what() << std::endl;
  }
  return "Light";
}

void SettingsManager::set_current_theme(const std::string& theme_name) {
  if (settings_.contains("theme")) {
    settings_["theme"]["name"] = theme_name;
  }
  dirty_ = true;
}

std::string SettingsManager::get_current_profile() const {
  try {
    if (settings_.contains("currentProfile")) {
      return settings_["currentProfile"].get<std::string>();
    }
  } catch (const std::exception& e) {
    std::cerr << "Error getting current profile: " << e.what() << std::endl;
  }
  return "Default";
}

void SettingsManager::set_current_profile(const std::string& profile_name) {
  settings_["currentProfile"] = profile_name;
  dirty_ = true;
}

std::string SettingsManager::get_shortcut(const std::string& command_id) const {
  try {
    if (settings_.contains("shortcuts")) {
      auto shortcuts = settings_["shortcuts"];
      if (shortcuts.contains(command_id)) {
        return shortcuts[command_id].get<std::string>();
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Error getting shortcut: " << e.what() << std::endl;
  }
  return "";
}

void SettingsManager::set_shortcut(const std::string& command_id,
                                   const std::string& key_sequence) {
  if (!settings_.contains("shortcuts")) {
    settings_["shortcuts"] = json::object();
  }
  settings_["shortcuts"][command_id] = key_sequence;
  dirty_ = true;
}

std::string SettingsManager::export_settings() const {
  return settings_.dump(2);
}

bool SettingsManager::import_settings(const std::string& json_string) {
  try {
    json imported = json::parse(json_string);
    settings_ = imported;
    migrate_settings();
    dirty_ = true;
    save();
    return true;
  } catch (const std::exception& e) {
    std::cerr << "Error importing settings: " << e.what() << std::endl;
    return false;
  }
}

}  // namespace bendcore
