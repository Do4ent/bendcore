#include <cassert>
#include <iostream>

#include "bendcore/document/cad_document.hpp"

using namespace bendcore::document;

void test_find_by_path_simple() {
    std::cout << "Testing find_by_path with simple hierarchy...\n";

    CadDocument doc;

    // Создать иерархию: Part -> Instance -> Body
    Part part;
    part.id = EntityId(EntityId::Type::Part);
    part.name = "Main";
    EntityId part_id = doc.add_entity(part);

    Instance inst;
    inst.id = EntityId(EntityId::Type::Instance);
    inst.name = "Inst1";
    EntityId inst_id = doc.add_entity(inst);
    doc.set_parent(inst_id, part_id);

    Body body;
    body.id = EntityId(EntityId::Type::Body);
    body.name = "Body1";
    EntityId body_id = doc.add_entity(body);
    doc.set_parent(body_id, inst_id);

    // Найти Part по пути
    auto found_part = doc.find_by_path("/Main");
    assert(found_part.has_value());
    assert(found_part.value() == part_id);

    // Найти Instance по пути
    auto found_inst = doc.find_by_path("/Main/Inst1");
    assert(found_inst.has_value());
    assert(found_inst.value() == inst_id);

    // Найти Body по пути
    auto found_body = doc.find_by_path("/Main/Inst1/Body1");
    assert(found_body.has_value());
    assert(found_body.value() == body_id);

    std::cout << "✓ find_by_path simple passed\n";
}

void test_find_by_path_nonexistent() {
    std::cout << "Testing find_by_path with nonexistent path...\n";

    CadDocument doc;

    Part part;
    part.id = EntityId(EntityId::Type::Part);
    part.name = "Main";
    doc.add_entity(part);

    // Найти несуществующий путь
    auto found = doc.find_by_path("/Main/Nonexistent");
    assert(!found.has_value());

    // Найти путь, что не начинается с существующей части
    found = doc.find_by_path("/Other");
    assert(!found.has_value());

    std::cout << "✓ find_by_path nonexistent passed\n";
}

void test_path_of_entity() {
    std::cout << "Testing path_of for entities...\n";

    CadDocument doc;

    Part part;
    part.id = EntityId(EntityId::Type::Part);
    part.name = "Part1";
    EntityId part_id = doc.add_entity(part);

    Instance inst;
    inst.id = EntityId(EntityId::Type::Instance);
    inst.name = "Inst1";
    EntityId inst_id = doc.add_entity(inst);
    doc.set_parent(inst_id, part_id);

    Body body;
    body.id = EntityId(EntityId::Type::Body);
    body.name = "Body1";
    EntityId body_id = doc.add_entity(body);
    doc.set_parent(body_id, inst_id);

    // Проверить пути
    assert(doc.path_of(part_id) == "/Part1");
    assert(doc.path_of(inst_id) == "/Part1/Inst1");
    assert(doc.path_of(body_id) == "/Part1/Inst1/Body1");

    std::cout << "✓ path_of entity passed\n";
}

void test_path_cache_consistency() {
    std::cout << "Testing path cache consistency...\n";

    CadDocument doc;

    Part part;
    part.id = EntityId(EntityId::Type::Part);
    part.name = "P1";
    EntityId part_id = doc.add_entity(part);

    Instance inst;
    inst.id = EntityId(EntityId::Type::Instance);
    inst.name = "I1";
    EntityId inst_id = doc.add_entity(inst);
    doc.set_parent(inst_id, part_id);

    // Первый вызов path_of заполняет кэш
    std::string path1 = doc.path_of(inst_id);

    // Второй вызов должен вернуть то же самое из кэша
    std::string path2 = doc.path_of(inst_id);
    assert(path1 == path2);
    assert(path1 == "/P1/I1");

    // find_by_path должен использовать кэш
    auto found = doc.find_by_path("/P1/I1");
    assert(found.has_value());
    assert(found.value() == inst_id);

    std::cout << "✓ path cache consistency passed\n";
}

