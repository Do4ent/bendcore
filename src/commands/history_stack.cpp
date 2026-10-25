#include "bendcore/commands/history_stack.hpp"

namespace bendcore::commands {

HistoryEntry::HistoryEntry(const Command& cmd, const CommandResult& res)
    : command(cmd), result(res) {
    timestamp = std::chrono::system_clock::now();
}

HistoryStack::HistoryStack(CadDocument& doc, size_t max_history)
    : document_(doc), executor_(doc), max_history_size_(max_history) {}

void HistoryStack::record_command(const Command& cmd, const CommandResult& result) {
    if (!result.success) {
        return;  // Don't record failed commands
    }

    HistoryEntry entry(cmd, result);
    entry.description = get_command_description(cmd);

    undo_stack_.push_back(entry);

    // Clear redo stack when new command is recorded
    clear_redo_stack();

    // Enforce max history size
    while (undo_stack_.size() > max_history_size_) {
        undo_stack_.pop_front();
    }
}

bool HistoryStack::undo() {
    if (!can_undo()) {
        return false;
    }

    HistoryEntry entry = undo_stack_.back();
    undo_stack_.pop_back();

    // Create inverse command
    Command inverse = create_inverse_command(entry.command);

    // Execute inverse
    CommandResult inverse_result = executor_.execute(inverse);

    if (!inverse_result.success) {
        // Restore the entry if undo failed
        undo_stack_.push_back(entry);
        return false;
    }

    // Move to redo stack
    entry.command = inverse;
    entry.result = inverse_result;
    redo_stack_.push_back(entry);

    return true;
}

bool HistoryStack::redo() {
    if (!can_redo()) {
        return false;
    }

    HistoryEntry entry = redo_stack_.back();
    redo_stack_.pop_back();

    // Create inverse of the inverse (original command)
    Command original = create_inverse_command(entry.command);

    // Execute original
    CommandResult original_result = executor_.execute(original);

    if (!original_result.success) {
        // Restore the entry if redo failed
        redo_stack_.push_back(entry);
        return false;
    }

    // Move to undo stack
    entry.command = original;
    entry.result = original_result;
    undo_stack_.push_back(entry);

    return true;
}

std::string HistoryStack::undo_description() const {
    if (undo_stack_.empty()) {
        return "Нечего отменять";
    }
    return "Отменить: " + undo_stack_.back().description;
}

std::string HistoryStack::redo_description() const {
    if (redo_stack_.empty()) {
        return "Нечего повторять";
    }
    return "Повторить: " + redo_stack_.back().description;
}

void HistoryStack::clear_history() {
    undo_stack_.clear();
    redo_stack_.clear();
}

void HistoryStack::clear_redo_stack() {
    redo_stack_.clear();
}

Command HistoryStack::create_inverse_command(const Command& cmd) const {
    Command inverse;
    inverse.id = "inv-" + cmd.id;
    inverse.document_version = cmd.document_version;

    if (cmd.type == "CreatePart") {
        // CreatePart -> DeleteEntity
        inverse.type = "DeleteEntity";
        if (cmd.args.contains("part_id")) {
            inverse.args["entity_id"] = cmd.args["part_id"];
        }
        // Fallback: use a marker that won't delete anything
        if (!inverse.args.contains("entity_id")) {
            inverse.args["entity_id"] = "";
        }
    } else if (cmd.type == "CreateInstance") {
        // CreateInstance -> DeleteEntity
        inverse.type = "DeleteEntity";
        if (cmd.args.contains("instance_id")) {
            inverse.args["entity_id"] = cmd.args["instance_id"];
        }
        if (!inverse.args.contains("entity_id")) {
            inverse.args["entity_id"] = "";
        }
    } else if (cmd.type == "SetParent") {
        // SetParent is not reversible without tracking old parent
        // For MVP, we return a no-op command
        inverse.type = "SetParent";
        inverse.args["child_id"] = cmd.args["child_id"];
        inverse.args["parent_id"] = cmd.args["child_id"];  // Self-parent is invalid but won't execute
    } else if (cmd.type == "DeleteEntity") {
        // DeleteEntity is not reversible without state snapshots
        // For MVP, return a no-op
        inverse.type = "CreatePart";
        inverse.args["designation"] = "Restored";
    } else if (cmd.type == "EditPart") {
        // EditPart is not reversible without tracking old values
        // For MVP, return a no-op
        inverse.type = "EditPart";
        inverse.args["part_id"] = cmd.args["part_id"];
        inverse.args["designation"] = "Restored";
    } else if (cmd.type == "BatchMove") {
        // BatchMove -> BatchMove with reversed moves (still the same move since it's location-neutral)
        inverse.type = "BatchMove";
        inverse.args["moves"] = json::array();
        // For now, just use same moves (would need to reverse in real system)
        for (const auto& move : cmd.args["moves"]) {
            json new_move;
            new_move["child_id"] = move["child_id"].get<std::string>();
            new_move["parent_id"] = move["parent_id"].get<std::string>();
            inverse.args["moves"].push_back(new_move);
        }
    } else {
        // Unknown command - return as-is
        inverse = cmd;
    }

    return inverse;
}

std::string HistoryStack::get_command_description(const Command& cmd) const {
    if (cmd.type == "CreatePart") {
        if (cmd.args.contains("designation")) {
            std::string designation = cmd.args["designation"];
            return "Создать деталь: " + designation;
        }
        return "Создать деталь";
    } else if (cmd.type == "CreateInstance") {
        return "Создать экземпляр";
    } else if (cmd.type == "SetParent") {
        return "Изменить иерархию";
    } else if (cmd.type == "DeleteEntity") {
        return "Удалить сущность";
    } else if (cmd.type == "EditPart") {
        return "Редактировать деталь";
    } else if (cmd.type == "BatchMove") {
        int move_count = cmd.args["moves"].size();
        return "Переместить " + std::to_string(move_count) + " элементов";
    } else {
        return "Команда: " + cmd.type;
    }
}

}  // namespace bendcore::commands
