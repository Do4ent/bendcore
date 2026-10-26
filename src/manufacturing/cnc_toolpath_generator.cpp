#include "cnc_toolpath_generator.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace bendcore::manufacturing::cnc {

// ============================================================================
// Geometry Implementation
// ============================================================================

void Polygon2D::calculateProperties() {
  area = 0.0;
  perimeter = 0.0;

  if (vertices.size() < 3) return;

  // Calculate area using shoelace formula
  for (size_t i = 0; i < vertices.size(); ++i) {
    size_t j = (i + 1) % vertices.size();
    area += vertices[i].x * vertices[j].y;
    area -= vertices[j].x * vertices[i].y;
  }
  area = std::abs(area) / 2.0;

  // Calculate perimeter
  for (size_t i = 0; i < vertices.size(); ++i) {
    size_t j = (i + 1) % vertices.size();
    perimeter += vertices[i].distance(vertices[j]);
  }
}

// ============================================================================
// ToolPathPoint Implementation
// ============================================================================

double ToolPathPoint::getMovementTime(const ToolPathPoint& from) const {
  double distance = position.distance(from.position);
  if (feed_rate_mm_min <= 0) return 0.0;

  double feed_rate_mm_sec = feed_rate_mm_min / 60.0;
  return distance / feed_rate_mm_sec;
}

// ============================================================================
// ToolPath Implementation
// ============================================================================

void ToolPath::calculateMetrics() {
  total_distance_mm = 0.0;
  cutting_distance_mm = 0.0;
  estimated_time_seconds = 0.0;

  for (size_t i = 1; i < points.size(); ++i) {
    double distance = points[i].position.distance(points[i - 1].position);
    total_distance_mm += distance;

    if (!points[i].rapid_move) {
      cutting_distance_mm += distance;
    }

    if (points[i].feed_rate_mm_min > 0) {
      double feed_rate_mm_sec = points[i].feed_rate_mm_min / 60.0;
      double time = distance / feed_rate_mm_sec;
      estimated_time_seconds += time;
    }
  }
}

// ============================================================================
// CollisionDetector Implementation
// ============================================================================

CollisionResult CollisionDetector::checkToolCollision(
    const Point3D& tool_position,
    double tool_diameter_mm,
    const std::vector<Point3D>& geometry,
    double z_safe_mm) {
  CollisionResult result;

  // Check if tool would collide with geometry
  double tool_radius = tool_diameter_mm / 2.0;
  double min_distance = std::numeric_limits<double>::max();
  Point3D closest_point = tool_position;

  for (const auto& geom_point : geometry) {
    // Only check points below the safe plane
    if (geom_point.z < tool_position.z - z_safe_mm) {
      double distance = tool_position.distance(geom_point);
      if (distance < min_distance) {
        min_distance = distance;
        closest_point = geom_point;
      }
    }
  }

  if (min_distance < tool_radius && min_distance != std::numeric_limits<double>::max()) {
    result.collision_detected = true;
    result.collision_point = closest_point;
    result.distance_to_obstacle_mm = tool_radius - min_distance;
  }

  return result;
}

CollisionResult CollisionDetector::checkRapidMoveCollision(
    const Point3D& from,
    const Point3D& to,
    double tool_diameter_mm,
    const std::vector<Point3D>& stock_geometry) {
  CollisionResult result;

  // Check intermediate points along the rapid move path
  const int num_check_points = 10;
  for (int i = 1; i < num_check_points; ++i) {
    double t = static_cast<double>(i) / num_check_points;
    Point3D check_point(
        from.x + (to.x - from.x) * t,
        from.y + (to.y - from.y) * t,
        from.z + (to.z - from.z) * t);

    auto collision = checkToolCollision(check_point, tool_diameter_mm, stock_geometry);
    if (collision.collision_detected) {
      return collision;
    }
  }

  return result;
}

std::vector<CollisionResult> CollisionDetector::verifyToolpath(
    const ToolPath& path,
    double tool_diameter_mm,
    const std::vector<Point3D>& geometry) {
  std::vector<CollisionResult> collisions;

  for (size_t i = 1; i < path.points.size(); ++i) {
    if (path.points[i].rapid_move) {
      auto collision = checkRapidMoveCollision(
          path.points[i - 1].position,
          path.points[i].position,
          tool_diameter_mm,
          geometry);
      if (collision.collision_detected) {
        collisions.push_back(collision);
      }
    }
  }

  return collisions;
}

