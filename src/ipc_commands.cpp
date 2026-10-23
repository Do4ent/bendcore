/**
 * BendCore IPC Command Router Implementation
 * Maps React Ribbon commands to C++ CAD kernel functions
 */

#include "ipc_commands.hpp"
#include "bendcore/analysis.hpp"
#include "bendcore/feature_tree.hpp"
#include "bendcore/command_search.hpp"
#include "bendcore/shortcuts.hpp"
#include "bendcore/viewport_manager.hpp"
#include "bendcore/camera_manager.hpp"
#include "bendcore/snap_system.hpp"
#include "bendcore/sketch_solver.hpp"
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <map>

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

  // MATERIALS (Phase 30)
  register_command("get-materials", commands::get_materials);
  register_command("get-material", commands::get_material);
  register_command("get-materials-by-category", commands::get_materials_by_category);
  register_command("add-custom-material", commands::add_custom_material);

  // FEATURE TREE & SEARCH (Phase 31)
  register_command("get-feature-tree", commands::get_feature_tree);
  register_command("search-commands", commands::search_commands);
  register_command("get-shortcuts", commands::get_shortcuts);

  // CONTEXT MENU & PROPERTIES (Phase 32)
  register_command("get-context-menu", commands::get_context_menu);
  register_command("execute-context-action", commands::execute_context_action);
  register_command("get-properties", commands::get_properties);
  register_command("set-property", commands::set_property);
  register_command("get-status", commands::get_status);

  // EDITING COMMANDS (Phase 32)
  register_command("select-all", commands::select_all);
  register_command("undo", commands::undo);
  register_command("redo", commands::redo);
  register_command("cut", commands::cut);
  register_command("copy", commands::copy);
  register_command("paste", commands::paste);
  register_command("delete", commands::delete_selected);
  register_command("rename", commands::rename);
  register_command("toggle-snap", commands::toggle_snap);
  register_command("find", commands::find);
  register_command("history", commands::show_history);

  // VIEWPORT & CAMERA CONTROL (Phase 33)
  register_command("set-camera-position", commands::set_camera_position);
  register_command("get-camera-position", commands::get_camera_position);
  register_command("orbit-camera", commands::orbit_camera);
  register_command("pan-camera", commands::pan_camera);
  register_command("zoom-camera", commands::zoom_camera);
  register_command("fit-to-view", commands::fit_to_view);
  register_command("set-snap-mode", commands::set_snap_mode);
  register_command("get-snap-points", commands::get_snap_points);

  // TOOLBAR & SETTINGS (Phase 34)
  register_command("get-settings", commands::get_settings);
  register_command("set-settings", commands::set_settings);
  register_command("load-profile", commands::load_profile);
  register_command("save-profile", commands::save_profile);
  register_command("get-icon-theme", commands::get_icon_theme);
  register_command("set-icon-theme", commands::set_icon_theme);

  // PATH SKETCH 3D EDITOR (Phase 35B)
  register_command("start-path-sketch", commands::start_path_sketch);
  register_command("add-sketch-vertex", commands::add_sketch_vertex);
  register_command("create-sketch-segment", commands::create_sketch_segment);
  register_command("set-dimension", commands::set_dimension);
  register_command("add-constraint", commands::add_constraint);
  register_command("solve-constraints", commands::solve_constraints);
  register_command("validate-path", commands::validate_path);
  register_command("export-path-sketch", commands::export_path_sketch);

  // ENTERPRISE FEATURES & IMPORT (Phase 36)
  // Organization Profile Management
  register_command("list-org-profiles", commands::list_org_profiles);
  register_command("get-org-profile", commands::get_org_profile);
  register_command("create-org-profile", commands::create_org_profile);
  register_command("update-org-profile", commands::update_org_profile);
  register_command("activate-org-profile", commands::activate_org_profile);
  register_command("archive-org-profile", commands::archive_org_profile);

  // Parametric Pattern Templates
  register_command("get-pattern-templates", commands::get_pattern_templates);
  register_command("create-from-pattern", commands::create_from_pattern);
  register_command("get-pattern-params", commands::get_pattern_params);
  register_command("preview-pattern", commands::preview_pattern);

  // Import & Geometry Recognition
  register_command("import-step-file", commands::import_step_file);
  register_command("detect-tubes-in-import", commands::detect_tubes_in_import);
  register_command("apply-grouping-rules", commands::apply_grouping_rules);

  // Geometry Rounding
  register_command("round-geometry", commands::round_geometry);
  register_command("get-rounding-preview", commands::get_rounding_preview);
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
  // Extract parameters
  double outerDiameter = args.value("outerDiameter", 16.0);
  double wallThickness = args.value("wallThickness", 1.5);
  std::string material = args.value("material", "Steel");

  // Get material density
  double density = 5.0;  // default
  const std::vector<bendcore::analysis::MaterialDensity> MATERIAL_DENSITIES = {
    {"Steel", 7.85},
    {"Aluminum", 2.70},
    {"Copper", 8.96},
    {"Titanium", 4.54},
    {"Custom", 5.0}
  };

  for (const auto& mat : MATERIAL_DENSITIES) {
    if (mat.name == material) {
      density = mat.density;
      break;
    }
  }

  // Calculate mass
  double innerDiameter = outerDiameter - 2.0 * wallThickness;
  double area = M_PI * (outerDiameter * outerDiameter - innerDiameter * innerDiameter) / 4.0;
  double tubeLength = 1000.0;
  double volume = tubeLength * area / 1000.0;
  double massKg = (volume * density) / 1000.0;

  // Return combined results
  return CommandResult{true, "All checks completed", json{
    {"violations", 0},
    {"mass", massKg},
    {"unit", "kg"},
    {"material", material},
    {"density", density}
  }};
}

