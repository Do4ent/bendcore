// Тесты для системы Undo/Redo
#include "bendcore/drawing_undo_redo.hpp"

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
    TEST("Инициализация менеджера Undo/Redo");

    UndoRedoManager manager;

    CHECK(manager.action_count() == 0, "Нет действий после инициализации");
    CHECK(!manager.can_undo(), "Невозможно отменить после инициализации");
    CHECK(!manager.can_redo(), "Невозможно повторить после инициализации");
    CHECK(manager.redo_count() == 0, "Нет действий для повтора");
}

static void test_record_action() {
    TEST("Запись действия");

    UndoRedoManager manager;

    UndoRedoManager::Action action;
    action.type = "edit_parameter";
    action.description = "Edited length parameter";
    action.previous_state = "{\"value\": 100}";
    action.new_state = "{\"value\": 150}";
    action.performed_by = "user123";

    manager.record_action(action);

    CHECK(manager.action_count() == 1, "Действие записано");
    CHECK(!manager.can_undo(), "Нельзя отменить первое действие");
    CHECK(!manager.can_redo(), "Нельзя повторить после записи действия");
}

static void test_undo_single_action() {
    TEST("Отмена одного действия");

    UndoRedoManager manager;

    UndoRedoManager::Action action1;
    action1.type = "edit_parameter";
    action1.description = "Edit 1";
    action1.previous_state = "{\"old\": 1}";
    action1.new_state = "{\"new\": 2}";

    UndoRedoManager::Action action2;
    action2.type = "edit_parameter";
    action2.description = "Edit 2";
    action2.previous_state = "{\"old\": 2}";
    action2.new_state = "{\"new\": 3}";

    manager.record_action(action1);
    manager.record_action(action2);

    CHECK(manager.can_undo(), "Можно отменить действие");

    manager.undo();

    CHECK(manager.can_redo(), "Можно повторить после отмены");
    CHECK(manager.redo_count() == 1, "Одно действие для повтора");
}

static void test_redo_action() {
    TEST("Повтор действия");

    UndoRedoManager manager;

    UndoRedoManager::Action action;
    action.type = "edit_parameter";
    action.description = "Edit";
    action.previous_state = "{\"old\": 1}";
    action.new_state = "{\"new\": 2}";

    manager.record_action(action);
    manager.undo();

    CHECK(manager.can_redo(), "Можно повторить");

    manager.redo();

    CHECK(!manager.can_redo(), "Нельзя повторить после повтора");
    CHECK(manager.can_undo(), "Можно отменить после повтора");
}

static void test_multiple_undo_redo() {
    TEST("Множественная отмена и повтор");

    UndoRedoManager manager;

    for (int i = 0; i < 5; ++i) {
        UndoRedoManager::Action action;
        action.type = "edit_parameter";
        action.description = "Edit " + std::to_string(i);
        action.previous_state = "{\"old\": " + std::to_string(i) + "}";
        action.new_state = "{\"new\": " + std::to_string(i + 1) + "}";
        manager.record_action(action);
    }

    CHECK(manager.action_count() == 5, "5 действий записано");

    for (int i = 0; i < 3; ++i) {
        manager.undo();
    }

    CHECK(manager.redo_count() == 3, "3 действия для повтора");

    for (int i = 0; i < 3; ++i) {
        manager.redo();
    }

    CHECK(manager.redo_count() == 0, "Все действия повторены");
}

static void test_clear_history() {
    TEST("Очистка истории");

    UndoRedoManager manager;

    UndoRedoManager::Action action;
    action.type = "edit_parameter";
    action.description = "Edit";

    manager.record_action(action);
    manager.record_action(action);

    CHECK(manager.action_count() > 0, "Есть действия в истории");

    manager.clear_history();

    CHECK(manager.action_count() == 0, "История очищена");
    CHECK(!manager.can_undo(), "Нельзя отменить после очистки");
}

