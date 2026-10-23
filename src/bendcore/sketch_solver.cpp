/**
 * sketch_solver.cpp - Phase 35B
 * Newton-Raphson constraint solver implementation
 */

#include "sketch_solver.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace bendcore {

SketchSolver::SketchSolver()
    : sketch_id_(""),
      profile_r_(10),
      profile_s_min_(2),
      profile_b_min_(5),
      profile_b_max_(180) {}

void SketchSolver::initialize(
    const std::string& sketch_id,
    const std::vector<Vertex>& vertices,
    const std::vector<Segment>& segments,
    const std::vector<ConstraintDef>& constraints,
    double profile_r,
    double profile_s_min,
    double profile_b_min,
    double profile_b_max) {
  sketch_id_ = sketch_id;
  vertices_ = vertices;
  initial_vertices_ = vertices;
  segments_ = segments;
  constraints_ = constraints;

  profile_r_ = profile_r;
  profile_s_min_ = profile_s_min;
  profile_b_min_ = profile_b_min;
  profile_b_max_ = profile_b_max;

  // Build vertex index for fast lookup
  vertex_index_.clear();
  for (size_t i = 0; i < vertices_.size(); ++i) {
    vertex_index_[vertices_[i].id] = i;
  }
}

SketchSolver::SolveResult SketchSolver::solve(int max_iterations,
                                              double tolerance) {
  SolveResult result;
  result.converged = false;
  result.iterations = 0;

  // Handle empty sketch
  if (vertices_.empty() || constraints_.empty()) {
    result.converged = true;
    result.residual = 0.0;
    return result;
  }

  // Newton-Raphson iteration
  for (int iter = 0; iter < max_iterations; ++iter) {
    // Compute residual
    double residual = compute_residual();
    result.residual = residual;
    result.iterations = iter + 1;

    // Check convergence
    if (residual < tolerance) {
      result.converged = true;
      break;
    }

    // Compute gradient
    auto gradient = compute_gradient();

    // Simple gradient descent step
    double step_size = 0.01;
    bool improved = false;

    // Update vertex positions along gradient
    for (size_t i = 0; i < vertices_.size(); ++i) {
      if (vertices_[i].is_fixed) continue;

      if (i * 3 + 2 < gradient.size()) {
        double dx = gradient[i * 3];
        double dy = gradient[i * 3 + 1];
        double dz = gradient[i * 3 + 2];

        double norm = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (norm > 1e-10) {
          vertices_[i].position.x -= step_size * (dx / norm) * 0.1;
          vertices_[i].position.y -= step_size * (dy / norm) * 0.1;
          vertices_[i].position.z -= step_size * (dz / norm) * 0.1;
          improved = true;
        }
      }
    }

    if (!improved) {
      result.converged = true;
      break;
    }

    // Reduce step size over iterations for better convergence
    step_size *= 0.95;
  }

  // Update constraint satisfaction status
  for (auto& constraint : constraints_) {
    constraint.error = std::abs(evaluate_constraint(constraint));
    constraint.is_satisfied = constraint.error < 0.01;
  }

  // Detect conflicts
  result.conflicts = detect_conflicts();
  result.over_constrained = detect_over_constrained();

  return result;
}

double SketchSolver::evaluate_constraint(const ConstraintDef& constraint) {
  switch (constraint.type) {
    case ConstraintType::COINCIDENT:
      if (constraint.entity_ids.size() >= 2) {
        return constraint_coincident(constraint.entity_ids[0],
                                     constraint.entity_ids[1]);
      }
      break;

    case ConstraintType::PARALLEL:
      if (constraint.entity_ids.size() >= 2) {
        return constraint_parallel(constraint.entity_ids[0],
                                   constraint.entity_ids[1]);
      }
      break;

    case ConstraintType::PERPENDICULAR:
      if (constraint.entity_ids.size() >= 2) {
        return constraint_perpendicular(constraint.entity_ids[0],
                                        constraint.entity_ids[1]);
      }
      break;

    case ConstraintType::EQUALITY:
      if (constraint.entity_ids.size() >= 2) {
        return constraint_equality(constraint.entity_ids[0],
                                   constraint.entity_ids[1]);
      }
      break;

    case ConstraintType::FIXED:
      if (constraint.entity_ids.size() >= 1) {
        return constraint_fixed(constraint.entity_ids[0]);
      }
      break;

    case ConstraintType::DISTANCE:
      if (constraint.entity_ids.size() >= 2) {
        return constraint_distance(constraint.entity_ids[0],
                                   constraint.entity_ids[1], constraint.value);
      }
      break;

    case ConstraintType::ANGLE:
      if (constraint.entity_ids.size() >= 2) {
        return constraint_angle(constraint.entity_ids[0], constraint.entity_ids[1],
                                constraint.value);
      }
      break;
  }

  return 0.0;
}

