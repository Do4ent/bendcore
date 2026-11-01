#pragma once

#include <string>
#include <vector>
#include "bendcore/tube_service.hpp"

namespace bendcore::server {

/**
 * FEA Analysis Service
 * Performs finite element analysis on tube geometry
 */

// Request structure for FEA analysis
struct FEARequest {
  std::string mesh_data;           // JSON-encoded mesh (vertices/indices)
  double pressure;                 // Internal pressure (MPa)
  std::string material;            // Material: "steel", "aluminum", "titanium"
  std::vector<double> load_case;   // Additional load case (optional)
};

// Response with FEA results
Response fea_response(
  const std::string& mesh_json,
  double pressure,
  const std::string& material
);

}  // namespace bendcore::server
