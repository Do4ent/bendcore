#include "bendcore/commands/transaction.hpp"
#include <algorithm>

namespace BendCore::CadCore {

Transaction::Transaction(CadDocument& document) noexcept
    : document_(document)
    , is_active_(true)
    , expected_revision_(document.getRevision())
    , revision_at_start_(document.getRevision())
{}

void Transaction::addCommand(std::unique_ptr<ICommand> command) {
    if (!is_active_) {
        throw std::runtime_error("Cannot add command to inactive transaction");
    }
    if (!command) {
        throw std::invalid_argument("Command is null");
    }
    commands_.push_back(std::move(command));
}

bool Transaction::commit() {
    if (!is_active_) {
        throw std::runtime_error("Transaction is not active");
    }

    try {
        // Execute all commands in order
        for (size_t i = 0; i < commands_.size(); ++i) {
            if (!commands_[i]->execute()) {
                // Rollback on failure
                rollback();
                is_active_ = false;
                return false;
            }
            executed_indices_.push_back(i);
        }

        expected_revision_ = document_.getRevision();
        is_active_ = false;
        return true;

    } catch (...) {
        rollback();
        is_active_ = false;
        return false;
    }
}

bool Transaction::rollback() {
    if (executed_indices_.empty()) {
        is_active_ = false;
        return true;
    }

    try {
        // Undo in reverse order
        for (auto it = executed_indices_.rbegin(); it != executed_indices_.rend(); ++it) {
            commands_[*it]->undo();
        }
        executed_indices_.clear();
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace BendCore::CadCore
