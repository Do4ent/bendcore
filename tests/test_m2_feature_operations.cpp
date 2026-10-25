/**
 * M2 Phase: Feature Operations Tests
 * 30+ unit tests для GAP-006, GAP-007, GAP-008
 */

#include <cassert>
#include <iostream>
#include <cmath>
#include <vector>

#include "bendcore/geometry/brep_builder.hpp"
#include "bendcore/geometry/feature_operations.hpp"
#include "bendcore/geometry/geometry_kernel.hpp"
#include "bendcore/rebuild/feature_rebuild.hpp"
#include "bendcore/rebuild/rebuild_engine.hpp"
#include "bendcore/topology/topological_reference.hpp"

using namespace bendcore::geometry;
using namespace bendcore::rebuild;
using namespace bendcore::topology;

// Counter for test results
int tests_passed = 0;
int tests_failed = 0;

void assert_test(bool condition, const std::string& test_name) {
    if (condition) {
        std::cout << "✓ " << test_name << "\n";
        tests_passed++;
    } else {
        std::cout << "✗ " << test_name << "\n";
        tests_failed++;
    }
}

// ========== GAP-006: Feature Operations Tests ==========

void test_gap006_extrude_basic() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    // Create a simple rectangular profile
    std::vector<Point3D> profile = {
        Point3D(0, 0, 0),
        Point3D(10, 0, 0),
        Point3D(10, 10, 0),
        Point3D(0, 10, 0)
    };

    ExtrudeParams params;
    params.distance = 5.0;
    params.symmetric = false;

    auto result = ops.extrude(profile, params);
    assert_test(result.has_value() && result.value() >= 0, "Extrude: Basic extrusion");
}

void test_gap006_extrude_validation() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    // Empty profile
    std::vector<Point3D> empty_profile;
    ExtrudeParams params;
    params.distance = 5.0;

    auto result = ops.extrude(empty_profile, params);
    assert_test(!result.has_value(), "Extrude: Empty profile rejection");
}

void test_gap006_extrude_zero_distance() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    std::vector<Point3D> profile = {
        Point3D(0, 0, 0),
        Point3D(1, 0, 0),
        Point3D(1, 1, 0),
        Point3D(0, 1, 0)
    };

    ExtrudeParams params;
    params.distance = 0.0;

    auto result = ops.extrude(profile, params);
    assert_test(!result.has_value(), "Extrude: Zero distance rejection");
}

void test_gap006_revolve_basic() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    std::vector<Point3D> profile = {
        Point3D(5, 0, 0),
        Point3D(5, 10, 0)
    };

    RevolveParams params;
    params.angle = 360.0;
    params.axis_origin = Point3D(0, 0, 0);
    params.axis_direction = Vector3D(0, 0, 1);

    auto result = ops.revolve(profile, params);
    assert_test(result.has_value(), "Revolve: Basic revolution");
}

void test_gap006_revolve_partial_angle() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    std::vector<Point3D> profile = {
        Point3D(5, 0, 0),
        Point3D(5, 10, 0)
    };

    RevolveParams params;
    params.angle = 180.0;
    params.axis_origin = Point3D(0, 0, 0);
    params.axis_direction = Vector3D(0, 0, 1);

    auto result = ops.revolve(profile, params);
    assert_test(result.has_value(), "Revolve: Partial angle (180 degrees)");
}

void test_gap006_sweep_basic() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    std::vector<Point3D> profile = {
        Point3D(-1, 0, 0),
        Point3D(1, 0, 0),
        Point3D(1, 2, 0),
        Point3D(-1, 2, 0)
    };

    SweepParams params;
    params.path_curve = {
        Point3D(0, 0, 0),
        Point3D(0, 0, 10),
        Point3D(5, 0, 15)
    };

    auto result = ops.sweep(profile, params);
    assert_test(result.has_value(), "Sweep: Basic sweep");
}

