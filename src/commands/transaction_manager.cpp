#include "bendcore/commands/transaction_manager.hpp"

namespace bendcore::commands {

TransactionManager::TransactionManager(CadDocument& doc)
    : document_(doc), executor_(doc) {}

void TransactionManager::begin_transaction() {
    if (is_active_) {
        throw std::runtime_error("Transaction already in progress");
    }

    is_active_ = true;
    snapshot_revision_ = document_.revision();
    transaction_commands_.clear();
    transaction_results_.clear();
}

CommandResult TransactionManager::execute_command(const Command& cmd) {
    if (!is_active_) {
        return CommandResult::error_result("No transaction in progress");
    }

    // Execute the command through executor
    CommandResult result = executor_.execute(cmd);

    // Store result regardless of success for commit/rollback
    transaction_commands_.push_back(cmd);
    transaction_results_.push_back(result);

    return result;
}

bool TransactionManager::commit() {
    if (!is_active_) {
        return false;
    }

    // Check if all commands succeeded
    bool all_success = true;
    for (const auto& result : transaction_results_) {
        if (!result.success) {
            all_success = false;
            break;
        }
    }

    if (!all_success) {
        // Rollback on failure
        rollback();
        return false;
    }

    // Transaction successful - clear state
    is_active_ = false;
    transaction_commands_.clear();
    transaction_results_.clear();
    snapshot_revision_ = -1;

    return true;
}

void TransactionManager::rollback() {
    if (!is_active_) {
        return;
    }

    // For now, we can't actually restore state since CadDocument doesn't
    // have a built-in checkpoint/restore mechanism. In a real system,
    // we would either:
    // 1. Store actual document snapshots
    // 2. Reverse each command with undo operations
    // 3. Use a copy-on-write approach

    // For Phase B MVP, we just mark the transaction as failed
    // Future versions (CAD-CORE-007 HistoryStack) will handle true rollback

    is_active_ = false;
    transaction_commands_.clear();
    transaction_results_.clear();
    snapshot_revision_ = -1;
}

}  // namespace bendcore::commands
