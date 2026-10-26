/**
 * M5 Phase: Advanced Feature Operations Tests
 * 30+ unit tests for GAP-020: Loft, Sweep, Revolve Features
 */

#include <cassert>
#include <iostream>
#include <cmath>
#include <vector>
#include <string>

#include "bendcore/features/loft.hpp"
#include "bendcore/features/sweep.hpp"
#include "bendcore/features/revolve.hpp"
#include "bendcore/features/feature_system.hpp"
#include "bendcore/document/entity_id.hpp"

using namespace BendCore::CadCore;

// Test counter
int tests_passed = 0;
int tests_failed = 0;

void assert_test(bool condition, const std::string& test_name) {
    if (condition) {
        std::cout << "PASS: " << test_name << "\n";
        tests_passed++;
    } else {
        std::cout << "FAIL: " << test_name << "\n";
        tests_failed++;
    }
}

// ========== Loft Feature Tests ==========

void test_loft_creation_basic() {
    EntityId id;
    LoftFeature loft(id, "TestLoft");
    assert_test(loft.getName() == "TestLoft", "Loft: Creation with name");
    assert_test(loft.getId() == id, "Loft: ID matches");
}

void test_loft_add_profile_single() {
    EntityId id;
    LoftFeature loft(id, "TestLoft");

    LoftFeature::Profile profile;
    profile.sketch_id = EntityId();
    profile.blend_parameter = 0.0;
    profile.is_start = true;

    loft.addProfile(profile);
    assert_test(loft.getProfileCount() == 1, "Loft: Add single profile");
}

void test_loft_add_multiple_profiles() {
    EntityId id;
    LoftFeature loft(id, "TestLoft");

    LoftFeature::Profile p1;
    p1.sketch_id = EntityId();
    p1.blend_parameter = 0.0;
    p1.is_start = true;
    loft.addProfile(p1);

    LoftFeature::Profile p2;
    p2.sketch_id = EntityId();
    p2.blend_parameter = 1.0;
    p2.is_end = true;
    loft.addProfile(p2);

    assert_test(loft.getProfileCount() == 2, "Loft: Add multiple profiles");
}

void test_loft_validate_minimum_profiles() {
    EntityId id;
    LoftFeature loft(id, "TestLoft");

    // Single profile should fail validation
    LoftFeature::Profile p1;
    p1.sketch_id = EntityId();
    p1.blend_parameter = 0.0;
    loft.addProfile(p1);

    assert_test(!loft.validate(), "Loft: Validate rejects single profile");
}

void test_loft_validate_two_profiles() {
    EntityId id;
    LoftFeature loft(id, "TestLoft");

    LoftFeature::Profile p1;
    p1.sketch_id = EntityId::generate();
    p1.blend_parameter = 0.0;
    loft.addProfile(p1);

    LoftFeature::Profile p2;
    p2.sketch_id = EntityId::generate();
    p2.blend_parameter = 1.0;
    loft.addProfile(p2);

    assert_test(loft.validate(), "Loft: Validate accepts two profiles");
}

void test_loft_validate_invalid_blend_parameter() {
    EntityId id;
    LoftFeature loft(id, "TestLoft");

    LoftFeature::Profile p1;
    p1.sketch_id = EntityId();
    p1.blend_parameter = -0.5;  // Invalid
    loft.addProfile(p1);

    LoftFeature::Profile p2;
    p2.sketch_id = EntityId();
    p2.blend_parameter = 1.0;
    loft.addProfile(p2);

    assert_test(!loft.validate(), "Loft: Validate rejects invalid blend parameter");
}

void test_loft_validate_out_of_order_profiles() {
    EntityId id;
    LoftFeature loft(id, "TestLoft");

    LoftFeature::Profile p1;
    p1.sketch_id = EntityId();
    p1.blend_parameter = 1.0;
    loft.addProfile(p1);

    LoftFeature::Profile p2;
    p2.sketch_id = EntityId();
    p2.blend_parameter = 0.5;  // Out of order
    loft.addProfile(p2);

    assert_test(!loft.validate(), "Loft: Validate rejects out-of-order profiles");
}

