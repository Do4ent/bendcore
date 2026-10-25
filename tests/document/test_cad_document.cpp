#include <cassert>
#include <iostream>

#include "bendcore/document/cad_document.hpp"

using namespace bendcore::document;

void test_document_creation() {
    std::cout << "Testing CadDocument creation...\n";

    CadDocument doc;
    assert(doc.id().is_valid());
    assert(doc.revision() == 0);

    std::cout << "✓ Document creation passed\n";
}

void test_add_and_get_part() {
    std::cout << "Testing add_entity and get_entity...\n";

    CadDocument doc;

    Part part;
    part.id = EntityId(EntityId::Type::Part);
    part.name = "Test Part";
    part.designation = "PART-001";

    EntityId part_id = doc.add_entity(part);
    assert(part_id.is_valid());
    assert(doc.revision() == 1);

    // Получить сущность
    Part* retrieved = doc.get_entity<Part>(part_id);
    assert(retrieved != nullptr);
    assert(retrieved->name == "Test Part");
    assert(retrieved->designation == "PART-001");

    std::cout << "✓ Add and get entity passed\n";
}

void test_hierarchy() {
    std::cout << "Testing hierarchy operations...\n";

    CadDocument doc;

    // Создаём Part и Instance
    Part part;
    part.id = EntityId(EntityId::Type::Part);
    part.name = "Part A";
    EntityId part_id = doc.add_entity(part);

    Instance instance;
    instance.id = EntityId(EntityId::Type::Instance);
    instance.name = "Instance A";
    instance.part_id = part_id;
    EntityId instance_id = doc.add_entity(instance);

    // Установить Instance как дитя Part
    bool success = doc.set_parent(instance_id, part_id);
    assert(success);

    // Проверить отношения
    assert(doc.parent_of(instance_id) == part_id);

    std::vector<EntityId> children = doc.children_of(part_id);
    assert(children.size() == 1);
    assert(children[0] == instance_id);

    std::cout << "✓ Hierarchy operations passed\n";
}

void test_cycle_detection() {
    std::cout << "Testing cycle detection...\n";

    CadDocument doc;

    // Создаём две сущности
    Part part1, part2;
    part1.id = EntityId(EntityId::Type::Part);
    part1.name = "Part 1";
    part2.id = EntityId(EntityId::Type::Part);
    part2.name = "Part 2";

    EntityId id1 = doc.add_entity(part1);
    EntityId id2 = doc.add_entity(part2);

    // Установить Part2 -> Part1
    doc.set_parent(id2, id1);

    // Попытка создать цикл Part1 -> Part2 должна быть заблокирована
    bool cycle_created = doc.set_parent(id1, id2);
    assert(!cycle_created);  // Цикл заблокирован

    assert(!doc.has_cycles());

    std::cout << "✓ Cycle detection passed\n";
}

void test_entity_queries() {
    std::cout << "Testing entity queries...\n";

    CadDocument doc;

    // Добавить несколько Parts и Parameters
    for (int i = 0; i < 5; i++) {
        Part part;
        part.id = EntityId(EntityId::Type::Part);
        part.name = "Part " + std::to_string(i);
        doc.add_entity(part);

        Parameter param;
        param.id = EntityId(EntityId::Type::Parameter);
        param.name = "Param " + std::to_string(i);
        doc.add_entity(param);
    }

    // Запросить все Parts
    auto all_parts = doc.all_parts();
    assert(all_parts.size() == 5);

    // Запросить все Parameters
    auto all_params = doc.all_parameters();
    assert(all_params.size() == 5);

    std::cout << "✓ Entity queries passed\n";
}

void test_tree_order() {
    std::cout << "Testing tree order...\n";

    CadDocument doc;

    // Создать иерархию:
    // Part1
    //  ├── Instance1
    //  └── Instance2
    // Part2

    Part part1;
    part1.id = EntityId(EntityId::Type::Part);
    part1.name = "Part 1";
    EntityId part1_id = doc.add_entity(part1);

    Instance inst1, inst2;
    inst1.id = EntityId(EntityId::Type::Instance);
    inst1.name = "Instance 1";
    inst2.id = EntityId(EntityId::Type::Instance);
    inst2.name = "Instance 2";
    EntityId inst1_id = doc.add_entity(inst1);
    EntityId inst2_id = doc.add_entity(inst2);

    Part part2;
    part2.id = EntityId(EntityId::Type::Part);
    part2.name = "Part 2";
    EntityId part2_id = doc.add_entity(part2);

    // Установить отношения
    doc.set_parent(inst1_id, part1_id);
    doc.set_parent(inst2_id, part1_id);

    // Получить порядок
    auto order = doc.tree_order();

    // Должно быть: Part1, Instance1, Instance2, Part2 (или Part2, в зависимости от добавления)
    assert(order.size() == 4);

    std::cout << "✓ Tree order passed\n";
}

void test_validity_check() {
    std::cout << "Testing validity check...\n";

    CadDocument doc;

    Part part;
    part.id = EntityId(EntityId::Type::Part);
    part.name = "Part";
    EntityId part_id = doc.add_entity(part);

    // Документ валиден
    assert(doc.is_valid_hierarchy());
    assert(!doc.has_cycles());

    std::cout << "✓ Validity check passed\n";
}

void test_contains_check() {
    std::cout << "Testing contains check...\n";

    CadDocument doc;

    Part part;
    part.id = EntityId(EntityId::Type::Part);
    EntityId part_id = doc.add_entity(part);

    assert(doc.contains(part_id));

    EntityId nonexistent(EntityId::Type::Instance);
    assert(!doc.contains(nonexistent));

    std::cout << "✓ Contains check passed\n";
}

int main() {
    std::cout << "=== CAD-CORE-003: CadDocument Tests ===\n\n";

    test_document_creation();
    test_add_and_get_part();
    test_hierarchy();
    test_cycle_detection();
    test_entity_queries();
    test_tree_order();
    test_validity_check();
    test_contains_check();

    std::cout << "\n✓ All CAD-CORE-003 tests passed!\n";
    return 0;
}
