#include "../../include/bendcore/assembly/assembly_constraints.hpp"
#include "../../include/bendcore/assembly/assembly_constraint_system.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <functional>

using namespace BendCore::CadCore;

// Simple test framework
class TestRunner {
private:
    int passed_ = 0;
    int failed_ = 0;
    std::string current_test_;

public:
    void runTest(const std::string& name, std::function<void()> test) {
        current_test_ = name;
        try {
            test();
            passed_++;
            std::cout << "✓ " << name << std::endl;
        } catch (const std::exception& e) {
            failed_++;
            std::cout << "✗ " << name << ": " << e.what() << std::endl;
        } catch (...) {
            failed_++;
            std::cout << "✗ " << name << ": Unknown exception" << std::endl;
        }
    }

    void printSummary() {
        std::cout << "\n========================================\n";
        std::cout << "Tests Passed: " << passed_ << "\n";
        std::cout << "Tests Failed: " << failed_ << "\n";
        std::cout << "Total Tests: " << (passed_ + failed_) << "\n";
        std::cout << "========================================\n";
    }

    int getFailureCount() const { return failed_; }
};

// ==================== FixedConstraint Tests ====================

void test_create_fixed_constraint() {
    auto constraint_id = EntityId::generate();
    auto component = EntityId::generate();
    Vector3D position(10.0, 20.0, 30.0);
    Vector3D orientation(0.0, 0.0, 1.0);

    FixedConstraint constraint(constraint_id, component, position, orientation);

    assert(constraint.getId() == constraint_id);
    assert(constraint.getType() == AssemblyConstraint::ConstraintType::FIXED);
    assert(constraint.getComponentId() == component);
    assert(constraint.getPosition() == position);
    assert(constraint.getOrientation() == orientation);
}

void test_fixed_constraint_validation() {
    auto constraint_id = EntityId::generate();
    auto component = EntityId::generate();
    Vector3D position(10.0, 20.0, 30.0);
    Vector3D orientation(0.0, 0.0, 1.0);

    FixedConstraint constraint(constraint_id, component, position, orientation);
    assert(constraint.validate());
}

void test_fixed_constraint_description() {
    auto constraint_id = EntityId::generate();
    auto component = EntityId::generate();
    Vector3D position(10.0, 20.0, 30.0);
    Vector3D orientation(0.0, 0.0, 1.0);

    FixedConstraint constraint(constraint_id, component, position, orientation);
    std::string desc = constraint.getDescription();
    assert(!desc.empty());
    assert(desc.find("Fixed") != std::string::npos);
}

void test_fixed_constraint_involved_entities() {
    auto constraint_id = EntityId::generate();
    auto component = EntityId::generate();
    Vector3D position(10.0, 20.0, 30.0);
    Vector3D orientation(0.0, 0.0, 1.0);

    FixedConstraint constraint(constraint_id, component, position, orientation);
    auto entities = constraint.getInvolvedEntities();
    assert(entities.size() == 1);
    assert(entities[0] == component);
}

// ==================== CoincidentConstraint Tests ====================

void test_create_coincident_constraint() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    CoincidentConstraint constraint(constraint_id, entity1, entity2);

    assert(constraint.getId() == constraint_id);
    assert(constraint.getType() == AssemblyConstraint::ConstraintType::COINCIDENT);
    assert(constraint.getEntity1() == entity1);
    assert(constraint.getEntity2() == entity2);
}

void test_coincident_constraint_validation() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    CoincidentConstraint constraint(constraint_id, entity1, entity2);
    assert(constraint.validate());
}

void test_coincident_constraint_validation_fails_same_entity() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();

    CoincidentConstraint constraint(constraint_id, entity1, entity1);

    try {
        constraint.validate();
        assert(false);  // Should have thrown
    } catch (const std::invalid_argument&) {
        // Expected
    }
}

void test_coincident_constraint_with_geometry_indices() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    CoincidentConstraint constraint(constraint_id, entity1, entity2, 1, 2);

    assert(constraint.getGeometry1Index() == 1);
    assert(constraint.getGeometry2Index() == 2);
}