void test_loft_remove_profile() {
    EntityId id;
    LoftFeature loft(id, "TestLoft");

    LoftFeature::Profile p1;
    p1.sketch_id = EntityId();
    p1.blend_parameter = 0.0;
    loft.addProfile(p1);

    LoftFeature::Profile p2;
    p2.sketch_id = EntityId();
    p2.blend_parameter = 1.0;
    loft.addProfile(p2);

    bool removed = loft.removeProfile(0);
    assert_test(removed && loft.getProfileCount() == 1, "Loft: Remove profile");
}

void test_loft_can_generate_surface() {
    EntityId id;
    LoftFeature loft(id, "TestLoft");

    LoftFeature::Profile p1;
    p1.sketch_id = EntityId();
    p1.blend_parameter = 0.0;
    loft.addProfile(p1);

    LoftFeature::Profile p2;
    p2.sketch_id = EntityId();
    p2.blend_parameter = 1.0;
    loft.addProfile(p2);

    assert_test(loft.canGenerateSurface(), "Loft: Can generate surface with 2 profiles");
}

void test_loft_smooth_transition() {
    EntityId id;
    LoftFeature loft(id, "TestLoft");

    LoftFeature::Config config = loft.getConfig();
    assert_test(config.smooth_transition == true, "Loft: Default smooth_transition is true");
}

void test_loft_ruled_surface_mode() {
    EntityId id;
    LoftFeature loft(id, "TestLoft");

    LoftFeature::Config config = loft.getConfig();
    config.ruled_surface = true;
    loft.setConfig(config);

    assert_test(loft.getConfig().ruled_surface == true, "Loft: Set ruled surface mode");
}

void test_loft_closed_loft() {
    EntityId id;
    LoftFeature loft(id, "TestLoft");

    LoftFeature::Config config = loft.getConfig();
    config.closed = true;
    loft.setConfig(config);

    assert_test(loft.getConfig().closed == true, "Loft: Set closed loft");
}

// ========== Sweep Feature Tests ==========

void test_sweep_creation_basic() {
    EntityId id;
    SweepFeature sweep(id, "TestSweep");
    assert_test(sweep.getName() == "TestSweep", "Sweep: Creation with name");
}

void test_sweep_validate_no_profile() {
    EntityId id;
    SweepFeature sweep(id, "TestSweep");
    assert_test(!sweep.validate(), "Sweep: Validate rejects no profile");
}

void test_sweep_validate_no_path() {
    EntityId id;
    SweepFeature sweep(id, "TestSweep");

    SweepFeature::Config config = sweep.getConfig();
    config.profile_sketch = EntityId();
    config.path_entity = EntityId();
    sweep.setConfig(config);

    assert_test(!sweep.validate(), "Sweep: Validate rejects no path");
}

void test_sweep_has_valid_path() {
    EntityId id;
    SweepFeature sweep(id, "TestSweep");

    SweepFeature::Config config = sweep.getConfig();
    config.path_entity = EntityId();
    sweep.setConfig(config);

    assert_test(!sweep.hasValidPath(), "Sweep: No valid path");
}

void test_sweep_has_valid_profile() {
    EntityId id;
    SweepFeature sweep(id, "TestSweep");

    SweepFeature::Config config = sweep.getConfig();
    config.profile_sketch = EntityId();
    sweep.setConfig(config);

    assert_test(!sweep.hasValidProfile(), "Sweep: No valid profile");
}

void test_sweep_set_scale() {
    EntityId id;
    SweepFeature sweep(id, "TestSweep");

    sweep.setScale(1.0, 2.0);
    SweepFeature::Config config = sweep.getConfig();

    assert_test(config.scale_start == 1.0 && config.scale_end == 2.0,
                "Sweep: Set scale start and end");
}

void test_sweep_set_twist() {
    EntityId id;
    SweepFeature sweep(id, "TestSweep");

    sweep.setTwist(45.0);
    SweepFeature::Config config = sweep.getConfig();

    assert_test(config.twist_angle == 45.0, "Sweep: Set twist angle");
}

void test_sweep_negative_scale_invalid() {
    EntityId id;
    SweepFeature sweep(id, "TestSweep");

    SweepFeature::Config config = sweep.getConfig();
    config.profile_sketch = EntityId();
    config.path_entity = EntityId();
    config.scale_start = -1.0;  // Invalid
    config.scale_end = 1.0;
    sweep.setConfig(config);

    assert_test(!sweep.validate(), "Sweep: Validate rejects negative scale");
}

