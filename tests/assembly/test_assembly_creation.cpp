#include "bendcore/assembly/assembly.hpp"
#include "bendcore/assembly/component_instance.hpp"
#include "bendcore/assembly/mate_solver.hpp"
#include "bendcore/document/entity_id.hpp"
#include "bendcore/geometry/vector.hpp"

#include <cassert>
#include <iostream>
#include <vector>
#include <set>
#include <cmath>

using namespace BendCore::CadCore;

static int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << std::endl;
        ++failures;
    } else {
        std::cout << "✓ " << what << std::endl;
    }
}

// ============================================================================
// Test: Assembly Creation and Basic Properties
// ============================================================================

void test_assembly_creation() {
    std::cout << "\n=== Assembly Creation ===\n";

    auto assembly = createAssembly("TestAssembly", "Test description");
    check(assembly != nullptr, "Assembly created");
    check(!assembly->getId().isNull(), "Assembly has valid ID");
    check(assembly->getName() == "TestAssembly", "Assembly name correct");
    check(assembly->getDescription() == "Test description", "Assembly description correct");
    check(assembly->getComponentInstanceCount() == 0, "Assembly initially empty");
}

void test_assembly_direct_constructor() {
    std::cout << "\n=== Assembly Direct Constructor ===\n";

    EntityId assembly_id = EntityId::generate();
    Assembly assembly(assembly_id, "MyAssembly", "Description");

    check(assembly.getId() == assembly_id, "Assembly ID matches constructor");
    check(assembly.getName() == "MyAssembly", "Assembly name from constructor");
    check(assembly.getDescription() == "Description", "Assembly description from constructor");
}

void test_assembly_name_modification() {
    std::cout << "\n=== Assembly Name Modification ===\n";

    auto assembly = createAssembly("Original", "Desc");
    check(assembly->getName() == "Original", "Original name set");

    assembly->setName("Modified");
    check(assembly->getName() == "Modified", "Name successfully modified");

    assembly->setName("");
    check(assembly->getName() == "", "Empty name allowed");
}

void test_assembly_description_modification() {
    std::cout << "\n=== Assembly Description Modification ===\n";

    auto assembly = createAssembly("Assy", "Original description");
    check(assembly->getDescription() == "Original description", "Original description");

    assembly->setDescription("New description");
    check(assembly->getDescription() == "New description", "Description modified");
}

// ============================================================================
// Test: Assembly Revision Management
// ============================================================================

void test_assembly_revision_tracking() {
    std::cout << "\n=== Assembly Revision Tracking ===\n";

    auto assembly = createAssembly("Test", "Desc");
    check(assembly->getRevision() == 0, "Initial revision is 0");

    assembly->incrementRevision();
    check(assembly->getRevision() == 1, "Revision incremented to 1");

    assembly->incrementRevision();
    check(assembly->getRevision() == 2, "Revision incremented to 2");
}

void test_revision_increment_on_modification() {
    std::cout << "\n=== Revision Increment on Modification ===\n";

    auto assembly = createAssembly("Test", "Desc");
    uint64_t rev1 = assembly->getRevision();

    assembly->setName("Modified");
    uint64_t rev2 = assembly->getRevision();
    check(rev2 > rev1, "Name modification increments revision");

    assembly->setDescription("New desc");
    uint64_t rev3 = assembly->getRevision();
    check(rev3 > rev2, "Description modification increments revision");
}

// ============================================================================
// Test: Component Instance Management
// ============================================================================

void test_component_instance_creation() {
    std::cout << "\n=== Component Instance Creation ===\n";

    EntityId part_id = EntityId::generate();
    Vector3D pos(10.0, 20.0, 30.0);
    Vector3D orient(45.0, 0.0, 0.0);

    auto instance = createComponentInstance(part_id, pos, orient);
    check(instance != nullptr, "Component instance created");
    check(!instance->getId().isNull(), "Instance has valid ID");
    check(instance->getPartId() == part_id, "Part ID matches");
    check(instance->getPosition() == pos, "Position matches");
    check(instance->getOrientation() == orient, "Orientation matches");
}

