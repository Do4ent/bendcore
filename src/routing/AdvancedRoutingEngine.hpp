#pragma once

#include <vector>
#include <Eigen/Dense>
#include <random>

namespace bendcore::routing {

using Point3 = Eigen::Vector3d;
using Vector3 = Eigen::Vector3d;

enum class Fitting {
  STRAIGHT,
  ELBOW_45,
  ELBOW_90,
  TEE,
  CROSS
};

struct Obstacle {
  Point3 position;
  double radius;
};

struct Segment {
  Point3 start;
  Point3 end;
  double length;

  Vector3 direction() const {
    return (end - start).normalized();
  }
};

struct Path {
  std::vector<Point3> waypoints;
  std::vector<Segment> segments;
  std::vector<Fitting> fittings;
  double total_length = 0.0;
  double cost = 0.0;
};

struct RoutingConstraints {
  double max_segment_length = 100.0;
  double min_bend_radius = 25.0;
  double max_bend_angle = 180.0;
  double workspace_bounds = 10000.0;
  bool avoid_intersections = true;
  bool optimize_length = true;
};

struct TreeNode {
  Point3 position;
  double cost;
  int parent;
  std::vector<int> children;
};

class AdvancedRoutingEngine {
 public:
  explicit AdvancedRoutingEngine(const RoutingConstraints& constraints);

  // Main RRT* path planning algorithm
  Path findOptimalRoute(
    const Point3& start,
    const Point3& end,
    const std::vector<Obstacle>& obstacles,
    const RoutingConstraints& constraints
  );

  // Get/set parameters
  void setMaxIterations(int iterations) { max_iterations_ = iterations; }
  void setRewireRadius(double radius) { rewire_radius_ = radius; }
  void setCollisionMargin(double margin) { collision_margin_ = margin; }

  int getTreeSize() const { return tree_nodes_.size(); }
  double getLastPathCost() const { return last_path_cost_; }

 private:
  // RRT* helper functions
  Vector3 sampleRandomPoint(const Point3& goal, int iteration) const;
  int findNearestNode(const Point3& point) const;
  std::vector<int> findNeighbors(int node_idx, double radius) const;

  bool isCollisionFree(
    const Point3& start,
    const Point3& end,
    const std::vector<Obstacle>& obstacles
  ) const;

  bool checkPointObstacleCollision(
    const Point3& point,
    const Obstacle& obstacle
  ) const;

  // Path processing
  Path extractPath(const Point3& goal);
  void insertFittings(Path& path, const RoutingConstraints& constraints);
  void smoothPath(Path& path, const std::vector<Obstacle>& obstacles);

  // Utility
  double estimateLength(const Path& path) const;

  // RRT* tree
  std::vector<TreeNode> tree_nodes_;
  std::vector<std::pair<int, int>> tree_edges_;

  // Parameters
  RoutingConstraints constraints_;
  int max_iterations_;
  double rewire_radius_;
  double collision_margin_;
  double last_path_cost_ = 0.0;

  // Random number generation
  mutable std::mt19937 rng_;
};

}  // namespace bendcore::routing
