#pragma once

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <cmath>

namespace bendcore::manufacturing::cnc {

// ============================================================================
// Geometry Types
// ============================================================================

struct Point2D {
  double x = 0.0;
  double y = 0.0;

  Point2D() = default;
  Point2D(double x_, double y_) : x(x_), y(y_) {}

  double distance(const Point2D& other) const {
    double dx = x - other.x;
    double dy = y - other.y;
    return std::sqrt(dx * dx + dy * dy);
  }
};

struct Point3D {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;

  Point3D() = default;
  Point3D(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

  double distance(const Point3D& other) const {
    double dx = x - other.x;
    double dy = y - other.y;
    double dz = z - other.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
  }
};

struct Polygon2D {
  std::vector<Point2D> vertices;
  double area = 0.0;
  double perimeter = 0.0;

  void calculateProperties();
};

// ============================================================================
// Machining Strategy Types
// ============================================================================

enum class MachiningStrategy {
  FACE_MILLING,
  POCKET_MILLING,
  CONTOUR_MILLING,
  DRILLING,
  TAPPING,
  BORING,
  THREADING,
  ADAPTIVE_CLEARING,
  RAMP_ENTRY,
  HELICAL_ENTRY,
  SLOT_CUTTING,
  PROFILING
};

enum class AxisConfiguration {
  AXIS_3,     // XYZ
  AXIS_4_XYZ, // XYZB (4th axis rotary)
  AXIS_5_FULL // Full 5-axis
};

// ============================================================================
// Toolpath Data Structures
// ============================================================================

struct ToolPathPoint {
  Point3D position;
  double spindle_speed_rpm = 0.0;
  double feed_rate_mm_min = 0.0;
  double depth_of_cut_mm = 0.0;
  bool rapid_move = false;
  int tool_id = 0;
  std::string comment;

  double getMovementTime(const ToolPathPoint& from) const;
};

struct ToolPath {
  std::vector<ToolPathPoint> points;
  int tool_id = 0;
  MachiningStrategy strategy = MachiningStrategy::CONTOUR_MILLING;
  double total_distance_mm = 0.0;
  double cutting_distance_mm = 0.0;
  double estimated_time_seconds = 0.0;
  double estimated_surface_finish_um = 3.2;

  void calculateMetrics();
};

struct MachiningOperation {
  std::string name;
  MachiningStrategy strategy;
  int tool_id = 0;
  Polygon2D operation_geometry;
  Point3D safe_plane;
  Point3D entry_point;
  Point3D exit_point;
  double depth_first_pass_mm = 1.0;
  double depth_subsequent_passes_mm = 2.0;
  double spindle_speed_rpm = 3000;
  double feed_rate_mm_min = 500;
  double stepover_percent = 50;  // For pocket milling
  bool use_coolant = true;
  std::vector<ToolPath> generated_toolpaths;

  ToolPath generateToolPath();
};

// ============================================================================
// Collision Detection
// ============================================================================

struct CollisionResult {
  bool collision_detected = false;
  Point3D collision_point;
  double distance_to_obstacle_mm = 0.0;
  std::string obstacle_description;
};

class CollisionDetector {
 public:
  // Check collision between tool and geometry
  CollisionResult checkToolCollision(
      const Point3D& tool_position,
      double tool_diameter_mm,
      const std::vector<Point3D>& geometry,
      double z_safe_mm = 5.0);

  // Check for rapid move collisions
  CollisionResult checkRapidMoveCollision(
      const Point3D& from,
      const Point3D& to,
      double tool_diameter_mm,
      const std::vector<Point3D>& stock_geometry);

  // Verify complete toolpath
  std::vector<CollisionResult> verifyToolpath(
      const ToolPath& path,
      double tool_diameter_mm,
      const std::vector<Point3D>& geometry);

 private:
  double point_to_geometry_distance(
      const Point3D& point,
      const std::vector<Point3D>& geometry);
};

// ============================================================================
// Toolpath Generator
// ============================================================================

class CNCToolPathGenerator {
 public:
  CNCToolPathGenerator();

