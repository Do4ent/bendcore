/**
 * Smart Tube Templates System
 * Provides template data model and auto-selection for tube design
 * Task 2: Smart Templates for BendCore CAD (Q3 2027)
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>

namespace bendcore {

// ============ DATA STRUCTURES ============

/**
 * Template parameter with min/max/default and unit
 */
struct TemplateParameter {
  double min;
  double max;
  double default_value;
  std::string unit;  // "mm", "°", "g", etc.

  // JSON serialization
  friend void to_json(nlohmann::json& j, const TemplateParameter& p) {
    j = nlohmann::json{
      {"min", p.min},
      {"max", p.max},
      {"default", p.default_value},
      {"unit", p.unit}
    };
  }

  friend void from_json(const nlohmann::json& j, TemplateParameter& p) {
    j.at("min").get_to(p.min);
    j.at("max").get_to(p.max);
    j.at("default").get_to(p.default_value);
    j.at("unit").get_to(p.unit);
  }
};

/**
 * Enhanced template definition per Task 2 specification
 * Includes bend-specific constraints and manufacturing info
 */
struct Template {
  std::string id;
  std::string name;
  std::string description;
  std::string category;  // "Standard", "Reinforced", "Large-Assembly", "Custom"

  // Geometry constraints
  TemplateParameter outer_diameter;
  TemplateParameter wall_thickness;
  TemplateParameter bend_radius_ratio;  // R/D ratio (e.g., min=1.5, max=3.0)

  // Bend constraints
  int max_consecutive_bends = 3;
  double max_bend_angle = 180.0;  // degrees
  double min_bend_spacing = 10.0; // mm

  // Manufacturing info
  std::string material_family;       // "Steel", "Aluminum", "Copper"
  std::vector<std::string> recommended_materials;
  std::string manufacturing_method;  // "3D bending", "CNC", "Hydraulic", "CNC 5-axis"
  double estimated_setup_time_min = 0.0;

  // Cost estimation
  double cost_per_unit = 0.0;
  double min_batch_size = 1.0;

  // Metadata
  std::string author = "System";
  std::string created_at;
  std::string modified_at;
  int popularity_score = 0;  // 0-100
  std::string use_case;

  // Legacy compatibility - keep old fields
  TemplateParameter length;

  // JSON serialization
  friend void to_json(nlohmann::json& j, const Template& t) {
    j = nlohmann::json{
      {"id", t.id},
      {"name", t.name},
      {"description", t.description},
      {"category", t.category},
      {"outer_diameter", t.outer_diameter},
      {"wall_thickness", t.wall_thickness},
      {"bend_radius_ratio", t.bend_radius_ratio},
      {"max_consecutive_bends", t.max_consecutive_bends},
      {"max_bend_angle", t.max_bend_angle},
      {"min_bend_spacing", t.min_bend_spacing},
      {"material_family", t.material_family},
      {"recommended_materials", t.recommended_materials},
      {"manufacturing_method", t.manufacturing_method},
      {"estimated_setup_time_min", t.estimated_setup_time_min},
      {"cost_per_unit", t.cost_per_unit},
      {"min_batch_size", t.min_batch_size},
      {"author", t.author},
      {"created_at", t.created_at},
      {"modified_at", t.modified_at},
      {"popularity_score", t.popularity_score},
      {"use_case", t.use_case},
      {"length", t.length}
    };
  }

  friend void from_json(const nlohmann::json& j, Template& t) {
    j.at("id").get_to(t.id);
    j.at("name").get_to(t.name);
    j.at("description").get_to(t.description);
    j.at("category").get_to(t.category);
    j.at("outer_diameter").get_to(t.outer_diameter);
    j.at("wall_thickness").get_to(t.wall_thickness);

    if (j.contains("bend_radius_ratio")) {
      j.at("bend_radius_ratio").get_to(t.bend_radius_ratio);
    }
    if (j.contains("max_consecutive_bends")) {
      j.at("max_consecutive_bends").get_to(t.max_consecutive_bends);
    }
    if (j.contains("max_bend_angle")) {
      j.at("max_bend_angle").get_to(t.max_bend_angle);
    }
    if (j.contains("min_bend_spacing")) {
      j.at("min_bend_spacing").get_to(t.min_bend_spacing);
    }
    if (j.contains("material_family")) {
      j.at("material_family").get_to(t.material_family);
    }
    if (j.contains("recommended_materials")) {
      j.at("recommended_materials").get_to(t.recommended_materials);
    }
    if (j.contains("manufacturing_method")) {
      j.at("manufacturing_method").get_to(t.manufacturing_method);
    }
    if (j.contains("estimated_setup_time_min")) {
      j.at("estimated_setup_time_min").get_to(t.estimated_setup_time_min);
    }
    if (j.contains("cost_per_unit")) {
      j.at("cost_per_unit").get_to(t.cost_per_unit);
    }
    if (j.contains("min_batch_size")) {
      j.at("min_batch_size").get_to(t.min_batch_size);
    }
    if (j.contains("author")) {
      j.at("author").get_to(t.author);
    }
    if (j.contains("created_at")) {
      j.at("created_at").get_to(t.created_at);
    }
    if (j.contains("modified_at")) {
      j.at("modified_at").get_to(t.modified_at);
    }
    if (j.contains("popularity_score")) {
      j.at("popularity_score").get_to(t.popularity_score);
    }
    if (j.contains("use_case")) {
      j.at("use_case").get_to(t.use_case);
    }
    if (j.contains("length")) {
      j.at("length").get_to(t.length);
    }
  }
};

