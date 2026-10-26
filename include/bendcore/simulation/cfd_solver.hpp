#pragma once

#include <vector>
#include <array>
#include <memory>
#include <map>
#include <string>
#include "../geometry/vector.hpp"
#include "../document/entity_id.hpp"

namespace BendCore::CadCore::Simulation {

/**
 * @brief Turbulence model for flow simulation
 */
enum class TurbulenceModel {
    LAMINAR,           // No turbulence modeling
    SPALART_ALLMARAS,  // Spalart-Allmaras one-equation model
    K_EPSILON,         // Standard k-epsilon two-equation model
    K_OMEGA_SST,       // k-omega SST model
};

/**
 * @brief Boundary condition types
 */
enum class BoundaryConditionType {
    DIRICHLET,     // Fixed value
    NEUMANN,       // Fixed flux/gradient
    MIXED,         // Combination of Dirichlet and Neumann
    SLIP,          // Free slip wall
    NO_SLIP,       // No slip wall
    PERIODIC,      // Periodic boundary
};

/**
 * @brief Boundary condition for CFD
 */
struct BoundaryCondition {
    BoundaryConditionType type;
    std::string name;
    double velocity_x = 0.0;
    double velocity_y = 0.0;
    double velocity_z = 0.0;
    double pressure = 0.0;
    double temperature = 0.0;
    std::vector<size_t> face_indices;  // Indices of mesh faces
};

/**
 * @brief Mesh point (vertex)
 */
struct MeshPoint {
    Vector3D position;
    size_t id;
    std::vector<size_t> element_indices;
};

/**
 * @brief Mesh element (tetrahedron)
 */
struct MeshElement {
    size_t id;
    std::array<size_t, 4> vertex_indices;  // 4 vertices for tetrahedron
    double volume = 0.0;
    Vector3D centroid;
};

/**
 * @brief Mesh face for boundary conditions
 */
struct MeshFace {
    size_t id;
    std::array<size_t, 3> vertex_indices;  // 3 vertices for triangle
    Vector3D normal;
    double area = 0.0;
};

/**
 * @brief CFD mesh representation
 */
class CFDMesh {
public:
    CFDMesh() = default;
    ~CFDMesh() = default;

    void addPoint(const MeshPoint& point);
    void addElement(const MeshElement& element);
    void addFace(const MeshFace& face);

    [[nodiscard]] const std::vector<MeshPoint>& getPoints() const { return points_; }
    [[nodiscard]] const std::vector<MeshElement>& getElements() const { return elements_; }
    [[nodiscard]] const std::vector<MeshFace>& getFaces() const { return faces_; }

    [[nodiscard]] size_t getNumPoints() const { return points_.size(); }
    [[nodiscard]] size_t getNumElements() const { return elements_.size(); }
    [[nodiscard]] size_t getNumFaces() const { return faces_.size(); }

    void clear();
    [[nodiscard]] double getTotalVolume() const;

private:
    std::vector<MeshPoint> points_;
    std::vector<MeshElement> elements_;
    std::vector<MeshFace> faces_;
};

/**
 * @brief Flow field variables at a point
 */
struct FlowField {
    double velocity_x = 0.0;
    double velocity_y = 0.0;
    double velocity_z = 0.0;
    double pressure = 0.0;
    double temperature = 0.0;
    double turbulent_kinetic_energy = 0.0;
    double turbulent_dissipation = 0.0;
};

/**
 * @brief CFD Solver for incompressible flow
 */
class CFDSolver {
public:
    CFDSolver();
    ~CFDSolver() = default;

    // Configuration
    void setTurbulenceModel(TurbulenceModel model);
    void setFluidDensity(double density);
    void setFluidViscosity(double viscosity);
    void setNumberOfIterations(size_t iterations);
    void setRelaxationFactor(double factor);

    // Mesh and boundary conditions
    void setMesh(const std::shared_ptr<CFDMesh>& mesh);
    void addBoundaryCondition(const BoundaryCondition& bc);

    // Solver execution
    bool solve();

    // Results retrieval
    [[nodiscard]] const std::vector<FlowField>& getVelocityField() const { return velocity_field_; }
    [[nodiscard]] const std::vector<double>& getPressureField() const { return pressure_field_; }
    [[nodiscard]] const std::vector<double>& getTemperatureField() const { return temperature_field_; }
    [[nodiscard]] bool isConverged() const { return converged_; }
    [[nodiscard]] double getResidual() const { return residual_; }
    [[nodiscard]] size_t getIterationCount() const { return iteration_count_; }

    // Field statistics
    [[nodiscard]] double getMaxVelocity() const;
    [[nodiscard]] double getMaxPressure() const;
    [[nodiscard]] double getMaxTemperature() const;
    [[nodiscard]] double getMinTemperature() const;

private:
    // Parameters
    TurbulenceModel turbulence_model_ = TurbulenceModel::K_EPSILON;
    double fluid_density_ = 1.225;        // kg/m^3 (air at sea level)
    double fluid_viscosity_ = 1.81e-5;    // Pa·s (air at 15°C)
    size_t max_iterations_ = 1000;
    double relaxation_factor_ = 0.7;
    double convergence_tolerance_ = 1e-6;

    // Mesh and boundary conditions
    std::shared_ptr<CFDMesh> mesh_;
    std::vector<BoundaryCondition> boundary_conditions_;

    // Solution fields
    std::vector<FlowField> velocity_field_;
    std::vector<double> pressure_field_;
    std::vector<double> temperature_field_;
    std::vector<double> turbulent_ke_field_;
    std::vector<double> turbulent_diss_field_;

    // Convergence tracking
    bool converged_ = false;
    double residual_ = 1.0;
    size_t iteration_count_ = 0;

    // Internal solver methods
    void initializeFields();
    void computeMomentumEquation();
    void computePressureCorrection();
    void correctVelocity();
    void computeTurbulenceFields();
    void computeTemperatureField();
    [[nodiscard]] double computeResidual();
};

}  // namespace BendCore::CadCore::Simulation