  // Main toolpath generation
  ToolPath generateFaceMilling(
      const Polygon2D& surface,
      double depth_mm,
      int tool_id,
      double spindle_speed_rpm,
      double feed_rate_mm_min,
      double stepover_percent = 80);

  ToolPath generatePocketMilling(
      const Polygon2D& pocket,
      double depth_mm,
      int tool_id,
      double spindle_speed_rpm,
      double feed_rate_mm_min,
      double stepover_percent = 50);

  ToolPath generateContourMilling(
      const std::vector<Point2D>& contour_path,
      double depth_mm,
      int tool_id,
      double spindle_speed_rpm,
      double feed_rate_mm_min);

  ToolPath generateDrillingPath(
      const std::vector<Point2D>& hole_positions,
      double hole_depth_mm,
      int tool_id,
      double spindle_speed_rpm,
      double feed_rate_mm_min);

  ToolPath generateTappingPath(
      const std::vector<Point2D>& hole_positions,
      double depth_mm,
      int tool_id,
      double spindle_speed_rpm);

  ToolPath generateBoringPath(
      const Point2D& bore_center,
      double bore_depth_mm,
      double bore_diameter_mm,
      int tool_id,
      double spindle_speed_rpm,
      double feed_rate_mm_min);

  ToolPath generateThreadingPath(
      const Point3D& thread_start,
      double thread_depth_mm,
      double pitch_mm,
      int tool_id,
      double spindle_speed_rpm);

  ToolPath generateAdaptiveClearing(
      const Polygon2D& pocket,
      double depth_mm,
      int tool_id,
      double spindle_speed_rpm,
      double feed_rate_mm_min);

  // Entry strategies
  ToolPath generateRampEntry(
      const Point2D& entry_point,
      double ramp_angle_degrees,
      double depth_mm,
      int tool_id,
      double spindle_speed_rpm,
      double feed_rate_mm_min);

  ToolPath generateHelicalEntry(
      const Point2D& center,
      double helix_radius_mm,
      double depth_mm,
      int tool_id,
      double spindle_speed_rpm,
      double feed_rate_mm_min);

  // Tool change and safety operations
  ToolPath generateToolChange(int from_tool_id, int to_tool_id);
  ToolPath generateSafetyRetreat(const Point3D& current_pos, double z_safe_mm);

  // Collision detection
  void setCollisionDetector(const std::shared_ptr<CollisionDetector>& detector) {
    collision_detector_ = detector;
  }

  // Generate complete multi-operation toolpath
  std::vector<ToolPath> generateCompleteToolpath(
      const std::vector<MachiningOperation>& operations);

  // Optimization
  ToolPath optimizeToolpath(const ToolPath& path);
  std::vector<ToolPath> optimizeSequence(const std::vector<ToolPath>& paths);

  // Analysis
  struct ToolPathAnalysis {
    double total_cutting_distance_mm = 0.0;
    double total_rapid_distance_mm = 0.0;
    double estimated_machining_time_seconds = 0.0;
    double estimated_surface_finish_um = 3.2;
    int number_of_tool_changes = 0;
    int number_of_operations = 0;
    bool collision_free = true;
  };

  ToolPathAnalysis analyzeToolpath(const std::vector<ToolPath>& paths);

 private:
  std::shared_ptr<CollisionDetector> collision_detector_;

  // Helper methods for toolpath generation
  std::vector<Point3D> generateLinearPath(
      const Point3D& start,
      const Point3D& end,
      double step_mm);

  std::vector<Point3D> generateArcPath(
      const Point3D& center,
      double radius_mm,
      double start_angle,
      double end_angle,
      double step_mm);

  std::vector<Point2D> generateOffsetPath(
      const std::vector<Point2D>& original_path,
      double offset_mm);

  // Optimize point spacing for smooth movement
  std::vector<Point3D> optimizePointSpacing(
      const std::vector<Point3D>& points,
      double max_gap_mm);

  // Convert 2D paths to 3D with depth
  std::vector<Point3D> extrude2DTo3D(
      const std::vector<Point2D>& path_2d,
      double z_depth_mm);
};

}  // namespace bendcore::manufacturing::cnc