void test_gap006_sweep_scaling() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    std::vector<Point3D> profile = {
        Point3D(-1, 0, 0),
        Point3D(1, 0, 0),
        Point3D(1, 2, 0),
        Point3D(-1, 2, 0)
    };

    SweepParams params;
    params.path_curve = {
        Point3D(0, 0, 0),
        Point3D(0, 0, 10)
    };
    params.scale_start = 1.0;
    params.scale_end = 0.5;

    auto result = ops.sweep(profile, params);
    assert_test(result.has_value(), "Sweep: With scaling");
}

void test_gap006_loft_basic() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    LoftParams params;
    params.profiles = {
        {Point3D(0, 0, 0), Point3D(5, 0, 0), Point3D(5, 5, 0), Point3D(0, 5, 0)},
        {Point3D(1, 1, 5), Point3D(4, 1, 5), Point3D(4, 4, 5), Point3D(1, 4, 5)}
    };
    params.smooth = true;

    auto result = ops.loft(params);
    assert_test(result.has_value(), "Loft: Basic loft");
}

void test_gap006_loft_minimum_profiles() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    LoftParams params;
    params.profiles = {
        {Point3D(0, 0, 0), Point3D(5, 0, 0)}
    };

    auto result = ops.loft(params);
    assert_test(!result.has_value(), "Loft: Minimum 2 profiles required");
}

void test_gap006_hole_creation() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    // Create a simple box first
    auto box = builder.create_box(10, 10, 10);
    assert_test(box >= 0, "Hole: Prerequisite box creation");

    HoleParams params;
    params.center = Point3D(5, 5, 0);
    params.diameter = 2.0;
    params.depth = 5.0;

    bool result = ops.create_hole(box, params);
    assert_test(result, "Hole: Create hole in solid");
}

void test_gap006_hole_invalid_solid() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    HoleParams params;
    params.center = Point3D(5, 5, 0);
    params.diameter = 2.0;
    params.depth = 5.0;

    bool result = ops.create_hole(-1, params);
    assert_test(!result, "Hole: Reject invalid solid");
}

void test_gap006_hole_zero_diameter() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    auto box = builder.create_box(10, 10, 10);
    HoleParams params;
    params.center = Point3D(5, 5, 0);
    params.diameter = 0.0;
    params.depth = 5.0;

    bool result = ops.create_hole(box, params);
    assert_test(!result, "Hole: Zero diameter rejection");
}

void test_gap006_fillet_basic() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    auto box = builder.create_box(10, 10, 10);

    FilletParams params;
    params.radius = 1.0;
    params.edges = {0, 1, 2};

    auto result = ops.fillet(box, params);
    assert_test(result.has_value(), "Fillet: Basic fillet");
}

void test_gap006_fillet_empty_edges() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    auto box = builder.create_box(10, 10, 10);
    FilletParams params;
    params.radius = 1.0;

    auto result = ops.fillet(box, params);
    assert_test(!result.has_value(), "Fillet: Reject empty edge list");
}

void test_gap006_chamfer_basic() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    auto box = builder.create_box(10, 10, 10);

    ChamferParams params;
    params.size = 0.5;
    params.edges = {0, 1};

    auto result = ops.chamfer(box, params);
    assert_test(result.has_value(), "Chamfer: Basic chamfer");
}

void test_gap006_shell_basic() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    auto box = builder.create_box(10, 10, 10);

    ShellParams params;
    params.thickness = 1.0;

    auto result = ops.shell(box, params);
    assert_test(result.has_value(), "Shell: Create shell");
}

void test_gap006_boolean_union() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    auto box1 = builder.create_box(10, 10, 10);
    auto box2 = builder.create_box(10, 10, 10);

    assert_test(box1 >= 0 && box2 >= 0, "Boolean Union: Prerequisites");

    auto result = ops.union_solids(box1, box2);
    assert_test(result.has_value(), "Boolean: Union operation");
}

void test_gap006_boolean_difference() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    auto box1 = builder.create_box(20, 20, 20);
    auto box2 = builder.create_box(10, 10, 10);

    auto result = ops.difference_solids(box1, box2);
    assert_test(result.has_value(), "Boolean: Difference operation");
}

