/**
 * BendCore Feature Tree Implementation
 * Design history and feature management
 */

#include "feature_tree.hpp"
#include <algorithm>
#include <sstream>

namespace bendcore {

// ============ FeatureNode Implementation ============

json FeatureNode::to_json() const {
  json j;
  j["id"] = id;
  j["name"] = name;
  j["type"] = type;
  j["iconColor"] = icon_color;
  j["visible"] = visible;
  j["expanded"] = expanded;

  // Add children
  j["children"] = json::array();
  for (const auto& child : children) {
    if (child) {
      j["children"].push_back(child->to_json());
    }
  }

  // Add metadata
  if (!metadata.empty()) {
    j["metadata"] = metadata;
  }

  return j;
}

std::shared_ptr<FeatureNode> FeatureNode::from_json(const json& j) {
  auto node = std::make_shared<FeatureNode>();
  node->id = j.value("id", "");
  node->name = j.value("name", "Feature");
  node->type = j.value("type", "");
  node->icon_color = j.value("iconColor", "#666666");
  node->visible = j.value("visible", true);
  node->expanded = j.value("expanded", false);

  // Reconstruct children
  if (j.contains("children") && j["children"].is_array()) {
    for (const auto& child_json : j["children"]) {
      auto child = FeatureNode::from_json(child_json);
      child->parent = node;
      node->children.push_back(child);
    }
  }

  // Restore metadata
  if (j.contains("metadata") && j["metadata"].is_object()) {
    node->metadata = j["metadata"].get<std::map<std::string, json>>();
  }

  return node;
}

// ============ DesignHistory Implementation ============

DesignHistory::DesignHistory() {
  // Create root document node
  root_ = std::make_shared<FeatureNode>();
  root_->id = "document-root";
  root_->name = "Document";
  root_->type = "document";
  root_->icon_color = "#4A5568";  // Dark gray
  root_->expanded = true;
  feature_map_["document-root"] = root_;
}

std::shared_ptr<FeatureNode> DesignHistory::get_root() const {
  return root_;
}

std::shared_ptr<FeatureNode> DesignHistory::get_feature(
    const std::string& id) const {
  auto it = feature_map_.find(id);
  if (it != feature_map_.end()) {
    return it->second;
  }
  return nullptr;
}

std::vector<std::shared_ptr<FeatureNode>> DesignHistory::get_features(
    const std::string& type) const {
  std::vector<std::shared_ptr<FeatureNode>> result;
  for (const auto& [id, node] : feature_map_) {
    if (node && node->type == type) {
      result.push_back(node);
    }
  }
  return result;
}

void DesignHistory::add_feature(std::shared_ptr<FeatureNode> feature,
                                 const std::string& parent_id) {
  if (!feature) {
    return;
  }

  // Find parent
  std::shared_ptr<FeatureNode> parent = root_;
  if (!parent_id.empty()) {
    auto parent_node = get_feature(parent_id);
    if (parent_node) {
      parent = parent_node;
    }
  }

  // Add to parent's children
  feature->parent = parent;
  parent->children.push_back(feature);

  // Index the feature
  feature_map_[feature->id] = feature;

  notify_change(parent->id);
}

void DesignHistory::remove_feature(const std::string& feature_id) {
  auto feature = get_feature(feature_id);
  if (!feature) {
    return;
  }

  // Get parent
  auto parent_ptr = feature->parent.lock();
  if (!parent_ptr) {
    return;
  }

  // Remove from parent's children
  auto& children = parent_ptr->children;
  children.erase(
      std::remove_if(children.begin(), children.end(),
                     [&feature_id](const std::shared_ptr<FeatureNode>& child) {
                       return child && child->id == feature_id;
                     }),
      children.end());

  // Remove from map
  feature_map_.erase(feature_id);

  notify_change(parent_ptr->id);
}

void DesignHistory::rename_feature(const std::string& feature_id,
                                    const std::string& name) {
  auto feature = get_feature(feature_id);
  if (feature) {
    feature->name = name;
    notify_change(feature_id);
  }
}

void DesignHistory::set_visible(const std::string& feature_id, bool visible) {
  auto feature = get_feature(feature_id);
  if (feature) {
    feature->visible = visible;
    notify_change(feature_id);
  }
}

bool DesignHistory::is_visible(const std::string& feature_id) const {
  auto feature = get_feature(feature_id);
  return feature ? feature->visible : true;
}

void DesignHistory::expand_feature(const std::string& feature_id) {
  auto feature = get_feature(feature_id);
  if (feature) {
    feature->expanded = true;
    notify_change(feature_id);
  }
}

void DesignHistory::collapse_feature(const std::string& feature_id) {
  auto feature = get_feature(feature_id);
  if (feature) {
    feature->expanded = false;
    notify_change(feature_id);
  }
}

json DesignHistory::to_json() const {
  json j;
  j["root"] = root_->to_json();
  j["version"] = 1;
  j["timestamp"] = std::time(nullptr);
  return j;
}

void DesignHistory::from_json(const json& j) {
  if (j.contains("root")) {
    root_ = FeatureNode::from_json(j["root"]);
    feature_map_.clear();

    // Re-index all features
    std::function<void(std::shared_ptr<FeatureNode>)> index_node =
        [this, &index_node](std::shared_ptr<FeatureNode> node) {
          if (!node) return;
          feature_map_[node->id] = node;
          for (auto& child : node->children) {
            if (child) {
              index_node(child);
            }
          }
        };

    index_node(root_);
  }
}

void DesignHistory::notify_change(const std::string& feature_id) {
  if (change_callback_) {
    change_callback_(feature_id);
  }
}

}  // namespace bendcore