void test_component_instance_direct_constructor() {
    std::cout << "\n=== Component Instance Direct Constructor ===\n";

    EntityId instance_id = EntityId::generate();
    EntityId part_id = EntityId::generate();
    Vector3D pos(1.0, 2.0, 3.0);
    Vector3D orient(30.0, 60.0, 90.0);

    ComponentInstance instance(instance_id, part_id, pos, orient);
    check(instance.getId() == instance_id, "Instance ID matches");
    check(instance.getPartId() == part_id, "Part ID matches");
    check(instance.getPosition() == pos, "Position matches");
    check(instance.getOrientation() == orient, "Orientation matches");
}

void test_add_single_component() {
    std::cout << "\n=== Add Single Component ===\n";

    auto assembly = createAssembly("Test", "");
    auto part_id = EntityId::generate();
    auto instance = createComponentInstance(part_id);

    EntityId added_id = assembly->addComponentInstance(instance);
    check(!added_id.isNull(), "Component added successfully");
    check(assembly->getComponentInstanceCount() == 1, "Component count is 1");
    check(assembly->getComponentInstance(added_id) == instance, "Component retrievable");
}

void test_add_multiple_components() {
    std::cout << "\n=== Add Multiple Components ===\n";

    auto assembly = createAssembly("Test", "");
    std::vector<std::shared_ptr<ComponentInstance>> instances;

    for (int i = 0; i < 5; ++i) {
        auto part_id = EntityId::generate();
        auto instance = createComponentInstance(part_id, Vector3D(i * 10, 0, 0));
        EntityId added_id = assembly->addComponentInstance(instance);
        std::string msg = "Component " + std::to_string(i) + " added";
        check(!added_id.isNull(), msg.c_str());
        instances.push_back(instance);
    }

    check(assembly->getComponentInstanceCount() == 5, "All 5 components added");
}

void test_remove_component() {
    std::cout << "\n=== Remove Component ===\n";

    auto assembly = createAssembly("Test", "");
    auto instance = createComponentInstance(EntityId::generate());
    EntityId instance_id = assembly->addComponentInstance(instance);

    check(assembly->getComponentInstanceCount() == 1, "Component added");

    bool removed = assembly->removeComponentInstance(instance_id);
    check(removed, "Component removed successfully");
    check(assembly->getComponentInstanceCount() == 0, "Assembly now empty");
    check(assembly->getComponentInstance(instance_id) == nullptr, "Removed component not found");
}

void test_remove_nonexistent_component() {
    std::cout << "\n=== Remove Non-existent Component ===\n";

    auto assembly = createAssembly("Test", "");
    EntityId fake_id = EntityId::generate();

    bool removed = assembly->removeComponentInstance(fake_id);
    check(!removed, "Remove non-existent returns false");
    check(assembly->getComponentInstanceCount() == 0, "Assembly still empty");
}

void test_get_component_at_index() {
    std::cout << "\n=== Get Component at Index ===\n";

    auto assembly = createAssembly("Test", "");
    std::vector<std::shared_ptr<ComponentInstance>> instances;

    for (int i = 0; i < 3; ++i) {
        auto instance = createComponentInstance(EntityId::generate());
        assembly->addComponentInstance(instance);
        instances.push_back(instance);
    }

    check(assembly->getComponentInstanceAt(0) == instances[0], "Index 0 correct");
    check(assembly->getComponentInstanceAt(1) == instances[1], "Index 1 correct");
    check(assembly->getComponentInstanceAt(2) == instances[2], "Index 2 correct");
    check(assembly->getComponentInstanceAt(5) == nullptr, "Out of bounds returns nullptr");
}

// ============================================================================
// Test: Component Instance Validation
// ============================================================================

