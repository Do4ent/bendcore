/**
 * BendCore Keyboard Shortcuts Implementation
 */

#include "shortcuts.hpp"
#include <algorithm>
#include <cctype>

namespace bendcore {

// ============ KeyboardShortcut ============

json KeyboardShortcut::to_json() const {
  json j;
  j["shortcut"] = shortcut;
  j["commandId"] = command_id;
  j["description"] = description;
  j["enabled"] = enabled;
  j["isMacCompatible"] = is_mac_compatible;
  return j;
}

// ============ ShortcutManager ============

ShortcutManager::ShortcutManager() {
  build_default_shortcuts();
}

void ShortcutManager::build_default_shortcuts() {
  // FILE OPERATIONS
  shortcuts_.push_back({"Ctrl+N", "new-document", "New Document"});
  shortcuts_.push_back({"Ctrl+O", "open", "Open Document"});
  shortcuts_.push_back({"Ctrl+S", "save", "Save Document"});
  shortcuts_.push_back({"Ctrl+Shift+S", "save-as", "Save As"});
  shortcuts_.push_back({"Ctrl+P", "print", "Print Document"});

  // PART DESIGN
  shortcuts_.push_back({"Ctrl+Shift+K", "new-sketch", "New Sketch"});
  shortcuts_.push_back({"Ctrl+K", "edit-sketch", "Edit Sketch"});
  shortcuts_.push_back({"Ctrl+T", "new-tube", "New Tube"});
  shortcuts_.push_back({"Ctrl+E", "edit-tube", "Edit Tube"});

  // ANALYSIS
  shortcuts_.push_back({"Ctrl+G", "check-gaps", "Check Gaps"});
  shortcuts_.push_back({"Ctrl+M", "calculate-mass", "Calculate Mass"});
  shortcuts_.push_back({"Ctrl+Alt+C", "check-collisions", "Check Collisions"});

  // DRAWING
  shortcuts_.push_back({"Ctrl+D", "new-sheet", "New Sheet"});

  // Build reverse map for O(1) lookups
  for (const auto& shortcut : shortcuts_) {
    shortcut_map_[shortcut.shortcut] = shortcut.command_id;
  }
}

std::string ShortcutManager::get_command(const std::string& shortcut) const {
  std::string normalized = shortcut;
  normalize_shortcut(normalized);

  auto it = shortcut_map_.find(normalized);
  if (it != shortcut_map_.end()) {
    return it->second;
  }

  return "";
}

bool ShortcutManager::register_shortcut(const std::string& shortcut,
                                         const std::string& command_id) {
  // Check for duplicates
  if (has_shortcut(shortcut)) {
    return false;
  }

  KeyboardShortcut ks;
  ks.shortcut = shortcut;
  ks.command_id = command_id;
  ks.description = command_id;  // Default description

  shortcuts_.push_back(ks);
  shortcut_map_[shortcut] = command_id;

  return true;
}

bool ShortcutManager::unregister_shortcut(const std::string& shortcut) {
  auto it = shortcut_map_.find(shortcut);
  if (it == shortcut_map_.end()) {
    return false;
  }

  // Remove from map
  shortcut_map_.erase(it);

  // Remove from vector
  auto vec_it =
      std::find_if(shortcuts_.begin(), shortcuts_.end(),
                   [&shortcut](const KeyboardShortcut& ks) {
                     return ks.shortcut == shortcut;
                   });

  if (vec_it != shortcuts_.end()) {
    shortcuts_.erase(vec_it);
    return true;
  }

  return false;
}

bool ShortcutManager::has_shortcut(const std::string& shortcut) const {
  return shortcut_map_.find(shortcut) != shortcut_map_.end();
}

std::vector<KeyboardShortcut> ShortcutManager::get_shortcuts_for_command(
    const std::string& command_id) const {
  std::vector<KeyboardShortcut> result;
  for (const auto& ks : shortcuts_) {
    if (ks.command_id == command_id) {
      result.push_back(ks);
    }
  }
  return result;
}

json ShortcutManager::to_json() const {
  json j = json::array();
  for (const auto& ks : shortcuts_) {
    j.push_back(ks.to_json());
  }
  return j;
}

void ShortcutManager::normalize_shortcut(std::string& shortcut) const {
  // Normalize to "Ctrl+X" format (not "ctrl+x" or "CTRL+X")
  // This is a simple placeholder; real implementation would handle
  // Ctrl/Cmd differences for macOS
}

}  // namespace bendcore
