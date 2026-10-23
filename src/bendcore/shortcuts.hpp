/**
 * BendCore Keyboard Shortcuts System
 * Maps keyboard shortcuts to commands
 * Part of Phase 31 foundational work
 */

#pragma once

#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace bendcore {

using json = nlohmann::json;

/**
 * KeyboardShortcut: Single keyboard shortcut mapping
 */
struct KeyboardShortcut {
  std::string shortcut;   // e.g., "Ctrl+N", "Cmd+S"
  std::string command_id; // e.g., "new-document"
  std::string description;
  bool enabled = true;
  bool is_mac_compatible = true;  // Works on macOS

  json to_json() const;
};

/**
 * ShortcutManager: Maps shortcuts to commands
 */
class ShortcutManager {
 public:
  static ShortcutManager& instance() {
    static ShortcutManager manager;
    return manager;
  }

  // Get command for shortcut
  std::string get_command(const std::string& shortcut) const;

  // Get all shortcuts
  const std::vector<KeyboardShortcut>& get_all_shortcuts() const {
    return shortcuts_;
  }

  // Register custom shortcut
  bool register_shortcut(const std::string& shortcut,
                         const std::string& command_id);

  // Unregister shortcut
  bool unregister_shortcut(const std::string& shortcut);

  // Check if shortcut exists
  bool has_shortcut(const std::string& shortcut) const;

  // Get shortcuts for command
  std::vector<KeyboardShortcut> get_shortcuts_for_command(
      const std::string& command_id) const;

  // Serialize to JSON
  json to_json() const;

 private:
  ShortcutManager();
  void build_default_shortcuts();
  void normalize_shortcut(std::string& shortcut) const;

  std::vector<KeyboardShortcut> shortcuts_;
  std::map<std::string, std::string> shortcut_map_;  // shortcut -> command_id
};

}  // namespace bendcore