void test_coincident_constraint_involved_entities() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    CoincidentConstraint constraint(constraint_id, entity1, entity2);
    auto entities = constraint.getInvolvedEntities();
    assert(entities.size() == 2);
    assert(entities[0] == entity1);
    assert(entities[1] == entity2);
}

// ==================== DistanceConstraint Tests ====================

void test_create_distance_constraint() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    double distance = 25.5;

    DistanceConstraint constraint(constraint_id, entity1, entity2, distance);

    assert(constraint.getId() == constraint_id);
    assert(constraint.getType() == AssemblyConstraint::ConstraintType::DISTANCE);
    assert(constraint.getDistance() == distance);
}

void test_distance_constraint_validation() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    double distance = 25.5;

    DistanceConstraint constraint(constraint_id, entity1, entity2, distance);
    assert(constraint.validate());
}

void test_distance_constraint_validation_fails_negative_distance() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    DistanceConstraint constraint(constraint_id, entity1, entity2, -5.0);

    try {
        constraint.validate();
        assert(false);  // Should have thrown
    } catch (const std::invalid_argument&) {
        // Expected
    }
}

void test_distance_constraint_modify() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    double distance = 25.5;

    DistanceConstraint constraint(constraint_id, entity1, entity2, distance);

    double new_distance = 50.0;
    constraint.setDistance(new_distance);
    assert(constraint.getDistance() == new_distance);
}

void test_distance_constraint_description() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    double distance = 25.5;

    DistanceConstraint constraint(constraint_id, entity1, entity2, distance);
    std::string desc = constraint.getDescription();
    assert(!desc.empty());
    assert(desc.find("Distance") != std::string::npos);
}

// ==================== ParallelConstraint Tests ====================

void test_create_parallel_constraint() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    ParallelConstraint constraint(constraint_id, entity1, entity2);

    assert(constraint.getId() == constraint_id);
    assert(constraint.getType() == AssemblyConstraint::ConstraintType::PARALLEL);
}

void test_parallel_constraint_validation() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    ParallelConstraint constraint(constraint_id, entity1, entity2);
    assert(constraint.validate());
}

void test_parallel_constraint_validation_fails_same_entity() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();

    ParallelConstraint constraint(constraint_id, entity1, entity1);

    try {
        constraint.validate();
        assert(false);  // Should have thrown
    } catch (const std::invalid_argument&) {
        // Expected
    }
}

// ==================== PerpendicularConstraint Tests ====================

void test_create_perpendicular_constraint() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    PerpendicularConstraint constraint(constraint_id, entity1, entity2);

    assert(constraint.getId() == constraint_id);
    assert(constraint.getType() == AssemblyConstraint::ConstraintType::PERPENDICULAR);
}

void test_perpendicular_constraint_validation() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    PerpendicularConstraint constraint(constraint_id, entity1, entity2);
    assert(constraint.validate());
}

// ==================== TangentConstraint Tests ====================

void test_create_tangent_constraint() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    TangentConstraint constraint(constraint_id, entity1, entity2);

    assert(constraint.getId() == constraint_id);
    assert(constraint.getType() == AssemblyConstraint::ConstraintType::TANGENT);
}

void test_tangent_constraint_validation() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    TangentConstraint constraint(constraint_id, entity1, entity2);
    assert(constraint.validate());
}

// ==================== SymmetricConstraint Tests ====================

void test_create_symmetric_constraint() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    auto symmetry_plane = EntityId::generate();

    SymmetricConstraint constraint(constraint_id, entity1, entity2, symmetry_plane);

    assert(constraint.getId() == constraint_id);
    assert(constraint.getType() == AssemblyConstraint::ConstraintType::SYMMETRIC);
    assert(constraint.getSymmetryPlane() == symmetry_plane);
}

void test_symmetric_constraint_validation() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    auto symmetry_plane = EntityId::generate();

    SymmetricConstraint constraint(constraint_id, entity1, entity2, symmetry_plane);
    assert(constraint.validate());
}

void test_symmetric_constraint_involved_entities() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    auto symmetry_plane = EntityId::generate();

    SymmetricConstraint constraint(constraint_id, entity1, entity2, symmetry_plane);
    auto entities = constraint.getInvolvedEntities();
    assert(entities.size() == 3);
}

