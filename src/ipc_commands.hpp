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

// Drawing
CommandResult new_sheet(const json& args);
CommandResult create_view(const json& args);
CommandResult insert_bend_table(const json& args);
CommandResult unfold_drawing(const json& args);
CommandResult add_dimension(const json& args);
CommandResult add_text(const json& args);

}  // namespace commands

}  // namespace bendcore
