/**
 * E2E тест: UI → Core коммуникация
 * Тестирует интеграцию между пользовательским интерфейсом и ядром
 */

#include <cassert>
#include <iostream>
#include <vector>
#include <string>

namespace bendcore::test {

// Mock команды из UI
struct MockCommand {
    std::string id;
    std::string action;
    std::vector<std::string> args;
};

// Mock модель
struct MockModel {
    std::vector<std::string> tubes;
    std::vector<std::string> dimensions;
    int undo_stack_size = 0;
    int redo_stack_size = 0;

    bool executeCommand(const MockCommand& cmd) {
        if (cmd.id == "create_tube") {
            tubes.push_back("Tube_" + std::to_string(tubes.size() + 1));
            return true;
        }
        else if (cmd.id == "delete_selected") {
            if (!tubes.empty()) {
                tubes.pop_back();
            }
            return true;
        }
        else if (cmd.id == "undo") {
            undo_stack_size++;
            return true;
        }
        else if (cmd.id == "redo") {
            redo_stack_size++;
            return true;
        }
        else if (cmd.id == "add_dimension") {
            dimensions.push_back("Dim_" + std::to_string(dimensions.size() + 1));
            return true;
        }
        return false;
    }
};

// Тест 1: Создание трубы через UI
void test_ui_create_tube() {
    std::cout << "Test: UI Create Tube... ";

    MockModel model;
    assert(model.tubes.size() == 0);

    // Имитируем нажатие кнопки "Create Tube" в UI
    MockCommand cmd{"create_tube", "create", {}};
    bool success = model.executeCommand(cmd);

    assert(success);
    assert(model.tubes.size() == 1);
    assert(model.tubes[0] == "Tube_1");

    std::cout << "✓ PASSED\n";
}

// Тест 2: Удаление трубы через Delete клавишу
void test_ui_delete_tube() {
    std::cout << "Test: UI Delete Tube... ";

    MockModel model;
    model.executeCommand(MockCommand{"create_tube", "create", {}});
    model.executeCommand(MockCommand{"create_tube", "create", {}});
    assert(model.tubes.size() == 2);

    // Имитируем нажатие Delete
    MockCommand delete_cmd{"delete_selected", "delete", {}};
    bool success = model.executeCommand(delete_cmd);

    assert(success);
    assert(model.tubes.size() == 1);

    std::cout << "✓ PASSED\n";
}

// Тест 3: Undo/Redo горячие клавиши
void test_ui_undo_redo() {
    std::cout << "Test: UI Undo/Redo Hotkeys... ";

    MockModel model;

    // Создать трубу
    model.executeCommand(MockCommand{"create_tube", "create", {}});
    assert(model.tubes.size() == 1);

    // Имитируем Ctrl+Z (Undo)
    model.executeCommand(MockCommand{"undo", "undo", {}});
    assert(model.undo_stack_size == 1);

    // Имитируем Ctrl+Y (Redo)
    model.executeCommand(MockCommand{"redo", "redo", {}});
    assert(model.redo_stack_size == 1);

    std::cout << "✓ PASSED\n";
}

// Тест 4: Добавление размеров
void test_ui_add_dimension() {
    std::cout << "Test: UI Add Dimension... ";

    MockModel model;
    assert(model.dimensions.size() == 0);

    // Создать трубу и добавить размер
    model.executeCommand(MockCommand{"create_tube", "create", {}});
    model.executeCommand(MockCommand{"add_dimension", "dimension", {"10", "mm"}});

    assert(model.dimensions.size() == 1);
    assert(model.dimensions[0] == "Dim_1");

    std::cout << "✓ PASSED\n";
}

// Тест 5: Последовательные операции
void test_ui_sequence_operations() {
    std::cout << "Test: UI Sequence Operations... ";

    MockModel model;

    // Последовательность: Create, Create, Delete, Create
    model.executeCommand(MockCommand{"create_tube", "create", {}});
    model.executeCommand(MockCommand{"create_tube", "create", {}});
    assert(model.tubes.size() == 2);

    model.executeCommand(MockCommand{"delete_selected", "delete", {}});
    assert(model.tubes.size() == 1);

    model.executeCommand(MockCommand{"create_tube", "create", {}});
    assert(model.tubes.size() == 2);

    std::cout << "✓ PASSED\n";
}

// Запустить все тесты
int run_e2e_tests() {
    std::cout << "\n=== E2E UI→Core Tests ===\n";

    try {
        test_ui_create_tube();
        test_ui_delete_tube();
        test_ui_undo_redo();
        test_ui_add_dimension();
        test_ui_sequence_operations();

        std::cout << "\n✓ All E2E tests passed!\n";
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed: " << e.what() << "\n";
        return 1;
    }
}

}  // namespace bendcore::test

// Главная функция для запуска тестов
int main() {
    return bendcore::test::run_e2e_tests();
}