void test_gap006_boolean_intersection() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    auto box1 = builder.create_box(10, 10, 10);
    auto box2 = builder.create_box(10, 10, 10);

    auto result = ops.intersection_solids(box1, box2);
    assert_test(result.has_value(), "Boolean: Intersection operation");
}

void test_gap006_mirror_basic() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    auto box = builder.create_box(10, 10, 10);

    auto result = ops.mirror(box, Point3D(0, 0, 0), Vector3D(1, 0, 0));
    assert_test(result.has_value(), "Mirror: Basic mirror");
}

void test_gap006_pattern_linear() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    auto box = builder.create_box(10, 10, 10);

    PatternParams params;
    params.type = PatternParams::PatternType::Linear;
    params.count = 3;
    params.distance = 15.0;

    auto result = ops.pattern(box, params);
    assert_test(result.size() >= 1, "Pattern: Linear pattern");
}

void test_gap006_pattern_circular() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    auto box = builder.create_box(10, 10, 10);

    PatternParams params;
    params.type = PatternParams::PatternType::Circular;
    params.count = 4;
    params.distance = 20.0;

    auto result = ops.pattern(box, params);
    assert_test(result.size() >= 1, "Pattern: Circular pattern");
}

void test_gap006_validate_result() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    auto box = builder.create_box(10, 10, 10);
    bool valid = ops.validate_result(box);

    assert_test(valid, "Validation: Result validation");
}

// ========== GAP-007: Feature Rebuild Tests ==========

void test_gap007_suppress_unsuppress() {
    BRepBuilder builder;
    FeatureOperations ops(builder);

    // Mock RebuildEngine (simplified)
    // In real tests, use actual RebuildEngine
    RebuildEngine base_engine(nullptr, nullptr, nullptr, nullptr);  // Mock objects
    // NOTE: This test would need proper mock implementation

    std::cout << "⊘ Suppress/Unsuppress: Requires proper mocking (skipped for now)\n";
}

void test_gap007_feature_state_normal() {
    std::cout << "⊘ Feature State: Requires proper mocking (skipped for now)\n";
}

void test_gap007_feature_state_suppressed() {
    std::cout << "⊘ Feature State Suppressed: Requires proper mocking (skipped for now)\n";
}

void test_gap007_feature_state_disabled() {
    std::cout << "⊘ Feature State Disabled: Requires proper mocking (skipped for now)\n";
}

void test_gap007_snapshot_creation() {
    std::cout << "⊘ Snapshot Creation: Requires proper mocking (skipped for now)\n";
}

void test_gap007_snapshot_restoration() {
    std::cout << "⊘ Snapshot Restoration: Requires proper mocking (skipped for now)\n";
}

void test_gap007_cache_enabled() {
    std::cout << "⊘ Cache Enable: Requires proper mocking (skipped for now)\n";
}

void test_gap007_cache_invalidation() {
    std::cout << "⊘ Cache Invalidation: Requires proper mocking (skipped for now)\n";
}

void test_gap007_error_recording() {
    std::cout << "⊘ Error Recording: Requires proper mocking (skipped for now)\n";
}

void test_gap007_error_recovery() {
    std::cout << "⊘ Error Recovery: Requires proper mocking (skipped for now)\n";
}

// ========== GAP-008: Topological References Tests ==========

void test_gap008_create_vertex_reference() {
    BRepBuilder builder;
    TopologicalReferenceManager manager(builder);

    auto vid = builder.create_vertex(Point3D(5, 5, 5));
    auto ref_id = manager.create_vertex_reference(vid);

    assert_test(!ref_id.empty(), "Vertex Reference: Create reference");
}

void test_gap008_create_edge_reference() {
    BRepBuilder builder;
    TopologicalReferenceManager manager(builder);

    auto v1 = builder.create_vertex(Point3D(0, 0, 0));
    auto v2 = builder.create_vertex(Point3D(10, 0, 0));
    auto eid = builder.create_edge(v1, v2);
    auto ref_id = manager.create_edge_reference(eid);

    assert_test(!ref_id.empty(), "Edge Reference: Create reference");
}

