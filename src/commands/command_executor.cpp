#include "bendcore/commands/command_executor.hpp"

namespace bendcore::commands {

CommandExecutor::CommandExecutor(CadDocument& doc) : document_(doc) {}

CommandResult CommandExecutor::execute(const Command& cmd) {
    affected_ids_.clear();

    // Валидировать команду
    auto validation_error = validator_.validate(cmd, document_);
    if (validation_error.has_value()) {
        return CommandResult::error_result(validation_error.value().message);
    }

    // Выполнить команду
    try {
        if (cmd.type == "CreatePart") {
            return execute_create_part(cmd);
        } else if (cmd.type == "CreateInstance") {
            return execute_create_instance(cmd);
        } else if (cmd.type == "SetParent") {
            return execute_set_parent(cmd);
        } else if (cmd.type == "DeleteEntity") {
            return execute_delete_entity(cmd);
        } else if (cmd.type == "EditPart") {
            return execute_edit_part(cmd);
        } else if (cmd.type == "BatchMove") {
            return execute_batch_move(cmd);
        } else {
            return CommandResult::error_result("Unknown command type");
        }
    } catch (const std::exception& e) {
        return CommandResult::error_result(std::string("Execution error: ") + e.what());
    }
}

CommandResult CommandExecutor::execute_create_part(const Command& cmd) {
    Part part;
    part.id = EntityId(EntityId::Type::Part);
    part.name = cmd.args["designation"];
    part.designation = cmd.args["designation"];

    EntityId part_id = document_.add_entity(part);
    record_affected_entity(part_id);

    json result;
    result["part_id"] = part_id.to_string();

    return CommandResult::success_result(result, get_current_version());
}

CommandResult CommandExecutor::execute_create_instance(const Command& cmd) {
    auto part_id_opt = EntityId::from_string(cmd.args["part_id"]);
    if (!part_id_opt.has_value()) {
        return CommandResult::error_result("Invalid part_id");
    }

    EntityId part_id = part_id_opt.value();
    Instance instance;
    instance.id = EntityId(EntityId::Type::Instance);
    instance.name = "Instance-" + part_id.to_string().substr(0, 8);
    instance.part_id = part_id;
    instance.adaptive = false;

    EntityId instance_id = document_.add_entity(instance);
    document_.set_parent(instance_id, part_id);
    record_affected_entity(instance_id);

    json result;
    result["instance_id"] = instance_id.to_string();

    return CommandResult::success_result(result, get_current_version());
}

CommandResult CommandExecutor::execute_set_parent(const Command& cmd) {
    auto child_opt = EntityId::from_string(cmd.args["child_id"]);
    auto parent_opt = EntityId::from_string(cmd.args["parent_id"]);

    if (!child_opt.has_value() || !parent_opt.has_value()) {
        return CommandResult::error_result("Invalid entity ids");
    }

    bool success = document_.set_parent(child_opt.value(), parent_opt.value());
    if (!success) {
        return CommandResult::error_result("Failed to set parent (cycle detected)");
    }

    record_affected_entity(child_opt.value());

    json result;
    result["success"] = true;

    return CommandResult::success_result(result, get_current_version());
}

CommandResult CommandExecutor::execute_delete_entity(const Command& cmd) {
    auto id_opt = EntityId::from_string(cmd.args["entity_id"]);
    if (!id_opt.has_value()) {
        return CommandResult::error_result("Invalid entity_id");
    }

    EntityId entity_id = id_opt.value();
    auto subtree_ids = document_.subtree(entity_id);

    for (EntityId id : subtree_ids) {
        document_.remove_entity(id);
        record_affected_entity(id);
    }

    json result;
    result["deleted_count"] = subtree_ids.size();
    json deleted_ids_array = json::array();
    for (EntityId id : subtree_ids) {
        deleted_ids_array.push_back(id.to_string());
    }
    result["deleted_ids"] = deleted_ids_array;

    return CommandResult::success_result(result, get_current_version());
}

CommandResult CommandExecutor::execute_edit_part(const Command& cmd) {
    auto part_id_opt = EntityId::from_string(cmd.args["part_id"]);
    if (!part_id_opt.has_value()) {
        return CommandResult::error_result("Invalid part_id");
    }

    EntityId part_id = part_id_opt.value();
    Part* part = document_.get_entity<Part>(part_id);
    if (!part) {
        return CommandResult::error_result("Part not found");
    }

    std::string old_designation = part->designation;
    part->designation = cmd.args["designation"];
    part->updated_at = std::chrono::system_clock::now();
    document_.increment_revision();
    record_affected_entity(part_id);

    json result;
    result["old_designation"] = old_designation;
    result["new_designation"] = part->designation;

    return CommandResult::success_result(result, get_current_version());
}

CommandResult CommandExecutor::execute_batch_move(const Command& cmd) {
    int success_count = 0;
    int failure_count = 0;
    json failures = json::array();

    for (size_t i = 0; i < cmd.args["moves"].size(); ++i) {
        const auto& move = cmd.args["moves"][i];

        auto child_opt = EntityId::from_string(move["child_id"]);
        auto parent_opt = EntityId::from_string(move["parent_id"]);

        if (!child_opt.has_value() || !parent_opt.has_value()) {
            failure_count++;
            failures.push_back(json{{"index", i}, {"reason", "Invalid entity ids"}});
            continue;
        }

        bool success = document_.move_subtree(child_opt.value(), parent_opt.value());
        if (success) {
            success_count++;
            record_affected_entity(child_opt.value());
        } else {
            failure_count++;
            failures.push_back(json{{"index", i}, {"reason", "Cycle detected or invalid operation"}});
        }
    }

    json result;
    result["success_count"] = success_count;
    result["failure_count"] = failure_count;
    result["failures"] = failures;

    return CommandResult::success_result(result, get_current_version());
}

void CommandExecutor::record_affected_entity(EntityId id) {
    if (id.is_valid()) {
        affected_ids_.push_back(id);
    }
}

std::string CommandExecutor::get_current_version() const {
    return std::to_string(document_.revision());
}

}  // namespace bendcore::commands
