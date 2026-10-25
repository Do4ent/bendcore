#pragma once

#include <string>
#include <cstdint>
#include <any>
#include <memory>

namespace BendCore::CadCore {

/**
 * @brief Base class for all CAD commands
 *
 * Every change to a CAD document must go through a command.
 * Commands support:
 * - Execution with validation
 * - Undo/Redo capability
 * - Result and error tracking
 * - Revision management
 */
class ICommand {
public:
    virtual ~ICommand() = default;

    /**
     * @brief Execute the command
     * @return true if successful
     */
    virtual bool execute() = 0;

    /**
     * @brief Undo the command
     * @return true if successful
     */
    virtual bool undo() = 0;

    /**
     * @brief Get human-readable description
     */
    virtual std::string getDescription() const = 0;

    /**
     * @brief Get estimated memory usage
     */
    virtual size_t getMemorySize() const = 0;
};

/**
 * @brief Command execution result
 */
struct CommandResult {
    bool success = false;
    std::string error_message;
    std::any result;
    uint64_t command_id = 0;
    uint64_t document_revision = 0;

    CommandResult() = default;
    CommandResult(bool ok, const std::string& msg = "")
        : success(ok), error_message(msg) {}
};

/**
 * @brief Command validation status
 */
enum class CommandStatus {
    PENDING,
    VALIDATED,
    EXECUTING,
    EXECUTED,
    FAILED,
    UNDONE
};

/**
 * @brief Command wrapper with metadata
 */
class Command {
public:
    /**
     * @brief Create command with unique ID
     */
    Command(std::unique_ptr<ICommand> impl, uint64_t id)
        : impl_(std::move(impl))
        , id_(id)
        , status_(CommandStatus::PENDING)
    {}

    /**
     * @brief Get command ID
     */
    [[nodiscard]] uint64_t getId() const noexcept { return id_; }

    /**
     * @brief Get command status
     */
    [[nodiscard]] CommandStatus getStatus() const noexcept { return status_; }

    /**
     * @brief Get description
     */
    [[nodiscard]] std::string getDescription() const {
        return impl_ ? impl_->getDescription() : "Unknown";
    }

    /**
     * @brief Get memory size
     */
    [[nodiscard]] size_t getMemorySize() const {
        return impl_ ? impl_->getMemorySize() : 0;
    }

    /**
     * @brief Execute command
     */
    bool execute() {
        if (!impl_) return false;
        bool result = impl_->execute();
        status_ = result ? CommandStatus::EXECUTED : CommandStatus::FAILED;
        return result;
    }

    /**
     * @brief Undo command
     */
    bool undo() {
        if (!impl_) return false;
        bool result = impl_->undo();
        status_ = result ? CommandStatus::UNDONE : CommandStatus::FAILED;
        return result;
    }

private:
    std::unique_ptr<ICommand> impl_;
    uint64_t id_;
    CommandStatus status_;
};

} // namespace BendCore::CadCore
