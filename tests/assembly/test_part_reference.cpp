// Part References system tests
#include "bendcore/assembly/part_reference.hpp"

#include <cstdio>
#include <string>

using namespace bendcore::assembly;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

template <class F>
static bool throws(F f) {
    try {
        f();
    } catch (const ReferenceError&) {
        return true;
    }
    return false;
}

// ========== PartReference Tests ==========

static void test_part_reference_creation() {
    PartReference ref("ref_001", "part_A");
    check(ref.getReferenceId() == "ref_001", "reference ID set correctly");
    check(ref.getReferencedPartId() == "part_A", "referenced part ID set correctly");
    check(ref.isValid(), "reference is valid on creation");
    check(ref.getVersion() == 1, "initial version is 1");
    check(!ref.getCreationTime().time_since_epoch().count() == 0, "creation time is set");
}

static void test_part_reference_invalid_id() {
    check(throws([&] { PartReference("", "part_A"); }), "empty reference ID throws");
    check(throws([&] { PartReference("ref_001", ""); }), "empty referenced part ID throws");
}

static void test_part_reference_update() {
    PartReference ref("ref_001", "part_A");
    int initial_version = ref.getVersion();
    ref.updateReferencedPart("part_B", "user1");
    check(ref.getReferencedPartId() == "part_B", "referenced part updated");
    check(ref.getVersion() == initial_version + 1, "version incremented");
    check(ref.getMetadata().last_modifier == "user1", "modifier recorded");
}

static void test_part_reference_metadata() {
    PartReference ref("ref_001", "part_A", "creator1");
    check(ref.getMetadata().creator == "creator1", "creator set in metadata");
    check(ref.getMetadata().version == 1, "version in metadata");
    check(!ref.getMetadata().custom_properties.empty() == false, "custom properties empty initially");
}

static void test_part_reference_custom_properties() {
    PartReference ref("ref_001", "part_A");
    ref.setCustomProperty("key1", "value1");
    check(ref.getCustomProperty("key1").value_or("") == "value1", "custom property set and retrieved");

    auto non_existent = ref.getCustomProperty("non_existent");
    check(!non_existent.has_value(), "non-existent property returns empty");

    ref.setCustomProperty("key2", "value2");
    check(ref.getCustomProperty("key1").value_or("") == "value1", "first property still exists");
}

static void test_part_reference_invalidate() {
    PartReference ref("ref_001", "part_A");
    check(ref.isValid(), "reference valid initially");
    ref.invalidate();
    check(!ref.isValid(), "reference invalid after invalidate");
}

// ========== ReferenceGraph Tests ==========

static void test_reference_graph_add_node() {
    ReferenceGraph graph;
    graph.addNode("part_A");
    check(!graph.isEmpty(), "graph not empty after adding node");
    check(graph.getNode("part_A").has_value(), "node retrieved successfully");
}

static void test_reference_graph_add_node_invalid() {
    ReferenceGraph graph;
    check(throws([&] { graph.addNode(""); }), "empty part ID throws");
}

static void test_reference_graph_add_edge() {
    ReferenceGraph graph;
    bool added = graph.addEdge("part_A", "part_B");
    check(added, "edge added successfully");
    check(graph.getReferencedParts("part_A").size() == 1, "referenced parts list updated");
    check(graph.getReferencingParts("part_B").size() == 1, "referencing parts list updated");
}

static void test_reference_graph_self_reference() {
    ReferenceGraph graph;
    check(throws([&] { graph.addEdge("part_A", "part_A"); }), "self-reference throws");
}

static void test_reference_graph_circular_detection() {
    ReferenceGraph graph;
    graph.addEdge("part_A", "part_B");
    graph.addEdge("part_B", "part_C");
    check(!graph.wouldCreateCycle("part_A", "part_B"), "no cycle for simple edge");
    check(graph.wouldCreateCycle("part_C", "part_A"), "cycle detected");
}

static void test_reference_graph_prevent_cycle() {
    ReferenceGraph graph;
    graph.addEdge("part_A", "part_B");
    graph.addEdge("part_B", "part_C");
    bool result = graph.addEdge("part_C", "part_A");
    check(!result, "cycle-creating edge rejected");
}

