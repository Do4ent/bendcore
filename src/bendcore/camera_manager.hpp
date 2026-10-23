/**
 * BendCore Camera Manager
 * Manages camera positions, named views, and view transitions
 */

#pragma once

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <map>
#include <string>
#include <memory>

namespace bendcore {

using json = nlohmann::json;

/**
 * CameraView: A named camera view with state
 */
struct CameraView {
  std::string name;
  glm::vec3 position{0, 0, 100};
  glm::vec3 target{0, 0, 0};
  float distance{100};
  float pitch{0};
  float yaw{0};
  std::string description;

  json to_json() const {
    return json{
        {"name", name},
        {"position", json{{"x", position.x}, {"y", position.y}, {"z", position.z}}},
        {"target", json{{"x", target.x}, {"y", target.y}, {"z", target.z}}},
        {"distance", distance},
        {"pitch", pitch},
        {"yaw", yaw},
        {"description", description},
    };
  }
};

/**
 * CameraManager: Manages camera views and transitions
 */
class CameraManager {
 public:
  static CameraManager& instance() {
    static CameraManager manager;
    return manager;
  }

  // View management
  void create_view(const std::string& name, const CameraView& view);
  void delete_view(const std::string& name);
  CameraView get_view(const std::string& name) const;
  std::vector<CameraView> get_all_views() const;

  // View recall
  bool has_view(const std::string& name) const;
  CameraView get_or_default(const std::string& name) const;

  // Default views setup
  void setup_default_views();

  // JSON serialization
  json to_json() const;

 private:
  CameraManager();

  std::map<std::string, CameraView> views_;
};

}  // namespace bendcore
