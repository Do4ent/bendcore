#pragma once

#include "command.hpp"
#include <deque>
#include <vector>

namespace BendCore::CadCore {

/**
 * @brief Undo/Redo history management
 */
class HistoryStack {
public:
    HistoryStack(size_t max_commands = 1000, size_t max_memory = 10000000) noexcept;

    void push(std::unique_ptr<ICommand> command) noexcept;
    bool undo() noexcept;
    bool redo() noexcept;
    bool canUndo() const noexcept;
    bool canRedo() const noexcept;

    [[nodiscard]] size_t getUndoCount() const noexcept { return undo_stack_.size(); }
    [[nodiscard]] size_t getRedoCount() const noexcept { return redo_stack_.size(); }
    [[nodiscard]] size_t size() const noexcept { return undo_stack_.size(); }
    [[nodiscard]] size_t getMemoryUsage() const noexcept { return memory_usage_; }
    [[nodiscard]] std::vector<std::string> getHistory() const;

    void clear() noexcept;
    void createBranchPoint() noexcept;
    bool restoreToBranchPoint() noexcept;

private:
    std::deque<std::unique_ptr<ICommand>> undo_stack_;
    std::deque<std::unique_ptr<ICommand>> redo_stack_;
    size_t max_commands_;
    size_t max_memory_;
    size_t memory_usage_;
};

} // namespace BendCore::CadCore