void test_component_instance_validation() {
    std::cout << "\n=== Component Instance Validation ===\n";

    EntityId instance_id = EntityId::generate();
    EntityId part_id = EntityId::generate();
    auto instance = std::make_shared<ComponentInstance>(instance_id, part_id);

    std::string error = instance->validate();
    check(error.empty(), "Valid instance passes validation");
}

void test_component_instance_null_id_validation() {
    std::cout << "\n=== Component Instance Null ID Validation ===\n";

    EntityId part_id = EntityId::generate();
    auto instance = std::make_shared<ComponentInstance>(EntityId(), part_id);

    std::string error = instance->validate();
    check(!error.empty(), "Null instance ID detected");
}

void test_component_instance_null_part_validation() {
    std::cout << "\n=== Component Instance Null Part Validation ===\n";

    EntityId instance_id = EntityId::generate();
    auto instance = std::make_shared<ComponentInstance>(instance_id, EntityId());

    std::string error = instance->validate();
    check(!error.empty(), "Null part ID detected");
}

// ============================================================================
// Test: Position and Orientation
// ============================================================================

void test_position_modification() {
    std::cout << "\n=== Position Modification ===\n";

    auto instance = createComponentInstance(EntityId::generate());
    check(instance->getPosition() == Vector3D(0, 0, 0), "Initial position is origin");

    Vector3D new_pos(100.0, 200.0, 300.0);
    instance->setPosition(new_pos);
    check(instance->getPosition() == new_pos, "Position modified");
}

void test_orientation_modification() {
    std::cout << "\n=== Orientation Modification ===\n";

    auto instance = createComponentInstance(EntityId::generate());
    check(instance->getOrientation() == Vector3D(0, 0, 0), "Initial orientation is identity");

    Vector3D new_orient(45.0, 90.0, 180.0);
    instance->setOrientation(new_orient);
    check(instance->getOrientation() == new_orient, "Orientation modified");
}

void test_transform_matrix_generation() {
    std::cout << "\n=== Transform Matrix Generation ===\n";

    auto instance = createComponentInstance(EntityId::generate(), Vector3D(1, 2, 3), Vector3D(0, 0, 0));
    auto matrix = instance->getTransformMatrix();

    check(matrix.size() == 16, "Transform matrix is 4x4 (16 elements)");
    check(std::abs(matrix[0] - 1.0) < 1e-6, "Identity rotation (m00)");
    check(std::abs(matrix[3] - 1.0) < 1e-6, "Translation X preserved");
    check(std::abs(matrix[7] - 2.0) < 1e-6, "Translation Y preserved");
    check(std::abs(matrix[11] - 3.0) < 1e-6, "Translation Z preserved");
    check(std::abs(matrix[15] - 1.0) < 1e-6, "Bottom-right element is 1");
}

// ============================================================================
// Test: Component Instance Visibility and Selection
// ============================================================================

void test_component_visibility() {
    std::cout << "\n=== Component Visibility ===\n";

    auto instance = createComponentInstance(EntityId::generate());
    check(instance->isVisible(), "Initially visible");

    instance->setVisible(false);
    check(!instance->isVisible(), "Visibility toggled");

    instance->setVisible(true);
    check(instance->isVisible(), "Visibility toggled back");
}

void test_component_selection() {
    std::cout << "\n=== Component Selection ===\n";

    auto instance = createComponentInstance(EntityId::generate());
    check(!instance->isSelected(), "Initially not selected");

    instance->setSelected(true);
    check(instance->isSelected(), "Selected");

    instance->setSelected(false);
    check(!instance->isSelected(), "Deselected");
}

void test_component_suppression() {
    std::cout << "\n=== Component Suppression ===\n";

    auto instance = createComponentInstance(EntityId::generate());
    check(!instance->isSuppressed(), "Initially not suppressed");

    instance->setSuppressed(true);
    check(instance->isSuppressed(), "Suppressed");

    instance->setSuppressed(false);
    check(!instance->isSuppressed(), "Unsuppressed");
}

// ============================================================================
// Test: Metadata Management
// ============================================================================