double CollisionDetector::point_to_geometry_distance(
    const Point3D& point,
    const std::vector<Point3D>& geometry) {
  double min_distance = std::numeric_limits<double>::max();

  for (const auto& geom : geometry) {
    double distance = point.distance(geom);
    min_distance = std::min(min_distance, distance);
  }

  return min_distance;
}

// ============================================================================
// CNCToolPathGenerator Implementation
// ============================================================================

CNCToolPathGenerator::CNCToolPathGenerator()
    : collision_detector_(std::make_shared<CollisionDetector>()) {}

ToolPath CNCToolPathGenerator::generateFaceMilling(
    const Polygon2D& surface,
    double depth_mm,
    int tool_id,
    double spindle_speed_rpm,
    double feed_rate_mm_min,
    double stepover_percent) {
  ToolPath path;
  path.tool_id = tool_id;
  path.strategy = MachiningStrategy::FACE_MILLING;

  double stepover = (stepover_percent / 100.0) * 10.0;  // Assume 10mm tool

  // Generate parallel lines across the surface
  double min_y = 0, max_y = 100;
  double min_x = 0, max_x = 100;

  for (double y = min_y; y <= max_y; y += stepover) {
    std::vector<Point3D> line_points;

    if (static_cast<int>(y / stepover) % 2 == 0) {
      // Left to right
      for (double x = min_x; x <= max_x; x += 1.0) {
        line_points.push_back(Point3D(x, y, -depth_mm));
      }
    } else {
      // Right to left
      for (double x = max_x; x >= min_x; x -= 1.0) {
        line_points.push_back(Point3D(x, y, -depth_mm));
      }
    }

    // Add points to path
    for (size_t i = 0; i < line_points.size(); ++i) {
      ToolPathPoint p;
      p.position = line_points[i];
      p.spindle_speed_rpm = spindle_speed_rpm;
      p.feed_rate_mm_min = feed_rate_mm_min;
      p.depth_of_cut_mm = stepover;
      p.tool_id = tool_id;
      path.points.push_back(p);
    }
  }

  path.calculateMetrics();
  return path;
}

ToolPath CNCToolPathGenerator::generatePocketMilling(
    const Polygon2D& pocket,
    double depth_mm,
    int tool_id,
    double spindle_speed_rpm,
    double feed_rate_mm_min,
    double stepover_percent) {
  ToolPath path;
  path.tool_id = tool_id;
  path.strategy = MachiningStrategy::POCKET_MILLING;

  // Generate concentric offsets for pocket clearing
  double offset_step = (stepover_percent / 100.0) * 10.0;
  double current_offset = offset_step;
  double max_offset = 50.0;

  while (current_offset <= max_offset) {
    // Generate offset contour
    auto offset_path = generateOffsetPath(pocket.vertices, current_offset);

    // Convert 2D to 3D and add to toolpath
    auto path_3d = extrude2DTo3D(offset_path, -depth_mm);

    for (const auto& point : path_3d) {
      ToolPathPoint tp;
      tp.position = point;
      tp.spindle_speed_rpm = spindle_speed_rpm;
      tp.feed_rate_mm_min = feed_rate_mm_min;
      tp.depth_of_cut_mm = offset_step;
      tp.tool_id = tool_id;
      path.points.push_back(tp);
    }

    current_offset += offset_step;
  }

  path.calculateMetrics();
  return path;
}

ToolPath CNCToolPathGenerator::generateContourMilling(
    const std::vector<Point2D>& contour_path,
    double depth_mm,
    int tool_id,
    double spindle_speed_rpm,
    double feed_rate_mm_min) {
  ToolPath path;
  path.tool_id = tool_id;
  path.strategy = MachiningStrategy::CONTOUR_MILLING;

  auto path_3d = extrude2DTo3D(contour_path, -depth_mm);
  path_3d = optimizePointSpacing(path_3d, 0.5);

  for (const auto& point : path_3d) {
    ToolPathPoint tp;
    tp.position = point;
    tp.spindle_speed_rpm = spindle_speed_rpm;
    tp.feed_rate_mm_min = feed_rate_mm_min;
    tp.tool_id = tool_id;
    path.points.push_back(tp);
  }

  path.calculateMetrics();
  return path;
}