// ==================== AngleConstraint Tests ====================

void test_create_angle_constraint() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    double angle = 45.0;

    AngleConstraint constraint(constraint_id, entity1, entity2, angle);

    assert(constraint.getId() == constraint_id);
    assert(constraint.getType() == AssemblyConstraint::ConstraintType::ANGLE);
    assert(constraint.getAngle() == angle);
}

void test_angle_constraint_validation() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    double angle = 45.0;

    AngleConstraint constraint(constraint_id, entity1, entity2, angle);
    assert(constraint.validate());
}

void test_angle_constraint_validation_fails_out_of_range() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    AngleConstraint constraint(constraint_id, entity1, entity2, 190.0);

    try {
        constraint.validate();
        assert(false);  // Should have thrown
    } catch (const std::invalid_argument&) {
        // Expected
    }
}

void test_angle_constraint_modify() {
    auto constraint_id = EntityId::generate();
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    double angle = 45.0;

    AngleConstraint constraint(constraint_id, entity1, entity2, angle);

    double new_angle = 90.0;
    constraint.setAngle(new_angle);
    assert(constraint.getAngle() == new_angle);
}

// ==================== AssemblyConstraintSystem Tests ====================

void test_create_system() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    assert(system.getConstraintCount() == 0);
}

void test_add_fixed_constraint() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto component = EntityId::generate();
    Vector3D pos(0.0, 0.0, 0.0);
    Vector3D orient(0.0, 0.0, 1.0);

    auto constraint = system.addFixedConstraint(component, pos, orient);
    assert(constraint != nullptr);
    assert(system.getConstraintCount() == 1);
}

void test_add_coincident_constraint() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    auto constraint = system.addCoincidentConstraint(entity1, entity2);
    assert(constraint != nullptr);
    assert(system.getConstraintCount() == 1);
}

void test_add_distance_constraint() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    auto constraint = system.addDistanceConstraint(entity1, entity2, 15.0);
    assert(constraint != nullptr);
    assert(system.getConstraintCount() == 1);
}

void test_add_parallel_constraint() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    auto constraint = system.addParallelConstraint(entity1, entity2);
    assert(constraint != nullptr);
    assert(system.getConstraintCount() == 1);
}

void test_add_perpendicular_constraint() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    auto constraint = system.addPerpendicularConstraint(entity1, entity2);
    assert(constraint != nullptr);
    assert(system.getConstraintCount() == 1);
}

void test_add_tangent_constraint() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    auto constraint = system.addTangentConstraint(entity1, entity2);
    assert(constraint != nullptr);
    assert(system.getConstraintCount() == 1);
}

void test_add_symmetric_constraint() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    auto plane = EntityId::generate();

    auto constraint = system.addSymmetricConstraint(entity1, entity2, plane);
    assert(constraint != nullptr);
    assert(system.getConstraintCount() == 1);
}

void test_add_angle_constraint() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    auto constraint = system.addAngleConstraint(entity1, entity2, 45.0);
    assert(constraint != nullptr);
    assert(system.getConstraintCount() == 1);
}

void test_remove_constraint() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto component = EntityId::generate();
    Vector3D pos(0.0, 0.0, 0.0);
    Vector3D orient(0.0, 0.0, 1.0);

    auto constraint = system.addFixedConstraint(component, pos, orient);
    auto constraint_id = constraint->getId();
    assert(system.getConstraintCount() == 1);

    bool removed = system.removeConstraint(constraint_id);
    assert(removed);
    assert(system.getConstraintCount() == 0);
}

void test_get_constraint() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto component = EntityId::generate();
    Vector3D pos(0.0, 0.0, 0.0);
    Vector3D orient(0.0, 0.0, 1.0);

    auto constraint = system.addFixedConstraint(component, pos, orient);
    auto constraint_id = constraint->getId();

    auto retrieved = system.getConstraint(constraint_id);
    assert(retrieved != nullptr);
    assert(retrieved->getId() == constraint_id);
}

