#include <cassert>
#include <iostream>
#include <memory>

#include "bendcore/commands/document_event_bus.hpp"
#include "bendcore/dependencies/dependency_graph.hpp"
#include "bendcore/document/cad_document.hpp"

using namespace bendcore::document;
using namespace bendcore::commands;
using namespace bendcore::dependencies;

int main() {
    std::cout << "Testing DependencyGraph...\n";

    // Setup
    auto doc = std::make_unique<CadDocument>();
    auto bus = std::make_unique<DocumentEventBus>();
    auto graph = std::make_unique<DependencyGraph>(*doc, *bus);

    // Create test entities
    Part part1;
    part1.id = EntityId(EntityId::Type::Part);
    part1.name = "Part1";
    auto entity1 = doc->add_entity(part1);

    Part part2;
    part2.id = EntityId(EntityId::Type::Part);
    part2.name = "Part2";
    auto entity2 = doc->add_entity(part2);

    Part part3;
    part3.id = EntityId(EntityId::Type::Part);
    part3.name = "Part3";
    auto entity3 = doc->add_entity(part3);

    // Test 1: add simple dependency
    {
        assert(graph->add_dependency(entity1, "diameter", entity2, "radius"));

        auto dependents = graph->get_dependents(entity2, "radius");
        assert(dependents.size() == 1);
        using PairType = std::pair<EntityId, std::string>;
        assert(dependents.count(PairType(entity1, std::string("diameter"))) > 0);
        std::cout << "✓ test_add_simple_dependency\n";
    }

    // Test 2: get predecessors
    {
        auto preds = graph->get_predecessors(entity1, "diameter");
        assert(preds.size() == 1);
        using PairType = std::pair<EntityId, std::string>;
        assert(preds.count(PairType(entity2, std::string("radius"))) > 0);
        std::cout << "✓ test_get_predecessors\n";
    }

    // Test 3: no cycle initially
    {
        assert(!graph->has_cycle());
        std::cout << "✓ test_no_cycle_initially\n";
    }

    // Test 4: detect direct cycle
    {
        // Try to add a reverse dependency that would create a cycle
        bool result = graph->add_dependency(entity2, "radius", entity1, "diameter");
        assert(!result);  // Should reject the cycle
        std::cout << "✓ test_detect_direct_cycle\n";
    }

    // Test 5: node count
    {
        assert(graph->node_count() >= 2);
        std::cout << "✓ test_node_count_after_adding_dependencies\n";
    }

    // Test 6: remove dependency
    {
        auto graph2 = std::make_unique<DependencyGraph>(*doc, *bus);
        graph2->add_dependency(entity1, "param", entity2, "base");
        assert(graph2->remove_dependency(entity1, "param", entity2, "base"));

        auto preds = graph2->get_predecessors(entity1, "param");
        assert(preds.size() == 0);
        std::cout << "✓ test_remove_dependency\n";
    }

    // Test 7: add multiple dependencies
    {
        auto graph3 = std::make_unique<DependencyGraph>(*doc, *bus);
        graph3->add_dependency(entity1, "param", entity2, "base");
        graph3->add_dependency(entity1, "param", entity3, "modifier");

        auto preds = graph3->get_predecessors(entity1, "param");
        assert(preds.size() == 2);
        std::cout << "✓ test_add_multiple_dependencies\n";
    }

    // Test 8: would create cycle
    {
        auto graph4 = std::make_unique<DependencyGraph>(*doc, *bus);
        graph4->add_dependency(entity1, "param", entity2, "base");

        // This would create a cycle
        bool result = graph4->would_create_cycle(entity2, "base", entity1, "param");
        assert(result);
        std::cout << "✓ test_would_create_cycle_returns_true\n";
    }

    // Test 9: would not create cycle
    {
        auto graph5 = std::make_unique<DependencyGraph>(*doc, *bus);
        graph5->add_dependency(entity1, "param", entity2, "base");

        // This should not create a cycle
        bool result = graph5->would_create_cycle(entity3, "param3", entity1, "param");
        assert(!result);
        std::cout << "✓ test_would_create_cycle_returns_false\n";
    }

    // Test 10: topological sort empty graph
    {
        auto graph6 = std::make_unique<DependencyGraph>(*doc, *bus);
        auto topo = graph6->topological_sort();
        assert(topo.size() == 0);
        std::cout << "✓ test_topological_sort_empty_graph\n";
    }

    // Test 11: topological sort single edge
    {
        auto graph7 = std::make_unique<DependencyGraph>(*doc, *bus);
        graph7->add_dependency(entity2, "b", entity1, "a");

        auto topo = graph7->topological_sort();
        assert(topo.size() == 2);

        // entity1:a should come before entity2:b
        auto it1 = std::find(topo.begin(), topo.end(), std::make_pair(entity1, std::string("a")));
        auto it2 = std::find(topo.begin(), topo.end(), std::make_pair(entity2, std::string("b")));
        assert(it1 != topo.end());
        assert(it2 != topo.end());
        assert(it1 < it2);
        std::cout << "✓ test_topological_sort_single_edge\n";
    }

    // Test 12: topological sort chain
    {
        auto graph8 = std::make_unique<DependencyGraph>(*doc, *bus);
        graph8->add_dependency(entity2, "b", entity1, "a");
        graph8->add_dependency(entity3, "c", entity2, "b");

        auto topo = graph8->topological_sort();
        assert(topo.size() == 3);

        // Should be in order: a, b, c
        assert(topo[0] == std::make_pair(entity1, std::string("a")));
        assert(topo[1] == std::make_pair(entity2, std::string("b")));
        assert(topo[2] == std::make_pair(entity3, std::string("c")));
        std::cout << "✓ test_topological_sort_chain\n";
    }

    // Test 13: transitive dependents
    {
        auto graph9 = std::make_unique<DependencyGraph>(*doc, *bus);
        graph9->add_dependency(entity2, "param2", entity1, "param1");
        graph9->add_dependency(entity3, "param3", entity2, "param2");

        auto transitive = graph9->get_transitive_dependents(entity1, "param1");
        assert(transitive.size() == 2);
        std::cout << "✓ test_transitive_dependents_simple\n";
    }

    // Test 14: transitive predecessors
    {
        auto graph10 = std::make_unique<DependencyGraph>(*doc, *bus);
        graph10->add_dependency(entity2, "param2", entity1, "param1");
        graph10->add_dependency(entity3, "param3", entity2, "param2");

        auto transitive = graph10->get_transitive_predecessors(entity3, "param3");
        assert(transitive.size() == 2);
        std::cout << "✓ test_transitive_predecessors_simple\n";
    }

    // Test 15: batch mode
    {
        auto graph11 = std::make_unique<DependencyGraph>(*doc, *bus);
        assert(!graph11->in_batch());

        graph11->begin_batch_update();
        assert(graph11->in_batch());

        graph11->end_batch_update();
        assert(!graph11->in_batch());
        std::cout << "✓ test_batch_mode_begin_end\n";
    }

    // Test 16: batch update multiple dependencies
    {
        auto graph12 = std::make_unique<DependencyGraph>(*doc, *bus);
        graph12->begin_batch_update();

        graph12->add_dependency(entity1, "a", entity2, "b");
        graph12->add_dependency(entity2, "b", entity3, "c");

        graph12->end_batch_update();

        assert(graph12->node_count() == 3);
        std::cout << "✓ test_batch_update_multiple_dependencies\n";
    }

    // Test 17: edge count
    {
        auto graph13 = std::make_unique<DependencyGraph>(*doc, *bus);
        graph13->add_dependency(entity1, "a", entity2, "b");
        graph13->add_dependency(entity1, "a", entity3, "c");

        assert(graph13->edge_count() == 2);
        std::cout << "✓ test_edge_count\n";
    }

    // Test 18: source nodes
    {
        auto graph14 = std::make_unique<DependencyGraph>(*doc, *bus);
        graph14->add_dependency(entity2, "b", entity1, "a");
        graph14->add_dependency(entity3, "c", entity1, "a");

        auto sources = graph14->get_source_nodes();
        assert(sources.size() == 1);
        assert(sources[0] == std::make_pair(entity1, std::string("a")));
        std::cout << "✓ test_source_nodes\n";
    }

    // Test 19: source count
    {
        auto graph15 = std::make_unique<DependencyGraph>(*doc, *bus);
        graph15->add_dependency(entity2, "b", entity1, "a");
        graph15->add_dependency(entity3, "c", entity1, "a");

        assert(graph15->source_count() == 1);
        std::cout << "✓ test_source_count\n";
    }

    // Test 20: invalidate parameter
    {
        auto graph16 = std::make_unique<DependencyGraph>(*doc, *bus);
        graph16->add_dependency(entity2, "b", entity1, "a");

        graph16->invalidate(entity1, "a");

        auto dirty = graph16->get_dirty_parameters();
        assert(dirty.size() >= 1);
        std::cout << "✓ test_invalidate_single_parameter\n";
    }

    // Test 21: find path direct
    {
        auto graph17 = std::make_unique<DependencyGraph>(*doc, *bus);
        graph17->add_dependency(entity2, "b", entity1, "a");

        auto path = graph17->find_path(entity1, "a", entity2, "b");
        assert(path.has_value());
        assert(path->length() == 2);
        std::cout << "✓ test_find_path_direct\n";
    }

    // Test 22: find path indirect
    {
        auto graph18 = std::make_unique<DependencyGraph>(*doc, *bus);
        graph18->add_dependency(entity2, "b", entity1, "a");
        graph18->add_dependency(entity3, "c", entity2, "b");

        auto path = graph18->find_path(entity1, "a", entity3, "c");
        assert(path.has_value());
        assert(path->length() == 3);
        std::cout << "✓ test_find_path_indirect\n";
    }

    // Test 23: find path nonexistent
    {
        auto graph19 = std::make_unique<DependencyGraph>(*doc, *bus);
        graph19->add_dependency(entity1, "a", entity2, "b");

        // No path from entity3:c to entity1:a
        auto path = graph19->find_path(entity3, "c", entity1, "a");
        assert(!path.has_value());
        std::cout << "✓ test_find_path_nonexistent\n";
    }

    // Test 24: diamond dependency
    {
        auto graph20 = std::make_unique<DependencyGraph>(*doc, *bus);
        graph20->add_dependency(entity2, "b", entity1, "a");
        graph20->add_dependency(entity3, "c", entity1, "a");
        graph20->add_dependency(entity2, "d", entity3, "c");

        auto topo = graph20->topological_sort();
        assert(topo.size() == 4);
        std::cout << "✓ test_diamond_dependency_pattern\n";
    }

    // Test 25: get all nodes
    {
        auto graph21 = std::make_unique<DependencyGraph>(*doc, *bus);
        graph21->add_dependency(entity1, "a", entity2, "b");
        graph21->add_dependency(entity2, "b", entity3, "c");

        auto nodes = graph21->get_all_nodes();
        assert(nodes.size() == 3);
        std::cout << "✓ test_get_all_nodes\n";
    }

    std::cout << "\nAll DependencyGraph tests passed! (25/25)\n";
    return 0;
}