ToolPath CNCToolPathGenerator::generateDrillingPath(
    const std::vector<Point2D>& hole_positions,
    double hole_depth_mm,
    int tool_id,
    double spindle_speed_rpm,
    double feed_rate_mm_min) {
  ToolPath path;
  path.tool_id = tool_id;
  path.strategy = MachiningStrategy::DRILLING;

  double z_safe = 5.0;

  for (const auto& hole_pos : hole_positions) {
    // Rapid to hole position at safe height
    ToolPathPoint rapid;
    rapid.position = Point3D(hole_pos.x, hole_pos.y, z_safe);
    rapid.rapid_move = true;
    rapid.spindle_speed_rpm = spindle_speed_rpm;
    rapid.tool_id = tool_id;
    path.points.push_back(rapid);

    // Drill down
    ToolPathPoint drill;
    drill.position = Point3D(hole_pos.x, hole_pos.y, -hole_depth_mm);
    drill.spindle_speed_rpm = spindle_speed_rpm;
    drill.feed_rate_mm_min = feed_rate_mm_min;
    drill.tool_id = tool_id;
    path.points.push_back(drill);

    // Retract
    ToolPathPoint retract;
    retract.position = Point3D(hole_pos.x, hole_pos.y, z_safe);
    retract.rapid_move = true;
    retract.tool_id = tool_id;
    path.points.push_back(retract);
  }

  path.calculateMetrics();
  return path;
}

ToolPath CNCToolPathGenerator::generateTappingPath(
    const std::vector<Point2D>& hole_positions,
    double depth_mm,
    int tool_id,
    double spindle_speed_rpm) {
  ToolPath path;
  path.tool_id = tool_id;
  path.strategy = MachiningStrategy::TAPPING;

  double z_safe = 5.0;
  // For tapping, feed rate is synchronized with spindle speed
  double feed_rate_mm_min = spindle_speed_rpm * 0.1;

  for (const auto& hole_pos : hole_positions) {
    // Rapid to hole position
    ToolPathPoint rapid;
    rapid.position = Point3D(hole_pos.x, hole_pos.y, z_safe);
    rapid.rapid_move = true;
    rapid.spindle_speed_rpm = spindle_speed_rpm;
    rapid.tool_id = tool_id;
    path.points.push_back(rapid);

    // Tap down
    ToolPathPoint tap;
    tap.position = Point3D(hole_pos.x, hole_pos.y, -depth_mm);
    tap.spindle_speed_rpm = spindle_speed_rpm;
    tap.feed_rate_mm_min = feed_rate_mm_min;
    tap.tool_id = tool_id;
    path.points.push_back(tap);

    // Retract
    ToolPathPoint retract;
    retract.position = Point3D(hole_pos.x, hole_pos.y, z_safe);
    retract.rapid_move = true;
    retract.tool_id = tool_id;
    path.points.push_back(retract);
  }

  path.calculateMetrics();
  return path;
}

ToolPath CNCToolPathGenerator::generateBoringPath(
    const Point2D& bore_center,
    double bore_depth_mm,
    double bore_diameter_mm,
    int tool_id,
    double spindle_speed_rpm,
    double feed_rate_mm_min) {
  ToolPath path;
  path.tool_id = tool_id;
  path.strategy = MachiningStrategy::BORING;

  double z_safe = 5.0;
  double radius = bore_diameter_mm / 2.0;
  double num_passes = 3;

  for (int pass = 0; pass < static_cast<int>(num_passes); ++pass) {
    double current_depth = -(bore_depth_mm / num_passes) * (pass + 1);

    // Spiral downward
    const int num_revolutions = 5;
    const int points_per_revolution = 36;
    double angle_step = (2.0 * M_PI) / points_per_revolution;

    for (int i = 0; i < num_revolutions * points_per_revolution; ++i) {
      double angle = i * angle_step;
      double x = bore_center.x + radius * std::cos(angle);
      double y = bore_center.y + radius * std::sin(angle);
      double z =
          (current_depth / (num_revolutions * points_per_revolution)) * i;

      ToolPathPoint p;
      p.position = Point3D(x, y, z);
      p.spindle_speed_rpm = spindle_speed_rpm;
      p.feed_rate_mm_min = feed_rate_mm_min;
      p.tool_id = tool_id;
      path.points.push_back(p);
    }
  }

  // Retract
  ToolPathPoint retract;
  retract.position = Point3D(bore_center.x, bore_center.y, z_safe);
  retract.rapid_move = true;
  retract.tool_id = tool_id;
  path.points.push_back(retract);

  path.calculateMetrics();
  return path;
}

