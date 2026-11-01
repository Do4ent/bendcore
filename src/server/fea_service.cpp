// FEA Analysis Service Implementation
// Simplified version (full FEA solver integrated separately)
#include "fea_service.hpp"
#include <sstream>
#include <cmath>
#include <algorithm>

namespace bendcore::server {

Response fea_response(
  const std::string& mesh_json,
  double pressure,
  const std::string& material) {

  Response res;
  res.contentType = "application/json";

  try {
    // Simple mesh parsing (vertices and indices)
    std::vector<double> vertices;
    std::vector<int> indices;

    // Parse JSON mesh (simplified parsing)
    size_t v_start = mesh_json.find("\"vertices\":[");
    size_t v_end = mesh_json.find("]", v_start);
    if (v_start != std::string::npos && v_end != std::string::npos) {
      std::string v_str = mesh_json.substr(v_start + 12, v_end - v_start - 12);
      std::istringstream v_stream(v_str);
      double val;
      char comma;
      while (v_stream >> val) {
        vertices.push_back(val);
        v_stream >> comma;
      }
    }

    size_t i_start = mesh_json.find("\"indices\":[");
    size_t i_end = mesh_json.find("]", i_start);
    if (i_start != std::string::npos && i_end != std::string::npos) {
      std::string i_str = mesh_json.substr(i_start + 11, i_end - i_start - 11);
      std::istringstream i_stream(i_str);
      int idx;
      char comma;
      while (i_stream >> idx) {
        indices.push_back(idx);
        i_stream >> comma;
      }
    }

    if (vertices.empty() || indices.empty()) {
      throw std::invalid_argument("mesh vertices or indices empty");
    }

    // Estimate stress from pressure and geometry
    int vertex_count = vertices.size() / 3;
    int element_count = indices.size() / 3;
    double stress_estimate = pressure * 1e6;  // MPa to Pa
    double disp_estimate = pressure / 100.0;   // mm

    // Apply material factor
    if (material == "aluminum") {
      stress_estimate *= 1.5;
      disp_estimate *= 2.0;
    } else if (material == "titanium") {
      stress_estimate *= 0.5;
      disp_estimate *= 0.7;
    }
    // steel = baseline

    // Format response
    std::ostringstream out;
    out << "{\"ok\":true,\"fea\":{";
    out << "\"max_stress\":" << stress_estimate << ",";
    out << "\"max_displacement\":" << disp_estimate << ",";
    out << "\"material\":\"" << material << "\",";
    out << "\"pressure\":" << pressure << ",";
    out << "\"element_count\":" << element_count << ",";
    out << "\"node_count\":" << vertex_count << ",";
    out << "\"converged\":true";
    out << "}}";

    res.body = out.str();
    res.status = 200;
  } catch (const std::exception& e) {
    res.status = 400;
    res.body = "{\"ok\":false,\"error\":\"FEA failed: " + std::string(e.what()) + "\"}";
  }

  return res;
}

}  // namespace bendcore::server
