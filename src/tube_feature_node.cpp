/**
 * BendCore Phase 35C: Tube Feature Node Implementation
 * Integration of TubeBody into FeatureTree design history
 */

#include "bendcore/tube_feature_node.hpp"
#include "bendcore/feature_tree.hpp"
#include <sstream>
#include <iomanip>

namespace bendcore::tube {

// ============ JSON SERIALIZATION ============

json TubeFeatureNode::to_json() const {
  json j;

  // Base FeatureNode properties
  j["id"] = id;
  j["name"] = name;
  j["type"] = type;
  j["icon_color"] = icon_color;
  j["visible"] = visible;
  j["expanded"] = expanded;

  // Tube-specific properties
  j["tube_id"] = geometry.tube_id;
  j["designation"] = designation;
  j["length"] = length;
  j["mass"] = mass;
  j["volume"] = volume;
  j["num_bends"] = num_bends;
  j["total_bend_angle"] = total_bend_angle;
  j["is_valid"] = is_valid;
  j["validation_errors"] = validation_errors;
  j["validation_warnings"] = validation_warnings;

  // Geometry
  j["geometry"] = geometry.to_json();

  // Profile and spec
  json profile_j;
  profile_j["profile_id"] = profile.profile_id;
  profile_j["name"] = profile.name;
  profile_j["radius"] = profile.radius;
  profile_j["min_bend_angle"] = profile.min_bend_angle;
  profile_j["max_bend_angle"] = profile.max_bend_angle;
  profile_j["min_straight"] = profile.min_straight;
  j["profile"] = profile_j;

  json spec_j;
  spec_j["tube_id"] = spec.tube_id;
  spec_j["outer_diameter"] = spec.outer_diameter;
  spec_j["inner_diameter"] = spec.inner_diameter;
  spec_j["material_id"] = spec.material_id;
  spec_j["density"] = spec.density;
  j["spec"] = spec_j;

  // Path segments
  json path_j = json::array();
  for (const auto& seg : path) {
    json seg_j;
    seg_j["segment_id"] = seg.segment_id;
    seg_j["start"] = {seg.start.x, seg.start.y, seg.start.z};
    seg_j["end"] = {seg.end.x, seg.end.y, seg.end.z};
    seg_j["bend_angle"] = seg.bend_angle;
    seg_j["is_straight"] = seg.is_straight;
    path_j.push_back(seg_j);
  }
  j["path"] = path_j;

  return j;
}

std::shared_ptr<TubeFeatureNode> TubeFeatureNode::from_json(const json& j) {
  auto node = std::make_shared<TubeFeatureNode>();

  // Base properties
  node->id = j.value("id", "");
  node->name = j.value("name", "");
  node->type = j.value("type", "tube");
  node->icon_color = j.value("icon_color", "#FF6B35");
  node->visible = j.value("visible", true);
  node->expanded = j.value("expanded", false);

  // Tube-specific properties
  node->designation = j.value("designation", "");
  node->length = j.value("length", 0.0);
  node->mass = j.value("mass", 0.0);
  node->volume = j.value("volume", 0.0);
  node->num_bends = j.value("num_bends", 0);
  node->total_bend_angle = j.value("total_bend_angle", 0.0);
  node->is_valid = j.value("is_valid", true);

  if (j.contains("validation_errors")) {
    node->validation_errors = j["validation_errors"].get<std::vector<std::string>>();
  }
  if (j.contains("validation_warnings")) {
    node->validation_warnings = j["validation_warnings"].get<std::vector<std::string>>();
  }

  return node;
}

// ============ UTILITY METHODS ============

std::string TubeFeatureNode::get_description() const {
  if (!designation.empty()) {
    return designation;
  }

  std::ostringstream oss;
  oss << "Tube " << id << " - ";
  oss << std::fixed << std::setprecision(1);
  oss << spec.outer_diameter << "×" << spec.inner_diameter << "mm";

  if (num_bends > 0) {
    oss << ", " << num_bends << " bends";
  }

  oss << ", " << std::fixed << std::setprecision(0) << length << "mm";

  return oss.str();
}

json TubeFeatureNode::get_manufacturing_spec() const {
  json j;

  j["designation"] = designation;
  j["material"] = spec.material_id;
  j["outer_diameter"] = spec.outer_diameter;
  j["inner_diameter"] = spec.inner_diameter;
  j["wall_thickness"] = (spec.outer_diameter - spec.inner_diameter) / 2.0;
  j["total_length"] = length;
  j["total_mass"] = mass;
  j["density"] = spec.density;

  return j;
}

json TubeFeatureNode::get_bending_details() const {
  json j;

  j["num_bends"] = num_bends;
  j["total_bend_angle"] = total_bend_angle;
  j["profile_id"] = profile.profile_id;
  j["profile_name"] = profile.name;
  j["profile_radius"] = profile.radius;

  // List each bend with its angle
  json bends = json::array();
  for (const auto& seg : path) {
    if (!seg.is_straight) {
      json bend_j;
      bend_j["segment_id"] = seg.segment_id;
      bend_j["angle_deg"] = seg.bend_angle * 180.0 / M_PI;
      bend_j["angle_rad"] = seg.bend_angle;
      bends.push_back(bend_j);
    }
  }
  j["bends"] = bends;

  return j;
}

json TubeFeatureNode::get_material_info() const {
  json j;

  j["material_id"] = spec.material_id;
  j["density"] = spec.density;
  j["volume_mm3"] = volume;
  j["volume_cm3"] = volume / 1000.0;
  j["mass_grams"] = mass;

  return j;
}

json TubeFeatureNode::get_validation_status() const {
  json j;

  j["is_valid"] = is_valid;
  j["errors"] = validation_errors;
  j["warnings"] = validation_warnings;

  return j;
}

void TubeFeatureNode::update_properties() {
  // Calculate properties from geometry
  length = geometry.total_length;
  volume = geometry.total_volume;
  mass = geometry.total_mass;
  is_valid = geometry.valid;
  validation_errors = geometry.validation_errors;
  validation_warnings = geometry.validation_warnings;

  // Update bend information
  num_bends = 0;
  total_bend_angle = 0.0;

  for (const auto& seg : path) {
    if (!seg.is_straight) {
      num_bends++;
      total_bend_angle += seg.bend_angle * 180.0 / M_PI;
    }
  }
}

std::vector<std::shared_ptr<FeatureNode>> TubeFeatureNode::create_bend_features() {
  std::vector<std::shared_ptr<FeatureNode>> features;

  for (size_t i = 0; i < geometry.bend_sections.size(); ++i) {
    auto bend = geometry.bend_sections[i];

    auto feature = std::make_shared<FeatureNode>();
    feature->id = bend.segment_id;
    feature->name = "Bend " + std::to_string(i + 1);
    feature->type = "bend";
    feature->icon_color = "#FF9700";  // Orange
    feature->visible = visible;

    json meta;
    meta["major_radius"] = bend.major_radius;
    meta["minor_radius"] = bend.minor_radius;
    meta["sweep_angle"] = bend.sweep_angle();
    feature->metadata["geometry"] = meta;

    features.push_back(feature);
  }

  return features;
}

std::vector<std::shared_ptr<FeatureNode>> TubeFeatureNode::create_straight_features() {
  std::vector<std::shared_ptr<FeatureNode>> features;

  for (size_t i = 0; i < geometry.straight_sections.size(); ++i) {
    auto straight = geometry.straight_sections[i];

    auto feature = std::make_shared<FeatureNode>();
    feature->id = straight.segment_id;
    feature->name = "Straight " + std::to_string(i + 1);
    feature->type = "straight_section";
    feature->icon_color = "#4A90E2";  // Blue
    feature->visible = visible;

    json meta;
    meta["length"] = straight.length;
    meta["outer_radius"] = straight.outer_radius;
    meta["inner_radius"] = straight.inner_radius;
    feature->metadata["geometry"] = meta;

    features.push_back(feature);
  }

  return features;
}

// ============ FACTORY ============

std::shared_ptr<TubeFeatureNode> TubeFeatureFactory::create_from_body(
    const TubeBody& body,
    const std::vector<PathSegment>& path,
    const BendingProfile& profile,
    const TubeSpecification& spec) {

  auto node = std::make_shared<TubeFeatureNode>();

  node->id = body.tube_id;
  node->name = "Tube_" + body.tube_id;
  node->geometry = body;
  node->path = path;
  node->profile = profile;
  node->spec = spec;

  node->update_properties();

  return node;
}

bool TubeFeatureFactory::add_to_history(
    std::shared_ptr<TubeFeatureNode> tube_node,
    const std::string& parent_id) {

  try {
    auto& history = bendcore::DesignHistory::instance();

    // Add main tube feature
    history.add_feature(tube_node, parent_id);

    // Add bend sub-features
    auto bend_features = tube_node->create_bend_features();
    for (auto& bend : bend_features) {
      bend->parent = tube_node;
      history.add_feature(bend, tube_node->id);
    }

    // Add straight section sub-features
    auto straight_features = tube_node->create_straight_features();
    for (auto& straight : straight_features) {
      straight->parent = tube_node;
      history.add_feature(straight, tube_node->id);
    }

    return true;
  } catch (const std::exception& e) {
    return false;
  }
}

}  // namespace bendcore::tube
