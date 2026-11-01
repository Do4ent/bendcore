// Topology Optimization Service Implementation
#include "topology_service.hpp"
#include "bendcore/tube_service.hpp"
#include <sstream>
#include <cmath>
#include <algorithm>

namespace bendcore::server {

Response topology_optimize(
  double stress,
  double displacement,
  double od,
  double wall,
  double length,
  const std::string& material,
  double target_reduction) {

  Response res;
  res.contentType = "application/json";
  res.status = 200;

  try {
    // Topology optimization: reduce wall thickness based on stress
    // Safety factor depends on material
    double safety_factor = 2.5;  // Default for steel
    if (material == "aluminum") {
      safety_factor = 3.0;  // Higher for aluminum
    } else if (material == "titanium") {
      safety_factor = 2.0;  // Lower for titanium
    }

    // Calculate allowable stress
    double allowable_stress = 400.0;  // MPa (typical for steel)
    if (material == "aluminum") {
      allowable_stress = 150.0;
    } else if (material == "titanium") {
      allowable_stress = 600.0;
    }

    // Stress utilization ratio
    double utilization = stress / (allowable_stress / safety_factor);

    // Optimize wall thickness
    double optimal_wall = wall;
    double weight_reduction = 0.0;
    std::string optimization_result = "No optimization needed";

    if (utilization < 0.7) {
      // Stress is low, can reduce wall thickness
      optimal_wall = wall * std::sqrt(utilization);
      weight_reduction = (1.0 - (optimal_wall / wall)) * 100.0;
      optimization_result = "Wall thickness reduced";
    } else if (utilization > 1.0) {
      // Stress is high, need thicker wall
      optimal_wall = wall * std::sqrt(utilization);
      weight_reduction = -1.0 * (optimal_wall / wall - 1.0) * 100.0;
      optimization_result = "Wall thickness increased for safety";
    } else {
      optimization_result = "Current design is near optimal";
    }

    // Ensure minimum wall thickness
    optimal_wall = std::max(optimal_wall, od * 0.05);  // Min 5% of OD

    // Estimate new weight (simplified)
    double current_weight = 3.14159 * od * wall * length * 0.001;  // kg (approximate)
    double optimal_weight = 3.14159 * od * optimal_wall * length * 0.001;
    double actual_reduction = ((current_weight - optimal_weight) / current_weight) * 100.0;

    // Format response
    std::ostringstream out;
    out << "{\"ok\":true,\"topology\":{";
    out << "\"original_wall\":" << wall << ",";
    out << "\"optimal_wall\":" << optimal_wall << ",";
    out << "\"wall_reduction_percent\":" << (100.0 * (wall - optimal_wall) / wall) << ",";
    out << "\"stress_utilization\":" << utilization << ",";
    out << "\"weight_reduction_percent\":" << actual_reduction << ",";
    out << "\"original_weight_kg\":" << current_weight << ",";
    out << "\"optimal_weight_kg\":" << optimal_weight << ",";
    out << "\"safety_factor\":" << safety_factor << ",";
    out << "\"recommendation\":\"" << optimization_result << "\",";
    out << "\"feasible\":" << (optimal_wall >= od * 0.05 ? "true" : "false");
    out << "}}";

    res.body = out.str();
  } catch (const std::exception& e) {
    res.status = 400;
    res.body = "{\"ok\":false,\"error\":\"Optimization failed: " + std::string(e.what()) + "\"}";
  }

  return res;
}

}  // namespace bendcore::server
