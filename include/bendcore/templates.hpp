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
 * Complete template definition
 */
struct Template {
  std::string id;
  std::string name;
  std::string description;
  std::string category;  // "Reinforced", "Standard", "Small-OD", "Custom"

  // Parameter ranges
  TemplateParameter outer_diameter;
  TemplateParameter wall_thickness;
  TemplateParameter length;

  // Optional metadata
  std::string use_case;
  std::vector<std::string> compatible_materials;

  // JSON serialization
  friend void to_json(nlohmann::json& j, const Template& t) {
    j = nlohmann::json{
      {"id", t.id},
      {"name", t.name},
      {"description", t.description},
      {"category", t.category},
      {"outer_diameter", t.outer_diameter},
      {"wall_thickness", t.wall_thickness},
      {"length", t.length},
      {"use_case", t.use_case},
      {"compatible_materials", t.compatible_materials}
    };
  }

  friend void from_json(const nlohmann::json& j, Template& t) {
    j.at("id").get_to(t.id);
    j.at("name").get_to(t.name);
    j.at("description").get_to(t.description);
    j.at("category").get_to(t.category);
    j.at("outer_diameter").get_to(t.outer_diameter);
    j.at("wall_thickness").get_to(t.wall_thickness);
    j.at("length").get_to(t.length);
    j.at("use_case").get_to(t.use_case);
    j.at("compatible_materials").get_to(t.compatible_materials);
  }
};

/**
 * Auto-selection result with confidence score
 */
struct TemplateMatch {
  std::string template_id;
  std::string template_name;
  double confidence;  // 0.0 to 100.0 percent

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
      {"templates", lib.templates},
      {"active_template_id", lib.active_template_id}
    };
  }

  friend void from_json(const nlohmann::json& j, TemplateLibrary& lib) {
    j.at("templates").get_to(lib.templates);
    j.at("active_template_id").get_to(lib.active_template_id);
  }
};

// ============ AUTO-SELECTION ENGINE ============

/**
 * Template selection engine with scoring algorithm
 * Weights: OD=0.5, wall_thickness=0.3, length=0.2
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
   * Returns 1.0 if value is within range, decreases with distance
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
