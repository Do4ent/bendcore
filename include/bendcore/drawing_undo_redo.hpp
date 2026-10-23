// Система Undo/Redo для чертежей (Q4 Task 1 Subtask 4)
// Неограниченный Undo/Redo с сохранением истории операций
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <ctime>

namespace bendcore::drawing {

class UndoRedoManager {
public:
    // Действие в редакторе
    struct Action {
        std::string type;              // "edit_parameter", "add_layer", "remove_annotation", и т.д.
        std::string description;       // Описание для пользователя ("Edited length parameter")
        std::string previous_state;    // Сериализованное предыдущее состояние (JSON)
        std::string new_state;         // Сериализованное новое состояние (JSON)
        std::string timestamp;         // Время выполнения действия
        std::string performed_by;      // Кто выполнил действие
        int id = -1;                   // Уникальный ID действия
    };

    // История операций
    struct History {
        std::vector<Action> actions;
        int current_index = -1;  // Индекс текущего состояния
    };

    UndoRedoManager();
    ~UndoRedoManager() = default;

    // Записать действие в историю
    void record_action(const Action& action);

    // Отменить последнее действие
    void undo();

    // Повторить последнее отменённое действие
    void redo();

    // Проверить, возможно ли отменить действие
    bool can_undo() const;

    // Проверить, возможно ли повторить действие
    bool can_redo() const;

    // Очистить историю (удалить все записи)
    void clear_history();

    // Получить текущее действие (последнее выполненное)
    const Action* get_current_action() const;

    // Получить действие перед текущим (для отката)
    const Action* get_previous_action() const;

    // Получить действие после текущего (для повтора)
    const Action* get_next_action() const;

    // Получить всю историю
    const History& get_history() const;

    // Получить количество действий в истории
    size_t action_count() const;

    // Получить количество отменённых действий (для повтора)
    size_t redo_count() const;

    // Получить максимальный размер истории
    size_t max_history_size() const;

    // Установить максимальный размер истории
    void set_max_history_size(size_t size);

    // Получить все действия (для отладки и логирования)
    std::vector<Action> get_all_actions() const;

    // Получить описание последнего действия (для отката)
    std::string get_undo_description() const;

    // Получить описание последнего отменённого действия (для повтора)
    std::string get_redo_description() const;

    // Переместиться на определённое действие в истории (для перехода по времени)
    bool jump_to_action(int action_id);

private:
    History history_;
    size_t max_history_size_;
    int next_action_id_;

    // Очистить все действия после текущей позиции
    void clear_future_actions();

    // Добавить новое действие в историю
    void add_action_to_history(const Action& action);

    // Проверить, достигнут ли лимит истории и удалить старые действия если нужно
    void check_history_limit();
};

} // namespace bendcore::drawing
