#pragma once

#include <string>

namespace bendcore::server {

struct Response;

/**
 * Topology Optimization Service
 * Optimizes tube geometry for minimal weight while maintaining strength
 */

// Request structure for topology optimization
struct TopoOptRequest {
  double stress;           // Current max stress (MPa)
  double displacement;     // Current max displacement (mm)
  double od;              // Outer diameter (mm)
  double wall;            // Wall thickness (mm)
  double length;          // Tube length (mm)
  std::string material;   // Material type
  double target_weight_reduction;  // Target weight reduction (%)
};

// Response with optimized geometry
Response topology_optimize(
  double stress,
  double displacement,
  double od,
  double wall,
  double length,
  const std::string& material,
  double target_reduction
);

}  // namespace bendcore::server
