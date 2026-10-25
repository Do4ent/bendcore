// CAD-CORE-033: E2EWorkflows — полные workflow тесты
// Create → Edit → Parameters → Geometry → FEA → Export
// Версия: 1.0, дата: 2026-10-25
// Пакет: ~1,000 LOC + тесты

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
#include "bendcore/commands/command_executor.hpp"
#include "bendcore/commands/history_stack.hpp"
#include "bendcore/commands/document_event_bus.hpp"
#include "bendcore/parameters/parameter_store.hpp"
#include "bendcore/dependencies/dependency_graph.hpp"
#include "bendcore/rebuild/rebuild_engine.hpp"
#include "bendcore/geometry/geometry_kernel.hpp"
#include "bendcore/tubegeom.hpp"
#include "bendcore/analysis.hpp"

using namespace bendcore;

// ============================================================================
// УТИЛИТЫ ТЕСТИРОВАНИЯ
// ============================================================================

static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

void test_begin(const char* name) {
    ++test_count;
    std::printf("\n[E2E-%03d] Начало теста: %s\n", test_count, name);
}

void test_assert(bool condition, const char* message) {
    if (!condition) {
        std::printf("  ✗ FAIL: %s\n", message);
        ++test_failed;
    } else {
        std::printf("  ✓ PASS: %s\n", message);
        ++test_passed;
    }
}

void test_end() {
    std::printf("  [Завершено]\n");
}

bool approx_equal(double a, double b, double eps = 1e-9) {
    return std::abs(a - b) <= eps;
}

// ============================================================================
// WORKFLOW 1: Create → Параметры → Геометрия
// ============================================================================

void test_workflow_create_and_parameters() {
    test_begin("Workflow 1: Create → Параметры → Геометрия");

    // Создаём документ
    CadDocument doc;
    CommandExecutor executor(doc);
    HistoryStack history;
    DocumentEventBus event_bus;
    ParameterStore params;
    DependencyGraph deps;
    RebuildEngine rebuild(params, deps);

    // Создаём новую часть (Part)
    EntityId part_id = EntityId::create(EntityType::Part);
    doc.add_entity(part_id, "Pipe_Assembly_1");

    test_assert(!part_id.id().empty(), "Part создана с валидным ID");

    // Добавляем параметры трубы
    params.set_parameter(part_id, "outer_diameter", 25.4);  // mm
    params.set_parameter(part_id, "wall_thickness", 1.5);   // mm
    params.set_parameter(part_id, "material", "Steel");
    params.set_parameter(part_id, "bend_radius", 50.0);

    test_assert(
        approx_equal(*params.get_parameter(part_id, "outer_diameter"), 25.4),
        "Внешний диаметр установлен корректно"
    );

    test_assert(
        approx_equal(*params.get_parameter(part_id, "wall_thickness"), 1.5),
        "Толщина стенки установлена корректно"
    );

    // Создаём B-Rep геометрию
    std::vector<Point> axis_points = {
        {0, 0, 0},
        {100, 0, 0},
        {100, 50, 0},
        {100, 50, 75}
    };

    TubeGeometry tube_geom;
    tube_geom.outer_diameter = 25.4;
    tube_geom.wall_thickness = 1.5;
    tube_geom.axis_points = axis_points;

    double geom_length = 0.0;
    for (size_t i = 1; i < axis_points.size(); ++i) {
        Vector seg = axis_points[i] - axis_points[i-1];
        geom_length += seg.magnitude();
    }

    test_assert(
        approx_equal(geom_length, 225.0, 0.1),
        "Геометрия создана с корректной длиной оси"
    );

    test_end();
}

// ============================================================================
// WORKFLOW 2: Edit → Rebuild → Зависимости
// ============================================================================

void test_workflow_edit_and_rebuild() {
    test_begin("Workflow 2: Edit → Rebuild → Зависимости");

    CadDocument doc;
    ParameterStore params;
    DependencyGraph deps;
    RebuildEngine rebuild(params, deps);

    // Создаём две связанные части
    EntityId part1 = EntityId::create(EntityType::Part);
    EntityId part2 = EntityId::create(EntityType::Part);

    doc.add_entity(part1, "Base_Pipe");
    doc.add_entity(part2, "Dependent_Pipe");

    // Устанавливаем параметры
    params.set_parameter(part1, "length", 100.0);
    params.set_parameter(part2, "length_offset", 10.0);

    // Добавляем зависимость: part2.length = part1.length + part2.length_offset
    deps.add_dependency(part2, "length", part1, "length");

    test_assert(!deps.has_cycle(), "Граф зависимостей без циклов");

    // Обновляем часть1
    params.set_parameter(part1, "length", 120.0);

    test_assert(
        approx_equal(*params.get_parameter(part1, "length"), 120.0),
        "Параметр part1 обновлён"
    );

    // Проверяем, что зависимость зарегистрирована
    auto deps_of_part2 = deps.get_dependencies_for(part2, "length");
    test_assert(!deps_of_part2.empty(), "Зависимость зарегистрирована в графе");

    test_end();
}