void test_move_subtree_simple() {
    std::cout << "Testing move_subtree operation...\n";

    CadDocument doc;

    Part part1;
    part1.id = EntityId(EntityId::Type::Part);
    part1.name = "Part1";
    EntityId part1_id = doc.add_entity(part1);

    Part part2;
    part2.id = EntityId(EntityId::Type::Part);
    part2.name = "Part2";
    EntityId part2_id = doc.add_entity(part2);

    Instance inst;
    inst.id = EntityId(EntityId::Type::Instance);
    inst.name = "Inst1";
    EntityId inst_id = doc.add_entity(inst);
    doc.set_parent(inst_id, part1_id);

    // Проверить начальное состояние
    assert(doc.parent_of(inst_id) == part1_id);

    // Переместить Instance из Part1 в Part2
    bool success = doc.move_subtree(inst_id, part2_id);
    assert(success);

    // Проверить новое состояние
    assert(doc.parent_of(inst_id) == part2_id);

    // Проверить, что Instance больше не в Part1
    auto part1_children = doc.children_of(part1_id);
    for (EntityId child : part1_children) {
        assert(child != inst_id);
    }

    // Проверить, что Instance в Part2
    auto part2_children = doc.children_of(part2_id);
    bool found_inst = false;
    for (EntityId child : part2_children) {
        if (child == inst_id) {
            found_inst = true;
            break;
        }
    }
    assert(found_inst);

    std::cout << "✓ move_subtree simple passed\n";
}

void test_move_subtree_cycle_prevention() {
    std::cout << "Testing move_subtree cycle prevention...\n";

    CadDocument doc;

    Part part1;
    part1.id = EntityId(EntityId::Type::Part);
    part1.name = "Part1";
    EntityId part1_id = doc.add_entity(part1);

    Instance inst;
    inst.id = EntityId(EntityId::Type::Instance);
    inst.name = "Inst1";
    EntityId inst_id = doc.add_entity(inst);
    doc.set_parent(inst_id, part1_id);

    // Попытка создать цикл (Part1 -> Inst1 -> Part1)
    bool success = doc.move_subtree(part1_id, inst_id);
    assert(!success);  // Должна быть заблокирована

    // Иерархия не должна измениться
    assert(doc.parent_of(inst_id) == part1_id);
    assert(!doc.has_cycles());

    std::cout << "✓ move_subtree cycle prevention passed\n";
}

void test_subtree_enumeration() {
    std::cout << "Testing subtree enumeration...\n";

    CadDocument doc;

    // Создать иерархию:
    // Part1
    //  ├── Inst1
    //  │    └── Body1
    //  └── Inst2
    //       └── Body2

    Part part;
    part.id = EntityId(EntityId::Type::Part);
    part.name = "Part1";
    EntityId part_id = doc.add_entity(part);

    Instance inst1;
    inst1.id = EntityId(EntityId::Type::Instance);
    inst1.name = "Inst1";
    EntityId inst1_id = doc.add_entity(inst1);
    doc.set_parent(inst1_id, part_id);

    Body body1;
    body1.id = EntityId(EntityId::Type::Body);
    body1.name = "Body1";
    EntityId body1_id = doc.add_entity(body1);
    doc.set_parent(body1_id, inst1_id);

    Instance inst2;
    inst2.id = EntityId(EntityId::Type::Instance);
    inst2.name = "Inst2";
    EntityId inst2_id = doc.add_entity(inst2);
    doc.set_parent(inst2_id, part_id);

    Body body2;
    body2.id = EntityId(EntityId::Type::Body);
    body2.name = "Body2";
    EntityId body2_id = doc.add_entity(body2);
    doc.set_parent(body2_id, inst2_id);

    // Получить поддерево Part1
    auto subtree_ids = doc.subtree(part_id);
    assert(subtree_ids.size() == 5);  // Part1, Inst1, Body1, Inst2, Body2

    // Первый элемент должен быть Part1
    assert(subtree_ids[0] == part_id);

    // Проверить наличие всех элементов
    bool has_inst1 = false, has_body1 = false, has_inst2 = false, has_body2 = false;
    for (EntityId id : subtree_ids) {
        if (id == inst1_id) has_inst1 = true;
        if (id == body1_id) has_body1 = true;
        if (id == inst2_id) has_inst2 = true;
        if (id == body2_id) has_body2 = true;
    }
    assert(has_inst1 && has_body1 && has_inst2 && has_body2);

    // Получить поддерево Inst1 (только Body1)
    auto inst1_subtree = doc.subtree(inst1_id);
    assert(inst1_subtree.size() == 2);  // Inst1, Body1
    assert(inst1_subtree[0] == inst1_id);

    std::cout << "✓ subtree enumeration passed\n";
}