void test_sweep_zero_scale_invalid() {
    EntityId id;
    SweepFeature sweep(id, "TestSweep");

    SweepFeature::Config config = sweep.getConfig();
    config.profile_sketch = EntityId();
    config.path_entity = EntityId();
    config.scale_start = 0.0;  // Invalid
    config.scale_end = 1.0;
    sweep.setConfig(config);

    assert_test(!sweep.validate(), "Sweep: Validate rejects zero scale");
}

void test_sweep_keep_profile_normal() {
    EntityId id;
    SweepFeature sweep(id, "TestSweep");

    SweepFeature::Config config = sweep.getConfig();
    config.keep_profile_normal = false;
    sweep.setConfig(config);

    assert_test(sweep.getConfig().keep_profile_normal == false,
                "Sweep: Set keep_profile_normal to false");
}

// ========== Revolve Feature Tests ==========

void test_revolve_creation_basic() {
    EntityId id;
    RevolveFeature revolve(id, "TestRevolve");
    assert_test(revolve.getName() == "TestRevolve", "Revolve: Creation with name");
}

void test_revolve_default_axis_z() {
    EntityId id;
    RevolveFeature revolve(id, "TestRevolve");

    RevolveFeature::Config config = revolve.getConfig();
    assert_test(config.axis_type == RevolveFeature::AxisType::Z,
                "Revolve: Default axis is Z");
}

void test_revolve_set_axis_x() {
    EntityId id;
    RevolveFeature revolve(id, "TestRevolve");

    revolve.setAxis(RevolveFeature::AxisType::X);
    RevolveFeature::Config config = revolve.getConfig();

    assert_test(config.axis_type == RevolveFeature::AxisType::X &&
                config.axis_direction.x == 1.0,
                "Revolve: Set axis to X");
}

void test_revolve_set_axis_y() {
    EntityId id;
    RevolveFeature revolve(id, "TestRevolve");

    revolve.setAxis(RevolveFeature::AxisType::Y);
    RevolveFeature::Config config = revolve.getConfig();

    assert_test(config.axis_type == RevolveFeature::AxisType::Y &&
                config.axis_direction.y == 1.0,
                "Revolve: Set axis to Y");
}

void test_revolve_set_custom_axis() {
    EntityId id;
    RevolveFeature revolve(id, "TestRevolve");

    Vector3D origin(1.0, 2.0, 3.0);
    Vector3D direction(1.0, 0.0, 0.0);
    revolve.setCustomAxis(origin, direction);

    RevolveFeature::Config config = revolve.getConfig();
    assert_test(config.axis_type == RevolveFeature::AxisType::CUSTOM,
                "Revolve: Set custom axis");
}

void test_revolve_set_angle() {
    EntityId id;
    RevolveFeature revolve(id, "TestRevolve");

    revolve.setAngle(180.0);
    RevolveFeature::Config config = revolve.getConfig();

    assert_test(config.angle == 180.0, "Revolve: Set angle to 180");
}

void test_revolve_full_revolution() {
    EntityId id;
    RevolveFeature revolve(id, "TestRevolve");

    revolve.setAngle(360.0);
    assert_test(revolve.isFullRevolution(), "Revolve: 360 degree is full revolution");
}

void test_revolve_partial_revolution() {
    EntityId id;
    RevolveFeature revolve(id, "TestRevolve");

    revolve.setAngle(180.0);
    assert_test(!revolve.isFullRevolution(), "Revolve: 180 degree is not full revolution");
}

void test_revolve_set_thin_wall() {
    EntityId id;
    RevolveFeature revolve(id, "TestRevolve");

    revolve.setThinWall(2.5);
    RevolveFeature::Config config = revolve.getConfig();

    assert_test(config.thin_wall == true && config.thickness == 2.5,
                "Revolve: Set thin wall with thickness");
}

void test_revolve_validate_no_profile() {
    EntityId id;
    RevolveFeature revolve(id, "TestRevolve");

    assert_test(!revolve.validate(), "Revolve: Validate rejects no profile");
}