ToolPath CNCToolPathGenerator::generateThreadingPath(
    const Point3D& thread_start,
    double thread_depth_mm,
    double pitch_mm,
    int tool_id,
    double spindle_speed_rpm) {
  ToolPath path;
  path.tool_id = tool_id;
  path.strategy = MachiningStrategy::THREADING;

  double feed_rate_mm_min = spindle_speed_rpm * pitch_mm;
  double z_safe = 5.0;

  // Rapid to start position
  ToolPathPoint rapid;
  rapid.position = thread_start;
  rapid.position.z = z_safe;
  rapid.rapid_move = true;
  rapid.spindle_speed_rpm = spindle_speed_rpm;
  rapid.tool_id = tool_id;
  path.points.push_back(rapid);

  // Threading pass
  int num_passes = static_cast<int>(std::ceil(thread_depth_mm / pitch_mm));

  for (int pass = 0; pass < num_passes; ++pass) {
    double depth = -(pitch_mm * (pass + 1));
    if (depth < -thread_depth_mm) {
      depth = -thread_depth_mm;
    }

    ToolPathPoint thread_point;
    thread_point.position = thread_start;
    thread_point.position.z = depth;
    thread_point.spindle_speed_rpm = spindle_speed_rpm;
    thread_point.feed_rate_mm_min = feed_rate_mm_min;
    thread_point.tool_id = tool_id;
    path.points.push_back(thread_point);

    // Retract after each pass
    ToolPathPoint retract;
    retract.position = thread_start;
    retract.position.z = z_safe;
    retract.rapid_move = true;
    retract.tool_id = tool_id;
    path.points.push_back(retract);
  }

  path.calculateMetrics();
  return path;
}

ToolPath CNCToolPathGenerator::generateAdaptiveClearing(
    const Polygon2D& pocket,
    double depth_mm,
    int tool_id,
    double spindle_speed_rpm,
    double feed_rate_mm_min) {
  // Adaptive clearing uses smaller stepover for better surface finish
  return generatePocketMilling(pocket, depth_mm, tool_id, spindle_speed_rpm,
                              feed_rate_mm_min, 30);  // 30% stepover
}

ToolPath CNCToolPathGenerator::generateRampEntry(
    const Point2D& entry_point,
    double ramp_angle_degrees,
    double depth_mm,
    int tool_id,
    double spindle_speed_rpm,
    double feed_rate_mm_min) {
  ToolPath path;
  path.tool_id = tool_id;
  path.strategy = MachiningStrategy::RAMP_ENTRY;

  double ramp_angle_rad = ramp_angle_degrees * M_PI / 180.0;
  double horizontal_distance = depth_mm / std::tan(ramp_angle_rad);

  const int num_steps = 20;
  for (int i = 0; i <= num_steps; ++i) {
    double t = static_cast<double>(i) / num_steps;
    double x = entry_point.x + (horizontal_distance * std::cos(ramp_angle_rad) * t);
    double y = entry_point.y + (horizontal_distance * std::sin(ramp_angle_rad) * t);
    double z = -depth_mm * t;

    ToolPathPoint p;
    p.position = Point3D(x, y, z);
    p.spindle_speed_rpm = spindle_speed_rpm;
    p.feed_rate_mm_min = feed_rate_mm_min;
    p.tool_id = tool_id;
    path.points.push_back(p);
  }

  path.calculateMetrics();
  return path;
}

