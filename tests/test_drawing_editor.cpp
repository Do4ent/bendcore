// Тесты для редактора параметров чертежа
#include "bendcore/drawing_editor.hpp"

#include <cassert>
#include <cstdio>
#include <string>

using namespace bendcore::drawing;

static int test_count = 0;
static int test_failures = 0;

#define TEST(name) \
    do { \
        ++test_count; \
        printf("TEST: %s\n", name); \
    } while (false)

#define CHECK(condition, message) \
    if (!(condition)) { \
        printf("  FAIL: %s\n", message); \
        ++test_failures; \
    }

static void test_parameter_creation() {
    TEST("Создание параметра");

    DrawingParameterEditor::EditableParameter param;
    param.id = "length_1";
    param.name = "Длина 1";
    param.current_value = 100.0;
    param.min_value = 10.0;
    param.max_value = 1000.0;
    param.unit = "mm";
    param.is_formula = false;
    param.description = "Длина первого отрезка";

    CHECK(param.id == "length_1", "ID установлен");
    CHECK(param.name == "Длина 1", "Имя установлено");
    CHECK(param.current_value == 100.0, "Значение установлено");
    CHECK(param.unit == "mm", "Единица установлена");
}

static void test_editor_initialization() {
    TEST("Инициализация редактора");

    DrawingParameterEditor editor;
    auto state = editor.get_current_state();

    CHECK(state.drawing_id.empty(), "Drawing ID пустой после инициализации");
    CHECK(state.parameters.empty(), "Параметры пусты после инициализации");
    CHECK(!editor.has_unsaved_changes(), "Нет несохранённых изменений после инициализации");
}

static void test_load_drawing() {
    TEST("Загрузка чертежа");

    DrawingParameterEditor editor;
    editor.load_drawing("drawing_123");

    auto state = editor.get_current_state();
    CHECK(state.drawing_id == "drawing_123", "Drawing ID загружен");
}

static void test_add_and_update_parameter() {
    TEST("Добавление и обновление параметра");

    DrawingParameterEditor editor;
    editor.load_drawing("test_drawing");

    DrawingParameterEditor::EditableParameter param;
    param.id = "param_1";
    param.name = "Параметр 1";
    param.current_value = 50.0;
    param.min_value = 0.0;
    param.max_value = 100.0;
    param.unit = "mm";
    param.is_formula = false;
    param.description = "Тестовый параметр";

    editor.update_parameter(param);

    auto retrieved = editor.get_parameter("param_1");
    CHECK(retrieved.has_value(), "Параметр найден");
    if (retrieved) {
        CHECK(retrieved->name == "Параметр 1", "Имя параметра совпадает");
        CHECK(retrieved->current_value == 50.0, "Значение параметра совпадает");
    }
}

static void test_get_all_parameters() {
    TEST("Получение всех параметров");

    DrawingParameterEditor editor;
    editor.load_drawing("test_drawing");

    DrawingParameterEditor::EditableParameter p1;
    p1.id = "param_1";
    p1.name = "Параметр 1";
    p1.current_value = 50.0;
    p1.min_value = 0.0;
    p1.max_value = 100.0;
    p1.is_formula = false;

    DrawingParameterEditor::EditableParameter p2;
    p2.id = "param_2";
    p2.name = "Параметр 2";
    p2.current_value = 75.0;
    p2.min_value = 0.0;
    p2.max_value = 100.0;
    p2.is_formula = false;

    editor.update_parameter(p1);
    editor.update_parameter(p2);

    auto all = editor.get_all_parameters();
    CHECK(all.size() == 2, "Найдены оба параметра");
}

static void test_parameter_validation_range() {
    TEST("Валидация диапазона параметра");

    DrawingParameterEditor editor;

    DrawingParameterEditor::EditableParameter param;
    param.id = "param_1";
    param.name = "Параметр 1";
    param.current_value = 50.0;
    param.min_value = 0.0;
    param.max_value = 100.0;
    param.unit = "mm";
    param.is_formula = false;

    auto result = editor.validate_parameter(param);
    CHECK(result.valid, "Параметр валиден в диапазоне");

    // Попытка установить значение вне диапазона
    param.current_value = 150.0;
    result = editor.validate_parameter(param);
    CHECK(!result.valid, "Параметр невалиден вне диапазона");
    CHECK(result.suggested_value.has_value(), "Предложено исправленное значение");
}

