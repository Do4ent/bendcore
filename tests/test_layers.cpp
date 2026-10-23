// Unit тесты для системы слоёв и управления видимостью (Q2 2027, Task 3)
// Слои группируют элементы чертежа по типам: контур, размеры, оси, разметка, вспомогательная геометрия

#include "bendcore/drawing.hpp"

#include <cstdio>
#include <numbers>

using namespace bendcore::drawing;

static int failures = 0;
static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

// Вспомогательные функции для тестов
static Path L_path() {
    return {
        Straight{100},
        Bend{50, std::numbers::pi / 2, 0},
        Straight{100}
    };
}

static DrawingInput make_test_drawing_input() {
    DrawingInput in;
    in.path = L_path();
    in.od = 20;
    in.wall = 2;
    in.standard = Standard::GOST;
    in.format = "A3";
    in.landscape = true;
    in.preset = Preset::Minimal;
    in.dims = DimMode::S;
    in.precision = 1;
    in.title["Обозначение"] = "БК.001";
    in.title["Наименование"] = "Трубный узел";
    return in;
}

int main() {
    std::printf("test_layers: Тестирование системы слоёв (Q2 2027, Task 3)\n");

    // ===== Тест 1: Создание слоёв по умолчанию (Task 3.1, 3.3) =====
    {
        auto layers = make_default_layers();
        check(layers.size() == 5, "Созданы 5 слоёв");
        check(layers[0].type == LayerType::Contour, "Первый слой — контур");
        check(layers[0].name == "Контур", "Название слоя корректно");
        check(layers[0].visible == true, "Слой видим по умолчанию");
        check(layers[0].locked == false, "Слой не заблокирован по умолчанию");
        check(layers[0].color == "#000000", "Цвет контура — чёрный");

        check(layers[1].type == LayerType::Dimensions, "Второй слой — размеры");
        check(layers[1].color == "#0000FF", "Цвет размеров — синий");

        check(layers[2].type == LayerType::Axis, "Третий слой — оси");
        check(layers[2].color == "#FF0000", "Цвет осей — красный");

        check(layers[3].type == LayerType::Markup, "Четвёртый слой — разметка");
        check(layers[4].type == LayerType::ConstructionGeometry, "Пятый слой — вспомогательная геометрия");
    }

    // ===== Тест 2: Переключение видимости слоя (Task 3.4) =====
    {
        LayerVisibility vis = make_layer_visibility();

        check(vis.is_visible(LayerType::Contour) == true, "Контур видим по умолчанию");
        check(vis.is_visible(LayerType::Dimensions) == true, "Размеры видимы по умолчанию");

        vis.set_visible(LayerType::Dimensions, false);
        check(vis.is_visible(LayerType::Dimensions) == false, "Размеры скрыты после set_visible");
        check(vis.is_visible(LayerType::Contour) == true, "Другие слои не изменились");

        vis.set_visible(LayerType::Dimensions, true);
        check(vis.is_visible(LayerType::Dimensions) == true, "Размеры видимы после повторного включения");
    }

    // ===== Тест 3: Функции layer_name и layer_color (Task 3.2) =====
    {
        check(layer_name(LayerType::Contour) == std::string("Контур"), "layer_name(Contour)");
        check(layer_name(LayerType::Dimensions) == std::string("Размеры"), "layer_name(Dimensions)");
        check(layer_name(LayerType::Axis) == std::string("Оси"), "layer_name(Axis)");
        check(layer_name(LayerType::Markup) == std::string("Разметка"), "layer_name(Markup)");
        check(layer_name(LayerType::ConstructionGeometry) == std::string("Вспомогательное"), "layer_name(ConstructionGeometry)");

        check(layer_color(LayerType::Contour) == std::string("#000000"), "layer_color(Contour)");
        check(layer_color(LayerType::Dimensions) == std::string("#0000FF"), "layer_color(Dimensions)");
        check(layer_color(LayerType::Axis) == std::string("#FF0000"), "layer_color(Axis)");
    }

    // ===== Тест 4: Инициализация слоёв в чертежном листе (Task 3.3, 3.5, 3.7) =====
    {
        DrawingInput in = make_test_drawing_input();
        auto sheets = make_part_drawing(in);

        check(!sheets.empty(), "Создан хотя бы один лист");

        const auto& sheet = sheets[0];
        check(sheet.layers.size() == 5, "На листе инициализированы 5 слоёв");
        check(sheet.layer_visibility.is_visible(LayerType::Contour), "Видимость слоёв инициализирована");
        check(sheet.layer_visibility.is_visible(LayerType::Dimensions), "Размеры видимы по умолчанию");
        check(sheet.layer_visibility.is_visible(LayerType::Axis), "Оси видимы по умолчанию");
    }

    // ===== Тест 5: Множественное переключение видимости (Task 3.4) =====
    {
        LayerVisibility vis = make_layer_visibility();

        // Скрыть все слои
        vis.set_visible(LayerType::Contour, false);
        vis.set_visible(LayerType::Dimensions, false);
        vis.set_visible(LayerType::Axis, false);
        vis.set_visible(LayerType::Markup, false);
        vis.set_visible(LayerType::ConstructionGeometry, false);

        check(!vis.is_visible(LayerType::Contour), "Контур скрыт");
        check(!vis.is_visible(LayerType::Dimensions), "Размеры скрыты");
        check(!vis.is_visible(LayerType::Axis), "Оси скрыты");
        check(!vis.is_visible(LayerType::Markup), "Разметка скрыта");
        check(!vis.is_visible(LayerType::ConstructionGeometry), "Вспомогательная геометрия скрыта");

        // Показать все слои
        vis.set_visible(LayerType::Contour, true);
        vis.set_visible(LayerType::Dimensions, true);
        vis.set_visible(LayerType::Axis, true);
        vis.set_visible(LayerType::Markup, true);
        vis.set_visible(LayerType::ConstructionGeometry, true);

        check(vis.is_visible(LayerType::Contour), "Контур видим");
        check(vis.is_visible(LayerType::Dimensions), "Размеры видимы");
    }

    // ===== Тест 6: Изоляция видимости между слоями (Task 3.4) =====
    {
        LayerVisibility vis = make_layer_visibility();

        // Скрыть только размеры
        vis.set_visible(LayerType::Dimensions, false);

        // Все остальные должны быть видимы
        check(vis.is_visible(LayerType::Contour) == true, "Контур видим при скрытых размерах");
        check(vis.is_visible(LayerType::Axis) == true, "Оси видимы при скрытых размерах");
        check(vis.is_visible(LayerType::Markup) == true, "Разметка видима при скрытых размерах");

        // Скрыть осей
        vis.set_visible(LayerType::Axis, false);
        check(vis.is_visible(LayerType::Contour) == true, "Контур видим");
        check(vis.is_visible(LayerType::Dimensions) == false, "Размеры скрыты");
        check(vis.is_visible(LayerType::Axis) == false, "Оси скрыты");
    }

    // ===== Тест 7: Layer структура и свойства (Task 3.1) =====
    {
        Layer layer;
        layer.type = LayerType::Contour;
        layer.name = "Контур";
        layer.visible = true;
        layer.locked = false;
        layer.color = "#000000";
        layer.element_count = 42;

        check(layer.type == LayerType::Contour, "Layer.type корректно");
        check(layer.name == "Контур", "Layer.name корректно");
        check(layer.visible == true, "Layer.visible корректно");
        check(layer.locked == false, "Layer.locked корректно");
        check(layer.color == "#000000", "Layer.color корректно");
        check(layer.element_count == 42, "Layer.element_count корректно");
    }

    // ===== Тест 8: Все 5 типов слоёв (Task 3.2) =====
    {
        int expected_types = 5;
        int found_types = 0;

        auto layers = make_default_layers();
        for (const auto& layer : layers) {
            if (layer.type == LayerType::Contour) found_types |= 1;
            if (layer.type == LayerType::Dimensions) found_types |= 2;
            if (layer.type == LayerType::Axis) found_types |= 4;
            if (layer.type == LayerType::Markup) found_types |= 8;
            if (layer.type == LayerType::ConstructionGeometry) found_types |= 16;
        }

        check(found_types == 31, "Все 5 типов слоёв присутствуют (binary: 11111 = 31)");
    }

    // ===== Тест 9: Состояние видимости в JSON представлении (Task 3.4) =====
    {
        LayerVisibility vis = make_layer_visibility();

        // Установить разные состояния
        vis.set_visible(LayerType::Contour, true);
        vis.set_visible(LayerType::Dimensions, false);
        vis.set_visible(LayerType::Axis, true);
        vis.set_visible(LayerType::Markup, false);
        vis.set_visible(LayerType::ConstructionGeometry, true);

        // Проверить, что map хранит правильные значения
        check(vis.layer_visibility[0] == true, "Состояние слоя 0 сохранено");
        check(vis.layer_visibility[1] == false, "Состояние слоя 1 сохранено");
        check(vis.layer_visibility[2] == true, "Состояние слоя 2 сохранено");
        check(vis.layer_visibility[3] == false, "Состояние слоя 3 сохранено");
        check(vis.layer_visibility[4] == true, "Состояние слоя 4 сохранено");
    }

    if (failures) {
        std::printf("test_layers: %d FAIL\n", failures);
        return 1;
    }
    std::printf("test_layers: OK (9 тестов пройдены)\n");
    return 0;
}