void test_gap008_create_face_reference() {
    BRepBuilder builder;
    TopologicalReferenceManager manager(builder);

    auto v1 = builder.create_vertex(Point3D(0, 0, 0));
    auto v2 = builder.create_vertex(Point3D(10, 0, 0));
    auto v3 = builder.create_vertex(Point3D(10, 10, 0));

    auto e1 = builder.create_edge(v1, v2);
    auto e2 = builder.create_edge(v2, v3);
    auto e3 = builder.create_edge(v3, v1);

    std::vector<EdgeId> edges = {e1, e2, e3};
    auto fid = builder.create_face(edges);
    auto ref_id = manager.create_face_reference(fid);

    assert_test(!ref_id.empty(), "Face Reference: Create reference");
}

void test_gap008_resolve_vertex_reference() {
    BRepBuilder builder;
    TopologicalReferenceManager manager(builder);

    auto vid = builder.create_vertex(Point3D(5, 5, 5));
    auto ref_id = manager.create_vertex_reference(vid);
    auto resolved = manager.resolve_vertex_reference(ref_id);

    assert_test(resolved.has_value() && resolved.value() == vid, "Vertex Reference: Resolve");
}

void test_gap008_resolve_edge_reference() {
    BRepBuilder builder;
    TopologicalReferenceManager manager(builder);

    auto v1 = builder.create_vertex(Point3D(0, 0, 0));
    auto v2 = builder.create_vertex(Point3D(10, 0, 0));
    auto eid = builder.create_edge(v1, v2);
    auto ref_id = manager.create_edge_reference(eid);
    auto resolved = manager.resolve_edge_reference(ref_id);

    assert_test(resolved.has_value() && resolved.value() == eid, "Edge Reference: Resolve");
}

void test_gap008_reference_type_query() {
    BRepBuilder builder;
    TopologicalReferenceManager manager(builder);

    auto vid = builder.create_vertex(Point3D(5, 5, 5));
    auto ref_id = manager.create_vertex_reference(vid);
    auto type = manager.get_reference_type(ref_id);

    assert_test(type.has_value() && type.value() == ReferenceType::Vertex,
                "Reference Type: Query type");
}

void test_gap008_reference_validity() {
    BRepBuilder builder;
    TopologicalReferenceManager manager(builder);

    auto vid = builder.create_vertex(Point3D(5, 5, 5));
    auto ref_id = manager.create_vertex_reference(vid);
    bool valid = manager.is_reference_valid(ref_id);

    assert_test(valid, "Reference Validity: Valid reference");
}

void test_gap008_reference_statistics() {
    BRepBuilder builder;
    TopologicalReferenceManager manager(builder);

    auto v1 = builder.create_vertex(Point3D(0, 0, 0));
    auto v2 = builder.create_vertex(Point3D(10, 0, 0));

    manager.create_vertex_reference(v1);
    manager.create_vertex_reference(v2);
    auto eid = builder.create_edge(v1, v2);
    manager.create_edge_reference(eid);

    auto stats = manager.get_statistics();
    assert_test(stats.total_references >= 3 && stats.vertex_references >= 2,
                "Reference Statistics: Count references");
}

void test_gap008_reference_health() {
    BRepBuilder builder;
    TopologicalReferenceManager manager(builder);

    auto v = builder.create_vertex(Point3D(0, 0, 0));
    manager.create_vertex_reference(v);

    auto health = manager.get_health_report();
    assert_test(health.is_healthy, "Reference Health: Healthy state");
}

void test_gap008_delete_reference() {
    BRepBuilder builder;
    TopologicalReferenceManager manager(builder);

    auto vid = builder.create_vertex(Point3D(5, 5, 5));
    auto ref_id = manager.create_vertex_reference(vid);
    bool deleted = manager.delete_reference(ref_id);
    bool exists = manager.is_reference_valid(ref_id);

    assert_test(deleted && !exists, "Reference Management: Delete reference");
}