void test_assembly_metadata() {
    std::cout << "\n=== Assembly Metadata ===\n";

    auto assembly = createAssembly("Test", "");
    assembly->setMetadata("key1", "value1");

    auto value = assembly->getMetadata("key1");
    check(value.has_value(), "Metadata key exists");
    check(value.value() == "value1", "Metadata value correct");
}

void test_metadata_missing_key() {
    std::cout << "\n=== Metadata Missing Key ===\n";

    auto assembly = createAssembly("Test", "");
    auto value = assembly->getMetadata("nonexistent");
    check(!value.has_value(), "Missing key returns nullopt");
}

void test_component_metadata() {
    std::cout << "\n=== Component Metadata ===\n";

    auto instance = createComponentInstance(EntityId::generate());
    instance->setMetadata("material", "aluminum");

    auto value = instance->getMetadata("material");
    check(value.has_value(), "Component metadata set");
    check(value.value() == "aluminum", "Component metadata value correct");
}

// ============================================================================
// Test: Assembly Validation and Health Check
// ============================================================================

void test_assembly_validation_empty() {
    std::cout << "\n=== Assembly Validation Empty ===\n";

    auto assembly = createAssembly("Test", "");
    std::string error = assembly->validate();
    check(error.empty(), "Valid empty assembly passes validation");
}

void test_assembly_validation_unnamed() {
    std::cout << "\n=== Assembly Validation Unnamed ===\n";

    Assembly assembly(EntityId::generate(), "", "");
    std::string error = assembly.validate();
    check(!error.empty(), "Empty name detected in validation");
}

void test_assembly_is_empty() {
    std::cout << "\n=== Assembly is Empty ===\n";

    auto assembly = createAssembly("Test", "");
    check(assembly->isEmpty(), "New assembly is empty");

    auto instance = createComponentInstance(EntityId::generate());
    assembly->addComponentInstance(instance);
    check(!assembly->isEmpty(), "Assembly not empty after adding component");
}

void test_assembly_clear() {
    std::cout << "\n=== Assembly Clear ===\n";

    auto assembly = createAssembly("Test", "");
    for (int i = 0; i < 5; ++i) {
        assembly->addComponentInstance(createComponentInstance(EntityId::generate()));
    }
    check(assembly->getComponentInstanceCount() == 5, "5 components added");

    assembly->clear();
    check(assembly->isEmpty(), "Assembly cleared");
    check(assembly->getComponentInstanceCount() == 0, "Component count is 0");
}

// ============================================================================
// Test: Duplicate Instance Prevention
// ============================================================================

void test_duplicate_instance_prevention() {
    std::cout << "\n=== Duplicate Instance Prevention ===\n";

    auto assembly = createAssembly("Test", "");
    auto instance = createComponentInstance(EntityId::generate());

    EntityId id1 = assembly->addComponentInstance(instance);
    check(!id1.isNull(), "First add succeeds");
    check(assembly->getComponentInstanceCount() == 1, "1 component in assembly");

    // Try to add same instance again
    EntityId id2 = assembly->addComponentInstance(instance);
    check(id2.isNull(), "Duplicate add returns null ID");
    check(assembly->getComponentInstanceCount() == 1, "Still only 1 component");
}

// ============================================================================
// Test: Primary Instance Flag
// ============================================================================

void test_primary_instance_flag() {
    std::cout << "\n=== Primary Instance Flag ===\n";

    auto instance = createComponentInstance(EntityId::generate());
    check(!instance->isPrimary(), "Initially not primary");

    instance->setPrimary(true);
    check(instance->isPrimary(), "Set as primary");

    instance->setPrimary(false);
    check(!instance->isPrimary(), "Set as non-primary");
}

// ============================================================================
// Test: Sub-assembly Support
// ============================================================================

