#pragma once

#include "command.hpp"
#include "../document/cad_document.hpp"
#include <deque>
#include <map>
#include <vector>

namespace BendCore::CadCore {

/**
 * @brief Manages all command execution and tracking
 *
 * Features:
 * - Execute commands with validation
 * - Track command history
 * - Validate document revision consistency
 * - Support for undo/redo (via history)
 * - Error handling and rollback
 */
class CommandManager {
public:
    /**
     * @brief Constructor
     * @param document The CAD document to manage
     * @param max_history Maximum commands in history
     */
    CommandManager(CadDocument& document, size_t max_history = 1000) noexcept;

    /**
     * @brief Execute a command
     * @throws std::invalid_argument if validation fails
     */
    CommandResult execute(std::unique_ptr<ICommand> command);

    /**
     * @brief Validate a command without executing
     */
    bool validate(ICommand* command) const noexcept;

    /**
     * @brief Get result of last command
     */
    [[nodiscard]] const CommandResult& getLastResult() const noexcept;

    /**
     * @brief Get command by ID
     */
    [[nodiscard]] const CommandResult* getResult(uint64_t command_id) const noexcept;

    /**
     * @brief Get command history
     */
    [[nodiscard]] std::vector<std::string> getHistory() const;

    /**
     * @brief Get executed command count
     */
    [[nodiscard]] size_t getExecutedCount() const noexcept { return executed_; }

    /**
     * @brief Get failed command count
     */
    [[nodiscard]] size_t getFailedCount() const noexcept { return failed_; }

    /**
     * @brief Clear all command history
     */
    void clearHistory() noexcept;

    /**
     * @brief Get memory usage
     */
    [[nodiscard]] size_t getMemoryUsage() const noexcept;

    /**
     * @brief Check if command at expected revision
     */
    [[nodiscard]] bool isExpectedRevision(uint64_t expected_revision) const noexcept;

    /**
     * @brief Get current document revision
     */
    [[nodiscard]] uint64_t getCurrentRevision() const noexcept;

private:
    CadDocument& document_;
    size_t max_history_;
    uint64_t next_command_id_;
    size_t executed_;
    size_t failed_;
    CommandResult last_result_;
    std::deque<CommandResult> history_;
    std::map<uint64_t, CommandResult> results_by_id_;

    /**
     * @brief Add result to history
     */
    void recordResult(const CommandResult& result) noexcept;
};

} // namespace BendCore::CadCore