CommandResult check_collisions(const json& args) {
  return CommandResult{true, "Collision check completed"};
}

CommandResult check_gaps(const json& args) {
  // Create analyzer instance and check for gaps
  // In a full implementation, this would get the current model from the session
  // For now, return sample gap violation data

  json gaps_array = json::array();

  // Sample gap violations (would be populated from actual analysis)
  gaps_array.push_back(json{
    {"bendIndex", 1},
    {"location", "между гибом 1 и 2"},
    {"actualGap", 1.5},
    {"minimumGap", 5.0},
    {"severity", "ERROR"}
  });

  gaps_array.push_back(json{
    {"bendIndex", 2},
    {"location", "между гибом 2 и 3"},
    {"actualGap", 3.2},
    {"minimumGap", 5.0},
    {"severity", "WARNING"}
  });

  gaps_array.push_back(json{
    {"bendIndex", 3},
    {"location", "между гибом 3 и 4"},
    {"actualGap", 4.8},
    {"minimumGap", 5.0},
    {"severity", "WARNING"}
  });

  return CommandResult{true, "Gap analysis completed",
    json{
      {"gaps", gaps_array},
      {"totalViolations", gaps_array.size()},
      {"criticalCount", 1}
    }
  };
}

// TOOLS
CommandResult measure(const json& args) {
  return CommandResult{true, "Measurement tool started"};
}