void test_subassembly() {
    std::cout << "\n=== Sub-assembly Support ===\n";

    auto parent_assembly = createAssembly("Parent", "");
    auto sub_assembly = createAssembly("Sub", "");

    check(!parent_assembly->isSubAssembly(), "Parent is not sub-assembly");
    check(!sub_assembly->isSubAssembly(), "Sub initially not sub-assembly");

    sub_assembly->setParentAssemblyId(parent_assembly->getId());
    check(sub_assembly->isSubAssembly(), "Sub now is sub-assembly");
    check(sub_assembly->getParentAssemblyId() == parent_assembly->getId(), "Parent ID correct");
}

// ============================================================================
// Test: Constraint Solver Integration
// ============================================================================

void test_assembly_has_constraint_solver() {
    std::cout << "\n=== Assembly Constraint Solver ===\n";

    auto assembly = createAssembly("Test", "");
    check(assembly->hasConstraintSolver(), "Assembly has constraint solver");

    auto& solver = assembly->getConstraintSolver();
    (void)solver;  // Use the reference to prevent warning
    check(true, "Can access constraint solver");
}

// ============================================================================
// Test: Component Instance Description
// ============================================================================

void test_component_instance_description() {
    std::cout << "\n=== Component Instance Description ===\n";

    auto instance = createComponentInstance(EntityId::generate());
    instance->setName("MyComponent");

    std::string desc = instance->getDescription();
    check(!desc.empty(), "Description generated");
    check(desc.find("MyComponent") != std::string::npos, "Name in description");
}

// ============================================================================
// Test: Large Assembly
// ============================================================================

void test_large_assembly() {
    std::cout << "\n=== Large Assembly (100 components) ===\n";

    auto assembly = createAssembly("LargeAssembly", "");
    const int COUNT = 100;

    for (int i = 0; i < COUNT; ++i) {
        auto instance = createComponentInstance(
            EntityId::generate(),
            Vector3D(i * 10, i * 5, i * 2)
        );
        assembly->addComponentInstance(instance);
    }

    check(assembly->getComponentInstanceCount() == COUNT, "All 100 components added");
    check(!assembly->isEmpty(), "Large assembly not empty");
    check(assembly->getComponentInstanceAt(50) != nullptr, "Can access middle component");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "=== CAD-CORE-M3: Assembly Creation (GAP-009) Tests ===\n" << std::endl;

    // Assembly Creation Tests
    test_assembly_creation();
    test_assembly_direct_constructor();
    test_assembly_name_modification();
    test_assembly_description_modification();

    // Revision Tests
    test_assembly_revision_tracking();
    test_revision_increment_on_modification();

    // Component Instance Tests
    test_component_instance_creation();
    test_component_instance_direct_constructor();
    test_add_single_component();
    test_add_multiple_components();
    test_remove_component();
    test_remove_nonexistent_component();
    test_get_component_at_index();

    // Validation Tests
    test_component_instance_validation();
    test_component_instance_null_id_validation();
    test_component_instance_null_part_validation();

    // Position and Orientation Tests
    test_position_modification();
    test_orientation_modification();
    test_transform_matrix_generation();

    // State Tests
    test_component_visibility();
    test_component_selection();
    test_component_suppression();

    // Metadata Tests
    test_assembly_metadata();
    test_metadata_missing_key();
    test_component_metadata();

    // Assembly Health Tests
    test_assembly_validation_empty();
    test_assembly_validation_unnamed();
    test_assembly_is_empty();
    test_assembly_clear();

    // Advanced Tests
    test_duplicate_instance_prevention();
    test_primary_instance_flag();
    test_subassembly();
    test_assembly_has_constraint_solver();
    test_component_instance_description();
    test_large_assembly();

    std::cout << "\n" << std::string(70, '=') << std::endl;
    if (failures == 0) {
        std::cout << "✓ All 38 tests passed!\n";
        std::cout << "CAD-CORE-M3: Assembly Creation PASSED\n";
        return 0;
    } else {
        std::cout << "✗ " << failures << " test(s) failed\n";
        std::cout << "CAD-CORE-M3: Assembly Creation FAILED\n";
        return 1;
    }
}
