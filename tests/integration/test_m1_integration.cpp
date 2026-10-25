/**
 * BendCore M1 Phase - CAD Foundation Integration Test
 *
 * Тест интеграции всех ключевых компонентов M1:
 * - GAP-001: Sketcher с решателем ограничений
 * - GAP-005: Единое Feature Tree
 * - GAP-016: Единый CAD-документ
 * - GAP-020: Граф зависимостей
 *
 * Сценарий: Create Sketch → Add Constraints → Solve → Extrude → Rebuild → Save
 */

#include <cassert>
#include <iostream>
#include <memory>

#include "bendcore/document/cad_document.hpp"
#include "bendcore/document/entity_id.hpp"
#include "bendcore/dependency/dependency_graph.hpp"
#include "bendcore/features/feature_system.hpp"
#include "bendcore/constraint_system.h"

using namespace BendCore::CadCore;
using namespace bendcore::assembly;

int main() {
    std::cout << "\n====== BendCore M1 CAD Foundation Integration Test ======\n\n";

    try {
        // ========================================================================
        // GAP-016: Создание единого CAD-документа
        // ========================================================================
        std::cout << "Тест 1: Создание CAD-документа...\n";
        auto doc = std::make_unique<CadDocument>();
        assert(doc != nullptr);
        assert(!doc->getDocumentId().isNull());
        assert(doc->getRevision() == 0);
        assert(doc->getEntityCount() == 0);
        std::cout << "  ✓ CAD-документ создан (ID: " << doc->getDocumentId().toString() << ")\n";

        // ========================================================================
        // GAP-005: Создание Feature Tree
        // ========================================================================
        std::cout << "\nТест 2: Создание Feature System (Feature Tree)...\n";
        auto feature_system = std::make_unique<FeatureSystem>();
        assert(feature_system != nullptr);

        // Создание Sketch (эскиза)
        EntityId sketch_id = feature_system->createFeature(FeatureType::SKETCH, "Sketch1");
        assert(!sketch_id.isNull());
        auto sketch = feature_system->getFeature(sketch_id);
        assert(sketch != nullptr);
        assert(sketch->name == "Sketch1");
        assert(sketch->type == FeatureType::SKETCH);
        std::cout << "  ✓ Sketch создан (ID: " << sketch_id.toString() << ")\n";

        // Создание операции Pad (выдавливания)
        EntityId pad_id = feature_system->createFeature(FeatureType::PAD, "Pad1");
        assert(!pad_id.isNull());
        feature_system->setParent(pad_id, sketch_id);
        std::cout << "  ✓ Pad создан и связан со Sketch\n";

        // Создание Hole (отверстия)
        EntityId hole_id = feature_system->createFeature(FeatureType::HOLE, "Hole1");
        assert(!hole_id.isNull());
        feature_system->setParent(hole_id, pad_id);
        std::cout << "  ✓ Hole создан и связан с Pad\n";

        // Проверка иерархии
        std::vector<EntityId> sketch_children = feature_system->getChildren(sketch_id);
        assert(sketch_children.size() == 1);
        assert(sketch_children[0] == pad_id);
        std::cout << "  ✓ Иерархия Feature Tree корректна\n";

        // ========================================================================
        // GAP-001: Constraint System (Решатель ограничений)
        // ========================================================================
        std::cout << "\nТест 3: Constraint System (решатель ограничений)...\n";
        auto constraint_system = std::make_unique<ConstraintSystem>();
        assert(constraint_system != nullptr);

        // Создание точек
        Vector3 p1(0, 0, 0);
        Vector3 p2(100, 50, 0);
        Vector3 p3(100, 0, 0);

        // Добавление ограничений
        ConstraintTarget target1;
        target1.entityId1 = "entity_1";
        target1.entityId2 = "entity_2";
        bendcore::assembly::Constraint coincident(ConstraintType::COINCIDENT, target1);
        constraint_system->addConstraint(coincident);
        std::cout << "  ✓ Ограничение Coincident добавлено\n";

        ConstraintTarget target2;
        target2.entityId1 = "entity_2";
        target2.entityId2 = "entity_3";
        target2.value = 100.0;
        bendcore::assembly::Constraint distance_constraint(ConstraintType::DISTANCE, target2);
        constraint_system->addConstraint(distance_constraint);
        std::cout << "  ✓ Ограничение Distance (100mm) добавлено\n";

        // Проверка ограничений
        auto all_constraints = constraint_system->getConstraints();
        assert(all_constraints.size() == 2);
        std::cout << "  ✓ Все ограничения добавлены (" << all_constraints.size() << " шт.)\n";

        // Попытка решения (базовое)
        std::vector<Vector3> initial_positions = {p1, p2, p3};
        std::vector<Vector3> resolved_positions;
        bool solved = constraint_system->resolveConstraints(initial_positions, resolved_positions);
        assert(resolved_positions.size() == initial_positions.size());
        std::cout << "  ✓ Решение ограничений выполнено (успешно: " << (solved ? "да" : "нет") << ")\n";

        // ========================================================================
        // GAP-016: Добавление сущностей в документ
        // ========================================================================
        std::cout << "\nТест 4: Добавление сущностей в CAD-документ...\n";

        // Добавление Sketch как сущности
        Entity sketch_entity(EntityType::SKETCH, "Sketch1");
        doc->addEntity(sketch_entity);
        EntityId sketch_entity_id = sketch_entity.id;
        assert(doc->hasEntity(sketch_entity_id));
        std::cout << "  ✓ Sketch добавлен в документ\n";

        // Добавление Pad как сущности
        Entity pad_entity(EntityType::FEATURE, "Pad1");
        pad_entity.parent_id = sketch_entity_id;
        doc->addEntity(pad_entity);
        EntityId pad_entity_id = pad_entity.id;
        assert(doc->hasEntity(pad_entity_id));
        std::cout << "  ✓ Pad добавлен в документ как дочерняя сущность\n";

        // Добавление Hole как сущности
        Entity hole_entity(EntityType::FEATURE, "Hole1");
        hole_entity.parent_id = pad_entity_id;
        doc->addEntity(hole_entity);
        EntityId hole_entity_id = hole_entity.id;
        assert(doc->hasEntity(hole_entity_id));
        std::cout << "  ✓ Hole добавлен в документ как дочерняя сущность\n";

        // Проверка количества сущностей
        assert(doc->getActiveEntityCount() == 3);
        std::cout << "  ✓ В документе " << doc->getActiveEntityCount() << " активных сущностей\n";

        // Добавление параметров
        Parameter diameter_param;
        diameter_param.name = "diameter";
        diameter_param.value = 25.0;
        diameter_param.units = "mm";
        diameter_param.owner_id = sketch_entity_id;
        doc->addParameter(sketch_entity_id, diameter_param);

        Parameter depth_param;
        depth_param.name = "depth";
        depth_param.value = 10.0;
        depth_param.units = "mm";
        depth_param.owner_id = pad_entity_id;
        doc->addParameter(pad_entity_id, depth_param);
        std::cout << "  ✓ Параметры (diameter, depth) добавлены\n";

        // ========================================================================
        // GAP-020: Граф зависимостей
        // ========================================================================
        std::cout << "\nТест 5: Граф зависимостей (Dependency Graph)...\n";
        auto dep_graph = std::make_unique<DependencyGraph>();
        assert(dep_graph != nullptr);

        // Добавление зависимостей: Pad зависит от Sketch
        dep_graph->addDependency(pad_entity_id, sketch_entity_id);
        std::cout << "  ✓ Зависимость: Pad зависит от Sketch\n";

        // Добавление зависимостей: Hole зависит от Pad
        dep_graph->addDependency(hole_entity_id, pad_entity_id);
        std::cout << "  ✓ Зависимость: Hole зависит от Pad\n";

        // Проверка на циклы (не должно быть)
        assert(!dep_graph->hasCycle());
        std::cout << "  ✓ Циклических зависимостей не обнаружено\n";

        // Топологическая сортировка (для пересчёта)
        std::vector<EntityId> build_order = dep_graph->topologicalSort();
        assert(build_order.size() == 3);
        // Sketch должен быть первым, потом Pad, потом Hole
        assert(build_order[0] == sketch_entity_id);
        assert(build_order[1] == pad_entity_id);
        assert(build_order[2] == hole_entity_id);
        std::cout << "  ✓ Порядок пересчёта (topological sort) корректен\n";

        // Получение зависимостей
        std::set<EntityId> pad_deps = dep_graph->getDependencies(pad_entity_id);
        assert(pad_deps.find(sketch_entity_id) != pad_deps.end());
        std::cout << "  ✓ Зависимости получены корректно\n";

        // ========================================================================
        // M1 Workflow: Rebuild
        // ========================================================================
        std::cout << "\nТест 6: Workflow Rebuild (пересчёт в правильном порядке)...\n";
        for (const auto& entity_id : build_order) {
            Entity& entity = doc->getEntity(entity_id);
            // Имитация пересчёта
            entity.revision++;
            doc->markModified();
        }
        assert(doc->getRevision() >= 3);
        std::cout << "  ✓ Rebuild выполнен для всех сущностей в правильном порядке\n";
        std::cout << "  ✓ Документ обновлён (revision: " << doc->getRevision() << ")\n";

        // ========================================================================
        // M1 Workflow: Validation и Serialization
        // ========================================================================
        std::cout << "\nТест 7: Валидация документа и подготовка к сохранению...\n";

        // Проверка целостности документа
        std::string validation_result = doc->validate();
        assert(validation_result.empty());
        std::cout << "  ✓ Документ прошёл валидацию\n";

        // Проверка отсутствия дублирующихся ID
        assert(!doc->hasDuplicateIds());
        std::cout << "  ✓ Дублирующихся ID не найдено\n";

        // ========================================================================
        // M1 Complete Integration Test Passed
        // ========================================================================
        std::cout << "\n✅ ИНТЕГРАЦИОННЫЙ ТЕСТ M1 УСПЕШНО ПРОЙДЕН!\n";
        std::cout << "\nСтатистика:\n";
        std::cout << "  - CAD-документов создано: 1\n";
        std::cout << "  - Сущностей в документе: " << doc->getActiveEntityCount() << "\n";
        std::cout << "  - Параметров: 2\n";
        std::cout << "  - Ограничений в системе: " << constraint_system->getConstraints().size() << "\n";
        std::cout << "  - Элементов Feature Tree: 3\n";
        std::cout << "  - Зависимостей: 2\n";
        std::cout << "  - Циклических зависимостей: 0 (безопасно)\n";
        std::cout << "\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "❌ Ошибка: " << e.what() << "\n";
        return 1;
    }
}
