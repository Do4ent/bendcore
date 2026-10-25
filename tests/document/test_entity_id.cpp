#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "bendcore/document/entity_id.hpp"

using namespace bendcore::document;

void test_entity_id_creation() {
    std::cout << "Testing EntityId creation...\n";

    // Создание невалидного ID
    EntityId invalid;
    assert(!invalid.is_valid());
    assert(invalid.type() == EntityId::Type::Invalid);

    // Создание Part ID
    EntityId part_id(EntityId::Type::Part);
    assert(part_id.is_valid());
    assert(part_id.type() == EntityId::Type::Part);
    assert(part_id.uuid().length() == 36);  // UUID формат

    // Создание Instance ID
    EntityId instance_id(EntityId::Type::Instance);
    assert(instance_id.is_valid());
    assert(instance_id.type() == EntityId::Type::Instance);

    // ID должны быть разными
    assert(part_id != instance_id);
    assert(part_id.uuid() != instance_id.uuid());

    std::cout << "✓ Creation tests passed\n";
}

void test_entity_id_uniqueness() {
    std::cout << "Testing EntityId uniqueness...\n";

    // Создаём 10000 ID и проверяем уникальность
    std::set<std::string> uuids;
    for (int i = 0; i < 10000; i++) {
        EntityId id(EntityId::Type::Part);
        uuids.insert(id.uuid());
    }

    assert(uuids.size() == 10000);  // Все ID уникальны
    std::cout << "✓ Generated 10000 unique IDs\n";
}

void test_entity_id_serialization() {
    std::cout << "Testing EntityId serialization...\n";

    EntityId original(EntityId::Type::Feature);
    std::string original_uuid = original.uuid();

    // Тест to_string / from_string
    std::string serialized = original.to_string();
    assert(!serialized.empty());

    auto deserialized_opt = EntityId::from_string(serialized);
    assert(deserialized_opt.has_value());

    EntityId deserialized = deserialized_opt.value();
    assert(deserialized == original);
    assert(deserialized.uuid() == original_uuid);
    assert(deserialized.type() == EntityId::Type::Feature);

    std::cout << "✓ Serialization tests passed\n";
}

void test_entity_id_json_serialization() {
    std::cout << "Testing EntityId JSON serialization...\n";

    EntityId original(EntityId::Type::Assembly);

    // Тест to_json / from_json
    std::string json = original.to_json();
    assert(json.find("\"type\":") != std::string::npos);
    assert(json.find("\"uuid\":") != std::string::npos);

    auto deserialized_opt = EntityId::from_json(json);
    assert(deserialized_opt.has_value());

    EntityId deserialized = deserialized_opt.value();
    assert(deserialized == original);

    std::cout << "✓ JSON serialization tests passed\n";
}

void test_entity_id_comparison() {
    std::cout << "Testing EntityId comparison...\n";

    EntityId id1(EntityId::Type::Part);
    EntityId id2(EntityId::Type::Part);
    EntityId id3 = id1;  // Копирование

    // Сравнение
    assert(id1 != id2);  // Разные UUID
    assert(id1 == id3);  // Одинаковые
    assert(id1 < id2 || id2 < id1);  // Упорядочивание

    // Использование в std::set (требует operator<)
    std::set<EntityId> id_set;
    id_set.insert(id1);
    id_set.insert(id2);
    id_set.insert(id3);
    assert(id_set.size() == 2);  // id1 и id3 — один элемент

    std::cout << "✓ Comparison tests passed\n";
}

void test_entity_id_in_containers() {
    std::cout << "Testing EntityId in containers...\n";

    // std::map (требует operator<)
    std::map<EntityId, std::string> map;
    EntityId id1(EntityId::Type::Part);
    EntityId id2(EntityId::Type::Instance);

    map[id1] = "Part A";
    map[id2] = "Instance B";
    assert(map.size() == 2);
    assert(map[id1] == "Part A");

    // std::unordered_map (требует std::hash)
    std::unordered_map<EntityId, std::string> unordered_map;
    unordered_map[id1] = "Part A";
    unordered_map[id2] = "Instance B";
    assert(unordered_map.size() == 2);

    // std::unordered_set (требует std::hash)
    std::unordered_set<EntityId> id_set;
    id_set.insert(id1);
    id_set.insert(id2);
    assert(id_set.size() == 2);
    assert(id_set.count(id1) == 1);

    std::cout << "✓ Container tests passed\n";
}

void test_entity_id_type_validation() {
    std::cout << "Testing EntityId type validation...\n";

    // Создание с явным UUID и типом
    EntityId id("550e8400-e29b-41d4-a716-446655440000", EntityId::Type::Body);
    assert(id.is_valid());
    assert(id.type() == EntityId::Type::Body);

    // Невалидный UUID должен создать invalid ID
    EntityId invalid_uuid("not-a-uuid", EntityId::Type::Sketch);
    assert(!invalid_uuid.is_valid());

    std::cout << "✓ Type validation tests passed\n";
}

void test_entity_id_debug_str() {
    std::cout << "Testing EntityId debug_str...\n";

    EntityId invalid;
    assert(invalid.debug_str() == "EntityId(INVALID)");

    EntityId part_id(EntityId::Type::Part);
    std::string debug = part_id.debug_str();
    assert(debug.find("Part") != std::string::npos);
    assert(debug.find("...") != std::string::npos);

    std::cout << "✓ Debug string tests passed\n";
}

void test_entity_id_persistence_stability() {
    std::cout << "Testing EntityId persistence stability...\n";

    // Создать ID
    EntityId original(EntityId::Type::Parameter);
    std::string uuid_before = original.uuid();

    // Сохранить
    std::string serialized = original.to_string();

    // "Загрузить" (в другой сессии это был бы новый объект)
    auto loaded_opt = EntityId::from_string(serialized);
    assert(loaded_opt.has_value());
    EntityId loaded = loaded_opt.value();

    // Проверить стабильность
    assert(loaded.uuid() == uuid_before);
    assert(loaded == original);

    std::cout << "✓ Persistence stability tests passed\n";
}

void test_all_entity_types() {
    std::cout << "Testing all EntityId types...\n";

    std::vector<EntityId::Type> types = {
        EntityId::Type::Part,
        EntityId::Type::Instance,
        EntityId::Type::Body,
        EntityId::Type::Sketch,
        EntityId::Type::Feature,
        EntityId::Type::Assembly,
        EntityId::Type::Parameter,
        EntityId::Type::Dimension,
        EntityId::Type::Reference,
        EntityId::Type::Drawing,
        EntityId::Type::Annotation,
        EntityId::Type::CustomProperty
    };

    for (auto type : types) {
        EntityId id(type);
        assert(id.is_valid());
        assert(id.type() == type);

        // Проверить сериализацию
        auto restored_opt = EntityId::from_string(id.to_string());
        assert(restored_opt.has_value());
        assert(restored_opt.value() == id);
    }

    std::cout << "✓ All " << types.size() << " types work correctly\n";
}

int main() {
    std::cout << "=== CAD-CORE-002: EntityId Tests ===\n\n";

    test_entity_id_creation();
    test_entity_id_uniqueness();
    test_entity_id_serialization();
    test_entity_id_json_serialization();
    test_entity_id_comparison();
    test_entity_id_in_containers();
    test_entity_id_type_validation();
    test_entity_id_debug_str();
    test_entity_id_persistence_stability();
    test_all_entity_types();

    std::cout << "\n✓ All CAD-CORE-002 tests passed!\n";
    return 0;
}
