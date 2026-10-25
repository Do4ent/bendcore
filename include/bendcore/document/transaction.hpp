#pragma once

#include "cad_document.hpp"
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <chrono>

namespace BendCore::CadCore {

/**
 * @brief Atomic document transaction (GAP-017: Transactions and Undo/Redo)
 *
 * All document modifications must occur within a transaction.
 * Transactions are all-or-nothing: either all operations succeed or none do.
 */
class DocumentTransaction {
public:
    DocumentTransaction(CadDocument& doc, const std::string& description = "");
    ~DocumentTransaction();

    /**
     * @brief Get transaction ID
     */
    [[nodiscard]] uint64_t getId() const noexcept { return id_; }

    /**
     * @brief Get transaction description
     */
    [[nodiscard]] const std::string& getDescription() const noexcept { return description_; }

    /**
     * @brief Get document revision before transaction
     */
    [[nodiscard]] uint64_t getBaseRevision() const noexcept { return base_revision_; }

    /**
     * @brief Get transaction timestamp
     */
    [[nodiscard]] std::chrono::system_clock::time_point getTimestamp() const noexcept {
        return timestamp_;
    }

    /**
     * @brief Commit transaction (apply all changes)
     * @return true if successful, false if validation failed
     */
    bool commit();

    /**
     * @brief Rollback transaction (discard all changes)
     * @return true if successful
     */
    bool rollback();

    /**
     * @brief Check if transaction is active
     */
    [[nodiscard]] bool isActive() const noexcept { return is_active_; }

    /**
     * @brief Get affected entity IDs
     */
    [[nodiscard]] std::vector<EntityId> getAffectedEntities() const noexcept {
        return affected_entities_;
    }

    /**
     * @brief Record entity change
     */
    void recordEntityChange(const EntityId& id);

    /**
     * @brief Get transaction size in bytes
     */
    [[nodiscard]] size_t getSize() const noexcept;

    /**
     * @brief Validate transaction consistency
     * @return empty string if valid, error message otherwise
     */
    [[nodiscard]] std::string validate() const;

private:
    uint64_t id_;
    std::string description_;
    CadDocument& document_;
    CadDocument snapshot_;  // Snapshot for rollback
    uint64_t base_revision_;
    bool is_active_;
    std::vector<EntityId> affected_entities_;
    std::chrono::system_clock::time_point timestamp_;

    static uint64_t next_transaction_id_;
};

/**
 * @brief Transaction guard - RAII wrapper for automatic rollback
 */
class TransactionGuard {
public:
    TransactionGuard(DocumentTransaction& tx) : transaction_(tx), should_commit_(false) {}

    ~TransactionGuard() {
        if (transaction_.isActive()) {
            if (should_commit_) {
                transaction_.commit();
            } else {
                transaction_.rollback();
            }
        }
    }

    /**
     * @brief Mark transaction for commit
     */
    void commit() noexcept { should_commit_ = true; }

    /**
     * @brief Release guard without cleanup
     */
    void release() noexcept { should_commit_ = false; }

private:
    DocumentTransaction& transaction_;
    bool should_commit_;
};

} // namespace BendCore::CadCore
