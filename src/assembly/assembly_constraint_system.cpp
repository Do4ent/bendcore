#include <nlohmann/json.hpp>
#include "../../include/bendcore/assembly/assembly_constraint_system.hpp"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <numeric>

namespace BendCore::CadCore {

// ==================== Constraint Management ====================

EntityId AssemblyConstraintSystem::generateConstraintId() noexcept {
    next_constraint_id_++;
    return EntityId::generate();
}

std::shared_ptr<AssemblyConstraint> AssemblyConstraintSystem::addFixedConstraint(
    const EntityId& component_id, const Vector3D& position, const Vector3D& orientation) {
    auto constraint = std::make_shared<FixedConstraint>(generateConstraintId(), component_id,
                                                         position, orientation);
    addConstraint(constraint);
    return constraint;
}

std::shared_ptr<AssemblyConstraint> AssemblyConstraintSystem::addCoincidentConstraint(
    const EntityId& entity1_id, const EntityId& entity2_id, size_t geometry1_index,
    size_t geometry2_index) {
    auto constraint = std::make_shared<CoincidentConstraint>(
        generateConstraintId(), entity1_id, entity2_id, geometry1_index, geometry2_index);
    addConstraint(constraint);
    return constraint;
}

std::shared_ptr<AssemblyConstraint> AssemblyConstraintSystem::addDistanceConstraint(
    const EntityId& entity1_id, const EntityId& entity2_id, double distance,
    size_t geometry1_index, size_t geometry2_index) {
    auto constraint = std::make_shared<DistanceConstraint>(
        generateConstraintId(), entity1_id, entity2_id, distance, geometry1_index,
        geometry2_index);
    addConstraint(constraint);
    return constraint;
}

std::shared_ptr<AssemblyConstraint> AssemblyConstraintSystem::addParallelConstraint(
    const EntityId& entity1_id, const EntityId& entity2_id, size_t geometry1_index,
    size_t geometry2_index) {
    auto constraint = std::make_shared<ParallelConstraint>(
        generateConstraintId(), entity1_id, entity2_id, geometry1_index, geometry2_index);
    addConstraint(constraint);
    return constraint;
}

std::shared_ptr<AssemblyConstraint> AssemblyConstraintSystem::addPerpendicularConstraint(
    const EntityId& entity1_id, const EntityId& entity2_id, size_t geometry1_index,
    size_t geometry2_index) {
    auto constraint = std::make_shared<PerpendicularConstraint>(
        generateConstraintId(), entity1_id, entity2_id, geometry1_index, geometry2_index);
    addConstraint(constraint);
    return constraint;
}

std::shared_ptr<AssemblyConstraint> AssemblyConstraintSystem::addTangentConstraint(
    const EntityId& entity1_id, const EntityId& entity2_id, size_t geometry1_index,
    size_t geometry2_index) {
    auto constraint = std::make_shared<TangentConstraint>(
        generateConstraintId(), entity1_id, entity2_id, geometry1_index, geometry2_index);
    addConstraint(constraint);
    return constraint;
}

std::shared_ptr<AssemblyConstraint> AssemblyConstraintSystem::addSymmetricConstraint(
    const EntityId& entity1_id, const EntityId& entity2_id, const EntityId& symmetry_plane_id,
    size_t geometry1_index, size_t geometry2_index) {
    auto constraint = std::make_shared<SymmetricConstraint>(
        generateConstraintId(), entity1_id, entity2_id, symmetry_plane_id, geometry1_index,
        geometry2_index);
    addConstraint(constraint);
    return constraint;
}

std::shared_ptr<AssemblyConstraint> AssemblyConstraintSystem::addAngleConstraint(
    const EntityId& entity1_id, const EntityId& entity2_id, double angle,
    size_t geometry1_index, size_t geometry2_index) {
    auto constraint = std::make_shared<AngleConstraint>(
        generateConstraintId(), entity1_id, entity2_id, angle, geometry1_index,
        geometry2_index);
    addConstraint(constraint);
    return constraint;
}

void AssemblyConstraintSystem::addConstraint(std::shared_ptr<AssemblyConstraint> constraint) {
    if (!constraint) {
        throw std::invalid_argument("Cannot add null constraint");
    }

    // Validate constraint
    if (!constraint->validate()) {
        throw std::invalid_argument("Constraint validation failed");
    }

    constraints_.push_back(constraint);
    constraint_map_[constraint->getId()] = constraint;
}

bool AssemblyConstraintSystem::removeConstraint(const EntityId& constraint_id) noexcept {
    auto it = constraint_map_.find(constraint_id);
    if (it != constraint_map_.end()) {
        constraint_map_.erase(it);
        constraints_.erase(
            std::remove_if(constraints_.begin(), constraints_.end(),
                           [&constraint_id](const std::shared_ptr<AssemblyConstraint>& c) {
                               return c->getId() == constraint_id;
                           }),
            constraints_.end());
        return true;
    }
    return false;
}

std::shared_ptr<AssemblyConstraint> AssemblyConstraintSystem::getConstraint(
    const EntityId& constraint_id) const noexcept {
    auto it = constraint_map_.find(constraint_id);
    if (it != constraint_map_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<AssemblyConstraint>>
AssemblyConstraintSystem::getConstraintsForEntity(const EntityId& entity_id) const noexcept {
    std::vector<std::shared_ptr<AssemblyConstraint>> result;
    for (const auto& constraint : constraints_) {
        auto entities = constraint->getInvolvedEntities();
        if (std::find(entities.begin(), entities.end(), entity_id) != entities.end()) {
            result.push_back(constraint);
        }
    }
    return result;
}

std::vector<std::shared_ptr<AssemblyConstraint>>
AssemblyConstraintSystem::getConstraintsByType(AssemblyConstraint::ConstraintType type) const noexcept {
    std::vector<std::shared_ptr<AssemblyConstraint>> result;
    for (const auto& constraint : constraints_) {
        if (constraint->getType() == type) {
            result.push_back(constraint);
        }
    }
    return result;
}

// ==================== Constraint Validation ====================

ConstraintValidationResult AssemblyConstraintSystem::validateAll() const noexcept {
    ConstraintValidationResult result;
    result.is_valid = true;

    for (const auto& constraint : constraints_) {
        try {
            constraint->validate();
        } catch (const std::exception& e) {
            result.is_valid = false;
            result.error_messages.push_back(std::string("Constraint ") +
                                            constraint->getId().toString() + ": " + e.what());
        }
    }

    return result;
}

ConstraintValidationResult AssemblyConstraintSystem::validateConstraint(
    const EntityId& constraint_id) const noexcept {
    ConstraintValidationResult result;
    auto constraint = getConstraint(constraint_id);
    if (!constraint) {
        result.is_valid = false;
        result.error_messages.push_back("Constraint not found");
        return result;
    }

    try {
        result.is_valid = constraint->validate();
    } catch (const std::exception& e) {
        result.is_valid = false;
        result.error_messages.push_back(e.what());
    }

    return result;
}

std::vector<EntityId> AssemblyConstraintSystem::detectConflicts() const noexcept {
    std::vector<EntityId> conflicts;

    // Check for perpendicular + parallel conflicts
    auto parallel_constraints = getConstraintsByType(AssemblyConstraint::ConstraintType::PARALLEL);
    auto perpendicular_constraints =
        getConstraintsByType(AssemblyConstraint::ConstraintType::PERPENDICULAR);

    for (const auto& p_constraint : parallel_constraints) {
        auto p_entities = p_constraint->getInvolvedEntities();
        for (const auto& pp_constraint : perpendicular_constraints) {
            auto pp_entities = pp_constraint->getInvolvedEntities();
            // If they involve the same entities, they conflict
            if ((p_entities[0] == pp_entities[0] && p_entities[1] == pp_entities[1]) ||
                (p_entities[0] == pp_entities[1] && p_entities[1] == pp_entities[0])) {
                conflicts.push_back(p_constraint->getId());
                conflicts.push_back(pp_constraint->getId());
            }
        }
    }

    // Check for fixed + distance conflicts
    auto fixed_constraints = getConstraintsByType(AssemblyConstraint::ConstraintType::FIXED);
    auto distance_constraints = getConstraintsByType(AssemblyConstraint::ConstraintType::DISTANCE);

    for (const auto& fixed : fixed_constraints) {
        if (auto fixed_cast = std::dynamic_pointer_cast<FixedConstraint>(fixed)) {
            for (const auto& distance : distance_constraints) {
                if (auto dist_cast = std::dynamic_pointer_cast<DistanceConstraint>(distance)) {
                    auto dist_entities = dist_cast->getInvolvedEntities();
                    if (std::find(dist_entities.begin(), dist_entities.end(),
                                  fixed_cast->getComponentId()) != dist_entities.end()) {
                        // Both constraints involve same component
                        conflicts.push_back(fixed->getId());
                        conflicts.push_back(distance->getId());
                    }
                }
            }
        }
    }

    return conflicts;
}

// ==================== Constraint Solver ====================

bool AssemblyConstraintSystem::solveConstraints(size_t max_iterations,
                                               double tolerance) noexcept {
    initializePositions();

    for (size_t iter = 0; iter < max_iterations; ++iter) {
        double total_error = 0.0;

        // Process each constraint
        for (auto& constraint : constraints_) {
            if (!constraint->isEnabled()) {
                continue;
            }

            try {
                constraint->validate();
                constraint->setStatus(AssemblyConstraint::ConstraintStatus::SATISFIED);
            } catch (...) {
                constraint->setStatus(AssemblyConstraint::ConstraintStatus::UNSATISFIED);
                // Accumulate error for a simplified measure
                total_error += 1.0;
            }
        }

        // Check convergence
        if (total_error < tolerance) {
            return true;
        }
    }

    return false;
}

ConstraintSolverResult AssemblyConstraintSystem::solve(size_t max_iterations,
                                                        double convergence_tolerance) noexcept {
    last_solver_result_ = ConstraintSolverResult();

    // Validate all constraints first
    auto validation = validateAll();
    if (!validation.is_valid) {
        // Store invalid constraints in unsatisfied_constraints
        for (const auto& constraint : constraints_) {
            last_solver_result_.unsatisfied_constraints.push_back(constraint->getId());
        }
        last_solver_result_.is_converged = false;
        return last_solver_result_;
    }

    // Solve constraints
    last_solver_result_.is_converged =
        solveConstraints(max_iterations, convergence_tolerance);
    last_solver_result_.iterations = max_iterations;

    // Check redundancy
    checkRedundancy();
    last_solver_result_.redundant_constraints = redundant_constraints_;

    // Collect unsatisfied constraints
    unsatisfied_constraints_.clear();
    for (const auto& constraint : constraints_) {
        if (!constraint->isEnabled()) {
            continue;
        }
        if (constraint->getStatus() == AssemblyConstraint::ConstraintStatus::UNSATISFIED) {
            unsatisfied_constraints_.push_back(constraint->getId());
        }
    }
    last_solver_result_.unsatisfied_constraints = unsatisfied_constraints_;

    // Detect conflicts
    conflicting_constraints_ = detectConflicts();
    last_solver_result_.conflicting_constraints = conflicting_constraints_;

    // Copy positions and orientations
    last_solver_result_.component_positions = last_solver_result_.component_positions;
    last_solver_result_.component_orientations = last_solver_result_.component_orientations;

    return last_solver_result_;
}

Vector3D AssemblyConstraintSystem::getComponentPosition(const EntityId& component_id) const noexcept {
    auto it = last_solver_result_.component_positions.find(component_id);
    if (it != last_solver_result_.component_positions.end()) {
        return it->second;
    }
    return Vector3D(0, 0, 0);
}

Vector3D AssemblyConstraintSystem::getComponentOrientation(
    const EntityId& component_id) const noexcept {
    auto it = last_solver_result_.component_orientations.find(component_id);
    if (it != last_solver_result_.component_orientations.end()) {
        return it->second;
    }
    return Vector3D(0, 0, 1);  // Default Z direction
}

// ==================== Constraint Enable/Disable ====================

bool AssemblyConstraintSystem::setConstraintEnabled(const EntityId& constraint_id,
                                                    bool enabled) noexcept {
    auto constraint = getConstraint(constraint_id);
    if (!constraint) {
        return false;
    }
    constraint->setEnabled(enabled);
    return true;
}

bool AssemblyConstraintSystem::isConstraintEnabled(const EntityId& constraint_id) const noexcept {
    auto constraint = getConstraint(constraint_id);
    if (!constraint) {
        return false;
    }
    return constraint->isEnabled();
}

bool AssemblyConstraintSystem::setConstraintPriority(const EntityId& constraint_id,
                                                     double priority) noexcept {
    auto constraint = getConstraint(constraint_id);
    if (!constraint) {
        return false;
    }
    constraint->setPriority(priority);
    return true;
}

// ==================== Serialization ====================

namespace {

using nlohmann::json;
using CT = AssemblyConstraint::ConstraintType;

const char* constraint_type_name(CT t) {
    switch (t) {
        case CT::FIXED: return "FIXED";
        case CT::COINCIDENT: return "COINCIDENT";
        case CT::DISTANCE: return "DISTANCE";
        case CT::PARALLEL: return "PARALLEL";
        case CT::PERPENDICULAR: return "PERPENDICULAR";
        case CT::TANGENT: return "TANGENT";
        case CT::SYMMETRIC: return "SYMMETRIC";
        case CT::ANGLE: return "ANGLE";
    }
    return "UNKNOWN";
}

json vec_json(const Vector3D& v) { return json::array({v.x, v.y, v.z}); }

Vector3D json_vec(const json& j) {
    if (!j.is_array() || j.size() != 3) throw std::runtime_error("ожидался вектор [x, y, z]");
    return Vector3D(j[0].get<double>(), j[1].get<double>(), j[2].get<double>());
}

// Пары сущностей с индексами геометрии — общая часть всех ограничений, кроме FIXED.
template <class C>
void put_pair(json& j, const C& c) {
    j["entity1"] = c.getEntity1().toString();
    j["entity2"] = c.getEntity2().toString();
    j["geometry1_index"] = c.getGeometry1Index();
    j["geometry2_index"] = c.getGeometry2Index();
}

}  // namespace

std::string AssemblyConstraintSystem::serializeToJSON() const {
    json constraints = json::array();
    for (const auto& c : constraints_) {
        json j;
        j["id"] = c->getId().toString();
        j["type"] = constraint_type_name(c->getType());
        switch (c->getType()) {
            case CT::FIXED: {
                const auto& f = static_cast<const FixedConstraint&>(*c);
                j["component"] = f.getComponentId().toString();
                j["position"] = vec_json(f.getPosition());
                j["orientation"] = vec_json(f.getOrientation());
                break;
            }
            case CT::COINCIDENT: put_pair(j, static_cast<const CoincidentConstraint&>(*c)); break;
            case CT::PARALLEL: put_pair(j, static_cast<const ParallelConstraint&>(*c)); break;
            case CT::PERPENDICULAR: put_pair(j, static_cast<const PerpendicularConstraint&>(*c)); break;
            case CT::TANGENT: put_pair(j, static_cast<const TangentConstraint&>(*c)); break;
            case CT::DISTANCE: {
                const auto& d = static_cast<const DistanceConstraint&>(*c);
                put_pair(j, d);
                j["distance"] = d.getDistance();
                break;
            }
            case CT::ANGLE: {
                const auto& a = static_cast<const AngleConstraint&>(*c);
                put_pair(j, a);
                j["angle"] = a.getAngle();
                break;
            }
            case CT::SYMMETRIC: {
                const auto& s = static_cast<const SymmetricConstraint&>(*c);
                put_pair(j, s);
                j["symmetry_plane"] = s.getSymmetryPlane().toString();
                break;
            }
        }
        j["description"] = c->getDescription();
        j["enabled"] = c->isEnabled();
        j["priority"] = c->getPriority();
        constraints.push_back(std::move(j));
    }
    json root{{"format", "bendcore-assembly-constraints"}, {"version", 1},
              {"assembly_id", assembly_id_.toString()}, {"constraints", std::move(constraints)}};
    return root.dump(2);
}

void AssemblyConstraintSystem::deserializeFromJSON(const std::string& text) {
    if (text.empty()) {
        throw std::runtime_error("JSON string is empty");
    }
    json root;
    try {
        root = json::parse(text);
    } catch (const json::exception& e) {
        throw std::runtime_error(std::string("некорректный JSON сопряжений: ") + e.what());
    }
    if (!root.is_object() || !root.contains("constraints") || !root["constraints"].is_array()) {
        throw std::runtime_error("в JSON нет массива constraints");
    }

    // Сначала разбираем всё, затем подменяем: при ошибке существующие сопряжения не теряются.
    std::vector<std::shared_ptr<AssemblyConstraint>> loaded;
    try {
        for (const auto& j : root["constraints"]) {
            const EntityId id = EntityId::parse(j.at("id").get<std::string>());
            const std::string type = j.at("type").get<std::string>();
            auto ent = [&](const char* key) { return EntityId::parse(j.at(key).get<std::string>()); };
            const size_t g1 = j.value("geometry1_index", size_t{0});
            const size_t g2 = j.value("geometry2_index", size_t{0});

            std::shared_ptr<AssemblyConstraint> c;
            if (type == "FIXED") {
                c = std::make_shared<FixedConstraint>(id, ent("component"), json_vec(j.at("position")),
                                                      json_vec(j.at("orientation")));
            } else if (type == "COINCIDENT") {
                c = std::make_shared<CoincidentConstraint>(id, ent("entity1"), ent("entity2"), g1, g2);
            } else if (type == "PARALLEL") {
                c = std::make_shared<ParallelConstraint>(id, ent("entity1"), ent("entity2"), g1, g2);
            } else if (type == "PERPENDICULAR") {
                c = std::make_shared<PerpendicularConstraint>(id, ent("entity1"), ent("entity2"), g1, g2);
            } else if (type == "TANGENT") {
                c = std::make_shared<TangentConstraint>(id, ent("entity1"), ent("entity2"), g1, g2);
            } else if (type == "DISTANCE") {
                c = std::make_shared<DistanceConstraint>(id, ent("entity1"), ent("entity2"),
                                                         j.at("distance").get<double>(), g1, g2);
            } else if (type == "ANGLE") {
                c = std::make_shared<AngleConstraint>(id, ent("entity1"), ent("entity2"),
                                                      j.at("angle").get<double>(), g1, g2);
            } else if (type == "SYMMETRIC") {
                c = std::make_shared<SymmetricConstraint>(id, ent("entity1"), ent("entity2"),
                                                          ent("symmetry_plane"), g1, g2);
            } else {
                throw std::runtime_error("неизвестный тип сопряжения: " + type);
            }
            if (!c->validate()) {
                throw std::runtime_error("сопряжение " + j.at("id").get<std::string>() + " не проходит проверку");
            }
            c->setEnabled(j.value("enabled", true));
            c->setPriority(j.value("priority", 1.0));
            loaded.push_back(std::move(c));
        }
    } catch (const json::exception& e) {
        throw std::runtime_error(std::string("неверная структура сопряжения: ") + e.what());
    } catch (const std::invalid_argument& e) {
        throw std::runtime_error(std::string("неверный идентификатор в сопряжении: ") + e.what());
    }

    if (root.contains("assembly_id")) {
        assembly_id_ = EntityId::parse(root["assembly_id"].get<std::string>());
    }
    clearConstraints();
    for (auto& c : loaded) addConstraint(std::move(c));
}

std::vector<uint8_t> AssemblyConstraintSystem::serializeToBinary() const {
    std::vector<uint8_t> result;

    // Store count
    uint32_t count = constraints_.size();
    result.push_back((count >> 24) & 0xFF);
    result.push_back((count >> 16) & 0xFF);
    result.push_back((count >> 8) & 0xFF);
    result.push_back(count & 0xFF);

    // Store each constraint type and enabled state
    for (const auto& constraint : constraints_) {
        uint8_t type = static_cast<uint8_t>(constraint->getType());
        uint8_t enabled = constraint->isEnabled() ? 1 : 0;
        result.push_back(type);
        result.push_back(enabled);
    }

    return result;
}

void AssemblyConstraintSystem::deserializeFromBinary(const std::vector<uint8_t>& data) {
    if (data.size() < 4) {
        throw std::runtime_error("Binary data too small");
    }

    uint32_t count = (static_cast<uint32_t>(data[0]) << 24) |
                     (static_cast<uint32_t>(data[1]) << 16) |
                     (static_cast<uint32_t>(data[2]) << 8) | static_cast<uint32_t>(data[3]);

    if (data.size() < 4 + count * 2) {
        throw std::runtime_error("Binary data size mismatch");
    }
}

// ==================== Diagnostics ====================

std::string AssemblyConstraintSystem::getDiagnostics() const noexcept {
    std::ostringstream oss;
    oss << "Assembly Constraint System Diagnostics\n";
    oss << "=====================================\n";
    oss << "Assembly ID: " << assembly_id_.toString() << "\n";
    oss << "Total Constraints: " << constraints_.size() << "\n";
    oss << "Over-constrained: " << (isOverConstrained() ? "YES" : "NO") << "\n";
    oss << "Under-constrained: " << (isUnderConstrained() ? "YES" : "NO") << "\n";
    oss << "Redundant Constraints: " << redundant_constraints_.size() << "\n";
    oss << "Unsatisfied Constraints: " << unsatisfied_constraints_.size() << "\n";
    oss << "Conflicting Constraints: " << conflicting_constraints_.size() << "\n";

    oss << "\nConstraint Breakdown:\n";
    for (const auto& type_val :
         {AssemblyConstraint::ConstraintType::FIXED, AssemblyConstraint::ConstraintType::COINCIDENT,
          AssemblyConstraint::ConstraintType::DISTANCE, AssemblyConstraint::ConstraintType::PARALLEL,
          AssemblyConstraint::ConstraintType::PERPENDICULAR,
          AssemblyConstraint::ConstraintType::TANGENT, AssemblyConstraint::ConstraintType::SYMMETRIC,
          AssemblyConstraint::ConstraintType::ANGLE}) {
        size_t count = getConstraintsByType(type_val).size();
        if (count > 0) {
            std::string type_name;
            switch (type_val) {
                case AssemblyConstraint::ConstraintType::FIXED:
                    type_name = "Fixed";
                    break;
                case AssemblyConstraint::ConstraintType::COINCIDENT:
                    type_name = "Coincident";
                    break;
                case AssemblyConstraint::ConstraintType::DISTANCE:
                    type_name = "Distance";
                    break;
                case AssemblyConstraint::ConstraintType::PARALLEL:
                    type_name = "Parallel";
                    break;
                case AssemblyConstraint::ConstraintType::PERPENDICULAR:
                    type_name = "Perpendicular";
                    break;
                case AssemblyConstraint::ConstraintType::TANGENT:
                    type_name = "Tangent";
                    break;
                case AssemblyConstraint::ConstraintType::SYMMETRIC:
                    type_name = "Symmetric";
                    break;
                case AssemblyConstraint::ConstraintType::ANGLE:
                    type_name = "Angle";
                    break;
            }
            oss << "  " << type_name << ": " << count << "\n";
        }
    }

    return oss.str();
}

std::string AssemblyConstraintSystem::getHealthStatus() const noexcept {
    std::ostringstream oss;

    if (constraints_.empty()) {
        oss << "EMPTY: No constraints defined";
    } else if (isUnderConstrained()) {
        oss << "UNDER-CONSTRAINED: Assembly needs more constraints";
    } else if (isOverConstrained()) {
        oss << "OVER-CONSTRAINED: Assembly has redundant constraints";
    } else if (!conflicting_constraints_.empty()) {
        oss << "CONFLICTING: Assembly has " << conflicting_constraints_.size()
            << " conflicting constraints";
    } else {
        oss << "HEALTHY: All constraints are satisfied";
    }

    return oss.str();
}

void AssemblyConstraintSystem::checkRedundancy() noexcept {
    redundant_constraints_.clear();

    // Simple redundancy check: if more than one Fixed constraint on same component
    std::map<EntityId, size_t> fixed_count;
    for (const auto& constraint : constraints_) {
        if (constraint->getType() == AssemblyConstraint::ConstraintType::FIXED) {
            if (auto fixed_cast = std::dynamic_pointer_cast<FixedConstraint>(constraint)) {
                fixed_count[fixed_cast->getComponentId()]++;
                if (fixed_count[fixed_cast->getComponentId()] > 1) {
                    redundant_constraints_.push_back(constraint->getId());
                }
            }
        }
    }
}

void AssemblyConstraintSystem::initializePositions() noexcept {
    // Initialize from Fixed constraints
    for (const auto& constraint : constraints_) {
        if (constraint->getType() == AssemblyConstraint::ConstraintType::FIXED) {
            if (auto fixed_cast = std::dynamic_pointer_cast<FixedConstraint>(constraint)) {
                last_solver_result_.component_positions[fixed_cast->getComponentId()] =
                    fixed_cast->getPosition();
                last_solver_result_.component_orientations[fixed_cast->getComponentId()] =
                    fixed_cast->getOrientation();
            }
        }
    }
}

} // namespace BendCore::CadCore
