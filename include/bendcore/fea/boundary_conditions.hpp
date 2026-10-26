// FEA Boundary Conditions Manager
// M6 Phase: FEA Integration (GAP-025)
#pragma once

#include "bendcore/vec3.hpp"
#include <vector>
#include <map>
#include <cstdint>
#include <memory>

namespace bendcore::fea {

// Boundary condition types
enum class BCType {
    Fixed,          // Fixed displacement (value = 0)
    Displacement,   // Prescribed displacement
    Symmetry,       // Symmetry condition
    Force,          // Point force
    Pressure,       // Distributed pressure
    Temperature,    // Thermal boundary condition
    ConvectionHTC   // Convection heat transfer coefficient
};

// Direction flags for boundary conditions
enum class Direction : uint8_t {
    X = 0x01,
    Y = 0x02,
    Z = 0x04,
    XY = X | Y,
    YZ = Y | Z,
    XZ = X | Z,
    XYZ = X | Y | Z
};

// Base class for boundary conditions
class BoundaryCondition {
public:
    BoundaryCondition(BCType type, int32_t id);
    virtual ~BoundaryCondition() = default;

    BCType get_type() const { return type_; }
    int32_t get_id() const { return id_; }

protected:
    BCType type_;
    int32_t id_;
};

// Displacement boundary condition
class DisplacementBC : public BoundaryCondition {
public:
    DisplacementBC(int32_t id, int32_t node_id, Direction dirs, const Vec3& displacement);
    DisplacementBC(int32_t id, int32_t node_id, Direction dirs, double value);

    int32_t get_node_id() const { return node_id_; }
    Direction get_directions() const { return directions_; }
    const Vec3& get_displacement() const { return displacement_; }

    bool applies_to_x() const { return (static_cast<uint8_t>(directions_) & static_cast<uint8_t>(Direction::X)) != 0; }
    bool applies_to_y() const { return (static_cast<uint8_t>(directions_) & static_cast<uint8_t>(Direction::Y)) != 0; }
    bool applies_to_z() const { return (static_cast<uint8_t>(directions_) & static_cast<uint8_t>(Direction::Z)) != 0; }

private:
    int32_t node_id_;
    Direction directions_;
    Vec3 displacement_;
};

// Force boundary condition
class ForceBC : public BoundaryCondition {
public:
    ForceBC(int32_t id, int32_t node_id, const Vec3& force);

    int32_t get_node_id() const { return node_id_; }
    const Vec3& get_force() const { return force_; }

private:
    int32_t node_id_;
    Vec3 force_;
};

// Pressure boundary condition
class PressureBC : public BoundaryCondition {
public:
    PressureBC(int32_t id, const std::vector<int32_t>& element_ids, double pressure, const Vec3& direction);

    const std::vector<int32_t>& get_element_ids() const { return element_ids_; }
    double get_pressure() const { return pressure_; }
    const Vec3& get_direction() const { return direction_; }

private:
    std::vector<int32_t> element_ids_;
    double pressure_;
    Vec3 direction_;
};

// Symmetry boundary condition
class SymmetryBC : public BoundaryCondition {
public:
    SymmetryBC(int32_t id, const std::vector<int32_t>& node_ids, Direction symmetric_plane);

    const std::vector<int32_t>& get_node_ids() const { return node_ids_; }
    Direction get_symmetric_plane() const { return plane_; }

private:
    std::vector<int32_t> node_ids_;
    Direction plane_;
};

// Temperature boundary condition
class TemperatureBC : public BoundaryCondition {
public:
    TemperatureBC(int32_t id, int32_t node_id, double temperature);

    int32_t get_node_id() const { return node_id_; }
    double get_temperature() const { return temperature_; }

private:
    int32_t node_id_;
    double temperature_;
};

// Convection boundary condition
class ConvectionBC : public BoundaryCondition {
public:
    ConvectionBC(int32_t id, int32_t node_id, double heat_transfer_coefficient, double ambient_temperature);

    int32_t get_node_id() const { return node_id_; }
    double get_heat_transfer_coefficient() const { return h_; }
    double get_ambient_temperature() const { return T_ambient_; }

private:
    int32_t node_id_;
    double h_;
    double T_ambient_;
};

// Contact constraint
class ContactConstraint {
public:
    ContactConstraint(int32_t id, int32_t node1, int32_t node2, double friction_coefficient = 0.0);

    int32_t get_id() const { return id_; }
    int32_t get_node1() const { return node1_; }
    int32_t get_node2() const { return node2_; }
    double get_friction_coefficient() const { return friction_coefficient_; }

private:
    int32_t id_;
    int32_t node1_;
    int32_t node2_;
    double friction_coefficient_;
};

// Boundary condition manager
class BoundaryConditionManager {
public:
    BoundaryConditionManager();
    ~BoundaryConditionManager();

    // Add boundary conditions
    void add_displacement_bc(const DisplacementBC& bc);
    void add_force_bc(const ForceBC& bc);
    void add_pressure_bc(const PressureBC& bc);
    void add_symmetry_bc(const SymmetryBC& bc);
    void add_temperature_bc(const TemperatureBC& bc);
    void add_convection_bc(const ConvectionBC& bc);

    // Add contact constraints
    void add_contact_constraint(const ContactConstraint& constraint);

    // Get boundary conditions
    const std::vector<DisplacementBC>& get_displacement_bcs() const { return displacement_bcs_; }
    const std::vector<ForceBC>& get_force_bcs() const { return force_bcs_; }
    const std::vector<PressureBC>& get_pressure_bcs() const { return pressure_bcs_; }
    const std::vector<SymmetryBC>& get_symmetry_bcs() const { return symmetry_bcs_; }
    const std::vector<TemperatureBC>& get_temperature_bcs() const { return temperature_bcs_; }
    const std::vector<ConvectionBC>& get_convection_bcs() const { return convection_bcs_; }
    const std::vector<ContactConstraint>& get_contact_constraints() const { return contact_constraints_; }

    // Get BCs for specific node
    std::vector<const DisplacementBC*> get_displacement_bcs_for_node(int32_t node_id) const;
    std::vector<const ForceBC*> get_force_bcs_for_node(int32_t node_id) const;
    std::vector<const TemperatureBC*> get_temperature_bcs_for_node(int32_t node_id) const;

    // Get total external force vector
    std::vector<Vec3> get_external_forces(int32_t num_nodes) const;

    // Get displacement constraints
    std::map<int32_t, Vec3> get_displacement_constraints() const;
    std::map<int32_t, Direction> get_displacement_directions() const;

    // Validation
    bool validate(int32_t num_nodes, int32_t num_elements) const;

    // Clear
    void clear();

private:
    std::vector<DisplacementBC> displacement_bcs_;
    std::vector<ForceBC> force_bcs_;
    std::vector<PressureBC> pressure_bcs_;
    std::vector<SymmetryBC> symmetry_bcs_;
    std::vector<TemperatureBC> temperature_bcs_;
    std::vector<ConvectionBC> convection_bcs_;
    std::vector<ContactConstraint> contact_constraints_;
};

}  // namespace bendcore::fea