void test_get_constraint_not_found() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto non_existent = EntityId::generate();

    auto retrieved = system.getConstraint(non_existent);
    assert(retrieved == nullptr);
}

void test_get_constraints_for_entity() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    auto entity3 = EntityId::generate();
    Vector3D pos(0.0, 0.0, 0.0);
    Vector3D orient(0.0, 0.0, 1.0);

    system.addFixedConstraint(entity1, pos, orient);
    system.addCoincidentConstraint(entity1, entity2);
    system.addDistanceConstraint(entity1, entity3, 15.0);

    auto constraints = system.getConstraintsForEntity(entity1);
    assert(constraints.size() == 3);
}

void test_get_constraints_by_type() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    Vector3D pos(0.0, 0.0, 0.0);
    Vector3D pos2(10.0, 0.0, 0.0);
    Vector3D orient(0.0, 0.0, 1.0);

    system.addFixedConstraint(entity1, pos, orient);
    system.addFixedConstraint(entity2, pos2, orient);
    system.addCoincidentConstraint(entity1, entity2);

    auto fixed_constraints = system.getConstraintsByType(AssemblyConstraint::ConstraintType::FIXED);
    assert(fixed_constraints.size() == 2);

    auto coincident_constraints =
        system.getConstraintsByType(AssemblyConstraint::ConstraintType::COINCIDENT);
    assert(coincident_constraints.size() == 1);
}

void test_clear_constraints() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    Vector3D pos(0.0, 0.0, 0.0);
    Vector3D orient(0.0, 0.0, 1.0);

    system.addFixedConstraint(entity1, pos, orient);
    system.addCoincidentConstraint(entity1, entity2);
    assert(system.getConstraintCount() == 2);

    system.clearConstraints();
    assert(system.getConstraintCount() == 0);
}

void test_validate_all() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    Vector3D pos(0.0, 0.0, 0.0);
    Vector3D orient(0.0, 0.0, 1.0);

    system.addFixedConstraint(entity1, pos, orient);
    system.addCoincidentConstraint(entity1, entity2);

    auto result = system.validateAll();
    assert(result.is_valid);
    assert(result.error_messages.empty());
}

void test_validate_single_constraint() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    Vector3D pos(0.0, 0.0, 0.0);
    Vector3D orient(0.0, 0.0, 1.0);

    auto constraint = system.addFixedConstraint(entity1, pos, orient);

    auto result = system.validateConstraint(constraint->getId());
    assert(result.is_valid);
}

void test_set_constraint_enabled() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    Vector3D pos(0.0, 0.0, 0.0);
    Vector3D orient(0.0, 0.0, 1.0);

    auto constraint = system.addFixedConstraint(entity1, pos, orient);
    auto constraint_id = constraint->getId();

    assert(system.isConstraintEnabled(constraint_id));

    system.setConstraintEnabled(constraint_id, false);
    assert(!system.isConstraintEnabled(constraint_id));

    system.setConstraintEnabled(constraint_id, true);
    assert(system.isConstraintEnabled(constraint_id));
}

void test_set_constraint_priority() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    Vector3D pos(0.0, 0.0, 0.0);
    Vector3D orient(0.0, 0.0, 1.0);

    auto constraint = system.addFixedConstraint(entity1, pos, orient);
    auto constraint_id = constraint->getId();

    assert(constraint->getPriority() == 1.0);

    system.setConstraintPriority(constraint_id, 2.5);
    assert(constraint->getPriority() == 2.5);
}

void test_solve_constraints() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    Vector3D pos1(0.0, 0.0, 0.0);
    Vector3D pos2(10.0, 0.0, 0.0);
    Vector3D orient(0.0, 0.0, 1.0);

    system.addFixedConstraint(entity1, pos1, orient);
    system.addFixedConstraint(entity2, pos2, orient);

    auto result = system.solve();
    assert(result.is_converged);
    assert(result.iterations >= 0);
}

void test_get_component_position() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    Vector3D pos(10.0, 20.0, 30.0);
    Vector3D orient(0.0, 0.0, 1.0);

    system.addFixedConstraint(entity1, pos, orient);
    system.solve();

    auto solved_pos = system.getComponentPosition(entity1);
    assert(solved_pos == pos);
}