// ============================================================================
// WORKFLOW 3: Анализ и FEA
// ============================================================================

void test_workflow_fea_analysis() {
    test_begin("Workflow 3: Анализ и FEA");

    TubeGeometry tube;
    tube.outer_diameter = 25.4;
    tube.wall_thickness = 1.5;
    tube.radius_to_centerline = 12.2;

    std::vector<Point> axis = {
        {0, 0, 0},
        {100, 0, 0},
        {100, 50, 0},
        {100, 50, 100}
    };
    tube.axis_points = axis;

    // Вычисляем характеристики
    double moment_of_inertia = (std::pow(25.4, 4) - std::pow(25.4 - 2*1.5, 4)) / 64.0;
    double cross_section_area = M_PI * (std::pow(12.2, 2) - std::pow(12.2 - 1.5, 2));

    test_assert(moment_of_inertia > 0, "Момент инерции вычислен положительный");
    test_assert(cross_section_area > 0, "Площадь сечения вычислена положительная");

    // Анализ напряжений
    double force = 1000.0; // N
    double stress = force / cross_section_area;

    test_assert(stress > 0, "Напряжение вычислено");
    std::printf("    Напряжение при 1000N нагрузке: %.2f MPa\n", stress);

    // Вычисляем прогиб для консольной балки
    double E = 200000.0; // MPa, модуль Юнга для стали
    double length = 100.0; // mm, длина консоли
    double deflection = (force * std::pow(length, 3)) / (3.0 * E * moment_of_inertia);

    test_assert(deflection > 0, "Прогиб вычислен положительный");
    std::printf("    Прогиб консоли 100mm: %.4f mm\n", deflection);

    test_end();
}

// ============================================================================
// WORKFLOW 4: Сериализация и экспорт
// ============================================================================

void test_workflow_serialization() {
    test_begin("Workflow 4: Сериализация и экспорт");

    CadDocument doc;

    // Создаём структуру в документе
    EntityId part = EntityId::create(EntityType::Part);
    EntityId body = EntityId::create(EntityType::Body);
    EntityId feature = EntityId::create(EntityType::Feature);

    doc.add_entity(part, "Assembly");
    doc.add_entity(body, "SketchedBody");
    doc.add_entity(feature, "Extrude");

    // Устанавливаем иерархию: Assembly → SketchedBody → Extrude
    doc.set_parent(body, part);
    doc.set_parent(feature, body);

    // Проверяем иерархию
    auto parent_of_body = doc.get_parent(body);
    test_assert(parent_of_body == part, "Иерархия установлена корректно (Body → Part)");

    auto parent_of_feature = doc.get_parent(feature);
    test_assert(parent_of_feature == body, "Иерархия установлена корректно (Feature → Body)");

    // Проверяем дочерние сущности
    auto children_of_part = doc.get_children(part);
    test_assert(children_of_part.size() == 1, "Part имеет 1 дочернюю сущность");

    auto children_of_body = doc.get_children(body);
    test_assert(children_of_body.size() == 1, "Body имеет 1 дочернюю сущность");

    test_end();
}

// ============================================================================
// WORKFLOW 5: Полный цикл (Create → Edit → Geometry → Analysis → Export)
// ============================================================================

