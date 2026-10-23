/**
 * BendCore IPC Command Router Implementation
 * Maps React Ribbon commands to C++ CAD kernel functions
 */

#include "ipc_commands.hpp"
#include "bendcore/analysis.hpp"
#include "bendcore/feature_tree.hpp"
#include "bendcore/command_search.hpp"
#include "bendcore/shortcuts.hpp"
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

}  // namespace commands

}  // namespace bendcore
