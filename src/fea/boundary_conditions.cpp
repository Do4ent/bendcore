// FEA Boundary Conditions - Implementation
// M6 Phase: FEA Integration (GAP-025)
#include "bendcore/fea/boundary_conditions.hpp"
#include <algorithm>

namespace bendcore::fea {

BoundaryCondition::BoundaryCondition(BCType type, int32_t id) : type_(type), id_(id) {}

DisplacementBC::DisplacementBC(int32_t id, int32_t node_id, Direction dirs, const Vec3& displacement)
    : BoundaryCondition(BCType::Displacement, id), node_id_(node_id), directions_(dirs), displacement_(displacement) {}

DisplacementBC::DisplacementBC(int32_t id, int32_t node_id, Direction dirs, double value)
    : BoundaryCondition(BCType::Displacement, id), node_id_(node_id), directions_(dirs), displacement_{value, value, value} {}

ForceBC::ForceBC(int32_t id, int32_t node_id, const Vec3& force)
    : BoundaryCondition(BCType::Force, id), node_id_(node_id), force_(force) {}

PressureBC::PressureBC(int32_t id, const std::vector<int32_t>& element_ids, double pressure, const Vec3& direction)
    : BoundaryCondition(BCType::Pressure, id), element_ids_(element_ids), pressure_(pressure), direction_(direction) {}

SymmetryBC::SymmetryBC(int32_t id, const std::vector<int32_t>& node_ids, Direction symmetric_plane)
    : BoundaryCondition(BCType::Symmetry, id), node_ids_(node_ids), plane_(symmetric_plane) {}

TemperatureBC::TemperatureBC(int32_t id, int32_t node_id, double temperature)
    : BoundaryCondition(BCType::Temperature, id), node_id_(node_id), temperature_(temperature) {}

ConvectionBC::ConvectionBC(int32_t id, int32_t node_id, double heat_transfer_coefficient, double ambient_temperature)
    : BoundaryCondition(BCType::ConvectionHTC, id), node_id_(node_id), h_(heat_transfer_coefficient), T_ambient_(ambient_temperature) {}

ContactConstraint::ContactConstraint(int32_t id, int32_t node1, int32_t node2, double friction_coefficient)
    : id_(id), node1_(node1), node2_(node2), friction_coefficient_(friction_coefficient) {}

BoundaryConditionManager::BoundaryConditionManager() = default;
BoundaryConditionManager::~BoundaryConditionManager() = default;

void BoundaryConditionManager::add_displacement_bc(const DisplacementBC& bc) {
    displacement_bcs_.push_back(bc);
}

void BoundaryConditionManager::add_force_bc(const ForceBC& bc) {
    force_bcs_.push_back(bc);
}

void BoundaryConditionManager::add_pressure_bc(const PressureBC& bc) {
    pressure_bcs_.push_back(bc);
}

void BoundaryConditionManager::add_symmetry_bc(const SymmetryBC& bc) {
    symmetry_bcs_.push_back(bc);
}

void BoundaryConditionManager::add_temperature_bc(const TemperatureBC& bc) {
    temperature_bcs_.push_back(bc);
}

void BoundaryConditionManager::add_convection_bc(const ConvectionBC& bc) {
    convection_bcs_.push_back(bc);
}

void BoundaryConditionManager::add_contact_constraint(const ContactConstraint& constraint) {
    contact_constraints_.push_back(constraint);
}

std::vector<const DisplacementBC*> BoundaryConditionManager::get_displacement_bcs_for_node(int32_t node_id) const {
    std::vector<const DisplacementBC*> result;
    for (const auto& bc : displacement_bcs_) {
        if (bc.get_node_id() == node_id) {
            result.push_back(&bc);
        }
    }
    return result;
}

std::vector<const ForceBC*> BoundaryConditionManager::get_force_bcs_for_node(int32_t node_id) const {
    std::vector<const ForceBC*> result;
    for (const auto& bc : force_bcs_) {
        if (bc.get_node_id() == node_id) {
            result.push_back(&bc);
        }
    }
    return result;
}

std::vector<const TemperatureBC*> BoundaryConditionManager::get_temperature_bcs_for_node(int32_t node_id) const {
    std::vector<const TemperatureBC*> result;
    for (const auto& bc : temperature_bcs_) {
        if (bc.get_node_id() == node_id) {
            result.push_back(&bc);
        }
    }
    return result;
}

std::vector<Vec3> BoundaryConditionManager::get_external_forces(int32_t num_nodes) const {
    std::vector<Vec3> forces(num_nodes, Vec3{0.0, 0.0, 0.0});

    for (const auto& bc : force_bcs_) {
        int32_t node_id = bc.get_node_id();
        if (node_id >= 0 && node_id < num_nodes) {
            forces[node_id] = add(forces[node_id], bc.get_force());
        }
    }

    return forces;
}

std::map<int32_t, Vec3> BoundaryConditionManager::get_displacement_constraints() const {
    std::map<int32_t, Vec3> constraints;

    for (const auto& bc : displacement_bcs_) {
        constraints[bc.get_node_id()] = bc.get_displacement();
    }

    return constraints;
}

std::map<int32_t, Direction> BoundaryConditionManager::get_displacement_directions() const {
    std::map<int32_t, Direction> directions;

    for (const auto& bc : displacement_bcs_) {
        directions[bc.get_node_id()] = bc.get_directions();
    }

    return directions;
}

bool BoundaryConditionManager::validate(int32_t num_nodes, int32_t num_elements) const {
    for (const auto& bc : displacement_bcs_) {
        if (bc.get_node_id() < 0 || bc.get_node_id() >= num_nodes) {
            return false;
        }
    }

    for (const auto& bc : force_bcs_) {
        if (bc.get_node_id() < 0 || bc.get_node_id() >= num_nodes) {
            return false;
        }
    }

    for (const auto& bc : temperature_bcs_) {
        if (bc.get_node_id() < 0 || bc.get_node_id() >= num_nodes) {
            return false;
        }
    }

    for (const auto& bc : pressure_bcs_) {
        for (int32_t elem_id : bc.get_element_ids()) {
            if (elem_id < 0 || elem_id >= num_elements) {
                return false;
            }
        }
    }

    return true;
}

void BoundaryConditionManager::clear() {
    displacement_bcs_.clear();
    force_bcs_.clear();
    pressure_bcs_.clear();
    symmetry_bcs_.clear();
    temperature_bcs_.clear();
    convection_bcs_.clear();
    contact_constraints_.clear();
}

}  // namespace bendcore::fea