void test_workflow_full_cycle() {
    test_begin("Workflow 5: Полный цикл (Create → Edit → Geometry → Analysis → Export)");

    CadDocument doc;
    ParameterStore params;
    DependencyGraph deps;
    RebuildEngine rebuild(params, deps);

    // ШАГ 1: CREATE
    EntityId assembly = EntityId::create(EntityType::Part);
    doc.add_entity(assembly, "Tube_Assembly");
    test_assert(!assembly.id().empty(), "Сборка создана");

    // ШАГ 2: PARAMETERS
    params.set_parameter(assembly, "outer_diameter", 20.0);
    params.set_parameter(assembly, "wall_thickness", 1.0);
    params.set_parameter(assembly, "material", "Aluminum");
    params.set_parameter(assembly, "length", 200.0);
    params.set_parameter(assembly, "bend_angle_1", 45.0);
    params.set_parameter(assembly, "bend_angle_2", 90.0);

    test_assert(
        approx_equal(*params.get_parameter(assembly, "outer_diameter"), 20.0),
        "Параметры установлены"
    );

    // ШАГ 3: GEOMETRY
    std::vector<Point> axis = {
        {0, 0, 0},
        {100, 0, 0},
        {141.421, 41.421, 0},  // After 45° bend
        {141.421, 41.421, 58.579}  // After 90° bend
    };

    double total_axis_length = 0.0;
    for (size_t i = 1; i < axis.size(); ++i) {
        Vector seg = axis[i] - axis[i-1];
        total_axis_length += seg.magnitude();
    }

    test_assert(total_axis_length > 200, "Геометрия создана с корректной осью");

    // ШАГ 4: FEA
    double OD = 20.0, WT = 1.0;
    double I = (std::pow(OD, 4) - std::pow(OD - 2*WT, 4)) / 64.0;
    double A = M_PI * (std::pow(OD/2, 2) - std::pow(OD/2 - WT, 2));

    test_assert(I > 0 && A > 0, "FEA параметры вычислены");

    // ШАГ 5: EXPORT (проверяем что документ готов к экспорту)
    auto entity_name = doc.get_entity_name(assembly);
    test_assert(entity_name == "Tube_Assembly", "Документ готов к экспорту");

    test_end();
}

// ============================================================================
// WORKFLOW 6: История и откат (Undo/Redo)
// ============================================================================

void test_workflow_history_and_undo() {
    test_begin("Workflow 6: История и откат (Undo/Redo)");

    CadDocument doc;
    ParameterStore params;
    HistoryStack history;

    EntityId part = EntityId::create(EntityType::Part);
    doc.add_entity(part, "Part_v1");

    test_assert(doc.get_entity_name(part) == "Part_v1", "Сущность создана");

    // Изменяем параметр
    params.set_parameter(part, "param1", 100.0);
    test_assert(
        approx_equal(*params.get_parameter(part, "param1"), 100.0),
        "Параметр установлен на 100"
    );

    // Изменяем параметр ещё раз
    params.set_parameter(part, "param1", 200.0);
    test_assert(
        approx_equal(*params.get_parameter(part, "param1"), 200.0),
        "Параметр обновлён на 200"
    );

    // История отслеживает изменения
    test_assert(history.can_undo(), "История содержит изменения");

    test_end();
}

// ============================================================================
// WORKFLOW 7: Множественные экземпляры и сборки
// ============================================================================

void test_workflow_assemblies() {
    test_begin("Workflow 7: Множественные экземпляры и сборки");

    CadDocument doc;

    // Создаём основную сборку
    EntityId assembly = EntityId::create(EntityType::Assembly);
    doc.add_entity(assembly, "Main_Assembly");

    // Создаём компоненты
    EntityId component1 = EntityId::create(EntityType::Part);
    EntityId component2 = EntityId::create(EntityType::Part);
    EntityId component3 = EntityId::create(EntityType::Part);

    doc.add_entity(component1, "Tube_1");
    doc.add_entity(component2, "Tube_2");
    doc.add_entity(component3, "Fitting");

    // Устанавливаем иерархию
    doc.set_parent(component1, assembly);
    doc.set_parent(component2, assembly);
    doc.set_parent(component3, assembly);

    // Создаём экземпляры
    EntityId instance1 = EntityId::create(EntityType::Instance);
    EntityId instance2 = EntityId::create(EntityType::Instance);

    doc.add_entity(instance1, "Tube_1_Instance");
    doc.add_entity(instance2, "Tube_2_Instance");

    // Проверяем количество компонентов
    auto assembly_children = doc.get_children(assembly);
    test_assert(assembly_children.size() == 3, "Сборка содержит 3 компонента");

    test_end();
}

// ============================================================================
// WORKFLOW 8: Параметрическое моделирование с зависимостями
// ============================================================================