static void test_reference_graph_remove_edge() {
    ReferenceGraph graph;
    graph.addEdge("part_A", "part_B");
    graph.removeEdge("part_A", "part_B");
    check(graph.getReferencedParts("part_A").empty(), "edge removed from referenced parts");
    check(graph.getReferencingParts("part_B").empty(), "edge removed from referencing parts");
}

static void test_reference_graph_remove_node() {
    ReferenceGraph graph;
    graph.addEdge("part_A", "part_B");
    graph.addEdge("part_B", "part_C");
    graph.removeNode("part_B");
    check(graph.getNode("part_B") == std::nullopt, "node removed");
    check(graph.getReferencedParts("part_A").empty(), "edges to removed node cleared");
}

static void test_reference_graph_get_all_nodes() {
    ReferenceGraph graph;
    graph.addNode("part_A");
    graph.addNode("part_B");
    graph.addNode("part_C");
    auto nodes = graph.getAllNodes();
    check(nodes.size() == 3, "all nodes retrieved");
}

static void test_reference_graph_hierarchy() {
    ReferenceGraph graph;
    graph.addEdge("part_A", "part_B");
    graph.addEdge("part_B", "part_C");
    graph.addEdge("part_A", "part_D");
    auto hierarchy = graph.getHierarchy("part_A");
    check(hierarchy.size() == 4, "hierarchy includes all reachable parts");
    check(hierarchy[0] == "part_A", "root is first in hierarchy");
}

static void test_reference_graph_statistics() {
    ReferenceGraph graph;
    graph.addEdge("part_A", "part_B");
    graph.addEdge("part_B", "part_C");
    graph.addNode("part_D");
    auto stats = graph.getStatistics();
    check(stats.total_nodes == 4, "node count in statistics");
    check(stats.total_edges == 2, "edge count in statistics");
}

static void test_reference_graph_nesting_depth() {
    ReferenceGraph graph;
    graph.addEdge("part_A", "part_B");
    graph.addEdge("part_B", "part_C");
    graph.addEdge("part_C", "part_D");
    auto node_a = graph.getNode("part_A");
    check(node_a.has_value() && node_a->nesting_depth == 3, "nesting depth calculated correctly");
}

static void test_reference_graph_usage_count() {
    ReferenceGraph graph;
    graph.addEdge("part_A", "part_B");
    graph.addEdge("part_C", "part_B");
    auto node_b = graph.getNode("part_B");
    check(node_b.has_value() && node_b->usage_count == 2, "usage count reflects referencing parts");
}

static void test_reference_graph_clear() {
    ReferenceGraph graph;
    graph.addNode("part_A");
    graph.addNode("part_B");
    graph.clear();
    check(graph.isEmpty(), "graph empty after clear");
}

// ========== ReferenceValidator Tests ==========

static void test_validator_valid_reference() {
    PartReference ref("ref_001", "part_A");
    auto result = ReferenceValidator::validateReference(ref);
    check(result.is_valid, "valid reference passes validation");
    check(result.errors.empty(), "no errors for valid reference");
}

static void test_validator_invalid_reference() {
    PartReference ref("ref_001", "part_A");
    ref.invalidate();
    auto result = ReferenceValidator::validateReference(ref);
    check(!result.is_valid, "invalid reference fails validation");
    check(!result.errors.empty(), "errors reported for invalid reference");
}

static void test_validator_circular_detection() {
    ReferenceGraph graph;
    graph.addEdge("part_A", "part_B");
    graph.addEdge("part_B", "part_C");
    // Try to add edge that would create cycle - should be prevented
    bool would_cycle = graph.wouldCreateCycle("part_C", "part_A");
    check(would_cycle, "cycle creation detected");
    bool added = graph.addEdge("part_C", "part_A");
    check(!added, "cycle-creating edge rejected");
}