void test_get_component_orientation() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    Vector3D pos(10.0, 20.0, 30.0);
    Vector3D orient(0.0, 0.0, 1.0);

    system.addFixedConstraint(entity1, pos, orient);
    system.solve();

    auto solved_orient = system.getComponentOrientation(entity1);
    assert(solved_orient == orient);
}

void test_detect_conflicts() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();

    system.addParallelConstraint(entity1, entity2);
    system.addPerpendicularConstraint(entity1, entity2);

    auto conflicts = system.detectConflicts();
    assert(conflicts.size() > 0);
}

void test_is_over_constrained() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    Vector3D pos1(0.0, 0.0, 0.0);
    Vector3D pos2(10.0, 0.0, 0.0);
    Vector3D orient(0.0, 0.0, 1.0);

    system.addFixedConstraint(entity1, pos1, orient);
    system.addFixedConstraint(entity1, pos2, orient);

    system.solve();

    assert(system.isOverConstrained());
}

void test_serialize_to_json() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    Vector3D pos(0.0, 0.0, 0.0);
    Vector3D orient(0.0, 0.0, 1.0);

    system.addFixedConstraint(entity1, pos, orient);
    system.addCoincidentConstraint(entity1, entity2);

    std::string json = system.serializeToJSON();
    assert(!json.empty());
    assert(json.find("assembly_id") != std::string::npos);
    assert(json.find("constraints") != std::string::npos);
}

// Полный круг: все 8 типов сопряжений с параметрами переживают сохранение и загрузку.
void test_json_round_trip_all_types() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto a = EntityId::generate(), b = EntityId::generate(), plane = EntityId::generate();

    auto fixed = system.addFixedConstraint(a, Vector3D(1.5, -2.0, 3.25), Vector3D(0.0, 0.0, 1.0));
    system.addCoincidentConstraint(a, b, 2, 3);
    auto dist = system.addDistanceConstraint(a, b, 42.5, 1, 4);
    system.addParallelConstraint(a, b);
    system.addPerpendicularConstraint(a, b);
    system.addTangentConstraint(a, b);
    system.addSymmetricConstraint(a, b, plane);
    system.addAngleConstraint(a, b, 30.0);
    dist->setEnabled(false);
    fixed->setPriority(5.0);

    const std::string text = system.serializeToJSON();

    AssemblyConstraintSystem restored(EntityId::generate());
    restored.deserializeFromJSON(text);

    assert(restored.getConstraints().size() == 8);
    assert(restored.serializeToJSON() == text);  // побайтно тот же документ

    auto rf = std::dynamic_pointer_cast<FixedConstraint>(restored.getConstraint(fixed->getId()));
    assert(rf && rf->getComponentId() == a);
    assert(rf->getPosition().x == 1.5 && rf->getPosition().z == 3.25);
    assert(rf->getPriority() == 5.0);

    auto rd = std::dynamic_pointer_cast<DistanceConstraint>(restored.getConstraint(dist->getId()));
    assert(rd && rd->getDistance() == 42.5);
    assert(rd->getGeometry1Index() == 1 && rd->getGeometry2Index() == 4);
    assert(!rd->isEnabled());

    auto syms = restored.getConstraintsByType(AssemblyConstraint::ConstraintType::SYMMETRIC);
    assert(syms.size() == 1);
    assert(std::static_pointer_cast<SymmetricConstraint>(syms[0])->getSymmetryPlane() == plane);
}

// Ошибка в данных не должна стирать уже загруженные сопряжения.
void test_json_invalid_keeps_state() {
    AssemblyConstraintSystem system(EntityId::generate());
    system.addParallelConstraint(EntityId::generate(), EntityId::generate());

    bool threw = false;
    try { system.deserializeFromJSON("{not json"); } catch (const std::runtime_error&) { threw = true; }
    assert(threw);

    threw = false;
    try {
        system.deserializeFromJSON(R"({"constraints":[{"id":"bad","type":"PARALLEL"}]})");
    } catch (const std::runtime_error&) { threw = true; }
    assert(threw);

    threw = false;
    try {
        system.deserializeFromJSON(
            R"({"constraints":[{"id":"00000000-0000-4000-8000-000000000001","type":"WELD"}]})");
    } catch (const std::runtime_error&) { threw = true; }
    assert(threw);

    assert(system.getConstraints().size() == 1);
}

