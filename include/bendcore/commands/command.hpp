#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"
#include "bendcore/document/entity_id.hpp"

namespace bendcore::commands {

using json = nlohmann::json;
using namespace bendcore::document;

/**
 * Command — описание операции для выполнения на CadDocument
 * CAD-CORE-005: CommandEngine
 */
struct Command {
    std::string id;  // Уникальный идентификатор
    std::string type;  // Тип: "CreatePart", "CreateInstance", "SetParent", etc.
    json args;  // Аргументы команды
    std::string document_version;  // Версия документа при выполнении
    std::chrono::system_clock::time_point timestamp;

    Command();
    explicit Command(const std::string& type);
};

/**
 * CommandResult — результат выполнения команды
 */
struct CommandResult {
    bool success;
    std::string error_message;  // Если success = false
    json result_data;  // Результат команды
    std::string new_document_version;  // Новая версия после выполнения
    std::vector<EntityId> affected_ids;  // Какие сущности изменились

    CommandResult();
    static CommandResult success_result(const json& data, const std::string& new_version);
    static CommandResult error_result(const std::string& error);
};

}  // namespace bendcore::commands
