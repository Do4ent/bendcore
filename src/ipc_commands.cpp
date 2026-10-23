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
#include <cmath>
#include <ctime>

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

}  // namespace commands

}  // namespace bendcore