void test_revolve_validate_zero_angle() {
    EntityId id;
    RevolveFeature revolve(id, "TestRevolve");

    RevolveFeature::Config config = revolve.getConfig();
    config.profile_sketch = EntityId();
    config.angle = 0.0;  // Invalid
    revolve.setConfig(config);

    assert_test(!revolve.validate(), "Revolve: Validate rejects zero angle");
}

void test_revolve_validate_negative_angle() {
    EntityId id;
    RevolveFeature revolve(id, "TestRevolve");

    RevolveFeature::Config config = revolve.getConfig();
    config.profile_sketch = EntityId();
    config.angle = -45.0;  // Invalid
    revolve.setConfig(config);

    assert_test(!revolve.validate(), "Revolve: Validate rejects negative angle");
}

void test_revolve_validate_angle_over_360() {
    EntityId id;
    RevolveFeature revolve(id, "TestRevolve");

    RevolveFeature::Config config = revolve.getConfig();
    config.profile_sketch = EntityId();
    config.angle = 450.0;  // Invalid
    revolve.setConfig(config);

    assert_test(!revolve.validate(), "Revolve: Validate rejects angle over 360");
}

void test_revolve_bidirectional_full_revolution() {
    EntityId id;
    RevolveFeature revolve(id, "TestRevolve");

    RevolveFeature::Config config = revolve.getConfig();
    config.profile_sketch = EntityId();
    config.angle = 360.0;
    config.bidirectional = true;  // Invalid combination
    revolve.setConfig(config);

    assert_test(!revolve.validate(), "Revolve: Validate rejects bidirectional with 360 degrees");
}

void test_revolve_thin_wall_zero_thickness() {
    EntityId id;
    RevolveFeature revolve(id, "TestRevolve");

    RevolveFeature::Config config = revolve.getConfig();
    config.profile_sketch = EntityId();
    config.thin_wall = true;
    config.thickness = 0.0;  // Invalid
    revolve.setConfig(config);

    assert_test(!revolve.validate(), "Revolve: Validate rejects zero thickness");
}

// ========== Feature System Tests ==========

void test_feature_system_create_feature() {
    FeatureSystem system;
    EntityId id = system.createFeature(FeatureType::PAD, "TestPad");

    const Feature* feature = system.getFeature(id);
    assert_test(feature != nullptr && feature->name == "TestPad",
                "FeatureSystem: Create and retrieve feature");
}

void test_feature_system_add_constraint() {
    FeatureSystem system;
    EntityId featureId = system.createFeature(FeatureType::PAD, "TestPad");

    Constraint constraint;
    constraint.name = "Length";
    constraint.value = 10.0;
    constraint.isActive = true;

    bool added = system.addConstraint(featureId, constraint);
    assert_test(added, "FeatureSystem: Add constraint");
}

void test_feature_system_get_all_features() {
    FeatureSystem system;
    EntityId id1 = system.createFeature(FeatureType::PAD, "Pad1");
    EntityId id2 = system.createFeature(FeatureType::POCKET, "Pocket1");

    auto features = system.getAllFeatures();
    assert_test(features.size() >= 2, "FeatureSystem: Get all features");
}

void test_feature_system_set_parent() {
    FeatureSystem system;
    EntityId parentId = system.createFeature(FeatureType::PAD, "Parent");
    EntityId childId = system.createFeature(FeatureType::POCKET, "Child");

    bool success = system.setParent(childId, parentId);
    assert_test(success, "FeatureSystem: Set parent");

    const Feature* child = system.getFeature(childId);
    assert_test(child->parentId == parentId, "FeatureSystem: Parent relationship correct");
}

void test_feature_system_get_children() {
    FeatureSystem system;
    EntityId parentId = system.createFeature(FeatureType::PAD, "Parent");
    EntityId childId = system.createFeature(FeatureType::POCKET, "Child");

    system.setParent(childId, parentId);
    auto children = system.getChildren(parentId);

    assert_test(children.size() == 1 && children[0] == childId,
                "FeatureSystem: Get children");
}

