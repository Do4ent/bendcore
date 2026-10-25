#include "bendcore/commands/command_validator.hpp"

namespace bendcore::commands {

std::optional<CommandValidator::ValidationError> CommandValidator::validate(
    const Command& cmd,
    const CadDocument& doc
) const {
    if (cmd.type == "CreatePart") {
        return validate_create_part(cmd.args);
    } else if (cmd.type == "CreateInstance") {
        return validate_create_instance(cmd.args, doc);
    } else if (cmd.type == "SetParent") {
        return validate_set_parent(cmd.args, doc);
    } else if (cmd.type == "DeleteEntity") {
        return validate_delete_entity(cmd.args, doc);
    } else if (cmd.type == "EditPart") {
        return validate_edit_part(cmd.args, doc);
    } else if (cmd.type == "BatchMove") {
        return validate_batch_move(cmd.args, doc);
    } else {
        return ValidationError{"UNKNOWN_COMMAND", "Unknown command type: " + cmd.type};
    }
}

std::optional<CommandValidator::ValidationError> CommandValidator::validate_create_part(
    const json& args
) const {
    if (!has_field(args, "designation")) {
        return ValidationError{"INVALID_ARGS", "Missing required field: designation"};
    }

    if (!is_string(args["designation"])) {
        return ValidationError{"INVALID_ARGS", "Field 'designation' must be string"};
    }

    std::string designation = args["designation"];
    if (designation.empty()) {
        return ValidationError{"INVALID_ARGS", "designation cannot be empty"};
    }

    return std::nullopt;
}

std::optional<CommandValidator::ValidationError> CommandValidator::validate_create_instance(
    const json& args,
    const CadDocument& doc
) const {
    if (!has_field(args, "part_id")) {
        return ValidationError{"INVALID_ARGS", "Missing required field: part_id"};
    }

    if (!is_string(args["part_id"])) {
        return ValidationError{"INVALID_ARGS", "Field 'part_id' must be string"};
    }

    auto part_id_opt = EntityId::from_string(args["part_id"]);
    if (!part_id_opt.has_value()) {
        return ValidationError{"INVALID_ARGS", "Invalid part_id format"};
    }

    EntityId part_id = part_id_opt.value();
    if (!doc.contains(part_id)) {
        return ValidationError{"ENTITY_NOT_FOUND", "Part with id " + args["part_id"].get<std::string>() + " not found"};
    }

    auto* part = doc.get_entity<Part>(part_id);
    if (!part) {
        return ValidationError{"TYPE_MISMATCH", "Entity is not a Part"};
    }

    return std::nullopt;
}

std::optional<CommandValidator::ValidationError> CommandValidator::validate_set_parent(
    const json& args,
    const CadDocument& doc
) const {
    if (!has_field(args, "child_id") || !has_field(args, "parent_id")) {
        return ValidationError{"INVALID_ARGS", "Missing required fields: child_id, parent_id"};
    }

    auto child_opt = EntityId::from_string(args["child_id"]);
    auto parent_opt = EntityId::from_string(args["parent_id"]);

    if (!child_opt.has_value() || !parent_opt.has_value()) {
        return ValidationError{"INVALID_ARGS", "Invalid entity id format"};
    }

    EntityId child_id = child_opt.value();
    EntityId parent_id = parent_opt.value();

    if (!doc.contains(child_id) || !doc.contains(parent_id)) {
        return ValidationError{"ENTITY_NOT_FOUND", "One or both entities not found in document"};
    }

    if (child_id == parent_id) {
        return ValidationError{"CONSTRAINT_VIOLATION", "Entity cannot be its own parent"};
    }

    if (doc.is_ancestor_of(child_id, parent_id)) {
        return ValidationError{"CYCLE_DETECTED", "Setting parent would create a cycle"};
    }

    return std::nullopt;
}

std::optional<CommandValidator::ValidationError> CommandValidator::validate_delete_entity(
    const json& args,
    const CadDocument& doc
) const {
    if (!has_field(args, "entity_id")) {
        return ValidationError{"INVALID_ARGS", "Missing required field: entity_id"};
    }

    auto id_opt = EntityId::from_string(args["entity_id"]);
    if (!id_opt.has_value()) {
        return ValidationError{"INVALID_ARGS", "Invalid entity_id format"};
    }

    if (!doc.contains(id_opt.value())) {
        return ValidationError{"ENTITY_NOT_FOUND", "Entity not found in document"};
    }

    return std::nullopt;
}

std::optional<CommandValidator::ValidationError> CommandValidator::validate_edit_part(
    const json& args,
    const CadDocument& doc
) const {
    if (!has_field(args, "part_id") || !has_field(args, "designation")) {
        return ValidationError{"INVALID_ARGS", "Missing required fields: part_id, designation"};
    }

    auto part_id_opt = EntityId::from_string(args["part_id"]);
    if (!part_id_opt.has_value()) {
        return ValidationError{"INVALID_ARGS", "Invalid part_id format"};
    }

    EntityId part_id = part_id_opt.value();
    if (!doc.contains(part_id)) {
        return ValidationError{"ENTITY_NOT_FOUND", "Part not found"};
    }

    auto* part = doc.get_entity<Part>(part_id);
    if (!part) {
        return ValidationError{"TYPE_MISMATCH", "Entity is not a Part"};
    }

    if (!is_string(args["designation"])) {
        return ValidationError{"INVALID_ARGS", "Field 'designation' must be string"};
    }

    return std::nullopt;
}

std::optional<CommandValidator::ValidationError> CommandValidator::validate_batch_move(
    const json& args,
    const CadDocument& doc
) const {
    if (!has_field(args, "moves")) {
        return ValidationError{"INVALID_ARGS", "Missing required field: moves"};
    }

    if (!args["moves"].is_array()) {
        return ValidationError{"INVALID_ARGS", "Field 'moves' must be array"};
    }

    for (size_t i = 0; i < args["moves"].size(); ++i) {
        const auto& move = args["moves"][i];
        if (!has_field(move, "child_id") || !has_field(move, "parent_id")) {
            return ValidationError{"INVALID_ARGS", "Move at index " + std::to_string(i) + " missing required fields"};
        }
    }

    return std::nullopt;
}

bool CommandValidator::has_field(const json& obj, const std::string& field) const {
    return obj.contains(field);
}

bool CommandValidator::is_string(const json& value) const {
    return value.is_string();
}

bool CommandValidator::is_number(const json& value) const {
    return value.is_number();
}

bool CommandValidator::is_positive(double value) const {
    return value > 0.0;
}

}  // namespace bendcore::commands