ToolPath CNCToolPathGenerator::generateHelicalEntry(
    const Point2D& center,
    double helix_radius_mm,
    double depth_mm,
    int tool_id,
    double spindle_speed_rpm,
    double feed_rate_mm_min) {
  ToolPath path;
  path.tool_id = tool_id;
  path.strategy = MachiningStrategy::HELICAL_ENTRY;

  const int total_revolutions = 5;
  const int points_per_revolution = 36;
  const int total_points = total_revolutions * points_per_revolution;

  for (int i = 0; i < total_points; ++i) {
    double angle = (2.0 * M_PI * i) / points_per_revolution;
    double x = center.x + helix_radius_mm * std::cos(angle);
    double y = center.y + helix_radius_mm * std::sin(angle);
    double z = -(depth_mm * i) / total_points;

    ToolPathPoint p;
    p.position = Point3D(x, y, z);
    p.spindle_speed_rpm = spindle_speed_rpm;
    p.feed_rate_mm_min = feed_rate_mm_min;
    p.tool_id = tool_id;
    path.points.push_back(p);
  }

  path.calculateMetrics();
  return path;
}

ToolPath CNCToolPathGenerator::generateToolChange(int from_tool_id,
                                                   int to_tool_id) {
  ToolPath path;
  path.tool_id = to_tool_id;

  // Tool change is an M6 command, represented as movement to tool changer
  ToolPathPoint tool_change;
  tool_change.position = Point3D(0, 0, 0);  // Tool changer position
  tool_change.tool_id = to_tool_id;
  tool_change.comment = "Tool change: T" + std::to_string(from_tool_id) + " -> T" +
                        std::to_string(to_tool_id);
  path.points.push_back(tool_change);

  return path;
}

ToolPath CNCToolPathGenerator::generateSafetyRetreat(const Point3D& current_pos,
                                                      double z_safe_mm) {
  ToolPath path;

  ToolPathPoint retreat;
  retreat.position = Point3D(current_pos.x, current_pos.y, z_safe_mm);
  retreat.rapid_move = true;
  retreat.comment = "Safety retreat";
  path.points.push_back(retreat);

  path.calculateMetrics();
  return path;
}

std::vector<ToolPath> CNCToolPathGenerator::generateCompleteToolpath(
    const std::vector<MachiningOperation>& operations) {
  std::vector<ToolPath> complete_toolpath;

  for (const auto& operation : operations) {
    ToolPath op_path = operation.generateToolPath();
    complete_toolpath.push_back(op_path);
  }

  return complete_toolpath;
}

ToolPath CNCToolPathGenerator::optimizeToolpath(const ToolPath& path) {
  ToolPath optimized = path;

  // Remove unnecessary moves
  std::vector<ToolPathPoint> filtered_points;

  for (size_t i = 0; i < path.points.size(); ++i) {
    if (i == 0 || i == path.points.size() - 1) {
      filtered_points.push_back(path.points[i]);
    } else {
      // Check if point is collinear with previous and next points
      auto& p1 = path.points[i - 1].position;
      auto& p2 = path.points[i].position;
      auto& p3 = path.points[i + 1].position;

      double cross_product = (p2.x - p1.x) * (p3.y - p1.y) -
                            (p2.y - p1.y) * (p3.x - p1.x);
      if (std::abs(cross_product) > 0.01) {  // Not collinear
        filtered_points.push_back(path.points[i]);
      }
    }
  }

  optimized.points = filtered_points;
  optimized.calculateMetrics();
  return optimized;
}

std::vector<ToolPath> CNCToolPathGenerator::optimizeSequence(
    const std::vector<ToolPath>& paths) {
  std::vector<ToolPath> optimized = paths;

  // Simple greedy optimization: minimize tool changes
  std::sort(optimized.begin(), optimized.end(),
            [](const ToolPath& a, const ToolPath& b) {
              return a.tool_id < b.tool_id;
            });

  return optimized;
}

