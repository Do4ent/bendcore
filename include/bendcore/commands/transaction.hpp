#pragma once

#include "command.hpp"
#include "../document/cad_document.hpp"
#include <vector>
#include <memory>
#include <stdexcept>

namespace BendCore::CadCore {

/**
 * @brief Transaction for atomic multi-command operations
 *
 * Features:
 * - Atomicity: all commands execute or all rollback
 * - Revision management
 * - Error handling with automatic rollback
 * - No partial state exposure
 */
class Transaction {
public:
    /**
     * @brief Constructor
     * @param document The CAD document
     */
    explicit Transaction(CadDocument& document) noexcept;

    /**
     * @brief Add command to transaction
     */
    void addCommand(std::unique_ptr<ICommand> command);

    /**
     * @brief Execute all commands atomically
     * @throws std::runtime_error if any command fails
     */
    bool commit();

    /**
     * @brief Rollback all executed commands
     */
    bool rollback();

    /**
     * @brief Get command count
     */
    [[nodiscard]] size_t getCommandCount() const noexcept { return commands_.size(); }

    /**
     * @brief Check if transaction is active
     */
    [[nodiscard]] bool isActive() const noexcept { return is_active_; }

    /**
     * @brief Get expected revision after commit
     */
    [[nodiscard]] uint64_t getExpectedRevision() const noexcept { return expected_revision_; }

private:
    CadDocument& document_;
    std::vector<std::unique_ptr<ICommand>> commands_;
    std::vector<size_t> executed_indices_;  // Track what was executed
    bool is_active_;
    uint64_t expected_revision_;
    uint64_t revision_at_start_;
};

} // namespace BendCore::CadCore
