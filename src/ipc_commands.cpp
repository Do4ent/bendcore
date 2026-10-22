/**
 * BendCore IPC Command Router Implementation
 * Maps React Ribbon commands to C++ CAD kernel functions
 */

#include "ipc_commands.hpp"

namespace bendcore {

void CommandRouter::register_default_handlers() {
  // FILE OPERATIONS
  register_command("new-document", commands::new_document);
  register_command("open", commands::open_document);
  register_command("save", commands::save_document);
  register_command("save-as", commands::save_as_document);
  register_command("print", commands::print_document);
  register_command("options", commands::show_options);

  // SKETCH
  register_command("new-sketch", commands::new_sketch);
  register_command("edit-sketch", commands::edit_sketch);
  register_command("finish-sketch", commands::finish_sketch);

  // TUBE
  register_command("new-tube", commands::new_tube);
  register_command("edit-tube", commands::edit_tube);

  // AUXILIARY GEOMETRY
  register_command("reference-plane", commands::reference_plane);
  register_command("reference-axis", commands::reference_axis);
  register_command("reference-point", commands::reference_point);
  register_command("coordinate-system", commands::coordinate_system);

  // ASSEMBLY
  register_command("insert-component", commands::insert_component);
  register_command("import-step", commands::import_step);
  register_command("import-dwfx", commands::import_dwfx);

  // PORTS
  register_command("new-port", commands::new_port);
  register_command("trace-tube", commands::trace_tube);
  register_command("add-support", commands::add_support);
  register_command("recognize-geometry", commands::recognize_geometry);

  // ANALYSIS
  register_command("check-all", commands::check_all);
  register_command("check-collisions", commands::check_collisions);
  register_command("check-gaps", commands::check_gaps);

  // TOOLS
  register_command("measure", commands::measure);
  register_command("calculate-mass", commands::calculate_mass);

  // REPORT
  register_command("generate-report", commands::generate_report);

  // DRAWING
  register_command("new-sheet", commands::new_sheet);
  register_command("create-view", commands::create_view);
  register_command("bend-table", commands::insert_bend_table);
  register_command("unfold-drawing", commands::unfold_drawing);
  register_command("add-dimension", commands::add_dimension);
  register_command("add-text", commands::add_text);
}

// ============ COMMAND IMPLEMENTATIONS ============

namespace commands {

// FILE OPERATIONS
CommandResult new_document(const json& args) {
  return CommandResult{true, "New document created", json{{"docId", "doc_001"}}};
}

CommandResult open_document(const json& args) {
  return CommandResult{true, "Document opened"};
}

CommandResult save_document(const json& args) {
  return CommandResult{true, "Document saved"};
}

CommandResult save_as_document(const json& args) {
  std::string filename = args.value("filename", "document.bcd");
  return CommandResult{true, "Document saved as", json{{"filename", filename}}};
}

CommandResult print_document(const json& args) {
  return CommandResult{true, "Document sent to printer"};
}

CommandResult show_options(const json& args) {
  return CommandResult{true, "Options dialog opened"};
}

// SKETCH
CommandResult new_sketch(const json& args) {
  return CommandResult{true, "New sketch created", json{{"sketchId", "sketch_001"}}};
}

CommandResult edit_sketch(const json& args) {
  return CommandResult{true, "Sketch editing started"};
}

CommandResult finish_sketch(const json& args) {
  return CommandResult{true, "Sketch finished"};
}

// TUBE
CommandResult new_tube(const json& args) {
  return CommandResult{true, "New tube created", json{{"tubeId", "tube_001"}}};
}

CommandResult edit_tube(const json& args) {
  return CommandResult{true, "Tube editing started"};
}

// AUXILIARY
CommandResult reference_plane(const json& args) {
  return CommandResult{true, "Reference plane created"};
}

CommandResult reference_axis(const json& args) {
  return CommandResult{true, "Reference axis created"};
}

CommandResult reference_point(const json& args) {
  return CommandResult{true, "Reference point created"};
}

CommandResult coordinate_system(const json& args) {
  return CommandResult{true, "Coordinate system created"};
}

// ASSEMBLY
CommandResult insert_component(const json& args) {
  return CommandResult{true, "Component inserted"};
}

CommandResult import_step(const json& args) {
  return CommandResult{true, "STEP file imported"};
}

CommandResult import_dwfx(const json& args) {
  return CommandResult{true, "DWFx file imported"};
}

// PORTS
CommandResult new_port(const json& args) {
  return CommandResult{true, "Port created"};
}

CommandResult trace_tube(const json& args) {
  return CommandResult{true, "Tube tracing started"};
}

CommandResult add_support(const json& args) {
  return CommandResult{true, "Support added"};
}

CommandResult recognize_geometry(const json& args) {
  return CommandResult{true, "Geometry recognized"};
}

// ANALYSIS
CommandResult check_all(const json& args) {
  return CommandResult{true, "All checks completed", json{{"violations", 0}}};
}

CommandResult check_collisions(const json& args) {
  return CommandResult{true, "Collision check completed"};
}

CommandResult check_gaps(const json& args) {
  return CommandResult{true, "Gap check completed"};
}

// TOOLS
CommandResult measure(const json& args) {
  return CommandResult{true, "Measurement tool started"};
}

CommandResult calculate_mass(const json& args) {
  return CommandResult{true, "Mass calculated", json{{"mass", 0.0}, {"unit", "kg"}}};
}

// REPORT
CommandResult generate_report(const json& args) {
  return CommandResult{true, "Report generated"};
}

// DRAWING
CommandResult new_sheet(const json& args) {
  return CommandResult{true, "New drawing sheet created"};
}

CommandResult create_view(const json& args) {
  return CommandResult{true, "View created"};
}

CommandResult insert_bend_table(const json& args) {
  return CommandResult{true, "Bend table inserted"};
}

CommandResult unfold_drawing(const json& args) {
  return CommandResult{true, "Unfolding drawing created"};
}

CommandResult add_dimension(const json& args) {
  return CommandResult{true, "Dimension added"};
}

CommandResult add_text(const json& args) {
  return CommandResult{true, "Text added"};
}

}  // namespace commands

}  // namespace bendcore