static void test_validator_find_circular_chains() {
    ReferenceGraph graph;
    graph.addEdge("part_A", "part_B");
    graph.addEdge("part_B", "part_C");
    // Don't create actual cycle to test circular chain detection
    auto chains = ReferenceValidator::findCircularChains(graph);
    // No cycles yet
    check(chains.empty(), "no circular chains in acyclic graph");
}

// ========== ReferenceResolver Tests ==========

static void test_resolver_resolve_reference() {
    PartReference ref("ref_001", "part_B");
    auto resolved = ReferenceResolver::resolvePartReference(ref);
    check(resolved == "part_B", "reference resolved to correct part");
}

static void test_resolver_resolve_invalid_reference() {
    PartReference ref("ref_001", "part_B");
    ref.invalidate();
    auto resolved = ReferenceResolver::resolvePartReference(ref);
    check(resolved.empty(), "invalid reference resolves to empty");
}

static void test_resolver_get_referenced_parts() {
    ReferenceGraph graph;
    graph.addEdge("part_A", "part_B");
    graph.addEdge("part_A", "part_C");
    auto refs = ReferenceResolver::getReferencedParts("part_A", graph);
    check(refs.size() == 2, "referenced parts retrieved correctly");
}

static void test_resolver_get_referencing_parts() {
    ReferenceGraph graph;
    graph.addEdge("part_A", "part_C");
    graph.addEdge("part_B", "part_C");
    auto refs = ReferenceResolver::getReferencingParts("part_C", graph);
    check(refs.size() == 2, "referencing parts retrieved correctly");
}

static void test_resolver_get_dependency_chain() {
    ReferenceGraph graph;
    graph.addEdge("part_A", "part_B");
    graph.addEdge("part_B", "part_C");
    graph.addEdge("part_C", "part_D");
    auto chain = ReferenceResolver::getDependencyChain("part_A", graph);
    check(chain.size() == 4, "dependency chain includes all parts");
}

// ========== ReferenceRegistry Tests ==========

static void test_registry_create_reference() {
    ReferenceRegistry registry;
    registry.createReference("ref_001", "part_A", "part_B");
    auto ref = registry.getReference("ref_001");
    check(ref.getReferenceId() == "ref_001", "reference created and retrieved");
}

static void test_registry_duplicate_reference() {
    ReferenceRegistry registry;
    registry.createReference("ref_001", "part_A", "part_B");
    check(throws([&] { registry.createReference("ref_001", "part_A", "part_C"); }),
          "duplicate reference throws");
}

static void test_registry_get_all_references() {
    ReferenceRegistry registry;
    registry.createReference("ref_001", "part_A", "part_B");
    registry.createReference("ref_002", "part_C", "part_D");
    auto refs = registry.getAllReferences();
    check(refs.size() == 2, "all references retrieved");
}

static void test_registry_get_references_for_part() {
    ReferenceRegistry registry;
    registry.createReference("ref_001", "part_A", "part_B");
    registry.createReference("ref_002", "part_C", "part_B");
    registry.createReference("ref_003", "part_D", "part_E");
    auto refs = registry.getReferencesForPart("part_B");
    check(refs.size() == 2, "references for specific part retrieved");
}

static void test_registry_update_reference() {
    ReferenceRegistry registry;
    registry.createReference("ref_001", "part_A", "part_B");
    registry.updateReference("ref_001", "part_C");
    auto ref = registry.getReference("ref_001");
    check(ref.getReferencedPartId() == "part_C", "reference updated");
}

static void test_registry_remove_reference() {
    ReferenceRegistry registry;
    registry.createReference("ref_001", "part_A", "part_B");
    registry.removeReference("ref_001");
    check(throws([&] { registry.getReference("ref_001"); }), "reference removed");
}

static void test_registry_validate_all() {
    ReferenceRegistry registry;
    registry.createReference("ref_001", "part_A", "part_B");
    registry.createReference("ref_002", "part_B", "part_C");
    auto result = registry.validateAll();
    check(result.is_valid, "valid registry passes validation");
}

