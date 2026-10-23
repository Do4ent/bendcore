/**
 * BendCore Camera Manager Implementation
 */

#include "camera_manager.hpp"

namespace bendcore {

CameraManager::CameraManager() {
  setup_default_views();
}

void CameraManager::create_view(const std::string& name, const CameraView& view) {
  views_[name] = view;
}

void CameraManager::delete_view(const std::string& name) {
  views_.erase(name);
}

CameraView CameraManager::get_view(const std::string& name) const {
  auto it = views_.find(name);
  if (it != views_.end()) {
    return it->second;
  }
  return CameraView{};
}

std::vector<CameraView> CameraManager::get_all_views() const {
  std::vector<CameraView> result;
  for (const auto& pair : views_) {
    result.push_back(pair.second);
  }
  return result;
}

bool CameraManager::has_view(const std::string& name) const {
  return views_.find(name) != views_.end();
}

CameraView CameraManager::get_or_default(const std::string& name) const {
  if (has_view(name)) {
    return get_view(name);
  }
  return CameraView{};
}

void CameraManager::setup_default_views() {
  // Front view
  create_view("Front", CameraView{
      "Front",
      glm::vec3(0, 0, 100),
      glm::vec3(0, 0, 0),
      100,
      0,
      0,
      "Front view along Z axis",
  });

  // Top view
  create_view("Top", CameraView{
      "Top",
      glm::vec3(0, 100, 0),
      glm::vec3(0, 0, 0),
      100,
      90,
      0,
      "Top view along Y axis",
  });

  // Right view
  create_view("Right", CameraView{
      "Right",
      glm::vec3(100, 0, 0),
      glm::vec3(0, 0, 0),
      100,
      0,
      90,
      "Right side view along X axis",
  });

  // Isometric view
  create_view("Isometric", CameraView{
      "Isometric",
      glm::vec3(70.7f, 70.7f, 70.7f),
      glm::vec3(0, 0, 0),
      120,
      35.26f,
      45,
      "Isometric view (3D perspective)",
  });

  // Bottom view
  create_view("Bottom", CameraView{
      "Bottom",
      glm::vec3(0, -100, 0),
      glm::vec3(0, 0, 0),
      100,
      -90,
      0,
      "Bottom view",
  });

  // Back view
  create_view("Back", CameraView{
      "Back",
      glm::vec3(0, 0, -100),
      glm::vec3(0, 0, 0),
      100,
      0,
      180,
      "Back view along negative Z axis",
  });

  // Left view
  create_view("Left", CameraView{
      "Left",
      glm::vec3(-100, 0, 0),
      glm::vec3(0, 0, 0),
      100,
      0,
      -90,
      "Left side view along negative X axis",
  });
}

json CameraManager::to_json() const {
  json views_array = json::array();
  for (const auto& view : get_all_views()) {
    views_array.push_back(view.to_json());
  }

  return json{
      {"views", views_array},
      {"count", views_array.size()},
  };
}

}  // namespace bendcore