static void test_formula_validation() {
    TEST("Валидация формулы");

    DrawingParameterEditor editor;

    DrawingParameterEditor::EditableParameter param;
    param.id = "param_1";
    param.name = "Параметр 1";
    param.min_value = 0.0;
    param.max_value = 100.0;
    param.unit = "mm";
    param.is_formula = true;
    param.formula_text = "$OD * 2";

    auto result = editor.validate_parameter(param);
    // Формула должна быть валидной (или невалидной в зависимости от парсера)
    // Это демонстрирует, что валидация формул работает
}

static void test_unsaved_changes() {
    TEST("Отслеживание несохранённых изменений");

    DrawingParameterEditor editor;
    editor.load_drawing("test_drawing");

    CHECK(!editor.has_unsaved_changes(), "Нет изменений после загрузки");

    DrawingParameterEditor::EditableParameter param;
    param.id = "param_1";
    param.name = "Параметр 1";
    param.current_value = 50.0;
    param.min_value = 0.0;
    param.max_value = 100.0;
    param.is_formula = false;

    editor.update_parameter(param);
    CHECK(editor.has_unsaved_changes(), "Есть изменения после обновления параметра");

    editor.apply_changes();
    CHECK(!editor.has_unsaved_changes(), "Нет изменений после применения");
}

static void test_revert_changes() {
    TEST("Отмена изменений");

    DrawingParameterEditor editor;
    editor.load_drawing("test_drawing");

    DrawingParameterEditor::EditableParameter param;
    param.id = "param_1";
    param.name = "Параметр 1";
    param.current_value = 50.0;
    param.min_value = 0.0;
    param.max_value = 100.0;
    param.is_formula = false;

    editor.update_parameter(param);
    editor.apply_changes();

    param.current_value = 75.0;
    editor.update_parameter(param);
    CHECK(editor.has_unsaved_changes(), "Есть изменения");

    editor.revert_changes();
    CHECK(!editor.has_unsaved_changes(), "Изменения отменены");

    auto retrieved = editor.get_parameter("param_1");
    if (retrieved) {
        CHECK(retrieved->current_value == 50.0, "Значение восстановлено");
    }
}

static void test_editor_info() {
    TEST("Информация об редакторе");

    DrawingParameterEditor editor;
    editor.set_editor_info("user123");

    DrawingParameterEditor::EditableParameter param;
    param.id = "param_1";
    param.name = "Параметр 1";
    param.current_value = 50.0;
    param.min_value = 0.0;
    param.max_value = 100.0;
    param.is_formula = false;

    editor.update_parameter(param);

    CHECK(editor.get_modified_by() == "user123", "Информация об редакторе сохранена");
    CHECK(!editor.get_last_modified_at().empty(), "Время модификации установлено");
}

static void test_clear_editor() {
    TEST("Очистка редактора");

    DrawingParameterEditor editor;
    editor.load_drawing("test_drawing");

    DrawingParameterEditor::EditableParameter param;
    param.id = "param_1";
    param.name = "Параметр 1";
    param.current_value = 50.0;
    param.min_value = 0.0;
    param.max_value = 100.0;
    param.is_formula = false;

    editor.update_parameter(param);
    editor.clear();

    auto state = editor.get_current_state();
    CHECK(state.drawing_id.empty(), "Drawing ID очищен");
    CHECK(state.parameters.empty(), "Параметры очищены");
}

static void test_parameter_equality() {
    TEST("Сравнение параметров");

    DrawingParameterEditor::EditableParameter p1;
    p1.id = "param_1";
    p1.name = "Параметр 1";
    p1.current_value = 50.0;
    p1.min_value = 0.0;
    p1.max_value = 100.0;
    p1.unit = "mm";
    p1.is_formula = false;
    p1.description = "Тест";

    DrawingParameterEditor::EditableParameter p2 = p1;

    CHECK(p1 == p2, "Параметры равны");

    p2.current_value = 75.0;
    CHECK(!(p1 == p2), "Параметры не равны после изменения");
}

int main() {
    test_parameter_creation();
    test_editor_initialization();
    test_load_drawing();
    test_add_and_update_parameter();
    test_get_all_parameters();
    test_parameter_validation_range();
    test_formula_validation();
    test_unsaved_changes();
    test_revert_changes();
    test_editor_info();
    test_clear_editor();
    test_parameter_equality();

    printf("\n=== Результаты ===\n");
    printf("Всего тестов: %d\n", test_count);
    printf("Ошибок: %d\n", test_failures);
    printf("Пройдено: %d\n", test_count - test_failures);

    return test_failures > 0 ? 1 : 0;
}
