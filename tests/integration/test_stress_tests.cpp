// CAD-CORE-035: StressTests — тесты производительности под нагрузкой
// Масштабируемость: 1K, 5K, 10K, 50K элементов в больших сборках
// Версия: 1.0, дата: 2026-10-25
// Пакет: ~700 LOC + тесты

#include <cstdio>
#include <cmath>
#include <cassert>
#include <chrono>
#include <string>
#include <vector>
#include <memory>
#include <optional>

#include "bendcore/document/cad_document.hpp"
#include "bendcore/document/entity_id.hpp"
#include "bendcore/parameters/parameter_store.hpp"
#include "bendcore/dependencies/dependency_graph.hpp"
#include "bendcore/rebuild/rebuild_engine.hpp"
#include "bendcore/geometry/geometry_kernel.hpp"

using namespace bendcore;

// ============================================================================
// УТИЛИТЫ ТЕСТИРОВАНИЯ
// ============================================================================

struct StressTestResult {
    int entity_count;
    double creation_time_ms;
    double parameter_setting_time_ms;
    double hierarchy_setup_time_ms;
    double total_time_ms;
    double memory_estimate_mb;
    bool passed;
    const char* status_message;
};

static std::vector<StressTestResult> results;

void print_header(const char* test_name, int entity_count) {
    std::printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    std::printf("║  STRESS TEST: %s\n", test_name);
    std::printf("║  Количество сущностей: %d\n", entity_count);
    std::printf("║  Дата/время: 2026-10-25\n");
    std::printf("╚════════════════════════════════════════════════════════════════╝\n");
}

void record_result(int entity_count, double create_time, double param_time,
                   double hier_time, bool passed, const char* msg) {
    StressTestResult result;
    result.entity_count = entity_count;
    result.creation_time_ms = create_time;
    result.parameter_setting_time_ms = param_time;
    result.hierarchy_setup_time_ms = hier_time;
    result.total_time_ms = create_time + param_time + hier_time;
    result.memory_estimate_mb = (entity_count * 256.0) / 1024.0;  // ~256 bytes per entity
    result.passed = passed;
    result.status_message = msg;
    results.push_back(result);
}

// ============================================================================
// STRESS TEST 1: 1000 элементов
// ============================================================================

void stress_test_1k_entities() {
    const int ENTITY_COUNT = 1000;
    print_header("1000 элементов", ENTITY_COUNT);

    CadDocument doc;
    ParameterStore params;
    std::vector<EntityId> entities;

    auto start = std::chrono::high_resolution_clock::now();

    // Создание сущностей
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        EntityId e = EntityId::create(EntityType::Part);
        doc.add_entity(e, std::string("Part_") + std::to_string(i));
        entities.push_back(e);
    }

    auto after_create = std::chrono::high_resolution_clock::now();
    double creation_time = std::chrono::duration<double, std::milli>(
        after_create - start).count();

    std::printf("  ✓ Созданы %d сущностей за %.2f ms\n", ENTITY_COUNT, creation_time);

    // Установка параметров
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        params.set_parameter(entities[i], "diameter", 20.0 + (i % 50) * 0.5);
        params.set_parameter(entities[i], "length", 100.0 + (i % 100));
        params.set_parameter(entities[i], "material_id", i % 10);
    }

    auto after_params = std::chrono::high_resolution_clock::now();
    double params_time = std::chrono::duration<double, std::milli>(
        after_params - after_create).count();

    std::printf("  ✓ Установлены параметры за %.2f ms\n", params_time);

    // Создание иерархии (линейная цепочка)
    for (int i = 1; i < ENTITY_COUNT; ++i) {
        if (i % 10 == 0) {  // Каждый 10-й элемент — корень подсборки
            doc.set_parent(entities[i], entities[0]);
        } else {
            doc.set_parent(entities[i], entities[i - 1]);
        }
    }

    auto after_hier = std::chrono::high_resolution_clock::now();
    double hier_time = std::chrono::duration<double, std::milli>(
        after_hier - after_params).count();

    std::printf("  ✓ Создана иерархия за %.2f ms\n", hier_time);

    auto end = std::chrono::high_resolution_clock::now();
    double total_time = std::chrono::duration<double, std::milli>(end - start).count();

    // Проверки
    bool passed = true;
    const char* msg = "✓ PASS";

    if (creation_time > 5000) {
        passed = false;
        msg = "✗ FAIL: Создание слишком медленное";
    }

    if (total_time > 10000) {
        passed = false;
        msg = "✗ FAIL: Общее время слишком велико";
    }

    std::printf("\n  Итого: %.2f ms (Creation: %.1f%%, Params: %.1f%%, Hierarchy: %.1f%%)\n",
                total_time,
                creation_time * 100 / total_time,
                params_time * 100 / total_time,
                hier_time * 100 / total_time);

    std::printf("  Статус: %s\n", msg);

    record_result(ENTITY_COUNT, creation_time, params_time, hier_time, passed, msg);
}

// ============================================================================
// STRESS TEST 2: 5000 элементов
// ============================================================================

