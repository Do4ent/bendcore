/**
 * Template Manager Implementation
 * Handles loading, saving, and CRUD operations for tube templates
 */

#include "bendcore/templates.hpp"
#include <fstream>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace bendcore {

// ============ TEMPLATE SELECTOR IMPLEMENTATION ============

std::vector<TemplateMatch> TemplateSelector::select_template(
    const TemplateLibrary& library,
    double outer_diameter,
    double wall_thickness,
    double length) {

  std::vector<TemplateMatch> matches;

  // Score each template
  for (const auto& template_obj : library.templates) {
    double score = calculate_template_score(
        template_obj, outer_diameter, wall_thickness, length);

    matches.push_back({
      template_obj.id,
      template_obj.name,
      score
    });
  }

  // Sort by confidence score (descending)
  std::sort(matches.begin(), matches.end());

  // Return top 3
  if (matches.size() > 3) {
    matches.resize(3);
  }

  return matches;
}

double TemplateSelector::calculate_parameter_score(
    double value,
    const TemplateParameter& param) {

  // If value is within range, return perfect score
  if (value >= param.min && value <= param.max) {
    return 1.0;
  }

  // If value is outside range, calculate penalty based on distance
  double distance = 0.0;
  if (value < param.min) {
    distance = param.min - value;
  } else {
    distance = value - param.max;
  }

  // Maximum penalty: distance as percentage of range
  double range = param.max - param.min;
  if (range == 0) return 0.0;

  // Score decreases linearly with distance
  // At distance = range, score = 0.0
  // At distance = 0, score = 1.0
  double score = std::max(0.0, 1.0 - (distance / range));
  return score;
}

double TemplateSelector::calculate_template_score(
    const Template& template_obj,
    double outer_diameter,
    double wall_thickness,
    double length) {

  double od_score = calculate_parameter_score(
      outer_diameter, template_obj.outer_diameter);
  double wall_score = calculate_parameter_score(
      wall_thickness, template_obj.wall_thickness);
  double length_score = calculate_parameter_score(
      length, template_obj.length);

  // Weighted sum: OD=50%, Wall=30%, Length=20%
  double total_score =
      (WEIGHT_OD * od_score) +
      (WEIGHT_WALL * wall_score) +
      (WEIGHT_LENGTH * length_score);

  // Convert to percentage
  return total_score * 100.0;
}

// ============ TEMPLATE MANAGER IMPLEMENTATION ============

TemplateManager::TemplateManager() {
  initialize_builtin_templates();
}

void TemplateManager::initialize_builtin_templates() {
  // Initialize with empty library
  library_.templates.clear();
  library_.active_template_id = "";
}

bool TemplateManager::load_templates(const std::string& filename) {
  try {
    std::ifstream file(filename);
    if (!file.is_open()) {
      throw std::runtime_error("Cannot open file: " + filename);
    }

    nlohmann::json j;
    file >> j;
    j.get_to(library_);

    return true;
  } catch (const std::exception& e) {
    // Log error and return false
    return false;
  }
}

bool TemplateManager::save_templates(const std::string& filename) const {
  try {
    std::ofstream file(filename);
    if (!file.is_open()) {
      throw std::runtime_error("Cannot open file for writing: " + filename);
    }

    nlohmann::json j = library_;
    file << j.dump(2) << std::endl;

    return true;
  } catch (const std::exception& e) {
    return false;
  }
}

void TemplateManager::add_template(const Template& template_obj) {
  // Check if template with this ID already exists
  if (library_.get_template(template_obj.id).has_value()) {
    throw std::runtime_error("Template with ID '" + template_obj.id + "' already exists");
  }

  library_.templates.push_back(template_obj);
}

bool TemplateManager::remove_template(const std::string& id) {
  auto it = std::find_if(
      library_.templates.begin(),
      library_.templates.end(),
      [&id](const Template& t) { return t.id == id; });

  if (it == library_.templates.end()) {
    return false;
  }

  // If this was the active template, clear active selection
  if (library_.active_template_id == id) {
    library_.active_template_id = "";
  }

  library_.templates.erase(it);
  return true;
}

std::optional<Template> TemplateManager::get_template(const std::string& id) const {
  return library_.get_template(id);
}

}  // namespace bendcore