static void test_registry_get_statistics() {
    ReferenceRegistry registry;
    registry.createReference("ref_001", "part_A", "part_B");
    registry.createReference("ref_002", "part_B", "part_C");
    auto stats = registry.getStatistics();
    check(stats.total_nodes > 0, "statistics contain node count");
    check(stats.total_edges > 0, "statistics contain edge count");
}

static void test_registry_clear() {
    ReferenceRegistry registry;
    registry.createReference("ref_001", "part_A", "part_B");
    registry.clear();
    auto refs = registry.getAllReferences();
    check(refs.empty(), "registry empty after clear");
}

// ========== Edge Case Tests ==========

static void test_deep_nesting() {
    ReferenceGraph graph;
    // Create a chain: A -> B -> C -> D -> E -> F
    graph.addEdge("part_A", "part_B");
    graph.addEdge("part_B", "part_C");
    graph.addEdge("part_C", "part_D");
    graph.addEdge("part_D", "part_E");
    graph.addEdge("part_E", "part_F");

    auto stats = graph.getStatistics();
    check(stats.max_depth == 5, "deep nesting depth calculated correctly");

    auto hierarchy = graph.getHierarchy("part_A");
    check(hierarchy.size() == 6, "entire chain in hierarchy");
}

static void test_diamond_dependency() {
    ReferenceGraph graph;
    // Diamond: A -> B,C; B,C -> D
    graph.addEdge("part_A", "part_B");
    graph.addEdge("part_A", "part_C");
    graph.addEdge("part_B", "part_D");
    graph.addEdge("part_C", "part_D");

    auto node_d = graph.getNode("part_D");
    check(node_d.has_value() && node_d->usage_count == 2, "diamond dependency usage count");

    auto hierarchy = graph.getHierarchy("part_A");
    check(hierarchy.size() == 4, "diamond hierarchy has all nodes");
}

static void test_multiple_independent_chains() {
    ReferenceGraph graph;
    // Chain 1: A -> B -> C
    graph.addEdge("part_A", "part_B");
    graph.addEdge("part_B", "part_C");

    // Chain 2: D -> E -> F
    graph.addEdge("part_D", "part_E");
    graph.addEdge("part_E", "part_F");

    auto stats = graph.getStatistics();
    check(stats.total_nodes == 6, "multiple chains node count");
    check(stats.total_edges == 4, "multiple chains edge count");
}

int main() {
    // PartReference tests
    test_part_reference_creation();
    test_part_reference_invalid_id();
    test_part_reference_update();
    test_part_reference_metadata();
    test_part_reference_custom_properties();
    test_part_reference_invalidate();

    // ReferenceGraph tests
    test_reference_graph_add_node();
    test_reference_graph_add_node_invalid();
    test_reference_graph_add_edge();
    test_reference_graph_self_reference();
    test_reference_graph_circular_detection();
    test_reference_graph_prevent_cycle();
    test_reference_graph_remove_edge();
    test_reference_graph_remove_node();
    test_reference_graph_get_all_nodes();
    test_reference_graph_hierarchy();
    test_reference_graph_statistics();
    test_reference_graph_nesting_depth();
    test_reference_graph_usage_count();
    test_reference_graph_clear();

    // ReferenceValidator tests
    test_validator_valid_reference();
    test_validator_invalid_reference();
    test_validator_circular_detection();
    test_validator_find_circular_chains();

    // ReferenceResolver tests
    test_resolver_resolve_reference();
    test_resolver_resolve_invalid_reference();
    test_resolver_get_referenced_parts();
    test_resolver_get_referencing_parts();
    test_resolver_get_dependency_chain();

    // ReferenceRegistry tests
    test_registry_create_reference();
    test_registry_duplicate_reference();
    test_registry_get_all_references();
    test_registry_get_references_for_part();
    test_registry_update_reference();
    test_registry_remove_reference();
    test_registry_validate_all();
    test_registry_get_statistics();
    test_registry_clear();

    // Edge case tests
    test_deep_nesting();
    test_diamond_dependency();
    test_multiple_independent_chains();

    if (failures) {
        std::printf("part_reference: %d FAIL\n", failures);
        return 1;
    }
    std::printf("part_reference: OK\n");
    return 0;
}