void stress_test_5k_entities() {
    const int ENTITY_COUNT = 5000;
    print_header("5000 элементов", ENTITY_COUNT);

    CadDocument doc;
    ParameterStore params;
    DependencyGraph deps;
    std::vector<EntityId> entities;

    auto start = std::chrono::high_resolution_clock::now();

    // Создание сущностей
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        EntityId e = EntityId::create(EntityType::Part);
        doc.add_entity(e, std::string("Part_") + std::to_string(i));
        entities.push_back(e);
    }

    auto after_create = std::chrono::high_resolution_clock::now();
    double creation_time = std::chrono::duration<double, std::milli>(
        after_create - start).count();

    std::printf("  ✓ Созданы %d сущностей за %.2f ms\n", ENTITY_COUNT, creation_time);

    // Установка параметров с зависимостями
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        params.set_parameter(entities[i], "value_a", 10.0 * i);
        params.set_parameter(entities[i], "value_b", 20.0 * i);

        // Добавляем зависимости каждые 100 элементов
        if (i > 0 && i % 100 == 0) {
            deps.add_dependency(entities[i], "value_a",
                              entities[i-1], "value_a");
        }
    }

    auto after_params = std::chrono::high_resolution_clock::now();
    double params_time = std::chrono::duration<double, std::milli>(
        after_params - after_create).count();

    std::printf("  ✓ Установлены параметры и зависимости за %.2f ms\n", params_time);

    // Создание иерархии (дерево глубины ~4)
    for (int i = 1; i < ENTITY_COUNT; ++i) {
        int parent_idx = i / 10;  // Каждые 10 элементов под одним родителем
        if (parent_idx > 0 && parent_idx < ENTITY_COUNT) {
            doc.set_parent(entities[i], entities[parent_idx]);
        } else {
            doc.set_parent(entities[i], entities[0]);
        }
    }

    auto after_hier = std::chrono::high_resolution_clock::now();
    double hier_time = std::chrono::duration<double, std::milli>(
        after_hier - after_params).count();

    std::printf("  ✓ Создана иерархия за %.2f ms\n", hier_time);

    // Проверка валидности графа зависимостей
    bool has_cycle = deps.has_cycle();

    auto end = std::chrono::high_resolution_clock::now();
    double total_time = std::chrono::duration<double, std::milli>(end - start).count();

    bool passed = !has_cycle && total_time < 30000;
    const char* msg = passed ? "✓ PASS" : "✗ FAIL";

    std::printf("\n  Итого: %.2f ms (Creation: %.1f%%, Params: %.1f%%, Hierarchy: %.1f%%)\n",
                total_time,
                creation_time * 100 / total_time,
                params_time * 100 / total_time,
                hier_time * 100 / total_time);

    std::printf("  Граф без циклов: %s\n", has_cycle ? "✗ FAIL" : "✓ PASS");
    std::printf("  Статус: %s\n", msg);

    record_result(ENTITY_COUNT, creation_time, params_time, hier_time, passed, msg);
}

// ============================================================================
// STRESS TEST 3: 10000 элементов
// ============================================================================

void stress_test_10k_entities() {
    const int ENTITY_COUNT = 10000;
    print_header("10000 элементов", ENTITY_COUNT);

    CadDocument doc;
    ParameterStore params;
    std::vector<EntityId> entities;

    auto start = std::chrono::high_resolution_clock::now();

    // Создание сущностей
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        EntityId e = EntityId::create(EntityType::Part);
        doc.add_entity(e, std::string("P") + std::to_string(i));
        entities.push_back(e);
    }

    auto after_create = std::chrono::high_resolution_clock::now();
    double creation_time = std::chrono::duration<double, std::milli>(
        after_create - start).count();

    std::printf("  ✓ Созданы %d сущностей за %.2f ms\n", ENTITY_COUNT, creation_time);

    // Установка параметров (минимальная информация)
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        params.set_parameter(entities[i], "id", static_cast<double>(i));
    }

    auto after_params = std::chrono::high_resolution_clock::now();
    double params_time = std::chrono::duration<double, std::milli>(
        after_params - after_create).count();

    std::printf("  ✓ Установлены параметры за %.2f ms\n", params_time);

    // Создание широкой плоской иерархии (все под одним корнем)
    for (int i = 1; i < ENTITY_COUNT; ++i) {
        doc.set_parent(entities[i], entities[0]);
    }

    auto after_hier = std::chrono::high_resolution_clock::now();
    double hier_time = std::chrono::duration<double, std::milli>(
        after_hier - after_params).count();

    std::printf("  ✓ Создана плоская иерархия за %.2f ms\n", hier_time);

    // Проверки доступа
    auto children = doc.get_children(entities[0]);
    bool access_ok = children.size() == ENTITY_COUNT - 1;

    auto end = std::chrono::high_resolution_clock::now();
    double total_time = std::chrono::duration<double, std::milli>(end - start).count();

    bool passed = access_ok && total_time < 60000;
    const char* msg = passed ? "✓ PASS" : "✗ FAIL";

    std::printf("\n  Дочерних сущностей: %zu (ожидается %d)\n", children.size(), ENTITY_COUNT - 1);
    std::printf("  Итого: %.2f ms\n", total_time);
    std::printf("  Статус: %s\n", msg);

    record_result(ENTITY_COUNT, creation_time, params_time, hier_time, passed, msg);
}

