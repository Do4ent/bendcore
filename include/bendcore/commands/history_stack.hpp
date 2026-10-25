#pragma once

#include <deque>
#include <optional>
#include <string>

#include "bendcore/commands/command.hpp"
#include "bendcore/commands/command_executor.hpp"
#include "bendcore/document/cad_document.hpp"

namespace bendcore::commands {

/**
 * HistoryEntry — запись в истории команд
 */
struct HistoryEntry {
    std::string id;
    Command command;
    CommandResult result;
    std::chrono::system_clock::time_point timestamp;
    std::string description;  // "Create Part", "Move Instance", etc.

    HistoryEntry() = default;
    HistoryEntry(const Command& cmd, const CommandResult& res);
};

/**
 * HistoryStack — реализует Undo/Redo функциональность
 * CAD-CORE-007: Управление историей команд
 */
class HistoryStack {
public:
    explicit HistoryStack(CadDocument& doc, size_t max_history = 100);
    ~HistoryStack() = default;

    // Запись в историю
    void record_command(const Command& cmd, const CommandResult& result);

    // Undo/Redo
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();

    // Состояние
    [[nodiscard]] bool can_undo() const { return !undo_stack_.empty(); }
    [[nodiscard]] bool can_redo() const { return !redo_stack_.empty(); }
    [[nodiscard]] size_t history_size() const { return undo_stack_.size(); }
    [[nodiscard]] size_t undo_count() const { return undo_stack_.size(); }
    [[nodiscard]] size_t redo_count() const { return redo_stack_.size(); }

    // Информация
    [[nodiscard]] std::string undo_description() const;
    [[nodiscard]] std::string redo_description() const;

    // Очистка
    void clear_history();
    void clear_redo_stack();

private:
    CadDocument& document_;
    CommandExecutor executor_;

    // История команд
    std::deque<HistoryEntry> undo_stack_;
    std::deque<HistoryEntry> redo_stack_;
    size_t max_history_size_;

    // Вспомогательные методы
    [[nodiscard]] Command create_inverse_command(const Command& cmd) const;
    [[nodiscard]] std::string get_command_description(const Command& cmd) const;
};

}  // namespace bendcore::commands
