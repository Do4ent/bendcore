#pragma once

#include <optional>
#include <string>
#include <vector>

#include "bendcore/commands/command.hpp"
#include "bendcore/document/cad_document.hpp"

namespace bendcore::commands {

/**
 * CommandValidator — проверяет корректность команд перед выполнением
 * CAD-CORE-005: CommandEngine validation layer
 */
class CommandValidator {
public:
    struct ValidationError {
        std::string code;  // INVALID_ARGS, ENTITY_NOT_FOUND, CYCLE_DETECTED, etc.
        std::string message;
        std::vector<std::string> details;
    };

    /**
     * Валидировать команду перед выполнением
     * @return nullopt если валидация успешна, ValidationError если есть ошибки
     */
    [[nodiscard]] std::optional<ValidationError> validate(
        const Command& cmd,
        const CadDocument& doc
    ) const;

private:
    // Валидаторы для каждого типа команды
    [[nodiscard]] std::optional<ValidationError> validate_create_part(
        const json& args
    ) const;

    [[nodiscard]] std::optional<ValidationError> validate_create_instance(
        const json& args,
        const CadDocument& doc
    ) const;

    [[nodiscard]] std::optional<ValidationError> validate_set_parent(
        const json& args,
        const CadDocument& doc
    ) const;

    [[nodiscard]] std::optional<ValidationError> validate_delete_entity(
        const json& args,
        const CadDocument& doc
    ) const;

    [[nodiscard]] std::optional<ValidationError> validate_edit_part(
        const json& args,
        const CadDocument& doc
    ) const;

    [[nodiscard]] std::optional<ValidationError> validate_batch_move(
        const json& args,
        const CadDocument& doc
    ) const;

    // Вспомогательные методы
    [[nodiscard]] bool has_field(const json& obj, const std::string& field) const;
    [[nodiscard]] bool is_string(const json& value) const;
    [[nodiscard]] bool is_number(const json& value) const;
    [[nodiscard]] bool is_positive(double value) const;
};

}  // namespace bendcore::commands