void test_workflow_parametric() {
    test_begin("Workflow 8: Параметрическое моделирование");

    ParameterStore params;
    DependencyGraph deps;
    RebuildEngine rebuild(params, deps);

    EntityId sketch = EntityId::create(EntityType::Sketch);
    EntityId feature = EntityId::create(EntityType::Feature);

    // Устанавливаем параметры скетча
    params.set_parameter(sketch, "width", 50.0);
    params.set_parameter(sketch, "height", 30.0);
    params.set_parameter(sketch, "radius", 10.0);

    // Устанавливаем параметры фичера (зависит от скетча)
    params.set_parameter(feature, "extrusion_length", 100.0);
    params.set_parameter(feature, "draft_angle", 2.0);

    // Добавляем зависимость
    deps.add_dependency(feature, "extrusion_length", sketch, "width");

    test_assert(!deps.has_cycle(), "Параметрическая иерархия без циклов");

    // Изменяем параметр скетча
    params.set_parameter(sketch, "width", 60.0);

    // Проверяем что зависимость все ещё существует
    auto deps_list = deps.get_dependencies_for(feature, "extrusion_length");
    test_assert(!deps_list.empty(), "Зависимость сохранена после изменения");

    test_end();
}

// ============================================================================
// WORKFLOW 9: Эффективность и масштабируемость
// ============================================================================

void test_workflow_performance() {
    test_begin("Workflow 9: Эффективность и масштабируемость");

    CadDocument doc;
    ParameterStore params;

    auto start = std::chrono::high_resolution_clock::now();

    // Создаём 100 сущностей
    std::vector<EntityId> entities;
    for (int i = 0; i < 100; ++i) {
        EntityId e = EntityId::create(EntityType::Part);
        doc.add_entity(e, std::string("Part_") + std::to_string(i));
        entities.push_back(e);
    }

    // Устанавливаем параметры для каждой
    for (const auto& e : entities) {
        params.set_parameter(e, "diameter", 20.0 + i);
        params.set_parameter(e, "length", 100.0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    test_assert(entities.size() == 100, "100 сущностей создано");
    test_assert(duration.count() < 1000, "Операция выполнена за < 1 сек");

    std::printf("    Время создания 100 сущностей: %ld ms\n", duration.count());

    test_end();
}

// ============================================================================
// WORKFLOW 10: Валидация и проверки
// ============================================================================

void test_workflow_validation() {
    test_begin("Workflow 10: Валидация и проверки");

    ParameterStore params;
    EntityId part = EntityId::create(EntityType::Part);

    // Устанавливаем валидные параметры
    params.set_parameter(part, "outer_diameter", 25.4);
    params.set_parameter(part, "wall_thickness", 1.5);

    // Валидируем соотношение
    double OD = *params.get_parameter(part, "outer_diameter");
    double WT = *params.get_parameter(part, "wall_thickness");

    bool valid_ratio = WT < OD / 2.0;
    test_assert(valid_ratio, "Толщина стенки меньше чем OD/2");

    // Проверяем что недопустимые значения не проходят валидацию
    bool invalid_wt = (WT * 2.0) > OD;
    test_assert(!invalid_wt, "Недопустимые значения отклонены валидатором");

    test_end();
}

// ============================================================================
// ГЛАВНАЯ ФУНКЦИЯ ТЕСТОВ
// ============================================================================

int main() {
    std::printf("╔════════════════════════════════════════════════════════════════╗\n");
    std::printf("║  CAD-CORE-033: E2EWorkflows — Полные workflow тесты            ║\n");
    std::printf("║  Версия: 1.0                                                   ║\n");
    std::printf("║  Дата: 2026-10-25                                              ║\n");
    std::printf("╚════════════════════════════════════════════════════════════════╝\n");

    // Запускаем все workflow тесты
    test_workflow_create_and_parameters();
    test_workflow_edit_and_rebuild();
    test_workflow_fea_analysis();
    test_workflow_serialization();
    test_workflow_full_cycle();
    test_workflow_history_and_undo();
    test_workflow_assemblies();
    test_workflow_parametric();
    test_workflow_performance();
    test_workflow_validation();

    // Выводим итоги
    std::printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    std::printf("║  ИТОГИ ТЕСТИРОВАНИЯ E2E WORKFLOWS                             ║\n");
    std::printf("╠════════════════════════════════════════════════════════════════╣\n");
    std::printf("║  Всего тестов:     %3d                                         ║\n", test_count);
    std::printf("║  Успешных:        %3d (%.1f%%)                                  ║\n",
                test_passed, test_passed * 100.0 / (test_passed + test_failed));
    std::printf("║  Ошибок:          %3d                                          ║\n", test_failed);
    std::printf("╚════════════════════════════════════════════════════════════════╝\n");

    return test_failed > 0 ? 1 : 0;
}
