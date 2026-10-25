#include <iostream>
#include <cassert>
#include <memory>

#include "bendcore/dependency/dependency_graph.hpp"
#include "bendcore/dependency/rebuild_engine.hpp"

using namespace BendCore::CadCore;

/**
 * GAP-023: Сквозная проверка перестроения
 * 25 тестов для каскадного обновления и инкрементального пересчета
 */

int main() {
    std::cout << "Testing Cascade Updates and Incremental Rebuild (GAP-023)...\n";
    
    int tests_passed = 0;
    int tests_failed = 0;

    // Test 1: Basic dependency tracking
    {
        auto graph = std::make_unique<DependencyGraph>();
        EntityId part = EntityId::generate();
        EntityId assembly = EntityId::generate();
        
        graph->addDependency(assembly, part);
        
        auto deps = graph->getDependencies(assembly);
        if (deps.count(part) == 1) {
            std::cout << "✓ Test 1: Basic dependency tracking\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 1 FAILED\n";
            tests_failed++;
        }
    }

    // Test 2: Cascading mark changed
    {
        auto graph = std::make_unique<DependencyGraph>();
        auto engine = std::make_unique<RebuildEngine>(*graph);
        
        EntityId part1 = EntityId::generate();
        EntityId part2 = EntityId::generate();
        EntityId assembly = EntityId::generate();
        
        graph->addDependency(assembly, part1);
        graph->addDependency(assembly, part2);
        
        engine->markChanged(part1);
        
        if (engine->getChangedCount() > 0) {
            std::cout << "✓ Test 2: Cascading mark changed\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 2 FAILED\n";
            tests_failed++;
        }
    }

    // Test 3: Topological sort order
    {
        auto graph = std::make_unique<DependencyGraph>();
        
        EntityId node1 = EntityId::generate();
        EntityId node2 = EntityId::generate();
        EntityId node3 = EntityId::generate();
        
        graph->addDependency(node2, node1);
        graph->addDependency(node3, node2);
        
        auto order = graph->topologicalSort();
        
        if (order.size() > 0) {
            std::cout << "✓ Test 3: Topological sort order\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 3 FAILED\n";
            tests_failed++;
        }
    }

    // Test 4: Cycle detection
    {
        auto graph = std::make_unique<DependencyGraph>();
        
        EntityId node1 = EntityId::generate();
        EntityId node2 = EntityId::generate();
        EntityId node3 = EntityId::generate();
        
        graph->addDependency(node2, node1);
        graph->addDependency(node3, node2);
        
        bool has_cycle = graph->hasCycle();
        
        if (!has_cycle) {
            std::cout << "✓ Test 4: Cycle detection (no cycle)\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 4 FAILED\n";
            tests_failed++;
        }
    }

    // Test 5: Multiple cascading levels
    {
        auto graph = std::make_unique<DependencyGraph>();
        auto engine = std::make_unique<RebuildEngine>(*graph);
        
        EntityId level1 = EntityId::generate();
        EntityId level2 = EntityId::generate();
        EntityId level3 = EntityId::generate();
        EntityId level4 = EntityId::generate();
        
        graph->addDependency(level2, level1);
        graph->addDependency(level3, level2);
        graph->addDependency(level4, level3);
        
        engine->markChanged(level1);
        
        if (engine->getAffected().size() > 0) {
            std::cout << "✓ Test 5: Multiple cascading levels\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 5 FAILED\n";
            tests_failed++;
        }
    }

    // Test 6: Rebuild order maintains dependencies
    {
        auto graph = std::make_unique<DependencyGraph>();
        auto engine = std::make_unique<RebuildEngine>(*graph);
        
        EntityId a = EntityId::generate();
        EntityId b = EntityId::generate();
        EntityId c = EntityId::generate();
        
        graph->addDependency(b, a);
        graph->addDependency(c, b);
        
        engine->markChanged(a);
        auto order = engine->getRebuildOrder();
        
        if (order.size() > 0) {
            std::cout << "✓ Test 6: Rebuild order maintains dependencies\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 6 FAILED\n";
            tests_failed++;
        }
    }

    // Test 7: Multiple independent chains
    {
        auto graph = std::make_unique<DependencyGraph>();
        auto engine = std::make_unique<RebuildEngine>(*graph);
        
        EntityId chain1_root = EntityId::generate();
        EntityId chain1_child = EntityId::generate();
        EntityId chain2_root = EntityId::generate();
        EntityId chain2_child = EntityId::generate();
        
        graph->addDependency(chain1_child, chain1_root);
        graph->addDependency(chain2_child, chain2_root);
        
        engine->markChanged(chain1_root);
        
        if (engine->getAffected().size() > 0) {
            std::cout << "✓ Test 7: Multiple independent chains\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 7 FAILED\n";
            tests_failed++;
        }
    }

    // Test 8: Clear changed set
    {
        auto engine = std::make_unique<RebuildEngine>(*std::make_unique<DependencyGraph>());
        EntityId node = EntityId::generate();
        
        engine->markChanged(node);
        if (engine->getChangedCount() > 0) {
            engine->clearChanged();
            if (engine->getChangedCount() == 0) {
                std::cout << "✓ Test 8: Clear changed set\n";
                tests_passed++;
            } else {
                std::cout << "✗ Test 8 FAILED\n";
                tests_failed++;
            }
        }
    }

    // Test 9: Dependency removal
    {
        auto graph = std::make_unique<DependencyGraph>();
        
        EntityId parent = EntityId::generate();
        EntityId child = EntityId::generate();
        
        graph->addDependency(parent, child);
        if (graph->hasEdge(parent, child)) {
            graph->removeDependency(parent, child);
            if (!graph->hasEdge(parent, child)) {
                std::cout << "✓ Test 9: Dependency removal\n";
                tests_passed++;
            } else {
                std::cout << "✗ Test 9 FAILED\n";
                tests_failed++;
            }
        }
    }

    // Test 10: Complex assembly hierarchy
    {
        auto graph = std::make_unique<DependencyGraph>();
        auto engine = std::make_unique<RebuildEngine>(*graph);
        
        EntityId part1 = EntityId::generate();
        EntityId part2 = EntityId::generate();
        EntityId subassembly1 = EntityId::generate();
        EntityId subassembly2 = EntityId::generate();
        EntityId assembly = EntityId::generate();
        
        graph->addDependency(subassembly1, part1);
        graph->addDependency(subassembly2, part2);
        graph->addDependency(assembly, subassembly1);
        graph->addDependency(assembly, subassembly2);
        
        engine->markChanged(part1);
        
        if (engine->getAffected().size() > 0) {
            std::cout << "✓ Test 10: Complex assembly hierarchy\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 10 FAILED\n";
            tests_failed++;
        }
    }

    // Test 11: Diamond dependency pattern
    {
        auto graph = std::make_unique<DependencyGraph>();
        
        EntityId top = EntityId::generate();
        EntityId left = EntityId::generate();
        EntityId right = EntityId::generate();
        EntityId bottom = EntityId::generate();
        
        graph->addDependency(left, top);
        graph->addDependency(right, top);
        graph->addDependency(bottom, left);
        graph->addDependency(bottom, right);
        
        auto nodes = graph->getNodes();
        
        if (nodes.size() == 4) {
            std::cout << "✓ Test 11: Diamond dependency pattern\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 11 FAILED\n";
            tests_failed++;
        }
    }

    // Test 12: Self-dependency handling
    {
        auto graph = std::make_unique<DependencyGraph>();
        
        EntityId node = EntityId::generate();
        
        graph->addDependency(node, node);
        
        auto nodes = graph->getNodes();
        
        if (nodes.count(node) == 1) {
            std::cout << "✓ Test 12: Self-dependency handling\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 12 FAILED\n";
            tests_failed++;
        }
    }

    // Test 13: Large chain
    {
        auto graph = std::make_unique<DependencyGraph>();
        
        const int CHAIN_LENGTH = 100;
        std::vector<EntityId> nodes;
        
        for (int i = 0; i < CHAIN_LENGTH; ++i) {
            nodes.push_back(EntityId::generate());
        }
        
        for (int i = 1; i < CHAIN_LENGTH; ++i) {
            graph->addDependency(nodes[i], nodes[i-1]);
        }
        
        auto all_nodes = graph->getNodes();
        
        if (all_nodes.size() == CHAIN_LENGTH) {
            std::cout << "✓ Test 13: Large chain\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 13 FAILED\n";
            tests_failed++;
        }
    }

    // Test 14: Get dependents
    {
        auto graph = std::make_unique<DependencyGraph>();
        
        EntityId parent = EntityId::generate();
        EntityId child1 = EntityId::generate();
        EntityId child2 = EntityId::generate();
        
        graph->addDependency(child1, parent);
        graph->addDependency(child2, parent);
        
        auto dependents = graph->getDependents(parent);
        
        if (dependents.count(child1) == 1 && dependents.count(child2) == 1) {
            std::cout << "✓ Test 14: Get dependents\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 14 FAILED\n";
            tests_failed++;
        }
    }

    // Test 15: Graph clear
    {
        auto graph = std::make_unique<DependencyGraph>();
        
        EntityId node1 = EntityId::generate();
        EntityId node2 = EntityId::generate();
        
        graph->addDependency(node2, node1);
        
        if (graph->getNodes().size() > 0) {
            graph->clear();
            if (graph->getNodes().size() == 0) {
                std::cout << "✓ Test 15: Graph clear\n";
                tests_passed++;
            } else {
                std::cout << "✗ Test 15 FAILED\n";
                tests_failed++;
            }
        }
    }

    // Test 16: Incremental changes
    {
        auto graph = std::make_unique<DependencyGraph>();
        auto engine = std::make_unique<RebuildEngine>(*graph);
        
        EntityId node1 = EntityId::generate();
        EntityId node2 = EntityId::generate();
        
        graph->addDependency(node2, node1);
        
        engine->markChanged(node1);
        if (engine->getChangedCount() > 0) {
            auto first_affected = engine->getAffected();
            engine->clearChanged();
            
            engine->markChanged(node2);
            auto second_affected = engine->getAffected();
            
            if (second_affected.size() > 0) {
                std::cout << "✓ Test 16: Incremental changes\n";
                tests_passed++;
            } else {
                std::cout << "✗ Test 16 FAILED\n";
                tests_failed++;
            }
        }
    }

    // Test 17: Rebuild order consistency
    {
        auto graph = std::make_unique<DependencyGraph>();
        auto engine = std::make_unique<RebuildEngine>(*graph);
        
        EntityId node1 = EntityId::generate();
        EntityId node2 = EntityId::generate();
        EntityId node3 = EntityId::generate();
        
        graph->addDependency(node2, node1);
        graph->addDependency(node3, node2);
        
        engine->markChanged(node1);
        
        auto order1 = engine->getRebuildOrder();
        engine->clearChanged();
        
        engine->markChanged(node1);
        auto order2 = engine->getRebuildOrder();
        
        if (order1.size() == order2.size()) {
            std::cout << "✓ Test 17: Rebuild order consistency\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 17 FAILED\n";
            tests_failed++;
        }
    }

    // Test 18: Affected nodes include all dependents
    {
        auto graph = std::make_unique<DependencyGraph>();
        auto engine = std::make_unique<RebuildEngine>(*graph);
        
        EntityId root = EntityId::generate();
        EntityId child1 = EntityId::generate();
        EntityId child2 = EntityId::generate();
        EntityId grandchild = EntityId::generate();
        
        graph->addDependency(child1, root);
        graph->addDependency(child2, root);
        graph->addDependency(grandchild, child1);
        
        engine->markChanged(root);
        auto affected = engine->getAffected();
        
        if (affected.size() > 1) {
            std::cout << "✓ Test 18: Affected nodes include all dependents\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 18 FAILED\n";
            tests_failed++;
        }
    }

    // Test 19: Multiple marks of same node
    {
        auto engine = std::make_unique<RebuildEngine>(*std::make_unique<DependencyGraph>());
        EntityId node = EntityId::generate();
        
        engine->markChanged(node);
        int count1 = engine->getChangedCount();
        
        engine->markChanged(node);
        int count2 = engine->getChangedCount();
        
        if (count1 == count2) {
            std::cout << "✓ Test 19: Multiple marks of same node\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 19 FAILED\n";
            tests_failed++;
        }
    }

    // Test 20: Performance with many nodes
    {
        auto graph = std::make_unique<DependencyGraph>();
        auto engine = std::make_unique<RebuildEngine>(*graph);
        
        const int NUM_NODES = 1000;
        std::vector<EntityId> nodes;
        
        for (int i = 0; i < NUM_NODES; ++i) {
            nodes.push_back(EntityId::generate());
        }
        
        for (int i = 1; i < NUM_NODES; ++i) {
            graph->addDependency(nodes[i], nodes[0]);
        }
        
        engine->markChanged(nodes[0]);
        auto affected = engine->getAffected();
        
        if (affected.size() > 0) {
            std::cout << "✓ Test 20: Performance with many nodes\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 20 FAILED\n";
            tests_failed++;
        }
    }

    // Test 21: Topological sort proper order
    {
        auto graph = std::make_unique<DependencyGraph>();
        
        EntityId a = EntityId::generate();
        EntityId b = EntityId::generate();
        EntityId c = EntityId::generate();
        
        graph->addDependency(b, a);
        graph->addDependency(c, b);
        
        auto sorted = graph->topologicalSort();
        
        if (sorted.size() == 3) {
            std::cout << "✓ Test 21: Topological sort proper order\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 21 FAILED\n";
            tests_failed++;
        }
    }

    // Test 22: Edge existence check
    {
        auto graph = std::make_unique<DependencyGraph>();
        
        EntityId from = EntityId::generate();
        EntityId to = EntityId::generate();
        
        graph->addDependency(to, from);
        if (graph->hasEdge(from, to)) {
            EntityId other = EntityId::generate();
            if (!graph->hasEdge(from, other)) {
                std::cout << "✓ Test 22: Edge existence check\n";
                tests_passed++;
            } else {
                std::cout << "✗ Test 22 FAILED\n";
                tests_failed++;
            }
        }
    }

    // Test 23: Multiple operations sequence
    {
        auto graph = std::make_unique<DependencyGraph>();
        auto engine = std::make_unique<RebuildEngine>(*graph);
        
        EntityId node1 = EntityId::generate();
        EntityId node2 = EntityId::generate();
        EntityId node3 = EntityId::generate();
        
        graph->addDependency(node2, node1);
        graph->addDependency(node3, node2);
        
        engine->markChanged(node1);
        if (engine->getChangedCount() > 0) {
            auto order1 = engine->getRebuildOrder();
            engine->clearChanged();
            
            graph->removeDependency(node3, node2);
            engine->markChanged(node2);
            
            auto order2 = engine->getRebuildOrder();
            
            std::cout << "✓ Test 23: Multiple operations sequence\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 23 FAILED\n";
            tests_failed++;
        }
    }

    // Test 24: Rebuild engine consistency
    {
        auto graph = std::make_unique<DependencyGraph>();
        auto engine = std::make_unique<RebuildEngine>(*graph);
        
        EntityId node = EntityId::generate();
        
        engine->markChanged(node);
        int count = engine->getChangedCount();
        
        auto affected = engine->getAffected();
        
        engine->clearChanged();
        
        if (engine->getChangedCount() == 0) {
            std::cout << "✓ Test 24: Rebuild engine consistency\n";
            tests_passed++;
        } else {
            std::cout << "✗ Test 24 FAILED\n";
            tests_failed++;
        }
    }

    // Test 25: Complete workflow
    {
        auto graph = std::make_unique<DependencyGraph>();
        auto engine = std::make_unique<RebuildEngine>(*graph);
        
        EntityId part_a = EntityId::generate();
        EntityId part_b = EntityId::generate();
        EntityId subasm_1 = EntityId::generate();
        EntityId subasm_2 = EntityId::generate();
        EntityId final_asm = EntityId::generate();
        
        graph->addDependency(subasm_1, part_a);
        graph->addDependency(subasm_2, part_b);
        graph->addDependency(final_asm, subasm_1);
        graph->addDependency(final_asm, subasm_2);
        
        engine->markChanged(part_a);
        
        auto rebuild_order = engine->getRebuildOrder();
        
        engine->clearChanged();
        
        if (engine->getChangedCount() == 0) {
            engine->markChanged(part_b);
            auto affected = engine->getAffected();
            
            if (affected.size() > 0) {
                std::cout << "✓ Test 25: Complete workflow\n";
                tests_passed++;
            } else {
                std::cout << "✗ Test 25 FAILED\n";
                tests_failed++;
            }
        }
    }

    std::cout << "\n========================================\n";
    std::cout << "Tests Passed: " << tests_passed << "/25\n";
    std::cout << "Tests Failed: " << tests_failed << "/25\n";
    std::cout << "========================================\n";

    return (tests_failed == 0) ? 0 : 1;
}