// ============================================================================
// STRESS TEST 4: 50000 элементов (big assembly)
// ============================================================================

void stress_test_50k_entities() {
    const int ENTITY_COUNT = 50000;
    print_header("50000 элементов (Big Assembly)", ENTITY_COUNT);

    CadDocument doc;
    ParameterStore params;
    std::vector<EntityId> entities;

    auto start = std::chrono::high_resolution_clock::now();

    // Создание сущностей (экономичное в памяти)
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        if (i % 10000 == 0) {
            std::printf("  ... создано %d сущностей\n", i);
        }
        EntityId e = EntityId::create(EntityType::Part);
        doc.add_entity(e, std::to_string(i));  // Минимальное имя
        entities.push_back(e);
    }

    auto after_create = std::chrono::high_resolution_clock::now();
    double creation_time = std::chrono::duration<double, std::milli>(
        after_create - start).count();

    std::printf("  ✓ Созданы %d сущностей за %.2f ms (%.3f ms/entity)\n",
                ENTITY_COUNT, creation_time, creation_time / ENTITY_COUNT);

    // Минимальная установка параметров
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        if (i % 10000 == 0) {
            std::printf("  ... параметры установлены для %d сущностей\n", i);
        }
        params.set_parameter(entities[i], "i", static_cast<double>(i));
    }

    auto after_params = std::chrono::high_resolution_clock::now();
    double params_time = std::chrono::duration<double, std::milli>(
        after_params - after_create).count();

    std::printf("  ✓ Установлены параметры за %.2f ms\n", params_time);

    // Иерархия: дерево с ветвлением ~20 (50000 / 20 / 20 = 125 уровней)
    for (int i = 1; i < ENTITY_COUNT; ++i) {
        if (i % 10000 == 0) {
            std::printf("  ... иерархия для %d сущностей\n", i);
        }
        int parent_idx = (i - 1) / 20;  // ~20 детей на одного родителя
        if (parent_idx < entities.size()) {
            doc.set_parent(entities[i], entities[parent_idx]);
        }
    }

    auto after_hier = std::chrono::high_resolution_clock::now();
    double hier_time = std::chrono::duration<double, std::milli>(
        after_hier - after_params).count();

    std::printf("  ✓ Создана иерархия за %.2f ms\n", hier_time);

    // Выборочные проверки
    bool access_ok = true;
    for (int i = 0; i < 100; ++i) {
        int idx = i * 500;  // Проверяем каждый 500-й
        auto parent = doc.get_parent(entities[idx]);
        if (idx > 0 && parent.id().empty()) {
            access_ok = false;
            break;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double total_time = std::chrono::duration<double, std::milli>(end - start).count();

    bool passed = access_ok && total_time < 300000;  // 5 минут максимум
    const char* msg = passed ? "✓ PASS" : "✗ FAIL";

    std::printf("\n  Проверка иерархии: %s\n", access_ok ? "✓ OK" : "✗ FAIL");
    std::printf("  Итого: %.2f ms (%.2f сек)\n", total_time, total_time / 1000);
    std::printf("  Статус: %s\n", msg);

    record_result(ENTITY_COUNT, creation_time, params_time, hier_time, passed, msg);
}

// ============================================================================
// ГЛАВНАЯ ФУНКЦИЯ ТЕСТОВ
// ============================================================================

int main() {
    std::printf("╔════════════════════════════════════════════════════════════════╗\n");
    std::printf("║  CAD-CORE-035: StressTests — Тесты производительности           ║\n");
    std::printf("║  Версия: 1.0                                                   ║\n");
    std::printf("║  Дата: 2026-10-25                                              ║\n");
    std::printf("╚════════════════════════════════════════════════════════════════╝\n");

    // Запускаем тесты масштабируемости
    stress_test_1k_entities();
    stress_test_5k_entities();
    stress_test_10k_entities();
    stress_test_50k_entities();

    // Выводим итоговый отчёт
    std::printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    std::printf("║  ИТОГОВЫЙ ОТЧЁТ STRESS TESTING                                 ║\n");
    std::printf("╠════════════════════════════════════════════════════════════════╣\n");

    int total_passed = 0;
    for (const auto& result : results) {
        std::printf("║  %d сущностей: %.0f ms total | %s\n",
                    result.entity_count,
                    result.total_time_ms,
                    result.passed ? "✓ PASS" : "✗ FAIL");
        if (result.passed) total_passed++;
    }

    std::printf("╠════════════════════════════════════════════════════════════════╣\n");
    std::printf("║  Успешно: %d/%zu тестов\n", total_passed, results.size());
    std::printf("╚════════════════════════════════════════════════════════════════╝\n");

    return (total_passed == results.size()) ? 0 : 1;
}
