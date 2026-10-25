#include "bendcore/commands/command.hpp"

namespace bendcore::commands {

Command::Command() : document_version("0") {
    timestamp = std::chrono::system_clock::now();
}

Command::Command(const std::string& type) : Command() {
    this->type = type;
}

CommandResult::CommandResult() : success(false) {}

CommandResult CommandResult::success_result(const json& data, const std::string& new_version) {
    CommandResult result;
    result.success = true;
    result.result_data = data;
    result.new_document_version = new_version;
    return result;
}

CommandResult CommandResult::error_result(const std::string& error) {
    CommandResult result;
    result.success = false;
    result.error_message = error;
    return result;
}

}  // namespace bendcore::commands
