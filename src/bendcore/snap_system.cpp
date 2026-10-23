/**
 * BendCore Snap System Implementation
 */

#include "snap_system.hpp"
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace bendcore {

std::string snap_mode_to_string(SnapMode mode) {
  switch (mode) {
    case SnapMode::ENDPOINT:
      return "endpoint";
    case SnapMode::CENTER:
      return "center";
    case SnapMode::GRID:
      return "grid";
    case SnapMode::AXIS:
      return "axis";
    case SnapMode::FACE:
      return "face";
    case SnapMode::EDGE:
      return "edge";
    default:
      return "unknown";
  }
}

SnapMode snap_mode_from_string(const std::string& str) {
  if (str == "endpoint") return SnapMode::ENDPOINT;
  if (str == "center") return SnapMode::CENTER;
  if (str == "grid") return SnapMode::GRID;
  if (str == "axis") return SnapMode::AXIS;
  if (str == "face") return SnapMode::FACE;
  if (str == "edge") return SnapMode::EDGE;
  return SnapMode::GRID;
}

void SnapSystem::set_snap_mode(SnapMode mode, bool enabled) {
  auto it = std::find(enabled_modes_.begin(), enabled_modes_.end(), mode);

  if (enabled && it == enabled_modes_.end()) {
    enabled_modes_.push_back(mode);
  } else if (!enabled && it != enabled_modes_.end()) {
    enabled_modes_.erase(it);
  }
}

void SnapSystem::set_snap_modes(const std::vector<SnapMode>& modes) {
  enabled_modes_ = modes;
}

bool SnapSystem::is_snap_enabled(SnapMode mode) const {
  return std::find(enabled_modes_.begin(), enabled_modes_.end(), mode) !=
         enabled_modes_.end();
}

std::vector<SnapMode> SnapSystem::get_enabled_modes() const {
  return enabled_modes_;
}

std::vector<SnapPoint> SnapSystem::detect_snap_points(
    float cursor_x,
    float cursor_y,
    const glm::mat4& view_matrix,
    const glm::mat4& projection_matrix,
    int viewport_width,
    int viewport_height
) {
  if (!snap_enabled_) {
    return {};
  }

  std::vector<SnapPoint> candidates;

  // Generate snap points based on enabled modes
  for (const auto& mode : enabled_modes_) {
    std::vector<SnapPoint> points;

    switch (mode) {
      case SnapMode::GRID:
        points = generate_grid_snap_points();
        break;
      case SnapMode::AXIS:
        points = generate_axis_snap_points();
        break;
      case SnapMode::ENDPOINT:
        points = generate_endpoint_snap_points();
        break;
      case SnapMode::CENTER:
        points = generate_center_snap_points();
        break;
      default:
        break;
    }

    candidates.insert(candidates.end(), points.begin(), points.end());
  }

  // Filter points near cursor
  std::vector<SnapPoint> result;

  for (auto& point : candidates) {
    const glm::vec2 screen_pos = world_to_screen(
        point.position,
        view_matrix,
        projection_matrix,
        viewport_width,
        viewport_height
    );

    float distance = std::hypot(
        screen_pos.x - cursor_x,
        screen_pos.y - cursor_y
    );

    if (distance < snap_radius_) {
      point.distance = distance;
      result.push_back(point);
    }
  }

  // Sort by distance and return closest
  std::sort(result.begin(), result.end(),
            [](const SnapPoint& a, const SnapPoint& b) {
              return a.distance < b.distance;
            });

  return result.empty() ? std::vector<SnapPoint>() :
         std::vector<SnapPoint>{result[0]};
}

std::vector<SnapPoint> SnapSystem::generate_grid_snap_points() const {
  std::vector<SnapPoint> points;
  const int grid_size = 10;
  const int grid_step = 5;

  for (int x = -grid_size; x <= grid_size; x += grid_step) {
    for (int y = -grid_size; y <= grid_size; y += grid_step) {
      points.push_back(SnapPoint{
          glm::vec3(x, y, 0),
          SnapMode::GRID,
          glm::vec3(0, 0, 1),
          0,
      });
    }
  }

  return points;
}

std::vector<SnapPoint> SnapSystem::generate_axis_snap_points() const {
  std::vector<SnapPoint> points;
  const int range = 20;
  const int step = 5;

  // X axis
  for (int x = -range; x <= range; x += step) {
    points.push_back(SnapPoint{
        glm::vec3(x, 0, 0),
        SnapMode::AXIS,
        glm::vec3(1, 0, 0),
        0,
    });
  }

  // Y axis
  for (int y = -range; y <= range; y += step) {
    points.push_back(SnapPoint{
        glm::vec3(0, y, 0),
        SnapMode::AXIS,
        glm::vec3(0, 1, 0),
        0,
    });
  }

  // Z axis
  for (int z = -range; z <= range; z += step) {
    points.push_back(SnapPoint{
        glm::vec3(0, 0, z),
        SnapMode::AXIS,
        glm::vec3(0, 0, 1),
        0,
    });
  }

  return points;
}

std::vector<SnapPoint> SnapSystem::generate_endpoint_snap_points() const {
  // In a full implementation, this would extract endpoints from
  // the actual geometry in the scene
  return {};
}

std::vector<SnapPoint> SnapSystem::generate_center_snap_points() const {
  // In a full implementation, this would extract centers from
  // the actual geometry in the scene
  return {};
}

glm::vec2 SnapSystem::world_to_screen(
    const glm::vec3& position,
    const glm::mat4& view_matrix,
    const glm::mat4& projection_matrix,
    int viewport_width,
    int viewport_height
) const {
  glm::vec4 clip_pos = projection_matrix * view_matrix * glm::vec4(position, 1.0f);
  glm::vec3 ndc = glm::vec3(clip_pos) / clip_pos.w;

  float x = (ndc.x * viewport_width) / 2.0f + viewport_width / 2.0f;
  float y = -(ndc.y * viewport_height) / 2.0f + viewport_height / 2.0f;

  return glm::vec2(x, y);
}

json SnapSystem::to_json() const {
  json modes_array = json::array();
  for (const auto& mode : enabled_modes_) {
    modes_array.push_back(snap_mode_to_string(mode));
  }

  return json{
      {"enabled", snap_enabled_},
      {"radius", snap_radius_},
      {"modes", modes_array},
  };
}

}  // namespace bendcore