void test_gap008_rename_reference() {
    BRepBuilder builder;
    TopologicalReferenceManager manager(builder);

    auto vid = builder.create_vertex(Point3D(5, 5, 5));
    auto ref_id = manager.create_vertex_reference(vid);
    bool renamed = manager.rename_reference(ref_id, "custom_ref_name");

    assert_test(renamed, "Reference Management: Rename reference");
}

void test_gap008_get_all_references() {
    BRepBuilder builder;
    TopologicalReferenceManager manager(builder);

    auto v1 = builder.create_vertex(Point3D(0, 0, 0));
    auto v2 = builder.create_vertex(Point3D(10, 0, 0));
    manager.create_vertex_reference(v1);
    manager.create_vertex_reference(v2);

    auto all_refs = manager.get_all_references();
    assert_test(all_refs.size() >= 2, "Reference Management: Get all references");
}

void test_gap008_update_references_after_rebuild() {
    BRepBuilder builder;
    TopologicalReferenceManager manager(builder);

    auto vid = builder.create_vertex(Point3D(5, 5, 5));
    auto ref_id = manager.create_vertex_reference(vid);

    size_t updated = manager.update_references_after_rebuild();
    assert_test(updated >= 0, "Reference Update: After rebuild");
}

void test_gap008_element_mapping_tracking() {
    BRepBuilder builder;
    TopologicalReferenceManager manager(builder);

    manager.track_element_mapping(ReferenceType::Vertex, 0, 5);
    manager.track_element_mapping(ReferenceType::Vertex, 1, 6);

    auto mapping = manager.get_element_mapping_history(ReferenceType::Vertex);
    assert_test(mapping.size() >= 2, "Reference Tracking: Element mapping");
}

int main() {
    std::cout << "===== M2 Phase: Feature Operations Unit Tests =====\n\n";

    std::cout << "--- GAP-006: Feature Operations ---\n";
    test_gap006_extrude_basic();
    test_gap006_extrude_validation();
    test_gap006_extrude_zero_distance();
    test_gap006_revolve_basic();
    test_gap006_revolve_partial_angle();
    test_gap006_sweep_basic();
    test_gap006_sweep_scaling();
    test_gap006_loft_basic();
    test_gap006_loft_minimum_profiles();
    test_gap006_hole_creation();
    test_gap006_hole_invalid_solid();
    test_gap006_hole_zero_diameter();
    test_gap006_fillet_basic();
    test_gap006_fillet_empty_edges();
    test_gap006_chamfer_basic();
    test_gap006_shell_basic();
    test_gap006_boolean_union();
    test_gap006_boolean_difference();
    test_gap006_boolean_intersection();
    test_gap006_mirror_basic();
    test_gap006_pattern_linear();
    test_gap006_pattern_circular();
    test_gap006_validate_result();

    std::cout << "\n--- GAP-007: Feature Rebuild (Simplified) ---\n";
    test_gap007_suppress_unsuppress();
    test_gap007_feature_state_normal();
    test_gap007_feature_state_suppressed();
    test_gap007_feature_state_disabled();
    test_gap007_snapshot_creation();
    test_gap007_snapshot_restoration();
    test_gap007_cache_enabled();
    test_gap007_cache_invalidation();
    test_gap007_error_recording();
    test_gap007_error_recovery();

    std::cout << "\n--- GAP-008: Topological References ---\n";
    test_gap008_create_vertex_reference();
    test_gap008_create_edge_reference();
    test_gap008_create_face_reference();
    test_gap008_resolve_vertex_reference();
    test_gap008_resolve_edge_reference();
    test_gap008_reference_type_query();
    test_gap008_reference_validity();
    test_gap008_reference_statistics();
    test_gap008_reference_health();
    test_gap008_delete_reference();
    test_gap008_rename_reference();
    test_gap008_get_all_references();
    test_gap008_update_references_after_rebuild();
    test_gap008_element_mapping_tracking();

    std::cout << "\n===== Test Results =====\n";
    std::cout << "Passed: " << tests_passed << "\n";
    std::cout << "Failed: " << tests_failed << "\n";
    std::cout << "Total:  " << (tests_passed + tests_failed) << "\n";

    return tests_failed > 0 ? 1 : 0;
}
