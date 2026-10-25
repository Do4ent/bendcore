/**
 * BendCore Context Manager
 * Handles context menus and right-click actions
 * Phase 32: Professional UI
 */

#ifndef BENDCORE_CONTEXT_MANAGER_HPP
#define BENDCORE_CONTEXT_MANAGER_HPP

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <nlohmann/json.hpp>

namespace bendcore {

using json = nlohmann::json;

/**
 * Context menu action
 */
struct ContextAction {
  std::string id;
  std::string label;
  std::string icon;
  std::string shortcut;
  bool disabled = false;
  bool divider = false;
  std::vector<ContextAction> submenu;
};

/**
 * Context manager for right-click menus
 */
class ContextManager {
 public:
  ContextManager();
  ~ContextManager();

  // Action handlers
  using ActionHandler = std::function<void(const std::string&)>;

  // Register action handler
  void register_action_handler(const std::string& action_id, ActionHandler handler);

  // Execute action
  void execute_action(const std::string& action_id);

  // Get context menu for different object types
  std::vector<ContextAction> get_tube_context_menu() const;
  std::vector<ContextAction> get_vertex_context_menu() const;
  std::vector<ContextAction> get_port_context_menu() const;
  std::vector<ContextAction> get_empty_space_context_menu() const;
  std::vector<ContextAction> get_tree_node_context_menu() const;
  std::vector<ContextAction> get_command_confirmation_menu() const;

  // Convert to JSON for IPC
  json menu_to_json(const std::vector<ContextAction>& menu) const;

 private:
  std::map<std::string, ActionHandler> action_handlers_;

  // Menu builders
  std::vector<ContextAction> build_tube_menu() const;
  std::vector<ContextAction> build_vertex_menu() const;
  std::vector<ContextAction> build_port_menu() const;
  std::vector<ContextAction> build_empty_space_menu() const;
  std::vector<ContextAction> build_tree_node_menu() const;
  std::vector<ContextAction> build_command_confirmation_menu() const;

  // Helper
  nlohmann::json action_to_json(const ContextAction& action) const;
};

}  // namespace bendcore

#endif  // BENDCORE_CONTEXT_MANAGER_HPP
