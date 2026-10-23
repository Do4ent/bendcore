// Тесты для менеджера слоёв и аннотаций
#include "bendcore/drawing_layers.hpp"

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

static void test_initialization() {
    TEST("Инициализация менеджера слоёв");

    DrawingLayerManager manager;

    CHECK(manager.layer_count() == 3, "Созданы 3 стандартных слоя");
    CHECK(manager.annotation_count() == 0, "Нет аннотаций после инициализации");
}

static void test_add_layer() {
    TEST("Добавление слоя");

    DrawingLayerManager manager;
    size_t initial_count = manager.layer_count();

    DrawingLayerManager::Layer layer;
    layer.id = "test_layer";
    layer.name = "Тестовый слой";
    layer.visible = true;
    layer.editable = true;

    manager.add_layer(layer);

    CHECK(manager.layer_count() == initial_count + 1, "Слой добавлен");

    auto retrieved = manager.get_layer("test_layer");
    CHECK(retrieved.has_value(), "Слой найден");
    if (retrieved) {
        CHECK(retrieved->name == "Тестовый слой", "Имя слоя совпадает");
    }
}

static void test_remove_layer() {
    TEST("Удаление слоя");

    DrawingLayerManager manager;

    DrawingLayerManager::Layer layer;
    layer.id = "test_layer";
    layer.name = "Тестовый слой";

    manager.add_layer(layer);
    size_t after_add = manager.layer_count();

    manager.remove_layer("test_layer");

    CHECK(manager.layer_count() == after_add - 1, "Слой удалён");
    CHECK(!manager.get_layer("test_layer").has_value(), "Слой не найден после удаления");
}

static void test_rename_layer() {
    TEST("Переименование слоя");

    DrawingLayerManager manager;

    DrawingLayerManager::Layer layer;
    layer.id = "test_layer";
    layer.name = "Старое имя";

    manager.add_layer(layer);
    manager.rename_layer("test_layer", "Новое имя");

    auto retrieved = manager.get_layer("test_layer");
    CHECK(retrieved.has_value(), "Слой найден");
    if (retrieved) {
        CHECK(retrieved->name == "Новое имя", "Имя слоя изменено");
    }
}

static void test_toggle_visibility() {
    TEST("Переключение видимости слоя");

    DrawingLayerManager manager;

    DrawingLayerManager::Layer layer;
    layer.id = "test_layer";
    layer.name = "Тестовый слой";
    layer.visible = true;

    manager.add_layer(layer);

    auto before = manager.get_layer("test_layer");
    CHECK(before && before->visible, "Слой видимый изначально");

    manager.toggle_layer_visibility("test_layer");

    auto after = manager.get_layer("test_layer");
    CHECK(after && !after->visible, "Слой невидимый после переключения");

    manager.toggle_layer_visibility("test_layer");
    after = manager.get_layer("test_layer");
    CHECK(after && after->visible, "Слой видимый после второго переключения");
}

static void test_layer_locking() {
    TEST("Блокировка слоя");

    DrawingLayerManager manager;

    DrawingLayerManager::Layer layer;
    layer.id = "test_layer";
    layer.name = "Тестовый слой";
    layer.locked = false;

    manager.add_layer(layer);
    manager.set_layer_locked("test_layer", true);

    auto retrieved = manager.get_layer("test_layer");
    CHECK(retrieved && retrieved->locked, "Слой заблокирован");
}

static void test_add_element_to_layer() {
    TEST("Добавление элемента в слой");

    DrawingLayerManager manager;

    manager.add_element_to_layer("layer_contours", "element_1");

    auto elements = manager.get_elements_in_layer("layer_contours");
    CHECK(!elements.empty(), "Элемент добавлен в слой");
    CHECK(elements[0] == "element_1", "Правильный элемент в слое");
}

static void test_remove_element_from_layer() {
    TEST("Удаление элемента из слоя");

    DrawingLayerManager manager;

    manager.add_element_to_layer("layer_contours", "element_1");
    manager.remove_element_from_layer("layer_contours", "element_1");

    auto elements = manager.get_elements_in_layer("layer_contours");
    CHECK(elements.empty(), "Элемент удалён из слоя");
}

static void test_get_layer_for_element() {
    TEST("Получение слоя по элементу");

    DrawingLayerManager manager;

    manager.add_element_to_layer("layer_contours", "element_1");

    auto layer_id = manager.get_layer_for_element("element_1");
    CHECK(layer_id.has_value(), "Слой найден по элементу");
    if (layer_id) {
        CHECK(layer_id.value() == "layer_contours", "Правильный слой");
    }
}

