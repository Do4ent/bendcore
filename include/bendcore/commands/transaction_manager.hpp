#pragma once

#include <vector>
#include "bendcore/commands/command.hpp"
#include "bendcore/commands/command_executor.hpp"
#include "bendcore/document/cad_document.hpp"

namespace bendcore::commands {

/**
 * TransactionManager — atomic batch operations
 * CAD-CORE-006: Ensures all-or-nothing execution
 */
class TransactionManager {
public:
    explicit TransactionManager(CadDocument& doc);
    ~TransactionManager() = default;

    // Transaction control
    void begin_transaction();
    [[nodiscard]] CommandResult execute_command(const Command& cmd);
    [[nodiscard]] bool commit();
    void rollback();

    // Query state
    [[nodiscard]] bool in_transaction() const { return is_active_; }
    [[nodiscard]] int transaction_size() const { return transaction_commands_.size(); }

private:
    CadDocument& document_;
    CommandExecutor executor_;

    // Transaction state
    bool is_active_ = false;
    std::vector<Command> transaction_commands_;
    std::vector<CommandResult> transaction_results_;
    int snapshot_revision_ = -1;

    // Rollback support
    bool restore_from_snapshot();
};

}  // namespace bendcore::commands
