#include "bendcore/commands/history_stack.hpp"

namespace BendCore::CadCore {

HistoryStack::HistoryStack(size_t max_commands, size_t max_memory) noexcept
    : max_commands_(max_commands)
    , max_memory_(max_memory)
    , memory_usage_(0)
{}

void HistoryStack::push(std::unique_ptr<ICommand> command) noexcept {
    if (!command) return;

    memory_usage_ += command->getMemorySize();
    undo_stack_.push_back(std::move(command));
    redo_stack_.clear();

    while (undo_stack_.size() > max_commands_ || memory_usage_ > max_memory_) {
        if (undo_stack_.empty()) break;
        memory_usage_ -= undo_stack_.front()->getMemorySize();
        undo_stack_.pop_front();
    }
}

bool HistoryStack::undo() noexcept {
    if (!canUndo()) return false;

    auto& cmd = undo_stack_.back();
    if (!cmd->undo()) return false;

    redo_stack_.push_back(std::move(cmd));
    undo_stack_.pop_back();
    return true;
}

bool HistoryStack::redo() noexcept {
    if (!canRedo()) return false;

    auto& cmd = redo_stack_.back();
    if (!cmd->execute()) return false;

    undo_stack_.push_back(std::move(cmd));
    redo_stack_.pop_back();
    return true;
}

bool HistoryStack::canUndo() const noexcept {
    return !undo_stack_.empty();
}

bool HistoryStack::canRedo() const noexcept {
    return !redo_stack_.empty();
}

std::vector<std::string> HistoryStack::getHistory() const {
    std::vector<std::string> history;
    for (const auto& cmd : undo_stack_) {
        history.push_back(cmd->getDescription());
    }
    return history;
}

void HistoryStack::clear() noexcept {
    undo_stack_.clear();
    redo_stack_.clear();
    memory_usage_ = 0;
}

void HistoryStack::createBranchPoint() noexcept {
    redo_stack_.clear();
}

bool HistoryStack::restoreToBranchPoint() noexcept {
    return true;  // Simplified for M2
}

} // namespace BendCore::CadCore