static void test_add_annotation() {
    TEST("Добавление аннотации");

    DrawingLayerManager manager;

    DrawingLayerManager::Annotation annot;
    annot.id = "annot_1";
    annot.type = "dimension";
    annot.text = "100 mm";
    annot.x = 50.0;
    annot.y = 50.0;
    annot.layer_id = "layer_dimensions";

    manager.add_annotation(annot);

    CHECK(manager.annotation_count() == 1, "Аннотация добавлена");

    auto retrieved = manager.get_annotation("annot_1");
    CHECK(retrieved.has_value(), "Аннотация найдена");
    if (retrieved) {
        CHECK(retrieved->text == "100 mm", "Текст аннотации совпадает");
    }
}

static void test_remove_annotation() {
    TEST("Удаление аннотации");

    DrawingLayerManager manager;

    DrawingLayerManager::Annotation annot;
    annot.id = "annot_1";
    annot.type = "dimension";
    annot.text = "100 mm";
    annot.layer_id = "layer_dimensions";

    manager.add_annotation(annot);
    manager.remove_annotation("annot_1");

    CHECK(manager.annotation_count() == 0, "Аннотация удалена");
    CHECK(!manager.get_annotation("annot_1").has_value(), "Аннотация не найдена");
}

static void test_get_annotations_for_layer() {
    TEST("Получение аннотаций по слою");

    DrawingLayerManager manager;

    DrawingLayerManager::Annotation annot1;
    annot1.id = "annot_1";
    annot1.type = "dimension";
    annot1.layer_id = "layer_dimensions";

    DrawingLayerManager::Annotation annot2;
    annot2.id = "annot_2";
    annot2.type = "note";
    annot2.layer_id = "layer_annotations";

    manager.add_annotation(annot1);
    manager.add_annotation(annot2);

    auto dims = manager.get_annotations_for_layer("layer_dimensions");
    CHECK(dims.size() == 1, "Найдена одна размерная аннотация");

    auto notes = manager.get_annotations_for_layer("layer_annotations");
    CHECK(notes.size() == 1, "Найдена одна записка");
}

static void test_get_annotations_by_type() {
    TEST("Получение аннотаций по типу");

    DrawingLayerManager manager;

    DrawingLayerManager::Annotation annot1;
    annot1.id = "annot_1";
    annot1.type = "dimension";
    annot1.layer_id = "layer_dimensions";

    DrawingLayerManager::Annotation annot2;
    annot2.id = "annot_2";
    annot2.type = "dimension";
    annot2.layer_id = "layer_dimensions";

    manager.add_annotation(annot1);
    manager.add_annotation(annot2);

    auto dimensions = manager.get_annotations_by_type("dimension");
    CHECK(dimensions.size() == 2, "Найдены две размерные аннотации");
}

static void test_move_annotation() {
    TEST("Перемещение аннотации");

    DrawingLayerManager manager;

    DrawingLayerManager::Annotation annot;
    annot.id = "annot_1";
    annot.type = "note";
    annot.x = 0.0;
    annot.y = 0.0;
    annot.layer_id = "layer_annotations";

    manager.add_annotation(annot);
    manager.move_annotation("annot_1", 100.0, 150.0);

    auto retrieved = manager.get_annotation("annot_1");
    CHECK(retrieved && retrieved->x == 100.0 && retrieved->y == 150.0, "Аннотация перемещена");
}

static void test_layer_group() {
    TEST("Создание группы слоёв");

    DrawingLayerManager manager;

    DrawingLayerManager::LayerGroup group;
    group.id = "group_1";
    group.name = "Группа 1";
    group.layer_ids = {"layer_contours", "layer_dimensions"};

    manager.create_layer_group(group);

    auto retrieved = manager.get_layer_group("group_1");
    CHECK(retrieved.has_value(), "Группа создана");
    if (retrieved) {
        CHECK(retrieved->layer_ids.size() == 2, "Группа содержит 2 слоя");
    }
}

static void test_clear_manager() {
    TEST("Очистка менеджера");

    DrawingLayerManager manager;

    DrawingLayerManager::Layer layer;
    layer.id = "test_layer";
    layer.name = "Тестовый слой";

    DrawingLayerManager::Annotation annot;
    annot.id = "annot_1";
    annot.type = "note";
    annot.layer_id = "layer_annotations";

    manager.add_layer(layer);
    manager.add_annotation(annot);

    manager.clear();

    CHECK(manager.layer_count() == 0, "Все слои удалены");
    CHECK(manager.annotation_count() == 0, "Все аннотации удалены");
}

int main() {
    test_initialization();
    test_add_layer();
    test_remove_layer();
    test_rename_layer();
    test_toggle_visibility();
    test_layer_locking();
    test_add_element_to_layer();
    test_remove_element_from_layer();
    test_get_layer_for_element();
    test_add_annotation();
    test_remove_annotation();
    test_get_annotations_for_layer();
    test_get_annotations_by_type();
    test_move_annotation();
    test_layer_group();
    test_clear_manager();

    printf("\n=== Результаты ===\n");
    printf("Всего тестов: %d\n", test_count);
    printf("Ошибок: %d\n", test_failures);
    printf("Пройдено: %d\n", test_count - test_failures);

    return test_failures > 0 ? 1 : 0;
}