void test_feature_system_suppress_feature() {
    FeatureSystem system;
    EntityId featureId = system.createFeature(FeatureType::PAD, "TestPad");

    bool success = system.suppressFeature(featureId);
    assert_test(success, "FeatureSystem: Suppress feature");

    const Feature* feature = system.getFeature(featureId);
    assert_test(feature->isSuppressed == true, "FeatureSystem: Feature is suppressed");
}

void test_feature_system_unsuppress_feature() {
    FeatureSystem system;
    EntityId featureId = system.createFeature(FeatureType::PAD, "TestPad");

    system.suppressFeature(featureId);
    bool success = system.unsuppressFeature(featureId);
    assert_test(success, "FeatureSystem: Unsuppress feature");

    const Feature* feature = system.getFeature(featureId);
    assert_test(feature->isSuppressed == false, "FeatureSystem: Feature is unsuppressed");
}

void test_feature_system_validate_tree() {
    FeatureSystem system;
    EntityId id1 = system.createFeature(FeatureType::PAD, "Pad1");
    EntityId id2 = system.createFeature(FeatureType::POCKET, "Pocket1");

    system.setParent(id2, id1);
    bool valid = system.validateFeatureTree();
    assert_test(valid, "FeatureSystem: Validate valid tree");
}

void test_feature_system_get_build_order() {
    FeatureSystem system;
    EntityId id1 = system.createFeature(FeatureType::SKETCH, "Sketch1");
    EntityId id2 = system.createFeature(FeatureType::PAD, "Pad1");
    EntityId id3 = system.createFeature(FeatureType::POCKET, "Pocket1");

    system.setParent(id2, id1);
    system.setParent(id3, id2);

    auto order = system.getBuildOrder();
    assert_test(order.size() == 3, "FeatureSystem: Get build order");
}

// ========== Main ==========

int main() {
    std::cout << "=== M5 Phase: Advanced Feature Operations Tests ===\n\n";

    // Loft tests
    std::cout << "Loft Feature Tests:\n";
    test_loft_creation_basic();
    test_loft_add_profile_single();
    test_loft_add_multiple_profiles();
    test_loft_validate_minimum_profiles();
    test_loft_validate_two_profiles();
    test_loft_validate_invalid_blend_parameter();
    test_loft_validate_out_of_order_profiles();
    test_loft_remove_profile();
    test_loft_can_generate_surface();
    test_loft_smooth_transition();
    test_loft_ruled_surface_mode();
    test_loft_closed_loft();

    std::cout << "\nSweep Feature Tests:\n";
    test_sweep_creation_basic();
    test_sweep_validate_no_profile();
    test_sweep_validate_no_path();
    test_sweep_has_valid_path();
    test_sweep_has_valid_profile();
    test_sweep_set_scale();
    test_sweep_set_twist();
    test_sweep_negative_scale_invalid();
    test_sweep_zero_scale_invalid();
    test_sweep_keep_profile_normal();

    std::cout << "\nRevolve Feature Tests:\n";
    test_revolve_creation_basic();
    test_revolve_default_axis_z();
    test_revolve_set_axis_x();
    test_revolve_set_axis_y();
    test_revolve_set_custom_axis();
    test_revolve_set_angle();
    test_revolve_full_revolution();
    test_revolve_partial_revolution();
    test_revolve_set_thin_wall();
    test_revolve_validate_no_profile();
    test_revolve_validate_zero_angle();
    test_revolve_validate_negative_angle();
    test_revolve_validate_angle_over_360();
    test_revolve_bidirectional_full_revolution();
    test_revolve_thin_wall_zero_thickness();

    std::cout << "\nFeature System Tests:\n";
    test_feature_system_create_feature();
    test_feature_system_add_constraint();
    test_feature_system_get_all_features();
    test_feature_system_set_parent();
    test_feature_system_get_children();
    test_feature_system_suppress_feature();
    test_feature_system_unsuppress_feature();
    test_feature_system_validate_tree();
    test_feature_system_get_build_order();

    std::cout << "\n=== Test Summary ===\n";
    std::cout << "Passed: " << tests_passed << "\n";
    std::cout << "Failed: " << tests_failed << "\n";
    std::cout << "Total:  " << (tests_passed + tests_failed) << "\n";

    return tests_failed == 0 ? 0 : 1;
}