std::vector<double> SketchSolver::compute_jacobian(size_t constraint_idx) {
  std::vector<double> jacobian(vertices_.size() * 3, 0.0);

  if (constraint_idx >= constraints_.size()) return jacobian;

  const auto& constraint = constraints_[constraint_idx];
  double base_error = evaluate_constraint(constraint);

  // Numerical differentiation
  for (size_t i = 0; i < vertices_.size(); ++i) {
    if (vertices_[i].is_fixed) continue;

    Vector3 original = vertices_[i].position;

    // Partial derivative wrt x
    vertices_[i].position.x += JACOBIAN_DELTA;
    double error_x = evaluate_constraint(constraint);
    jacobian[i * 3] = (error_x - base_error) / JACOBIAN_DELTA;
    vertices_[i].position.x = original.x;

    // Partial derivative wrt y
    vertices_[i].position.y += JACOBIAN_DELTA;
    double error_y = evaluate_constraint(constraint);
    jacobian[i * 3 + 1] = (error_y - base_error) / JACOBIAN_DELTA;
    vertices_[i].position.y = original.y;

    // Partial derivative wrt z
    vertices_[i].position.z += JACOBIAN_DELTA;
    double error_z = evaluate_constraint(constraint);
    jacobian[i * 3 + 2] = (error_z - base_error) / JACOBIAN_DELTA;
    vertices_[i].position.z = original.z;
  }

  return jacobian;
}

// ===== CONSTRAINT IMPLEMENTATIONS =====

double SketchSolver::constraint_coincident(const std::string& v1_id,
                                           const std::string& v2_id) {
  auto it1 = vertex_index_.find(v1_id);
  auto it2 = vertex_index_.find(v2_id);

  if (it1 == vertex_index_.end() || it2 == vertex_index_.end()) return 0.0;

  const Vector3& p1 = vertices_[it1->second].position;
  const Vector3& p2 = vertices_[it2->second].position;

  return (p1 - p2).length();
}

double SketchSolver::constraint_parallel(const std::string& s1_id,
                                         const std::string& s2_id) {
  Vector3 d1 = get_segment_direction(s1_id);
  Vector3 d2 = get_segment_direction(s2_id);

  d1 = d1.normalize();
  d2 = d2.normalize();

  // Parallel if cross product is near zero
  Vector3 cross = d1.cross(d2);
  return cross.length();
}

double SketchSolver::constraint_perpendicular(const std::string& s1_id,
                                              const std::string& s2_id) {
  Vector3 d1 = get_segment_direction(s1_id);
  Vector3 d2 = get_segment_direction(s2_id);

  d1 = d1.normalize();
  d2 = d2.normalize();

  // Perpendicular if dot product is near zero
  return std::abs(d1.dot(d2));
}

double SketchSolver::constraint_equality(const std::string& e1_id,
                                         const std::string& e2_id) {
  // For now, treat as equal lengths
  double len1 = get_segment_length(e1_id);
  double len2 = get_segment_length(e2_id);

  return std::abs(len1 - len2);
}

double SketchSolver::constraint_fixed(const std::string& v_id) {
  auto it = vertex_index_.find(v_id);
  if (it == vertex_index_.end()) return 0.0;

  const Vertex& v = vertices_[it->second];
  const Vertex& v_init = initial_vertices_[it->second];

  return (v.position - v_init.position).length();
}

double SketchSolver::constraint_distance(const std::string& v1_id,
                                         const std::string& v2_id,
                                         double target) {
  auto it1 = vertex_index_.find(v1_id);
  auto it2 = vertex_index_.find(v2_id);

  if (it1 == vertex_index_.end() || it2 == vertex_index_.end()) return 0.0;

  const Vector3& p1 = vertices_[it1->second].position;
  const Vector3& p2 = vertices_[it2->second].position;

  double actual = p1.distance(p2);
  return actual - target;
}

double SketchSolver::constraint_angle(const std::string& s1_id,
                                      const std::string& s2_id,
                                      double target_angle) {
  double actual = get_angle_between_segments(s1_id, s2_id);
  double target_rad = target_angle * M_PI / 180.0;

  // Handle angle wrapping
  double diff = actual - target_rad;
  while (diff > M_PI) diff -= 2 * M_PI;
  while (diff < -M_PI) diff += 2 * M_PI;

  return diff;
}