/**
 * Auto-selection result with confidence score and constraint violations
 */
struct TemplateMatch {
  std::string template_id;
  std::string template_name;
  double confidence;  // 0.0 to 100.0 percent
  std::vector<std::string> constraint_violations;
  std::vector<std::string> recommendations;

  bool operator<(const TemplateMatch& other) const {
    return confidence > other.confidence;  // Sort descending
  }
};

/**
 * Template library - collection of templates with active selection
 */
struct TemplateLibrary {
  std::vector<Template> templates;
  std::string active_template_id;
  std::string version = "1.0";

  // Find template by ID
  std::optional<Template> get_template(const std::string& id) const {
    for (const auto& t : templates) {
      if (t.id == id) return t;
    }
    return std::nullopt;
  }

  // JSON serialization
  friend void to_json(nlohmann::json& j, const TemplateLibrary& lib) {
    j = nlohmann::json{
      {"version", lib.version},
      {"templates", lib.templates},
      {"active_template_id", lib.active_template_id}
    };
  }

  friend void from_json(const nlohmann::json& j, TemplateLibrary& lib) {
    if (j.contains("version")) {
      j.at("version").get_to(lib.version);
    }
    j.at("templates").get_to(lib.templates);
    j.at("active_template_id").get_to(lib.active_template_id);
  }
};

// ============ AUTO-SELECTION ENGINE ============

/**
 * Template matcher with confidence scoring algorithm
 * Weights: OD=30%, wall_thickness=20%, bend_angle=25%, radius_ratio=15%, spacing=10%
 */
class TemplateMatcher {
 public:
  /**
   * Match tube parameters to best templates
   * Returns top 3 candidates sorted by confidence score
   */
  static std::vector<TemplateMatch> find_best_matches(
    const TemplateLibrary& library,
    double outer_diameter,
    double wall_thickness,
    double bend_angle = 90.0,
    double radius_ratio = 2.0,
    double bend_spacing = 20.0,
    int num_results = 3
  );

  /**
   * Calculate single parameter match score (0.0 to 1.0)
   */
  static double calculate_parameter_score(
    double value,
    const TemplateParameter& param
  );

  /**
   * Calculate weighted template match score
   */
  static double calculate_template_score(
    const Template& template_obj,
    double outer_diameter,
    double wall_thickness,
    double bend_angle,
    double radius_ratio,
    double bend_spacing
  );

  /**
   * Check constraint violations
   */
  static std::vector<std::string> check_constraint_violations(
    const Template& template_obj,
    double outer_diameter,
    double wall_thickness,
    double bend_angle,
    double radius_ratio,
    double bend_spacing
  );

  /**
   * Generate manufacturing recommendations
   */
  static std::vector<std::string> generate_recommendations(
    const Template& template_obj,
    double outer_diameter,
    double wall_thickness,
    const std::vector<std::string>& violations
  );

 private:
  static constexpr double WEIGHT_OD = 0.30;
  static constexpr double WEIGHT_WALL = 0.20;
  static constexpr double WEIGHT_BEND = 0.25;
  static constexpr double WEIGHT_RADIUS = 0.15;
  static constexpr double WEIGHT_SPACING = 0.10;
};

// ============ LEGACY COMPATIBILITY CLASS ============

/**
 * Template selection engine (legacy)
 * Deprecated: Use TemplateMatcher instead
 */
class TemplateSelector {
 public:
  /**
   * Select best matching templates for given tube parameters
   * Returns top 3 candidates sorted by confidence score
   */
  static std::vector<TemplateMatch> select_template(
    const TemplateLibrary& library,
    double outer_diameter,
    double wall_thickness,
    double length
  );

  /**
   * Calculate single parameter match score (0.0 to 1.0)
   */
  static double calculate_parameter_score(
    double value,
    const TemplateParameter& param
  );

  /**
   * Calculate overall match score using weighted sum
   */
  static double calculate_template_score(
    const Template& template_obj,
    double outer_diameter,
    double wall_thickness,
    double length
  );

 private:
  static constexpr double WEIGHT_OD = 0.5;
  static constexpr double WEIGHT_WALL = 0.3;
  static constexpr double WEIGHT_LENGTH = 0.2;
};

// ============ TEMPLATE MANAGER ============

/**
 * Manages template loading, saving, and CRUD operations
 */
class TemplateManager {
 public:
  TemplateManager();

  /**
   * Load templates from JSON file
   */
  bool load_templates(const std::string& filename);

  /**
   * Save templates to JSON file
   */
  bool save_templates(const std::string& filename) const;

  /**
   * Add a new template
   */
  void add_template(const Template& template_obj);

  /**
   * Remove template by ID
   */
  bool remove_template(const std::string& id);

  /**
   * Get template by ID
   */
  std::optional<Template> get_template(const std::string& id) const;

  /**
   * Get all templates
   */
  const std::vector<Template>& get_all_templates() const {
    return library_.templates;
  }

  /**
   * Get template library
   */
  const TemplateLibrary& get_library() const { return library_; }

  /**
   * Set active template
   */
  void set_active_template(const std::string& id) {
    if (library_.get_template(id).has_value()) {
      library_.active_template_id = id;
    }
  }

  /**
   * Get active template
   */
  std::optional<Template> get_active_template() const {
    return library_.get_template(library_.active_template_id);
  }

 private:
  TemplateLibrary library_;

  // Initialize with built-in templates
  void initialize_builtin_templates();
};

}  // namespace bendcore