void test_serialize_to_binary() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    Vector3D pos(0.0, 0.0, 0.0);
    Vector3D orient(0.0, 0.0, 1.0);

    system.addFixedConstraint(entity1, pos, orient);
    system.addCoincidentConstraint(entity1, entity2);

    auto binary = system.serializeToBinary();
    assert(binary.size() > 0);
}

void test_get_diagnostics() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    Vector3D pos(0.0, 0.0, 0.0);
    Vector3D orient(0.0, 0.0, 1.0);

    system.addFixedConstraint(entity1, pos, orient);
    system.addCoincidentConstraint(entity1, entity2);

    std::string diagnostics = system.getDiagnostics();
    assert(!diagnostics.empty());
    assert(diagnostics.find("Constraint") != std::string::npos);
}

void test_get_health_status() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    Vector3D pos(0.0, 0.0, 0.0);
    Vector3D orient(0.0, 0.0, 1.0);

    system.addFixedConstraint(entity1, pos, orient);

    std::string status = system.getHealthStatus();
    assert(!status.empty());
}

void test_multi_constraint_scenario() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    Vector3D pos1(0.0, 0.0, 0.0);
    Vector3D orient(0.0, 0.0, 1.0);

    system.addFixedConstraint(entity1, pos1, orient);
    system.addCoincidentConstraint(entity1, entity2);
    system.addDistanceConstraint(entity1, entity2, 5.0);

    assert(system.getConstraintCount() == 3);

    auto result = system.solve();
    assert(result.is_converged);
}

void test_complex_assembly_scenario() {
    auto assembly_id = EntityId::generate();
    AssemblyConstraintSystem system(assembly_id);
    auto entity1 = EntityId::generate();
    auto entity2 = EntityId::generate();
    auto entity3 = EntityId::generate();
    Vector3D origin(0, 0, 0);
    Vector3D pos_x(10, 0, 0);
    Vector3D pos_y(0, 10, 0);
    Vector3D dir_z(0, 0, 1);

    system.addFixedConstraint(entity1, origin, dir_z);
    system.addDistanceConstraint(entity1, entity2, 10.0);
    system.addParallelConstraint(entity1, entity2);
    system.addCoincidentConstraint(entity2, entity3);
    system.addAngleConstraint(entity2, entity3, 90.0);

    assert(system.getConstraintCount() == 5);

    auto result = system.solve();
    assert(result.component_positions.size() >= 0);
}