void test_siblings_of() {
    std::cout << "Testing siblings_of operation...\n";

    CadDocument doc;

    Part part;
    part.id = EntityId(EntityId::Type::Part);
    part.name = "Part1";
    EntityId part_id = doc.add_entity(part);

    Instance inst1;
    inst1.id = EntityId(EntityId::Type::Instance);
    inst1.name = "Inst1";
    EntityId inst1_id = doc.add_entity(inst1);
    doc.set_parent(inst1_id, part_id);

    Instance inst2;
    inst2.id = EntityId(EntityId::Type::Instance);
    inst2.name = "Inst2";
    EntityId inst2_id = doc.add_entity(inst2);
    doc.set_parent(inst2_id, part_id);

    Instance inst3;
    inst3.id = EntityId(EntityId::Type::Instance);
    inst3.name = "Inst3";
    EntityId inst3_id = doc.add_entity(inst3);
    doc.set_parent(inst3_id, part_id);

    // Получить братьев Inst1
    auto siblings = doc.siblings_of(inst1_id);
    assert(siblings.size() == 3);  // Inst1, Inst2, Inst3 (including self)

    // Проверить наличие Inst2 и Inst3
    bool has_inst2 = false, has_inst3 = false;
    for (EntityId id : siblings) {
        if (id == inst2_id) has_inst2 = true;
        if (id == inst3_id) has_inst3 = true;
    }
    assert(has_inst2 && has_inst3);

    // Root не должен иметь братьев
    auto part_siblings = doc.siblings_of(part_id);
    assert(part_siblings.empty());

    std::cout << "✓ siblings_of passed\n";
}

void test_is_ancestor_of() {
    std::cout << "Testing is_ancestor_of operation...\n";

    CadDocument doc;

    Part part;
    part.id = EntityId(EntityId::Type::Part);
    part.name = "Part1";
    EntityId part_id = doc.add_entity(part);

    Instance inst;
    inst.id = EntityId(EntityId::Type::Instance);
    inst.name = "Inst1";
    EntityId inst_id = doc.add_entity(inst);
    doc.set_parent(inst_id, part_id);

    Body body;
    body.id = EntityId(EntityId::Type::Body);
    body.name = "Body1";
    EntityId body_id = doc.add_entity(body);
    doc.set_parent(body_id, inst_id);

    // Проверить отношения предка
    assert(doc.is_ancestor_of(part_id, inst_id));
    assert(doc.is_ancestor_of(part_id, body_id));
    assert(doc.is_ancestor_of(inst_id, body_id));

    // Обратные отношения не должны быть верны
    assert(!doc.is_ancestor_of(inst_id, part_id));
    assert(!doc.is_ancestor_of(body_id, part_id));
    assert(!doc.is_ancestor_of(body_id, inst_id));

    // Сущность не предок самого себя
    assert(!doc.is_ancestor_of(part_id, part_id));

    std::cout << "✓ is_ancestor_of passed\n";
}

void test_deep_hierarchy() {
    std::cout << "Testing deep hierarchy (100+ levels)...\n";

    CadDocument doc;

    // Создать цепь из 50 уровней
    EntityId current;
    for (int i = 0; i < 50; ++i) {
        if (i == 0) {
            Part part;
            part.id = EntityId(EntityId::Type::Part);
            part.name = "Level0";
            current = doc.add_entity(part);
        } else {
            Instance inst;
            inst.id = EntityId(EntityId::Type::Instance);
            inst.name = "Level" + std::to_string(i);
            EntityId new_id = doc.add_entity(inst);
            doc.set_parent(new_id, current);
            current = new_id;
        }
    }

    // Проверить, что нет циклов
    assert(!doc.has_cycles());

    // Проверить путь до самого глубокого элемента
    std::string path = doc.path_of(current);
    assert(!path.empty());
    assert(path.find("Level49") != std::string::npos);

    std::cout << "✓ deep hierarchy passed\n";
}

void test_rebuild_paths() {
    std::cout << "Testing rebuild_paths function...\n";

    CadDocument doc;

    Part part;
    part.id = EntityId(EntityId::Type::Part);
    part.name = "Part1";
    EntityId part_id = doc.add_entity(part);

    Instance inst;
    inst.id = EntityId(EntityId::Type::Instance);
    inst.name = "Inst1";
    EntityId inst_id = doc.add_entity(inst);
    doc.set_parent(inst_id, part_id);

    // Кэшировать пути
    std::string path = doc.path_of(inst_id);
    assert(!path.empty());

    // Перестроить пути
    doc.rebuild_paths();

    // Пути должны быть доступны после перестройки
    std::string rebuilt_path = doc.path_of(inst_id);
    assert(rebuilt_path == path);

    std::cout << "✓ rebuild_paths passed\n";
}

int main() {
    std::cout << "=== CAD-CORE-004: Extended Hierarchy Tests ===\n\n";

    test_find_by_path_simple();
    test_find_by_path_nonexistent();
    test_path_of_entity();
    test_path_cache_consistency();
    test_move_subtree_simple();
    test_move_subtree_cycle_prevention();
    test_subtree_enumeration();
    test_siblings_of();
    test_is_ancestor_of();
    test_deep_hierarchy();
    test_rebuild_paths();

    std::cout << "\n✓ All CAD-CORE-004 extended hierarchy tests passed!\n";
    return 0;
}
