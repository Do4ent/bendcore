/**
 * BendCore Context Manager Implementation
 * Handles context menus and right-click actions
 */

#include "context_manager.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace bendcore {

ContextManager::ContextManager() {
  // Register default action handlers
}

ContextManager::~ContextManager() = default;

void ContextManager::register_action_handler(const std::string& action_id,
                                             ActionHandler handler) {
  action_handlers_[action_id] = handler;
}

void ContextManager::execute_action(const std::string& action_id) {
  auto it = action_handlers_.find(action_id);
  if (it != action_handlers_.end()) {
    it->second(action_id);
  }
}

std::vector<ContextAction> ContextManager::get_tube_context_menu() const {
  return build_tube_menu();
}

std::vector<ContextAction> ContextManager::get_vertex_context_menu() const {
  return build_vertex_menu();
}

std::vector<ContextAction> ContextManager::get_port_context_menu() const {
  return build_port_menu();
}

std::vector<ContextAction> ContextManager::get_empty_space_context_menu() const {
  return build_empty_space_menu();
}

std::vector<ContextAction> ContextManager::get_tree_node_context_menu() const {
  return build_tree_node_menu();
}

std::vector<ContextAction> ContextManager::get_command_confirmation_menu() const {
  return build_command_confirmation_menu();
}

std::vector<ContextAction> ContextManager::build_tube_menu() const {
  return {
      {"bend-table", "Bend Table", "📊", "Ctrl+Shift+B", false, false, {}},
      {"edit-spec", "Edit Specification", "✎", "Ctrl+E", false, false, {}},
      {"divider", "", "", "", false, true, {}},
      {"show-deps", "Show Dependencies", "🔗", "", false, false, {}},
      {"divider", "", "", "", false, true, {}},
      {"hide", "Hide", "👁️", "", false, false, {}},
      {"isolate", "Isolate", "⊙", "", false, false, {}},
      {"divider", "", "", "", false, true, {}},
      {"properties", "Properties", "⚙", "", false, false, {}},
  };
}

std::vector<ContextAction> ContextManager::build_vertex_menu() const {
  return {
      {"edit", "Edit", "✎", "F2", false, false, {}},
      {"delete", "Delete", "✕", "Delete", false, false, {}},
      {"divider", "", "", "", false, true, {}},
      {"show-deps", "Show Dependencies", "🔗", "", false, false, {}},
      {"divider", "", "", "", false, true, {}},
      {"properties", "Properties", "⚙", "", false, false, {}},
  };
}

std::vector<ContextAction> ContextManager::build_port_menu() const {
  return {
      {"edit-params", "Edit Parameters", "✎", "", false, false, {}},
      {"delete", "Delete", "✕", "Delete", false, false, {}},
      {"hide", "Hide", "👁️", "", false, false, {}},
      {"divider", "", "", "", false, true, {}},
      {"properties", "Properties", "⚙", "", false, false, {}},
  };
}

std::vector<ContextAction> ContextManager::build_empty_space_menu() const {
  return {
      {"new-tube", "New Tube", "➕", "Ctrl+T", false, false, {}},
      {"new-sketch", "New Sketch", "✏", "Ctrl+Shift+K", false, false, {}},
      {"aux-geometry", "Aux Geometry", "⊞", "", false, false, {}},
      {"paste", "Paste", "📋", "Ctrl+V", true, false, {}},
  };
}

std::vector<ContextAction> ContextManager::build_tree_node_menu() const {
  return {
      {"rename", "Rename", "✎", "F2", false, false, {}},
      {"edit", "Edit", "✎", "", false, false, {}},
      {"delete", "Delete", "✕", "Delete", false, false, {}},
      {"divider", "", "", "", false, true, {}},
      {"duplicate", "Duplicate", "⧉", "Ctrl+D", false, false, {}},
      {"divider", "", "", "", false, true, {}},
      {"hide", "Hide", "👁️", "", false, false, {}},
      {"show", "Show", "👁️", "", false, false, {}},
      {"isolate", "Isolate", "⊙", "", false, false, {}},
      {"divider", "", "", "", false, true, {}},
      {"show-deps", "Show Dependencies", "🔗", "", false, false, {}},
      {"properties", "Properties", "⚙", "", false, false, {}},
  };
}

std::vector<ContextAction> ContextManager::build_command_confirmation_menu() const {
  return {
      {"ok", "✓ OK", "", "Enter", false, false, {}},
      {"cancel", "✗ Cancel", "", "Esc", false, false, {}},
      {"divider", "", "", "", false, true, {}},
      {"repeat", "↻ Repeat", "", "", false, false, {}},
  };
}

json ContextManager::action_to_json(const ContextAction& action) const {
  json j;
  j["id"] = action.id;
  j["label"] = action.label;
  j["icon"] = action.icon;
  j["shortcut"] = action.shortcut;
  j["disabled"] = action.disabled;
  j["divider"] = action.divider;

  if (!action.submenu.empty()) {
    j["submenu"] = json::array();
    for (const auto& sub : action.submenu) {
      j["submenu"].push_back(action_to_json(sub));
    }
  }

  return j;
}

json ContextManager::menu_to_json(const std::vector<ContextAction>& menu) const {
  json j = json::array();
  for (const auto& action : menu) {
    j.push_back(action_to_json(action));
  }
  return j;
}

}  // namespace bendcore
