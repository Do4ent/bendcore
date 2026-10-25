#pragma once

#include "bendcore/commands/command.hpp"
#include "bendcore/commands/command_validator.hpp"
#include "bendcore/document/cad_document.hpp"

namespace bendcore::commands {

/**
 * CommandExecutor — выполняет команды на CadDocument с валидацией
 * CAD-CORE-005: CommandEngine execution layer
 */
class CommandExecutor {
public:
    explicit CommandExecutor(CadDocument& doc);
    ~CommandExecutor() = default;

    /**
     * Выполнить команду
     * @return результат выполнения
     */
    [[nodiscard]] CommandResult execute(const Command& cmd);

private:
    CadDocument& document_;
    CommandValidator validator_;

    // Обработчики для каждого типа команды
    [[nodiscard]] CommandResult execute_create_part(const Command& cmd);
    [[nodiscard]] CommandResult execute_create_instance(const Command& cmd);
    [[nodiscard]] CommandResult execute_set_parent(const Command& cmd);
    [[nodiscard]] CommandResult execute_delete_entity(const Command& cmd);
    [[nodiscard]] CommandResult execute_edit_part(const Command& cmd);
    [[nodiscard]] CommandResult execute_batch_move(const Command& cmd);

    // Вспомогательные методы
    void record_affected_entity(EntityId id);
    std::string get_current_version() const;

    // Состояние текущего выполнения
    std::vector<EntityId> affected_ids_;
};

}  // namespace bendcore::commands
