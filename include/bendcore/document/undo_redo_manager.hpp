#pragma once

#include "cad_document.hpp"
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace BendCore::CadCore {

/**
 * @brief Document state snapshot for undo/redo
 */
struct DocumentSnapshot {
    uint64_t revision;
    std::string description;
    std::vector<uint8_t> data;  // Serialized document state
    size_t size() const noexcept { return data.size(); }
};

/**
 * @brief Undo/Redo manager (GAP-017: Transactions and Undo/Redo)
 *
 * Manages document history with undo/redo capabilities.
 * Stores snapshots of document state at various points.
 */
class UndoRedoManager {
public:
    /**
     * @brief Create undo/redo manager
     * @param max_snapshots Maximum number of snapshots to keep (0 = unlimited)
     * @param max_memory Maximum memory to use for snapshots in bytes
     */
    UndoRedoManager(size_t max_snapshots = 100, size_t max_memory = 100000000);

    /**
     * @brief Save current document state
     * @param description Human-readable description of the change
     * @return true if successful
     */
    bool saveSnapshot(const CadDocument& doc, const std::string& description = "");

    /**
     * @brief Undo last action
     * @param doc Document to restore into
     * @return true if successful
     */
    bool undo(CadDocument& doc);

    /**
     * @brief Redo last undone action
     * @param doc Document to restore into
     * @return true if successful
     */
    bool redo(CadDocument& doc);

    /**
     * @brief Check if undo is available
     */
    [[nodiscard]] bool canUndo() const noexcept { return !undo_stack_.empty(); }

    /**
     * @brief Check if redo is available
     */
    [[nodiscard]] bool canRedo() const noexcept { return !redo_stack_.empty(); }

    /**
     * @brief Get undo stack size
     */
    [[nodiscard]] size_t getUndoCount() const noexcept { return undo_stack_.size(); }

    /**
     * @brief Get redo stack size
     */
    [[nodiscard]] size_t getRedoCount() const noexcept { return redo_stack_.size(); }

    /**
     * @brief Get undo history descriptions
     */
    [[nodiscard]] std::vector<std::string> getUndoHistory(size_t limit = 10) const;

    /**
     * @brief Get redo history descriptions
     */
    [[nodiscard]] std::vector<std::string> getRedoHistory(size_t limit = 10) const;

    /**
     * @brief Create a branch point (checkpoint)
     */
    void createBranchPoint();

    /**
     * @brief Restore to last branch point
     */
    bool restoreToBranchPoint(CadDocument& doc);

    /**
     * @brief Clear history
     */
    void clear() noexcept;

    /**
     * @brief Get total memory used by snapshots
     */
    [[nodiscard]] size_t getMemoryUsage() const noexcept { return memory_usage_; }

    /**
     * @brief Get snapshot count
     */
    [[nodiscard]] size_t getSnapshotCount() const noexcept {
        return undo_stack_.size() + redo_stack_.size();
    }

    /**
     * @brief Set callback for undo/redo events
     */
    using UndoRedoCallback = std::function<void(const std::string& description, bool isUndo)>;
    void setCallback(const UndoRedoCallback& callback) noexcept { callback_ = callback; }

private:
    std::deque<DocumentSnapshot> undo_stack_;
    std::deque<DocumentSnapshot> redo_stack_;
    std::unique_ptr<DocumentSnapshot> branch_point_;

    size_t max_snapshots_;
    size_t max_memory_;
    size_t memory_usage_;

    UndoRedoCallback callback_;

    /**
     * @brief Trim snapshots if memory limit exceeded
     */
    void trimSnapshots();

    /**
     * @brief Serialize document to bytes
     */
    static std::vector<uint8_t> serializeDocument(const CadDocument& doc);

    /**
     * @brief Deserialize document from bytes
     */
    static bool deserializeDocument(CadDocument& doc, const std::vector<uint8_t>& data);
};

} // namespace BendCore::CadCore