int main() {
    TestRunner runner;

    // FixedConstraint tests
    runner.runTest("test_json_round_trip_all_types", test_json_round_trip_all_types);
    runner.runTest("test_json_invalid_keeps_state", test_json_invalid_keeps_state);
    runner.runTest("test_create_fixed_constraint", test_create_fixed_constraint);
    runner.runTest("test_fixed_constraint_validation", test_fixed_constraint_validation);
    runner.runTest("test_fixed_constraint_description", test_fixed_constraint_description);
    runner.runTest("test_fixed_constraint_involved_entities", test_fixed_constraint_involved_entities);

    // CoincidentConstraint tests
    runner.runTest("test_create_coincident_constraint", test_create_coincident_constraint);
    runner.runTest("test_coincident_constraint_validation", test_coincident_constraint_validation);
    runner.runTest("test_coincident_constraint_validation_fails_same_entity",
                   test_coincident_constraint_validation_fails_same_entity);
    runner.runTest("test_coincident_constraint_with_geometry_indices",
                   test_coincident_constraint_with_geometry_indices);
    runner.runTest("test_coincident_constraint_involved_entities",
                   test_coincident_constraint_involved_entities);

    // DistanceConstraint tests
    runner.runTest("test_create_distance_constraint", test_create_distance_constraint);
    runner.runTest("test_distance_constraint_validation", test_distance_constraint_validation);
    runner.runTest("test_distance_constraint_validation_fails_negative_distance",
                   test_distance_constraint_validation_fails_negative_distance);
    runner.runTest("test_distance_constraint_modify", test_distance_constraint_modify);
    runner.runTest("test_distance_constraint_description", test_distance_constraint_description);

    // ParallelConstraint tests
    runner.runTest("test_create_parallel_constraint", test_create_parallel_constraint);
    runner.runTest("test_parallel_constraint_validation", test_parallel_constraint_validation);
    runner.runTest("test_parallel_constraint_validation_fails_same_entity",
                   test_parallel_constraint_validation_fails_same_entity);

    // PerpendicularConstraint tests
    runner.runTest("test_create_perpendicular_constraint", test_create_perpendicular_constraint);
    runner.runTest("test_perpendicular_constraint_validation", test_perpendicular_constraint_validation);

    // TangentConstraint tests
    runner.runTest("test_create_tangent_constraint", test_create_tangent_constraint);
    runner.runTest("test_tangent_constraint_validation", test_tangent_constraint_validation);

    // SymmetricConstraint tests
    runner.runTest("test_create_symmetric_constraint", test_create_symmetric_constraint);
    runner.runTest("test_symmetric_constraint_validation", test_symmetric_constraint_validation);
    runner.runTest("test_symmetric_constraint_involved_entities", test_symmetric_constraint_involved_entities);

    // AngleConstraint tests
    runner.runTest("test_create_angle_constraint", test_create_angle_constraint);
    runner.runTest("test_angle_constraint_validation", test_angle_constraint_validation);
    runner.runTest("test_angle_constraint_validation_fails_out_of_range",
                   test_angle_constraint_validation_fails_out_of_range);
    runner.runTest("test_angle_constraint_modify", test_angle_constraint_modify);

    // AssemblyConstraintSystem tests
    runner.runTest("test_create_system", test_create_system);
    runner.runTest("test_add_fixed_constraint", test_add_fixed_constraint);
    runner.runTest("test_add_coincident_constraint", test_add_coincident_constraint);
    runner.runTest("test_add_distance_constraint", test_add_distance_constraint);
    runner.runTest("test_add_parallel_constraint", test_add_parallel_constraint);
    runner.runTest("test_add_perpendicular_constraint", test_add_perpendicular_constraint);
    runner.runTest("test_add_tangent_constraint", test_add_tangent_constraint);
    runner.runTest("test_add_symmetric_constraint", test_add_symmetric_constraint);
    runner.runTest("test_add_angle_constraint", test_add_angle_constraint);
    runner.runTest("test_remove_constraint", test_remove_constraint);
    runner.runTest("test_get_constraint", test_get_constraint);
    runner.runTest("test_get_constraint_not_found", test_get_constraint_not_found);
    runner.runTest("test_get_constraints_for_entity", test_get_constraints_for_entity);
    runner.runTest("test_get_constraints_by_type", test_get_constraints_by_type);
    runner.runTest("test_clear_constraints", test_clear_constraints);
    runner.runTest("test_validate_all", test_validate_all);
    runner.runTest("test_validate_single_constraint", test_validate_single_constraint);
    runner.runTest("test_set_constraint_enabled", test_set_constraint_enabled);
    runner.runTest("test_set_constraint_priority", test_set_constraint_priority);
    runner.runTest("test_solve_constraints", test_solve_constraints);
    runner.runTest("test_get_component_position", test_get_component_position);
    runner.runTest("test_get_component_orientation", test_get_component_orientation);
    runner.runTest("test_detect_conflicts", test_detect_conflicts);
    runner.runTest("test_is_over_constrained", test_is_over_constrained);
    runner.runTest("test_serialize_to_json", test_serialize_to_json);
    runner.runTest("test_serialize_to_binary", test_serialize_to_binary);
    runner.runTest("test_get_diagnostics", test_get_diagnostics);
    runner.runTest("test_get_health_status", test_get_health_status);
    runner.runTest("test_multi_constraint_scenario", test_multi_constraint_scenario);
    runner.runTest("test_complex_assembly_scenario", test_complex_assembly_scenario);

    runner.printSummary();

    return runner.getFailureCount();
}
