/**
 * BendCore Profile Manager Implementation
 * Loads and validates bending profiles from JSON database
 * Phase 35A: Profile Database and New Tube Dialog
 */

#include "bendcore/profile_manager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace bendcore {

ProfileManager& ProfileManager::instance() {
  static ProfileManager manager;
  return manager;
}

bool ProfileManager::load_profiles(const std::string& data_dir) {
  if (profiles_loaded_) {
    return true; // Already loaded
  }

  std::string profile_path = data_dir + "/bending_profiles.json";
  std::ifstream file(profile_path);

  if (!file.is_open()) {
    return false; // File not found
  }

  try {
    json data;
    file >> data;

    if (!data.contains("profiles") || !data["profiles"].is_array()) {
      return false; // Invalid structure
    }

    profiles_.clear();
    for (const auto& profile_json : data["profiles"]) {
      profiles_.push_back(parse_profile(profile_json));
    }

    profiles_loaded_ = true;
    return true;
  } catch (const std::exception&) {
    return false; // Parse error
  }
}

BendingProfile ProfileManager::parse_profile(const json& j) {
  BendingProfile profile;

  profile.id = j.value("id", "");
  profile.name = j.value("name", "");
  profile.category = j.value("category", "");
  profile.manufacturer = j.value("manufacturer", "");
  profile.type = j.value("type", "");
  profile.description = j.value("description", "");
  profile.capacity_max_OD = j.value("capacity_max_OD", 0.0);
  profile.capacity_max_wall = j.value("capacity_max_wall", 0.0);

  if (j.contains("diameters") && j["diameters"].is_array()) {
    for (const auto& diam_json : j["diameters"]) {
      profile.diameters.push_back(parse_diameter(diam_json));
    }
  }

  return profile;
}

ProfileDiameter ProfileManager::parse_diameter(const json& j) {
  ProfileDiameter diameter;

  diameter.id = j.value("id", "");
  diameter.OD = j.value("OD", 0.0);
  diameter.R = j.value("R", 0.0);
  diameter.S_min = j.value("S_min", 0.0);
  diameter.N = j.value("N", 1);
  diameter.B_min = j.value("B_min", 0.0);
  diameter.B_max = j.value("B_max", 179.0);
  diameter.discrete_type = j.value("discrete_type", "continuous");

  if (j.contains("discrete_values") && j["discrete_values"].is_array()) {
    for (const auto& val : j["discrete_values"]) {
      diameter.discrete_values.push_back(val.get<double>());
    }
  }

  return diameter;
}

std::optional<BendingProfile> ProfileManager::get_profile(
    const std::string& id) {
  if (!profiles_loaded_) {
    return std::nullopt;
  }

  for (const auto& profile : profiles_) {
    if (profile.id == id) {
      return profile;
    }
  }

  return std::nullopt;
}

std::vector<std::string> ProfileManager::list_profile_ids() {
  std::vector<std::string> ids;
  for (const auto& profile : profiles_) {
    ids.push_back(profile.id);
  }
  return ids;
}

std::vector<ProfileDiameter> ProfileManager::get_diameters(
    const std::string& profile_id) {
  auto profile = get_profile(profile_id);
  if (profile) {
    return profile->diameters;
  }
  return {};
}

std::optional<ProfileDiameter> ProfileManager::get_diameter_constraints(
    const std::string& profile_id, double diameter) {
  auto profile = get_profile(profile_id);
  if (!profile) {
    return std::nullopt;
  }

  // Match diameter with some tolerance (0.001 inch)
  const double tolerance = 0.001;
  for (const auto& d : profile->diameters) {
    if (std::fabs(d.OD - diameter) < tolerance) {
      return d;
    }
  }

  return std::nullopt;
}

std::vector<std::string> ProfileManager::validate_tube_spec(
    const TubeSpecification& spec) {
  std::vector<std::string> errors;

  // Validate profile exists
  if (!profile_exists(spec.profile_id)) {
    errors.push_back("Profile not found: " + spec.profile_id);
    return errors; // Can't validate further without profile
  }

  // Get diameter constraints
  auto constraints = get_diameter_constraints(spec.profile_id, spec.diameter);
  if (!constraints) {
    errors.push_back("Diameter not supported by profile: " +
                     std::to_string(spec.diameter));
    return errors; // Can't validate further without constraints
  }

  // Validate wall thickness
  auto profile = get_profile(spec.profile_id);
  if (profile && profile->capacity_max_wall > 0 &&
      spec.wall_thickness > profile->capacity_max_wall) {
    errors.push_back("Wall thickness exceeds profile capacity: " +
                     std::to_string(spec.wall_thickness) + " > " +
                     std::to_string(profile->capacity_max_wall));
  }

  // Validate first segment allowance
  if (spec.first_segment_allowance < constraints->S_min) {
    errors.push_back("First segment allowance below minimum: " +
                     std::to_string(spec.first_segment_allowance) + " < " +
                     std::to_string(constraints->S_min));
  }

  // Validate wall thickness is positive
  if (spec.wall_thickness <= 0) {
    errors.push_back("Wall thickness must be positive");
  }

  // Validate diameter is positive
  if (spec.diameter <= 0) {
    errors.push_back("Diameter must be positive");
  }

  return errors; // Empty = valid
}

bool ProfileManager::profile_exists(const std::string& id) {
  return get_profile(id).has_value();
}

json ProfileManager::get_profile_metadata(const std::string& id) {
  auto profile = get_profile(id);
  if (!profile) {
    return json::object();
  }

  json metadata;
  metadata["id"] = profile->id;
  metadata["name"] = profile->name;
  metadata["category"] = profile->category;
  metadata["manufacturer"] = profile->manufacturer;
  metadata["type"] = profile->type;
  metadata["description"] = profile->description;
  metadata["capacity_max_OD"] = profile->capacity_max_OD;
  metadata["capacity_max_wall"] = profile->capacity_max_wall;
  metadata["diameter_count"] = profile->diameters.size();

  return metadata;
}

} // namespace bendcore
