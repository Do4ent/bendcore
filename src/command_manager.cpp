#include "bendcore/commands/command_manager.hpp"
#include <algorithm>

namespace BendCore::CadCore {

CommandManager::CommandManager(CadDocument& document, size_t max_history) noexcept
    : document_(document)
    , max_history_(max_history)
    , next_command_id_(1)
    , executed_(0)
    , failed_(0)
{}

CommandResult CommandManager::execute(std::unique_ptr<ICommand> command) {
    CommandResult result;
    result.command_id = next_command_id_++;
    result.document_revision = document_.getRevision();

    if (!command) {
        result.success = false;
        result.error_message = "Command is null";
        failed_++;
        last_result_ = result;
        return result;
    }

    try {
        // Execute command
        if (!command->execute()) {
            result.success = false;
            result.error_message = "Command execution failed";
            failed_++;
            last_result_ = result;
            return result;
        }

        result.success = true;
        result.document_revision = document_.getRevision();
        executed_++;
        recordResult(result);
        last_result_ = result;
        return result;

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Exception: ") + e.what();
        failed_++;
        last_result_ = result;
        return result;
    } catch (...) {
        result.success = false;
        result.error_message = "Unknown exception";
        failed_++;
        last_result_ = result;
        return result;
    }
}

bool CommandManager::validate(ICommand* command) const noexcept {
    return command != nullptr;
}

const CommandResult& CommandManager::getLastResult() const noexcept {
    return last_result_;
}

const CommandResult* CommandManager::getResult(uint64_t command_id) const noexcept {
    auto it = results_by_id_.find(command_id);
    if (it != results_by_id_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> CommandManager::getHistory() const {
    std::vector<std::string> history;
    for (const auto& result : history_) {
        std::string entry = "Command " + std::to_string(result.command_id) +
                           " (" + (result.success ? "OK" : "FAILED") + ")";
        history.push_back(entry);
    }
    return history;
}

void CommandManager::clearHistory() noexcept {
    history_.clear();
    results_by_id_.clear();
    executed_ = 0;
    failed_ = 0;
    next_command_id_ = 1;
}

size_t CommandManager::getMemoryUsage() const noexcept {
    size_t total = 0;
    for (const auto& result : history_) {
        total += sizeof(result);
    }
    total += sizeof(last_result_);
    return total;
}

bool CommandManager::isExpectedRevision(uint64_t expected_revision) const noexcept {
    return document_.getRevision() >= expected_revision;
}

uint64_t CommandManager::getCurrentRevision() const noexcept {
    return document_.getRevision();
}

void CommandManager::recordResult(const CommandResult& result) noexcept {
    history_.push_back(result);
    results_by_id_[result.command_id] = result;

    // Limit history size
    while (history_.size() > max_history_) {
        const auto& oldest = history_.front();
        results_by_id_.erase(oldest.command_id);
        history_.pop_front();
    }
}

} // namespace BendCore::CadCore