Vector3 SketchSolver::get_segment_direction(const std::string& segment_id) {
  for (const auto& seg : segments_) {
    if (seg.id == segment_id) {
      auto it1 = vertex_index_.find(seg.from_vertex);
      auto it2 = vertex_index_.find(seg.to_vertex);

      if (it1 != vertex_index_.end() && it2 != vertex_index_.end()) {
        const Vector3& p1 = vertices_[it1->second].position;
        const Vector3& p2 = vertices_[it2->second].position;
        return (p2 - p1).normalize();
      }
    }
  }

  return Vector3(0, 0, 0);
}

double SketchSolver::get_segment_length(const std::string& segment_id) {
  for (const auto& seg : segments_) {
    if (seg.id == segment_id) {
      auto it1 = vertex_index_.find(seg.from_vertex);
      auto it2 = vertex_index_.find(seg.to_vertex);

      if (it1 != vertex_index_.end() && it2 != vertex_index_.end()) {
        const Vector3& p1 = vertices_[it1->second].position;
        const Vector3& p2 = vertices_[it2->second].position;
        return p1.distance(p2);
      }
    }
  }

  return 0.0;
}

double SketchSolver::get_angle_between_segments(const std::string& s1_id,
                                                const std::string& s2_id) {
  Vector3 d1 = get_segment_direction(s1_id);
  Vector3 d2 = get_segment_direction(s2_id);

  d1 = d1.normalize();
  d2 = d2.normalize();

  double dot = d1.dot(d2);
  dot = std::max(-1.0, std::min(1.0, dot));

  return std::acos(dot);
}

double SketchSolver::compute_residual() {
  double residual = 0.0;

  for (const auto& constraint : constraints_) {
    double error = evaluate_constraint(constraint);
    residual += error * error;
  }

  return std::sqrt(residual);
}

std::vector<double> SketchSolver::compute_gradient() {
  std::vector<double> gradient(vertices_.size() * 3, 0.0);

  for (size_t i = 0; i < constraints_.size(); ++i) {
    auto jac = compute_jacobian(i);
    double error = evaluate_constraint(constraints_[i]);

    for (size_t j = 0; j < jac.size(); ++j) {
      gradient[j] += jac[j] * error;
    }
  }

  return gradient;
}

std::vector<std::string> SketchSolver::detect_conflicts() {
  std::vector<std::string> conflicts;

  for (const auto& constraint : constraints_) {
    if (!constraint.is_satisfied && constraint.error > 0.1) {
      conflicts.push_back(constraint.id);
    }
  }

  return conflicts;
}

std::vector<std::string> SketchSolver::detect_over_constrained() {
  // Simple heuristic: if solver doesn't converge, sketch might be over-constrained
  std::vector<std::string> over_constrained;

  int unsatisfied_count = 0;
  for (const auto& constraint : constraints_) {
    if (!constraint.is_satisfied) {
      unsatisfied_count++;
      over_constrained.push_back(constraint.id);
    }
  }

  return over_constrained;
}

void SketchSolver::reset() {
  vertices_ = initial_vertices_;
  vertex_index_.clear();
  for (size_t i = 0; i < vertices_.size(); ++i) {
    vertex_index_[vertices_[i].id] = i;
  }
}

json SketchSolver::to_json() const {
  json vertices_json = json::array();
  for (const auto& v : vertices_) {
    vertices_json.push_back({
        {"id", v.id},
        {"position", {{"x", v.position.x}, {"y", v.position.y}, {"z", v.position.z}}},
        {"is_fixed", v.is_fixed},
    });
  }

  json constraints_json = json::array();
  for (const auto& c : constraints_) {
    std::string type_str;
    switch (c.type) {
      case ConstraintType::COINCIDENT:
        type_str = "COINCIDENT";
        break;
      case ConstraintType::PARALLEL:
        type_str = "PARALLEL";
        break;
      case ConstraintType::PERPENDICULAR:
        type_str = "PERPENDICULAR";
        break;
      case ConstraintType::EQUALITY:
        type_str = "EQUALITY";
        break;
      case ConstraintType::FIXED:
        type_str = "FIXED";
        break;
      case ConstraintType::DISTANCE:
        type_str = "DISTANCE";
        break;
      case ConstraintType::ANGLE:
        type_str = "ANGLE";
        break;
    }

    constraints_json.push_back({
        {"id", c.id},
        {"type", type_str},
        {"is_satisfied", c.is_satisfied},
        {"error", c.error},
    });
  }

  return json{
      {"sketch_id", sketch_id_},
      {"vertices", vertices_json},
      {"constraints", constraints_json},
  };
}

}  // namespace bendcore