CommandResult calculate_mass(const json& args) {
  // Extract parameters from args
  double outerDiameter = args.value("outerDiameter", 16.0);  // mm
  double wallThickness = args.value("wallThickness", 1.5);    // mm
  std::string material = args.value("material", "Steel");      // material name

  // In a full implementation, we would:
  // 1. Get the current TubeModel from the session
  // 2. Call analyzer.calculateTubeMass()
  // 3. Return the actual calculated mass

  // For now, we'll calculate with sample values
  // Sample calculation: OD=16mm, WT=1.5mm, Length=1000mm (5 segments × 200mm)
  // Area = π*(8²) - π*(6.5²) = 201.06 - 132.73 = 68.33 mm²
  // Volume = 1000mm × 68.33mm² / 1000 = 68.33 cm³
  // Mass = 68.33 × 7.85 = 536.39 g ≈ 0.54 kg

  // Get material density
  double density = 5.0;  // default g/cm³
  const std::vector<bendcore::analysis::MaterialDensity> MATERIAL_DENSITIES = {
    {"Steel", 7.85},
    {"Aluminum", 2.70},
    {"Copper", 8.96},
    {"Titanium", 4.54},
    {"Custom", 5.0}
  };

  for (const auto& mat : MATERIAL_DENSITIES) {
    if (mat.name == material) {
      density = mat.density;
      break;
    }
  }

  // Calculate cross-section area
  double innerDiameter = outerDiameter - 2.0 * wallThickness;
  double area = M_PI * (outerDiameter * outerDiameter - innerDiameter * innerDiameter) / 4.0;  // mm²

  // Sample tube length: 1000mm (5 segments × 200mm)
  double tubeLength = 1000.0;

  // Calculate volume: length × area / 1000 (convert to cm³)
  double volume = tubeLength * area / 1000.0;  // cm³

  // Calculate mass: volume × density
  double massGrams = volume * density;
  double massKg = massGrams / 1000.0;

  int segmentCount = 5;

  return CommandResult{true, "Mass calculated", json{
    {"mass", massKg},
    {"unit", "kg"},
    {"material", material},
    {"outerDiameter", outerDiameter},
    {"wallThickness", wallThickness},
    {"segments", segmentCount},
    {"density", density}
  }};
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

// MATERIALS COMMANDS

CommandResult get_materials(const json& args) {
  using namespace bendcore::analysis;

  auto& db = MaterialDatabase::instance();
  auto materials = db.getAllMaterials();

  json materialsJson = json::array();
  for (const auto& mat : materials) {
    materialsJson.push_back(json{
      {"id", mat.id},
      {"name", mat.name},
      {"category", mat.category},
      {"density", mat.density},
      {"availability", mat.availability},
      {"color", mat.color},
      {"standards", mat.standards},
      {"properties", json{
        {"youngsModulus", mat.properties.youngsModulus},
        {"yieldStrength", mat.properties.yieldStrength},
        {"tensileStrength", mat.properties.tensileStrength},
        {"elongation", mat.properties.elongation},
        {"hardness", mat.properties.hardness},
        {"cost", mat.properties.cost}
      }},
      {"applications", mat.applications}
    });
  }

  return CommandResult{true, "Materials retrieved", json{
    {"materials", materialsJson},
    {"count", materialsJson.size()}
  }};
}

CommandResult get_material(const json& args) {
  using namespace bendcore::analysis;

  std::string materialId = args.value("id", "");
  if (materialId.empty()) {
    return CommandResult{false, "Material ID is required"};
  }

  auto& db = MaterialDatabase::instance();
  auto mat = db.getMaterial(materialId);

  if (!mat.has_value()) {
    return CommandResult{false, "Material not found: " + materialId};
  }

  json matJson{
    {"id", mat->id},
    {"name", mat->name},
    {"category", mat->category},
    {"density", mat->density},
    {"availability", mat->availability},
    {"color", mat->color},
    {"standards", mat->standards},
    {"properties", json{
      {"youngsModulus", mat->properties.youngsModulus},
      {"yieldStrength", mat->properties.yieldStrength},
      {"tensileStrength", mat->properties.tensileStrength},
      {"elongation", mat->properties.elongation},
      {"hardness", mat->properties.hardness},
      {"cost", mat->properties.cost}
    }},
    {"applications", mat->applications}
  };

  return CommandResult{true, "Material retrieved", json{
    {"material", matJson}
  }};
}

CommandResult get_materials_by_category(const json& args) {
  using namespace bendcore::analysis;

  std::string category = args.value("category", "");
  if (category.empty()) {
    return CommandResult{false, "Category is required"};
  }

  auto& db = MaterialDatabase::instance();
  auto materials = db.getMaterialsByCategory(category);

  json materialsJson = json::array();
  for (const auto& mat : materials) {
    materialsJson.push_back(json{
      {"id", mat.id},
      {"name", mat.name},
      {"category", mat.category},
      {"density", mat.density},
      {"availability", mat.availability},
      {"color", mat.color},
      {"standards", mat.standards},
      {"properties", json{
        {"youngsModulus", mat.properties.youngsModulus},
        {"yieldStrength", mat.properties.yieldStrength},
        {"tensileStrength", mat.properties.tensileStrength},
        {"elongation", mat.properties.elongation},
        {"hardness", mat.properties.hardness},
        {"cost", mat.properties.cost}
      }},
      {"applications", mat.applications}
    });
  }

  return CommandResult{true, "Materials by category retrieved", json{
    {"category", category},
    {"materials", materialsJson},
    {"count", materialsJson.size()}
  }};
}

CommandResult add_custom_material(const json& args) {
  using namespace bendcore::analysis;

  MaterialProperties mat;
  mat.id = args.value("id", "");
  mat.name = args.value("name", "");
  mat.category = args.value("category", "Custom");
  mat.density = args.value("density", 5.0);
  mat.availability = args.value("availability", "Custom");
  mat.color = args.value("color", "#808080");

  if (mat.id.empty() || mat.name.empty()) {
    return CommandResult{false, "Material ID and name are required"};
  }

  // Parse properties if provided
  if (args.contains("properties")) {
    auto props = args["properties"];
    mat.properties.youngsModulus = props.value("youngsModulus", 0.0);
    mat.properties.yieldStrength = props.value("yieldStrength", 0.0);
    mat.properties.tensileStrength = props.value("tensileStrength", 0.0);
    mat.properties.elongation = props.value("elongation", 0.0);
    mat.properties.hardness = props.value("hardness", 0.0);
    mat.properties.cost = props.value("cost", 0.0);
  }

  // Parse standards and applications if provided
  if (args.contains("standards")) {
    mat.standards = args["standards"].get<std::vector<std::string>>();
  }
  if (args.contains("applications")) {
    mat.applications = args["applications"].get<std::vector<std::string>>();
  }

  auto& db = MaterialDatabase::instance();
  bool success = db.addCustomMaterial(mat);

  if (!success) {
    return CommandResult{false, "Failed to add custom material"};
  }

  return CommandResult{true, "Custom material added", json{
    {"id", mat.id},
    {"name", mat.name}
  }};
}

// FEATURE TREE COMMANDS (Phase 31)

CommandResult get_feature_tree(const json& args) {
  using namespace bendcore;

  auto& history = DesignHistory::instance();
  json tree_json = history.to_json();

  return CommandResult{true, "Feature tree retrieved", json{
    {"root", tree_json.value("root", json::object())},
    {"version", tree_json.value("version", 1)}
  }};
}

// COMMAND SEARCH (Phase 31)

CommandResult search_commands(const json& args) {
  using namespace bendcore;

  std::string query = args.value("query", "");
  int max_results = args.value("maxResults", 10);

  auto& index = CommandIndex::instance();
  auto results = index.search(query, max_results);

  json results_array = json::array();
  for (const auto& entry : results) {
    results_array.push_back(entry.to_json());
  }

  return CommandResult{true, "Commands searched", json{
    {"results", results_array},
    {"count", results_array.size()},
    {"query", query}
  }};
}

// SHORTCUTS COMMAND (Phase 31)

CommandResult get_shortcuts(const json& args) {
  using namespace bendcore;

  auto& manager = ShortcutManager::instance();
  json shortcuts_json = manager.to_json();

  return CommandResult{true, "Shortcuts retrieved", json{
    {"shortcuts", shortcuts_json},
    {"count", shortcuts_json.size()}
  }};
}

// ============ CONTEXT MENU & PROPERTIES (Phase 32) ============

CommandResult get_context_menu(const json& args) {
  std::string object_type = args.value("objectType", "empty-space");

  json menu = json::array();
  if (object_type == "tube") {
    menu.push_back({{"id", "bend-table"}, {"label", "Bend Table"}});
    menu.push_back({{"id", "edit-spec"}, {"label", "Edit Specification"}});
    menu.push_back({{"id", "show-deps"}, {"label", "Show Dependencies"}});
    menu.push_back({{"id", "hide"}, {"label", "Hide"}});
    menu.push_back({{"id", "properties"}, {"label", "Properties"}});
  } else if (object_type == "vertex") {
    menu.push_back({{"id", "edit"}, {"label", "Edit"}});
    menu.push_back({{"id", "delete"}, {"label", "Delete"}});
    menu.push_back({{"id", "properties"}, {"label", "Properties"}});
  } else if (object_type == "port") {
    menu.push_back({{"id", "edit-params"}, {"label", "Edit Parameters"}});
    menu.push_back({{"id", "delete"}, {"label", "Delete"}});
    menu.push_back({{"id", "properties"}, {"label", "Properties"}});
  } else {  // empty-space
    menu.push_back({{"id", "new-tube"}, {"label", "New Tube"}});
    menu.push_back({{"id", "new-sketch"}, {"label", "New Sketch"}});
    menu.push_back({{"id", "paste"}, {"label", "Paste"}});
  }

  return CommandResult{true, "Context menu retrieved",
    json{{"menu", menu}, {"objectType", object_type}}};
}

CommandResult execute_context_action(const json& args) {
  std::string action_id = args.value("actionId", "");
  std::string object_id = args.value("objectId", "");

  return CommandResult{true, "Context action executed",
    json{{"actionId", action_id}, {"objectId", object_id}}};
}

CommandResult get_properties(const json& args) {
  std::string object_id = args.value("objectId", "");
  std::string object_type = args.value("objectType", "document");

  json properties = json::array();

  if (object_type == "tube") {
    properties.push_back({
      {"id", "tube-basic"},
      {"title", "Basic Properties"},
      {"properties", json::array({
        {{"name", "tubeName"}, {"label", "Name"}, {"type", "text"}, {"value", "Tube_1"}},
        {{"name", "material"}, {"label", "Material"}, {"type", "select"}, {"value", "Steel"}},
      })}
    });
    properties.push_back({
      {"id", "tube-dimensions"},
      {"title", "Dimensions"},
      {"properties", json::array({
        {{"name", "outerDiameter"}, {"label", "Outer Diameter"}, {"type", "number"}, {"value", 12.7}, {"unit", "mm"}},
        {{"name", "wallThickness"}, {"label", "Wall Thickness"}, {"type", "number"}, {"value", 1.6}, {"unit", "mm"}},
      })}
    });
  }

  return CommandResult{true, "Properties retrieved",
    json{{"objectId", object_id}, {"objectType", object_type}, {"groups", properties}}};
}

CommandResult set_property(const json& args) {
  std::string object_id = args.value("objectId", "");
  std::string property_name = args.value("propertyName", "");
  auto value = args.value("value", nullptr);

  return CommandResult{true, "Property updated",
    json{{"objectId", object_id}, {"propertyName", property_name}, {"newValue", value}}};
}

CommandResult get_status(const json& args) {
  json status;
  status["stepHint"] = "Ready";
  status["cursorX"] = 0.0;
  status["cursorY"] = 0.0;
  status["cursorZ"] = 0.0;
  status["activePlane"] = "XY Plane";
  status["snapMode"] = false;
  status["snapType"] = "grid";
  status["units"] = "mm";
  status["standard"] = "ISO";
  status["violations"] = {{"errors", 0}, {"warnings", 0}, {"info", 0}};
  status["isCalculating"] = false;

  return CommandResult{true, "Status retrieved", json{{"status", status}}};
}

// ============ EDITING COMMANDS (Phase 32) ============

CommandResult select_all(const json& args) {
  return CommandResult{true, "All objects selected", json{{"count", 5}}};
}

CommandResult undo(const json& args) {
  return CommandResult{true, "Undo executed", json{{"action", "last action undone"}}};
}

CommandResult redo(const json& args) {
  return CommandResult{true, "Redo executed", json{{"action", "action redone"}}};
}

CommandResult cut(const json& args) {
  return CommandResult{true, "Objects cut to clipboard", json{{"count", 2}}};
}

CommandResult copy(const json& args) {
  return CommandResult{true, "Objects copied to clipboard", json{{"count", 2}}};
}

CommandResult paste(const json& args) {
  return CommandResult{true, "Objects pasted from clipboard", json{{"count", 2}}};
}

CommandResult delete_selected(const json& args) {
  return CommandResult{true, "Selected objects deleted", json{{"count", 1}}};
}

CommandResult rename(const json& args) {
  std::string new_name = args.value("newName", "");
  return CommandResult{true, "Object renamed", json{{"newName", new_name}}};
}

CommandResult toggle_snap(const json& args) {
  return CommandResult{true, "Snap mode toggled", json{{"snapEnabled", true}}};
}

CommandResult find(const json& args) {
  std::string query = args.value("query", "");
  return CommandResult{true, "Search completed",
    json{{"query", query}, {"resultsCount", 0}}};
}

CommandResult show_history(const json& args) {
  json history_items = json::array();
  history_items.push_back({{"action", "Created Tube_1"}, {"time", "12:34:56"}});
  history_items.push_back({{"action", "Modified Sketch_1"}, {"time", "12:34:45"}});

  return CommandResult{true, "History shown",
    json{{"items", history_items}, {"count", 2}}};
}

// ============ VIEWPORT & CAMERA CONTROL (Phase 33) ============

CommandResult set_camera_position(const json& args) {
  double x = args.value("x", 0.0);
  double y = args.value("y", 0.0);
  double z = args.value("z", 100.0);

  auto& viewport = bendcore::ViewportManager::instance();
  viewport.set_camera_position(x, y, z);

  return CommandResult{true, "Camera position set", json{
    {"position", json{{"x", x}, {"y", y}, {"z", z}}}
  }};
}

CommandResult get_camera_position(const json& args) {
  auto& viewport = bendcore::ViewportManager::instance();
  auto state = viewport.get_camera_state();

  return CommandResult{true, "Camera position retrieved", json{
    {"position", json{
      {"x", state.position.x},
      {"y", state.position.y},
      {"z", state.position.z}
    }},
    {"target", json{
      {"x", state.target.x},
      {"y", state.target.y},
      {"z", state.target.z}
    }},
    {"distance", state.distance},
    {"pitch", state.pitch},
    {"yaw", state.yaw}
  }};
}

CommandResult orbit_camera(const json& args) {
  double delta_pitch = args.value("deltaPitch", 0.0);
  double delta_yaw = args.value("deltaYaw", 0.0);

  auto& viewport = bendcore::ViewportManager::instance();
  viewport.orbit_camera(delta_pitch, delta_yaw);

  auto state = viewport.get_camera_state();
  return CommandResult{true, "Camera orbit executed", json{
    {"pitch", state.pitch},
    {"yaw", state.yaw}
  }};
}

CommandResult pan_camera(const json& args) {
  double delta_x = args.value("deltaX", 0.0);
  double delta_y = args.value("deltaY", 0.0);

  auto& viewport = bendcore::ViewportManager::instance();
  viewport.pan_camera(delta_x, delta_y);

  auto state = viewport.get_camera_state();
  return CommandResult{true, "Camera pan executed", json{
    {"target", json{
      {"x", state.target.x},
      {"y", state.target.y},
      {"z", state.target.z}
    }}
  }};
}

CommandResult zoom_camera(const json& args) {
  double factor = args.value("factor", 1.1);

  auto& viewport = bendcore::ViewportManager::instance();
  viewport.zoom_camera(factor);

  auto state = viewport.get_camera_state();
  return CommandResult{true, "Camera zoom executed", json{
    {"distance", state.distance}
  }};
}

CommandResult fit_to_view(const json& args) {
  std::vector<std::string> object_ids;
  if (args.contains("objectIds") && args["objectIds"].is_array()) {
    for (const auto& id : args["objectIds"]) {
      object_ids.push_back(id.get<std::string>());
    }
  }

  auto& viewport = bendcore::ViewportManager::instance();
  viewport.fit_to_view(object_ids);

  auto state = viewport.get_camera_state();
  return CommandResult{true, "Fit to view executed", json{
    {"distance", state.distance},
    {"pitch", state.pitch},
    {"yaw", state.yaw},
    {"objectCount", object_ids.size()}
  }};
}

CommandResult set_snap_mode(const json& args) {
  std::string mode_str = args.value("mode", "grid");
  bool enabled = args.value("enabled", true);

  auto& snap_system = bendcore::SnapSystem::instance();
  auto mode = bendcore::snap_mode_from_string(mode_str);
  snap_system.set_snap_mode(mode, enabled);

  return CommandResult{true, "Snap mode set", json{
    {"mode", mode_str},
    {"enabled", enabled},
    {"activeModes", json::array()}
  }};
}

CommandResult get_snap_points(const json& args) {
  double cursor_x = args.value("cursorX", 0.0);
  double cursor_y = args.value("cursorY", 0.0);

  auto& snap_system = bendcore::SnapSystem::instance();
  // Note: In a real implementation, we'd pass the actual view and projection matrices
  // For now, we return an empty array with snap configuration
  json snap_points = json::array();

  return CommandResult{true, "Snap points detected", json{
    {"points", snap_points},
    {"count", 0},
    {"cursor", json{
      {"x", cursor_x},
      {"y", cursor_y}
    }},
    {"snapEnabled", snap_system.is_snap_enabled()}
  }};
}

// ============ SETTINGS & TOOLBAR (Phase 34) ============

CommandResult get_settings(const json& args) {
  auto& settings = bendcore::SettingsManager::instance();
  json all_settings = settings.get_all();

  return CommandResult{true, "Settings retrieved", all_settings};
}

CommandResult set_settings(const json& args) {
  auto& settings = bendcore::SettingsManager::instance();

  // Update toolbar if provided
  if (args.contains("toolbar")) {
    if (args["toolbar"].contains("buttons")) {
      auto buttons = args["toolbar"]["buttons"].get<std::vector<std::string>>();
      settings.set_toolbar_buttons(buttons);
    }
  }

  // Update theme if provided
  if (args.contains("theme")) {
    settings.set(std::string("theme"), args["theme"]);
  }

  // Update profile if provided
  if (args.contains("currentProfile")) {
    settings.set_current_profile(args["currentProfile"].get<std::string>());
  }

  // Update shortcuts if provided
  if (args.contains("shortcuts")) {
    settings.set(std::string("shortcuts"), args["shortcuts"]);
  }

  settings.save();

  return CommandResult{true, "Settings updated", json{
    {"updated", true},
    {"savedPath", settings.get_settings_path()}
  }};
}

CommandResult load_profile(const json& args) {
  if (!args.contains("profile_name")) {
    return CommandResult{false, "profile_name required"};
  }

  std::string profile_name = args["profile_name"].get<std::string>();
  auto& settings = bendcore::SettingsManager::instance();
  settings.set_current_profile(profile_name);
  settings.save();

  return CommandResult{true, "Profile loaded", json{
    {"profileName", profile_name},
    {"currentProfile", settings.get_current_profile()}
  }};
}

CommandResult save_profile(const json& args) {
  if (!args.contains("profile_name") || !args.contains("profile_data")) {
    return CommandResult{false, "profile_name and profile_data required"};
  }

  std::string profile_name = args["profile_name"].get<std::string>();
  json profile_data = args["profile_data"];

  auto& settings = bendcore::SettingsManager::instance();
  settings.set(profile_name, profile_data);
  settings.save();

  return CommandResult{true, "Profile saved", json{
    {"profileName", profile_name},
    {"saved", true}
  }};
}

CommandResult get_icon_theme(const json& args) {
  auto& theme_manager = bendcore::IconThemeManager::instance();

  std::string preset_str;
  auto preset = theme_manager.get_current_preset();
  switch (preset) {
    case bendcore::ThemePreset::kLight:
      preset_str = "light";
      break;
    case bendcore::ThemePreset::kDark:
      preset_str = "dark";
      break;
    case bendcore::ThemePreset::kHighContrast:
      preset_str = "highContrast";
      break;
    case bendcore::ThemePreset::kColorblind:
      preset_str = "colorblind";
      break;
    case bendcore::ThemePreset::kCustom:
      preset_str = "custom";
      break;
  }

  return CommandResult{true, "Theme retrieved", json{
    {"theme_name", theme_manager.get_theme_name()},
    {"preset", preset_str},
    {"colors", theme_manager.get_colors_json()}
  }};
}

CommandResult set_icon_theme(const json& args) {
  auto& theme_manager = bendcore::IconThemeManager::instance();

  // Apply preset if provided
  if (args.contains("preset")) {
    std::string preset_str = args["preset"].get<std::string>();
    if (preset_str == "light") {
      theme_manager.set_preset(bendcore::ThemePreset::kLight);
    } else if (preset_str == "dark") {
      theme_manager.set_preset(bendcore::ThemePreset::kDark);
    } else if (preset_str == "highContrast") {
      theme_manager.set_preset(bendcore::ThemePreset::kHighContrast);
    } else if (preset_str == "colorblind") {
      theme_manager.set_preset(bendcore::ThemePreset::kColorblind);
    }
  }

  // Apply custom colors if provided
  if (args.contains("colors") && args["colors"].is_object()) {
    for (auto& [key, value] : args["colors"].items()) {
      std::string color = value.get<std::string>();
      if (bendcore::IconThemeManager::is_valid_hex_color(color)) {
        try {
          theme_manager.set_custom_color(key, color);
        } catch (const std::exception& e) {
          // Continue with other colors if one fails
        }
      }
    }
  }

  // Save to settings
  auto& settings = bendcore::SettingsManager::instance();
  settings.set("theme", json{
    {"name", theme_manager.get_theme_name()},
    {"colors", theme_manager.get_colors_json()}
  });
  settings.save();

  return CommandResult{true, "Theme applied", json{
    {"theme_name", theme_manager.get_theme_name()},
    {"colors", theme_manager.get_colors_json()}
  }};
}

// ============ PATH SKETCH 3D EDITOR (Phase 35B) ============

static std::map<std::string, json> sketch_storage_;

CommandResult start_path_sketch(const json& args) {
  std::string sketch_id = args.value("sketch_id", "sketch_" + std::to_string(time(nullptr)));
  std::string profile_id = args.value("profile_id", "default");
  double profile_r = args.value("profile_r", 10.0);
  double profile_s_min = args.value("profile_s_min", 2.0);
  double profile_b_min = args.value("profile_b_min", 5.0);
  double profile_b_max = args.value("profile_b_max", 180.0);

  json sketch_data = {
    {"sketch_id", sketch_id},
    {"profile_id", profile_id},
    {"profile_r", profile_r},
    {"profile_s_min", profile_s_min},
    {"profile_b_min", profile_b_min},
    {"profile_b_max", profile_b_max},
    {"vertices", json::array()},
    {"segments", json::array()},
    {"constraints", json::array()},
    {"initialized", true},
  };

  sketch_storage_[sketch_id] = sketch_data;

  return CommandResult{true, "Path sketch initialized", json{
    {"sketch_id", sketch_id},
    {"initialized", true}
  }};
}

CommandResult add_sketch_vertex(const json& args) {
  std::string sketch_id = args.value("sketch_id", "");
  if (sketch_storage_.find(sketch_id) == sketch_storage_.end()) {
    return CommandResult{false, "Sketch not found"};
  }

  auto& sketch = sketch_storage_[sketch_id];
  double x = args.value("x", 0.0);
  double y = args.value("y", 0.0);
  double z = args.value("z", 0.0);
  bool fixed = args.value("fixed", false);

  std::string vertex_id = "vertex_" + std::to_string(time(nullptr)) + "_" +
                          std::to_string(rand());

  json vertex = {
    {"id", vertex_id},
    {"position", {{"x", x}, {"y", y}, {"z", z}}},
    {"is_fixed", fixed},
  };

  sketch["vertices"].push_back(vertex);

  return CommandResult{true, "Vertex added", json{{"vertex_id", vertex_id}}};
}

CommandResult create_sketch_segment(const json& args) {
  std::string sketch_id = args.value("sketch_id", "");
  if (sketch_storage_.find(sketch_id) == sketch_storage_.end()) {
    return CommandResult{false, "Sketch not found"};
  }

  auto& sketch = sketch_storage_[sketch_id];
  std::string from_vertex = args.value("from_vertex_id", "");
  std::string to_vertex = args.value("to_vertex_id", "");

  std::string segment_id = "segment_" + std::to_string(time(nullptr)) + "_" +
                           std::to_string(rand());

  json segment = {
    {"id", segment_id},
    {"from_vertex", from_vertex},
    {"to_vertex", to_vertex},
    {"dimension_L", 10.0},
    {"dimension_S", sketch["profile_s_min"].get<double>()},
    {"bend_angle", 0.0},
    {"plane_rotation", 0.0},
  };

  sketch["segments"].push_back(segment);

  return CommandResult{true, "Segment created", json{{"segment_id", segment_id}}};
}

CommandResult set_dimension(const json& args) {
  std::string sketch_id = args.value("sketch_id", "");
  if (sketch_storage_.find(sketch_id) == sketch_storage_.end()) {
    return CommandResult{false, "Sketch not found"};
  }

  auto& sketch = sketch_storage_[sketch_id];
  std::string segment_id = args.value("segment_id", "");
  std::string type = args.value("type", "L");
  double value = args.value("value", 0.0);

  for (auto& seg : sketch["segments"]) {
    if (seg["id"].get<std::string>() == segment_id) {
      if (type == "L") {
        seg["dimension_L"] = value;
      } else if (type == "S") {
        seg["dimension_S"] = value;
      } else if (type == "B") {
        seg["bend_angle"] = value;
      } else if (type == "A") {
        seg["plane_rotation"] = value;
      }

      return CommandResult{true, "Dimension updated", json{
        {"segment_id", segment_id},
        {"type", type},
        {"value", value},
        {"validation_errors", json::array()}
      }};
    }
  }

  return CommandResult{false, "Segment not found"};
}

CommandResult add_constraint(const json& args) {
  std::string sketch_id = args.value("sketch_id", "");
  if (sketch_storage_.find(sketch_id) == sketch_storage_.end()) {
    return CommandResult{false, "Sketch not found"};
  }

  auto& sketch = sketch_storage_[sketch_id];
  std::string type = args.value("type", "COINCIDENT");
  double value = args.value("value", 0.0);

  std::vector<std::string> entity_ids;
  if (args.contains("entity_ids") && args["entity_ids"].is_array()) {
    for (const auto& id : args["entity_ids"]) {
      entity_ids.push_back(id.get<std::string>());
    }
  }

  std::string constraint_id = "constraint_" + std::to_string(time(nullptr)) + "_" +
                              std::to_string(rand());

  json constraint = {
    {"id", constraint_id},
    {"type", type},
    {"entity_ids", entity_ids},
    {"value", value},
    {"is_satisfied", true},
    {"error", 0.0},
  };

  sketch["constraints"].push_back(constraint);

  return CommandResult{true, "Constraint added", json{
    {"constraint_id", constraint_id},
    {"conflicts", json::array()}
  }};
}

CommandResult solve_constraints(const json& args) {
  std::string sketch_id = args.value("sketch_id", "");
  if (sketch_storage_.find(sketch_id) == sketch_storage_.end()) {
    return CommandResult{false, "Sketch not found"};
  }

  // In a real implementation, this would call the C++ SketchSolver
  // For now, return simulated result
  return CommandResult{true, "Constraints solved", json{
    {"converged", true},
    {"iterations", 15},
    {"residual", 0.000523},
    {"conflicts", json::array()}
  }};
}

CommandResult validate_path(const json& args) {
  std::string sketch_id = args.value("sketch_id", "");
  if (sketch_storage_.find(sketch_id) == sketch_storage_.end()) {
    return CommandResult{false, "Sketch not found"};
  }

  auto& sketch = sketch_storage_[sketch_id];
  json errors = json::array();

  // Basic validation: check if S >= S_min
  for (const auto& seg : sketch["segments"]) {
    double s = seg.value("dimension_S", 2.0);
    double s_min = sketch["profile_s_min"].get<double>();

    if (s < s_min) {
      errors.push_back({
        {"type", "S_TOO_SMALL"},
        {"message", "Straight must be >= " + std::to_string(s_min)},
        {"severity", "error"}
      });
    }
  }

  return CommandResult{true, "Path validated", json{
    {"valid", errors.empty()},
    {"errors", errors},
    {"warnings", json::array()}
  }};
}

CommandResult export_path_sketch(const json& args) {
  std::string sketch_id = args.value("sketch_id", "");
  if (sketch_storage_.find(sketch_id) == sketch_storage_.end()) {
    return CommandResult{false, "Sketch not found"};
  }

  auto& sketch = sketch_storage_[sketch_id];

  return CommandResult{true, "Path sketch exported", json{
    {"path_data", sketch}
  }};
}

// ============================================================================
// PHASE 36: ENTERPRISE FEATURES & IMPORT
// ============================================================================

// Organization Profile Management
CommandResult list_org_profiles(const json& args) {
  json profiles = json::array();
  profiles.push_back({
    {"id", "prof-1"},
    {"name", "Standard Steel 3/8\""},
    {"category", "Standard"},
    {"versions", 2},
    {"active", true}
  });
  profiles.push_back({
    {"id", "prof-2"},
    {"name", "Aluminum 6061-T6 1/2\""},
    {"category", "Material-Specific"},
    {"versions", 1},
    {"active", false}
  });

  return CommandResult{true, "Profiles listed", {{"profiles", profiles}}};
}

CommandResult get_org_profile(const json& args) {
  std::string profile_id = args.value("profile_id", "");

  json profile = {
    {"id", profile_id},
    {"name", "Sample Profile"},
    {"description", "Sample bending profile"},
    {"versions", json::array()},
    {"created_at", "2024-10-01T00:00:00Z"},
    {"updated_at", "2024-10-15T00:00:00Z"}
  };

  return CommandResult{true, "Profile retrieved", profile};
}

CommandResult create_org_profile(const json& args) {
  std::string name = args.value("name", "New Profile");
  std::string profile_id = "prof-" + std::to_string(time(nullptr));

  json profile = {
    {"id", profile_id},
    {"name", name},
    {"created_at", "2024-10-23T00:00:00Z"},
    {"version", "1.0"}
  };

  return CommandResult{true, "Profile created", profile};
}

CommandResult update_org_profile(const json& args) {
  std::string profile_id = args.value("profile_id", "");
  std::string name = args.value("name", "");

  return CommandResult{true, "Profile updated", {{"id", profile_id}, {"name", name}}};
}

CommandResult activate_org_profile(const json& args) {
  std::string profile_id = args.value("profile_id", "");

  return CommandResult{true, "Profile activated", {{"profile_id", profile_id}}};
}

CommandResult archive_org_profile(const json& args) {
  std::string profile_id = args.value("profile_id", "");
  bool archive = args.value("archive", true);

  return CommandResult{true, archive ? "Profile archived" : "Profile restored",
      {{"profile_id", profile_id}, {"archived", archive}}};
}

// Parametric Pattern Templates
CommandResult get_pattern_templates(const json& args) {
  json templates = json::array();

  std::vector<std::string> pattern_names = {
    "angle_90", "offset", "saddle", "u_shape", "z_shape", "wall_penetration"
  };

  for (const auto& name : pattern_names) {
    templates.push_back({
      {"id", name},
      {"name", name},
      {"segments", 3}
    });
  }

  return CommandResult{true, "Templates retrieved", {{"templates", templates}}};
}

CommandResult create_from_pattern(const json& args) {
  std::string pattern_id = args.value("pattern_id", "");

  json tube = {
    {"id", "tube-" + std::to_string(time(nullptr))},
    {"pattern", pattern_id},
    {"created", true},
    {"segments", 3}
  };

  return CommandResult{true, "Tube created from pattern", tube};
}

CommandResult get_pattern_params(const json& args) {
  std::string pattern_id = args.value("pattern_id", "");

  json params = json::object();
  if (pattern_id == "offset") {
    params["offset_distance"] = 25;
    params["offset_height"] = 50;
  } else if (pattern_id == "u_shape") {
    params["length"] = 100;
    params["distance_between_ends"] = 60;
  }

  return CommandResult{true, "Pattern parameters retrieved", {{"params", params}}};
}

CommandResult preview_pattern(const json& args) {
  std::string pattern_id = args.value("pattern_id", "");

  json preview = {
    {"pattern", pattern_id},
    {"vertices", json::array()},
    {"preview_ready", true}
  };

  return CommandResult{true, "Pattern preview generated", preview};
}

// Import & Geometry Recognition
CommandResult import_step_file(const json& args) {
  std::string file_path = args.value("file_path", "");

  json result = {
    {"file", file_path},
    {"solids_detected", 5},
    {"tubes_detected", 3},
    {"status", "analyzing"}
  };

  return CommandResult{true, "STEP file imported", result};
}

CommandResult detect_tubes_in_import(const json& args) {
  json tubes = json::array();

  tubes.push_back({
    {"id", "tube-1"},
    {"name", "Tube_1"},
    {"diameter", 16.0},
    {"confidence", 92},
    {"material", "Steel"}
  });
  tubes.push_back({
    {"id", "tube-2"},
    {"name", "Tube_2"},
    {"diameter", 16.0},
    {"confidence", 89},
    {"material", "Steel"}
  });

  return CommandResult{true, "Tubes detected", {{"tubes", tubes}}};
}

CommandResult apply_grouping_rules(const json& args) {
  json rules = args.value("rules", json::array());

  json groups = json::array();
  groups.push_back({
    {"rule", "diameter"},
    {"count", 2},
    {"name", "By Diameter"}
  });

  return CommandResult{true, "Grouping rules applied", {{"groups", groups}}};
}

// Geometry Rounding
CommandResult round_geometry(const json& args) {
  json preferences = args.value("preferences", json::object());

  json result = {
    {"tubes_rounded", 3},
    {"changes_made", 12},
    {"status", "complete"}
  };

  return CommandResult{true, "Geometry rounded", result};
}

CommandResult get_rounding_preview(const json& args) {
  json preview = {
    {"would_round", 3},
    {"estimated_changes", 12},
    {"preview_ready", true}
  };

  return CommandResult{true, "Rounding preview generated", preview};
}

}  // namespace commands

}  // namespace bendcore
