// Реализация системы Undo/Redo
#include "bendcore/drawing_undo_redo.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace bendcore::drawing {

UndoRedoManager::UndoRedoManager() : max_history_size_(1000), next_action_id_(0) {
}

void UndoRedoManager::record_action(const Action& action) {
    Action new_action = action;

    // Если нет ID, сгенерировать его
    if (new_action.id < 0) {
        new_action.id = next_action_id_++;
    }

    // Если нет временной метки, добавить текущее время
    if (new_action.timestamp.empty()) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
        new_action.timestamp = ss.str();
    }

    // Если это новое действие (не в истории), очистить всё после текущей позиции
    if (history_.current_index + 1 < static_cast<int>(history_.actions.size())) {
        clear_future_actions();
    }

    add_action_to_history(new_action);
}

void UndoRedoManager::undo() {
    if (can_undo()) {
        history_.current_index--;
    }
}

void UndoRedoManager::redo() {
    if (can_redo()) {
        history_.current_index++;
    }
}

bool UndoRedoManager::can_undo() const {
    return history_.current_index > 0;
}

bool UndoRedoManager::can_redo() const {
    return history_.current_index + 1 < static_cast<int>(history_.actions.size());
}

void UndoRedoManager::clear_history() {
    history_.actions.clear();
    history_.current_index = -1;
    next_action_id_ = 0;
}

const UndoRedoManager::Action* UndoRedoManager::get_current_action() const {
    if (history_.current_index >= 0 && history_.current_index < static_cast<int>(history_.actions.size())) {
        return &history_.actions[history_.current_index];
    }
    return nullptr;
}

const UndoRedoManager::Action* UndoRedoManager::get_previous_action() const {
    int prev_index = history_.current_index - 1;
    if (prev_index >= 0 && prev_index < static_cast<int>(history_.actions.size())) {
        return &history_.actions[prev_index];
    }
    return nullptr;
}

const UndoRedoManager::Action* UndoRedoManager::get_next_action() const {
    int next_index = history_.current_index + 1;
    if (next_index >= 0 && next_index < static_cast<int>(history_.actions.size())) {
        return &history_.actions[next_index];
    }
    return nullptr;
}

const UndoRedoManager::History& UndoRedoManager::get_history() const {
    return history_;
}

size_t UndoRedoManager::action_count() const {
    return history_.actions.size();
}

size_t UndoRedoManager::redo_count() const {
    int count = static_cast<int>(history_.actions.size()) - history_.current_index - 1;
    return std::max(0, count);
}

size_t UndoRedoManager::max_history_size() const {
    return max_history_size_;
}

void UndoRedoManager::set_max_history_size(size_t size) {
    max_history_size_ = size;
    check_history_limit();
}

std::vector<UndoRedoManager::Action> UndoRedoManager::get_all_actions() const {
    return history_.actions;
}

std::string UndoRedoManager::get_undo_description() const {
    if (can_undo()) {
        const auto* prev = get_previous_action();
        if (prev) {
            return "Отменить: " + prev->description;
        }
    }
    return "Отменить";
}

std::string UndoRedoManager::get_redo_description() const {
    if (can_redo()) {
        const auto* next = get_next_action();
        if (next) {
            return "Повторить: " + next->description;
        }
    }
    return "Повторить";
}

bool UndoRedoManager::jump_to_action(int action_id) {
    for (int i = 0; i < static_cast<int>(history_.actions.size()); ++i) {
        if (history_.actions[i].id == action_id) {
            history_.current_index = i;
            return true;
        }
    }
    return false;
}

void UndoRedoManager::clear_future_actions() {
    if (history_.current_index + 1 < static_cast<int>(history_.actions.size())) {
        history_.actions.erase(history_.actions.begin() + history_.current_index + 1,
                              history_.actions.end());
    }
}

void UndoRedoManager::add_action_to_history(const Action& action) {
    history_.actions.push_back(action);
    history_.current_index = static_cast<int>(history_.actions.size()) - 1;
    check_history_limit();
}

void UndoRedoManager::check_history_limit() {
    if (history_.actions.size() > max_history_size_) {
        // Удалить старые действия (слева)
        size_t to_remove = history_.actions.size() - max_history_size_;
        history_.actions.erase(history_.actions.begin(),
                              history_.actions.begin() + to_remove);
        history_.current_index -= to_remove;

        // Убедиться, что индекс остаётся валидным
        if (history_.current_index < 0) {
            history_.current_index = 0;
        }
    }
}

} // namespace bendcore::drawing
