/**
 * BendCore Feature Tree / Design History
 * Manages design history and document structure
 * Part of Phase 31 foundational work
 */

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace bendcore {

using json = nlohmann::json;

/**
 * FeatureNode: Represents a single element in the design tree
 * Hierarchy: Document → Bodies → Features (sketches, tubes, ports)
 */
struct FeatureNode {
  std::string id;              // Unique identifier
  std::string name;            // Display name
  std::string type;            // body, sketch, tube, port, dimension, constraint
  std::string icon_color;      // Hex color code (#0066FF for blue, etc.)
  bool visible = true;
  bool expanded = false;
  std::vector<std::shared_ptr<FeatureNode>> children;
  std::weak_ptr<FeatureNode> parent;
  std::map<std::string, json> metadata;  // Custom properties

  json to_json() const;
  static std::shared_ptr<FeatureNode> from_json(const json& j);
};

/**
 * DesignHistory: Manages the design tree and feature operations
 */
class DesignHistory {
 public:
  static DesignHistory& instance() {
    static DesignHistory history;
    return history;
  }

  // Tree operations
  std::shared_ptr<FeatureNode> get_root() const;
  std::shared_ptr<FeatureNode> get_feature(const std::string& id) const;
  std::vector<std::shared_ptr<FeatureNode>> get_features(
      const std::string& type) const;

  // Add/remove features
  void add_feature(std::shared_ptr<FeatureNode> feature,
                   const std::string& parent_id = "");
  void remove_feature(const std::string& feature_id);
  void rename_feature(const std::string& feature_id, const std::string& name);

  // Feature visibility
  void set_visible(const std::string& feature_id, bool visible);
  bool is_visible(const std::string& feature_id) const;

  // Tree navigation
  void expand_feature(const std::string& feature_id);
  void collapse_feature(const std::string& feature_id);

  // Serialization
  json to_json() const;
  void from_json(const json& j);

  // Event callback
  using ChangeCallback = std::function<void(const std::string& feature_id)>;
  void on_change(ChangeCallback callback) { change_callback_ = callback; }

 private:
  DesignHistory();

  std::shared_ptr<FeatureNode> root_;
  std::map<std::string, std::shared_ptr<FeatureNode>> feature_map_;
  ChangeCallback change_callback_;

  std::shared_ptr<FeatureNode> find_feature(
      std::shared_ptr<FeatureNode> node,
      const std::string& id) const;

  void notify_change(const std::string& feature_id);
};

}  // namespace bendcore