static void test_action_id_generation() {
    TEST("Генерация ID действия");

    UndoRedoManager manager;

    UndoRedoManager::Action action1;
    action1.type = "edit";
    action1.description = "Edit 1";

    UndoRedoManager::Action action2;
    action2.type = "edit";
    action2.description = "Edit 2";

    manager.record_action(action1);
    manager.record_action(action2);

    auto history = manager.get_history();
    CHECK(history.actions[0].id >= 0, "ID первого действия установлен");
    CHECK(history.actions[1].id >= 0, "ID второго действия установлен");
    CHECK(history.actions[0].id != history.actions[1].id, "ID действий уникальны");
}

static void test_get_undo_description() {
    TEST("Получение описания отмены");

    UndoRedoManager manager;

    UndoRedoManager::Action action;
    action.type = "edit_parameter";
    action.description = "Edited length parameter";

    manager.record_action(action);

    std::string desc = manager.get_undo_description();
    CHECK(desc.find("Edited length parameter") != std::string::npos,
          "Описание содержит текст действия");
}

static void test_get_redo_description() {
    TEST("Получение описания повтора");

    UndoRedoManager manager;

    UndoRedoManager::Action action;
    action.type = "edit_parameter";
    action.description = "Edited length parameter";

    manager.record_action(action);
    manager.undo();

    std::string desc = manager.get_redo_description();
    CHECK(desc.find("Edited length parameter") != std::string::npos,
          "Описание повтора содержит текст действия");
}

static void test_new_action_clears_redo() {
    TEST("Новое действие очищает стек повтора");

    UndoRedoManager manager;

    UndoRedoManager::Action action1;
    action1.type = "edit";
    action1.description = "Edit 1";

    UndoRedoManager::Action action2;
    action2.type = "edit";
    action2.description = "Edit 2";

    UndoRedoManager::Action action3;
    action3.type = "edit";
    action3.description = "Edit 3";

    manager.record_action(action1);
    manager.record_action(action2);
    manager.undo();

    CHECK(manager.redo_count() == 1, "Одно действие для повтора");

    manager.record_action(action3);

    CHECK(manager.redo_count() == 0, "Стек повтора очищен");
    CHECK(manager.action_count() == 2, "2 действия в истории");
}

static void test_max_history_size() {
    TEST("Максимальный размер истории");

    UndoRedoManager manager;
    manager.set_max_history_size(5);

    for (int i = 0; i < 10; ++i) {
        UndoRedoManager::Action action;
        action.type = "edit";
        action.description = "Edit " + std::to_string(i);
        manager.record_action(action);
    }

    CHECK(manager.action_count() <= 5, "История ограничена максимальным размером");
}

static void test_jump_to_action() {
    TEST("Переход к определённому действию");

    UndoRedoManager manager;

    std::vector<int> action_ids;
    for (int i = 0; i < 5; ++i) {
        UndoRedoManager::Action action;
        action.type = "edit";
        action.description = "Edit " + std::to_string(i);
        manager.record_action(action);
    }

    auto history = manager.get_history();
    int target_id = history.actions[2].id;

    bool result = manager.jump_to_action(target_id);
    CHECK(result, "Переход выполнен");

    auto current = manager.get_current_action();
    CHECK(current && current->id == target_id, "Текущее действие изменено");
}

int main() {
    test_initialization();
    test_record_action();
    test_undo_single_action();
    test_redo_action();
    test_multiple_undo_redo();
    test_clear_history();
    test_action_id_generation();
    test_get_undo_description();
    test_get_redo_description();
    test_new_action_clears_redo();
    test_max_history_size();
    test_jump_to_action();

    printf("\n=== Результаты ===\n");
    printf("Всего тестов: %d\n", test_count);
    printf("Ошибок: %d\n", test_failures);
    printf("Пройдено: %d\n", test_count - test_failures);

    return test_failures > 0 ? 1 : 0;
}
