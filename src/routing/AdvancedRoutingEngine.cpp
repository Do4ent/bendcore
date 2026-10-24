#include "AdvancedRoutingEngine.hpp"
#include <algorithm>
#include <random>
#include <cmath>
#include <Eigen/Dense>
#include <Eigen/Geometry>

namespace bendcore::routing {

AdvancedRoutingEngine::AdvancedRoutingEngine(
    const RoutingConstraints& constraints)
    : constraints_(constraints),
      max_iterations_(5000),
      rewire_radius_(0.5),
      collision_margin_(0.05) {
  std::random_device rd;
  rng_.seed(rd());
}

Path AdvancedRoutingEngine::findOptimalRoute(
    const Point3& start,
    const Point3& end,
    const std::vector<Obstacle>& obstacles,
    const RoutingConstraints& constraints) {

  // Reset search state
  tree_nodes_.clear();
  tree_edges_.clear();

  // 1. Create root node
  TreeNode root{
    .position = start,
    .cost = 0.0,
    .parent = -1,
    .children = {}
  };
  tree_nodes_.push_back(root);

  // 2. RRT* main loop
  Point3 best_goal = start;
  double best_cost = std::numeric_limits<double>::infinity();

  for (int iter = 0; iter < max_iterations_; iter++) {
    // Sample random point (with goal biasing)
    Point3 random_point = sampleRandomPoint(end, iter);

    // Find nearest node
    int nearest_idx = findNearestNode(random_point);
    const TreeNode& nearest = tree_nodes_[nearest_idx];

    // Extend tree
    Vector3 direction = (random_point - nearest.position).normalized();
    Point3 new_pos = nearest.position +
                     direction * constraints.max_segment_length;

    // Check collision
    if (!isCollisionFree(nearest.position, new_pos, obstacles)) {
      continue;
    }

    // Create new node
    double edge_cost = (new_pos - nearest.position).norm();
    double node_cost = nearest.cost + edge_cost;

    TreeNode new_node{
      .position = new_pos,
      .cost = node_cost,
      .parent = nearest_idx,
      .children = {}
    };
    int new_idx = tree_nodes_.size();
    tree_nodes_.push_back(new_node);
    tree_nodes_[nearest_idx].children.push_back(new_idx);

    // Rewire neighbors
    std::vector<int> neighbors = findNeighbors(new_idx, rewire_radius_);

    for (int neighbor_idx : neighbors) {
      double through_new_cost = new_node.cost +
        (tree_nodes_[neighbor_idx].position - new_pos).norm();

      if (through_new_cost < tree_nodes_[neighbor_idx].cost) {
        // Check collision on new path
        if (isCollisionFree(new_pos,
                           tree_nodes_[neighbor_idx].position,
                           obstacles)) {
          // Rewire parent
          int old_parent = tree_nodes_[neighbor_idx].parent;
          if (old_parent >= 0) {
            auto& children = tree_nodes_[old_parent].children;
            children.erase(
              std::remove(children.begin(), children.end(), neighbor_idx),
              children.end()
            );
          }

          tree_nodes_[neighbor_idx].parent = new_idx;
          tree_nodes_[neighbor_idx].cost = through_new_cost;
          new_node.children.push_back(neighbor_idx);
        }
      }
    }

    // Check if close to goal
    double dist_to_goal = (new_pos - end).norm();
    if (dist_to_goal < constraints.max_segment_length * 2) {
      if (isCollisionFree(new_pos, end, obstacles)) {
        double goal_cost = node_cost + dist_to_goal;
        if (goal_cost < best_cost) {
          best_cost = goal_cost;
          best_goal = end;
        }
      }
    }
  }

  // 3. Extract path
  Path path = extractPath(best_goal);

  // 4. Insert fittings
  insertFittings(path, constraints);

  // 5. Smooth path
  smoothPath(path, obstacles);

  return path;
}

Vector3 AdvancedRoutingEngine::sampleRandomPoint(
    const Point3& goal,
    int iteration) const {

  // Goal biasing: 10% chance to sample goal
  std::uniform_real_distribution<double> dist(0.0, 1.0);
  if (dist(rng_) < 0.1) {
    return goal;
  }

  // Sample random point in workspace
  std::uniform_real_distribution<double> coord_dist(
    -constraints_.workspace_bounds,
    constraints_.workspace_bounds
  );

  return Point3{
    coord_dist(rng_),
    coord_dist(rng_),
    coord_dist(rng_)
  };
}

int AdvancedRoutingEngine::findNearestNode(
    const Point3& point) const {

  int nearest_idx = 0;
  double min_dist = std::numeric_limits<double>::infinity();

  for (size_t i = 0; i < tree_nodes_.size(); i++) {
    double dist = (tree_nodes_[i].position - point).norm();
    if (dist < min_dist) {
      min_dist = dist;
      nearest_idx = i;
    }
  }

  return nearest_idx;
}

std::vector<int> AdvancedRoutingEngine::findNeighbors(
    int node_idx,
    double radius) const {

  std::vector<int> neighbors;
  const Point3& node_pos = tree_nodes_[node_idx].position;

  for (size_t i = 0; i < tree_nodes_.size(); i++) {
    if (i == node_idx) continue;

    double dist = (tree_nodes_[i].position - node_pos).norm();
    if (dist < radius && dist > 1e-6) {
      neighbors.push_back(i);
    }
  }

  return neighbors;
}

bool AdvancedRoutingEngine::isCollisionFree(
    const Point3& start,
    const Point3& end,
    const std::vector<Obstacle>& obstacles) const {

  Vector3 direction = (end - start).normalized();
  double distance = (end - start).norm();

  int samples = static_cast<int>(
    std::ceil(distance / collision_margin_)
  );

  for (int i = 0; i <= samples; i++) {
    Point3 check_point = start +
      direction * (distance * i / samples);

    // Check against all obstacles
    for (const auto& obstacle : obstacles) {
      if (checkPointObstacleCollision(check_point, obstacle)) {
        return false;
      }
    }
  }

  return true;
}

bool AdvancedRoutingEngine::checkPointObstacleCollision(
    const Point3& point,
    const Obstacle& obstacle) const {

  // Simple sphere collision check
  Vector3 to_point = point - obstacle.position;
  double dist = to_point.norm();

  return dist < (obstacle.radius + collision_margin_);
}

Path AdvancedRoutingEngine::extractPath(
    const Point3& goal) {

  Path path;

  // Find goal node (or closest node)
  int goal_node_idx = findNearestNode(goal);

  // Trace back to root
  int current = goal_node_idx;
  while (current >= 0) {
    path.waypoints.insert(path.waypoints.begin(),
                         tree_nodes_[current].position);
    current = tree_nodes_[current].parent;
  }

  // Create segments
  for (size_t i = 1; i < path.waypoints.size(); i++) {
    Segment seg{
      .start = path.waypoints[i-1],
      .end = path.waypoints[i],
      .length = (path.waypoints[i] - path.waypoints[i-1]).norm()
    };
    path.segments.push_back(seg);
  }

  path.total_length = std::accumulate(
    path.segments.begin(), path.segments.end(), 0.0,
    [](double sum, const Segment& seg) { return sum + seg.length; }
  );

  return path;
}

void AdvancedRoutingEngine::insertFittings(
    Path& path,
    const RoutingConstraints& constraints) {

  path.fittings.resize(path.segments.size());

  for (size_t i = 0; i < path.segments.size(); i++) {
    if (i == 0) {
      path.fittings[i] = Fitting::STRAIGHT;
      continue;
    }

    Vector3 dir1 = path.segments[i-1].direction();
    Vector3 dir2 = path.segments[i].direction();

    double angle = std::acos(
      std::max(-1.0, std::min(1.0, dir1.dot(dir2)))
    );

    // Select fitting based on angle
    if (angle < 0.1) {  // < 6 degrees
      path.fittings[i] = Fitting::STRAIGHT;
    } else if (angle < M_PI/6) {  // < 30 degrees
      path.fittings[i] = Fitting::ELBOW_45;
    } else if (angle < M_PI/3) {  // < 60 degrees
      path.fittings[i] = Fitting::ELBOW_90;
    } else {
      path.fittings[i] = Fitting::TEE;
    }
  }
}

void AdvancedRoutingEngine::smoothPath(
    Path& path,
    const std::vector<Obstacle>& obstacles) {

  // Catmull-Rom smoothing iterations
  for (int iter = 0; iter < 5; iter++) {
    if (path.waypoints.size() < 3) break;

    std::vector<Point3> smoothed_points = path.waypoints;

    for (size_t i = 1; i < path.waypoints.size() - 1; i++) {
      Point3 p_prev = path.waypoints[i-1];
      Point3 p_curr = path.waypoints[i];
      Point3 p_next = path.waypoints[i+1];

      // Catmull-Rom interpolation
      Point3 new_pos = (p_prev + 2*p_curr + p_next) / 4.0;

      // Verify collision-free
      if (isCollisionFree(p_prev, new_pos, obstacles) &&
          isCollisionFree(new_pos, p_next, obstacles)) {
        smoothed_points[i] = new_pos;
      }
    }

    path.waypoints = smoothed_points;
  }

  // Recalculate segments after smoothing
  path.segments.clear();
  for (size_t i = 1; i < path.waypoints.size(); i++) {
    Segment seg{
      .start = path.waypoints[i-1],
      .end = path.waypoints[i],
      .length = (path.waypoints[i] - path.waypoints[i-1]).norm()
    };
    path.segments.push_back(seg);
  }

  path.total_length = std::accumulate(
    path.segments.begin(), path.segments.end(), 0.0,
    [](double sum, const Segment& seg) { return sum + seg.length; }
  );
}

double AdvancedRoutingEngine::estimateLength(
    const Path& path) const {

  return path.total_length;
}

}  // namespace bendcore::routing