CNCToolPathGenerator::ToolPathAnalysis CNCToolPathGenerator::analyzeToolpath(
    const std::vector<ToolPath>& paths) {
  ToolPathAnalysis analysis;

  for (const auto& path : paths) {
    analysis.total_cutting_distance_mm += path.cutting_distance_mm;
    analysis.total_rapid_distance_mm +=
        path.total_distance_mm - path.cutting_distance_mm;
    analysis.estimated_machining_time_seconds += path.estimated_time_seconds;
    analysis.number_of_operations++;
  }

  // Count tool changes
  int current_tool = -1;
  for (const auto& path : paths) {
    if (path.tool_id != current_tool && current_tool != -1) {
      analysis.number_of_tool_changes++;
    }
    current_tool = path.tool_id;
  }

  return analysis;
}

std::vector<Point3D> CNCToolPathGenerator::generateLinearPath(
    const Point3D& start,
    const Point3D& end,
    double step_mm) {
  std::vector<Point3D> points;
  double distance = start.distance(end);
  int num_steps = static_cast<int>(distance / step_mm) + 1;

  for (int i = 0; i <= num_steps; ++i) {
    double t = (num_steps > 0) ? static_cast<double>(i) / num_steps : 0.0;
    Point3D p(
        start.x + (end.x - start.x) * t,
        start.y + (end.y - start.y) * t,
        start.z + (end.z - start.z) * t);
    points.push_back(p);
  }

  return points;
}

std::vector<Point3D> CNCToolPathGenerator::generateArcPath(
    const Point3D& center,
    double radius_mm,
    double start_angle,
    double end_angle,
    double step_mm) {
  std::vector<Point3D> points;

  double arc_length = radius_mm * std::abs(end_angle - start_angle);
  int num_steps = static_cast<int>(arc_length / step_mm) + 1;

  for (int i = 0; i <= num_steps; ++i) {
    double t = (num_steps > 0) ? static_cast<double>(i) / num_steps : 0.0;
    double angle = start_angle + (end_angle - start_angle) * t;

    Point3D p(
        center.x + radius_mm * std::cos(angle),
        center.y + radius_mm * std::sin(angle),
        center.z);
    points.push_back(p);
  }

  return points;
}

std::vector<Point2D> CNCToolPathGenerator::generateOffsetPath(
    const std::vector<Point2D>& original_path,
    double offset_mm) {
  // Simplified offset - just shrink towards center
  std::vector<Point2D> offset_path;

  for (const auto& point : original_path) {
    double scale = 1.0 - (offset_mm / 100.0);
    offset_path.push_back(Point2D(point.x * scale, point.y * scale));
  }

  return offset_path;
}

std::vector<Point3D> CNCToolPathGenerator::optimizePointSpacing(
    const std::vector<Point3D>& points,
    double max_gap_mm) {
  std::vector<Point3D> optimized;

  for (size_t i = 0; i < points.size(); ++i) {
    optimized.push_back(points[i]);

    if (i < points.size() - 1) {
      double distance = points[i].distance(points[i + 1]);

      if (distance > max_gap_mm) {
        int num_intermediate =
            static_cast<int>(std::ceil(distance / max_gap_mm)) - 1;

        for (int j = 1; j <= num_intermediate; ++j) {
          double t = static_cast<double>(j) / (num_intermediate + 1);
          Point3D intermediate(
              points[i].x + (points[i + 1].x - points[i].x) * t,
              points[i].y + (points[i + 1].y - points[i].y) * t,
              points[i].z + (points[i + 1].z - points[i].z) * t);
          optimized.push_back(intermediate);
        }
      }
    }
  }

  return optimized;
}

std::vector<Point3D> CNCToolPathGenerator::extrude2DTo3D(
    const std::vector<Point2D>& path_2d,
    double z_depth_mm) {
  std::vector<Point3D> path_3d;

  for (const auto& point_2d : path_2d) {
    path_3d.push_back(Point3D(point_2d.x, point_2d.y, z_depth_mm));
  }

  return path_3d;
}

ToolPath MachiningOperation::generateToolPath() {
  // Simple implementation - just return empty path
  // In a real system, this would call the appropriate generator
  ToolPath path;
  path.tool_id = tool_id;
  path.strategy = strategy;
  return path;
}

}  // namespace bendcore::manufacturing::cnc
