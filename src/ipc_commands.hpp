/**
 * BendCore IPC Command Router
 * Maps React commands to C++ CAD kernel functions
 * Interfaces between Electron IPC and BendCore core
 */

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

namespace bendcore {

using json = nlohmann::json;

/**
 * CommandResult: Return value from command handlers
 */
struct CommandResult {
  bool success = true;
  std::string message;
  json data;

  json to_json() const {
    return json{
        {"success", success},
        {"message", message},
        {"data", data},
    };
  }
};

/**
 * CommandHandler: Function signature for command handlers
 */
using CommandHandler = std::function<CommandResult(const json& args)>;

/**
 * CommandRouter: Maps command IDs to C++ handlers
 */
class CommandRouter {
 public:
  static CommandRouter& instance() {
    static CommandRouter router;
    return router;
  }

  // Register a command handler
  void register_command(const std::string& command_id, CommandHandler handler) {
    handlers_[command_id] = handler;
  }

  // Execute a command
  CommandResult execute(const std::string& command_id, const json& args) {
    auto it = handlers_.find(command_id);
    if (it == handlers_.end()) {
      return CommandResult{false, "Unknown command: " + command_id};
    }

    try {
      return it->second(args);
    } catch (const std::exception& e) {
      return CommandResult{false, std::string("Command error: ") + e.what()};
    }
  }

 private:
  CommandRouter() { register_default_handlers(); }

  void register_default_handlers();

  std::map<std::string, CommandHandler> handlers_;
};

// Command handler implementations (defined in ipc_commands.cpp)
namespace commands {

// File operations
CommandResult new_document(const json& args);
CommandResult open_document(const json& args);
CommandResult save_document(const json& args);
CommandResult save_as_document(const json& args);
CommandResult print_document(const json& args);
CommandResult show_options(const json& args);

// Part design
CommandResult new_sketch(const json& args);
CommandResult edit_sketch(const json& args);
CommandResult finish_sketch(const json& args);

CommandResult new_tube(const json& args);
CommandResult edit_tube(const json& args);

CommandResult reference_plane(const json& args);
CommandResult reference_axis(const json& args);
CommandResult reference_point(const json& args);
CommandResult coordinate_system(const json& args);

// Assembly
CommandResult insert_component(const json& args);
CommandResult import_step(const json& args);
CommandResult import_dwfx(const json& args);

CommandResult new_port(const json& args);
CommandResult trace_tube(const json& args);
CommandResult add_support(const json& args);
CommandResult recognize_geometry(const json& args);

// Analysis
CommandResult check_all(const json& args);
CommandResult check_collisions(const json& args);
CommandResult check_gaps(const json& args);

CommandResult measure(const json& args);
CommandResult calculate_mass(const json& args);
CommandResult generate_report(const json& args);

// Materials
CommandResult get_materials(const json& args);
CommandResult get_material(const json& args);
CommandResult get_materials_by_category(const json& args);
CommandResult add_custom_material(const json& args);

// Feature Tree & Search (Phase 31)
CommandResult get_feature_tree(const json& args);
CommandResult search_commands(const json& args);
CommandResult get_shortcuts(const json& args);

// Drawing
CommandResult new_sheet(const json& args);
CommandResult create_view(const json& args);
CommandResult insert_bend_table(const json& args);
CommandResult unfold_drawing(const json& args);
CommandResult add_dimension(const json& args);
CommandResult add_text(const json& args);

// Context Menu & Properties (Phase 32)
CommandResult get_context_menu(const json& args);
CommandResult execute_context_action(const json& args);
CommandResult get_properties(const json& args);
CommandResult set_property(const json& args);
CommandResult get_status(const json& args);

// Editing Commands (Phase 32)
CommandResult select_all(const json& args);
CommandResult undo(const json& args);
CommandResult redo(const json& args);
CommandResult cut(const json& args);
CommandResult copy(const json& args);
CommandResult paste(const json& args);
CommandResult delete_selected(const json& args);
CommandResult rename(const json& args);
CommandResult toggle_snap(const json& args);
CommandResult find(const json& args);
CommandResult show_history(const json& args);

// Viewport & Camera Control (Phase 33)
CommandResult set_camera_position(const json& args);
CommandResult get_camera_position(const json& args);
CommandResult orbit_camera(const json& args);
CommandResult pan_camera(const json& args);
CommandResult zoom_camera(const json& args);
CommandResult fit_to_view(const json& args);
CommandResult set_snap_mode(const json& args);
CommandResult get_snap_points(const json& args);

// Settings & Profiles (Phase 34)
CommandResult get_settings(const json& args);
CommandResult set_settings(const json& args);
CommandResult load_profile(const json& args);
CommandResult save_profile(const json& args);
CommandResult get_icon_theme(const json& args);
CommandResult set_icon_theme(const json& args);

// Path Sketch 3D Editor (Phase 35B)
CommandResult start_path_sketch(const json& args);
CommandResult add_sketch_vertex(const json& args);
CommandResult create_sketch_segment(const json& args);
CommandResult set_dimension(const json& args);
CommandResult add_constraint(const json& args);
CommandResult solve_constraints(const json& args);
CommandResult validate_path(const json& args);
CommandResult export_path_sketch(const json& args);

// Enterprise Features (Phase 36)
// Organization Profile Management
CommandResult list_org_profiles(const json& args);
CommandResult get_org_profile(const json& args);
CommandResult create_org_profile(const json& args);
CommandResult update_org_profile(const json& args);
CommandResult activate_org_profile(const json& args);
CommandResult archive_org_profile(const json& args);

// Parametric Pattern Templates
CommandResult get_pattern_templates(const json& args);
CommandResult create_from_pattern(const json& args);
CommandResult get_pattern_params(const json& args);
CommandResult preview_pattern(const json& args);

// Import & Geometry Recognition
CommandResult import_step_file(const json& args);
CommandResult detect_tubes_in_import(const json& args);
CommandResult apply_grouping_rules(const json& args);

// Geometry Rounding
CommandResult round_geometry(const json& args);
CommandResult get_rounding_preview(const json& args);

}  // namespace commands

}  // namespace bendcore
