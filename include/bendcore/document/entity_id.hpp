#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace bendcore::document {

// Типизированный идентификатор сущности CAD
// UUID-основано для стабильности после сохранения/загрузки
class EntityId {
public:
    enum class Type : uint8_t {
        Invalid = 0,
        Part = 1,
        Instance = 2,
        Body = 3,
        Sketch = 4,
        Feature = 5,
        Assembly = 6,
        Parameter = 7,
        Dimension = 8,
        Reference = 9,
        Drawing = 10,
        Annotation = 11,
        CustomProperty = 12
    };

    // Конструкторы
    EntityId() = default;  // Invalid ID
    explicit EntityId(const std::string& uuid_str, Type type);
    explicit EntityId(Type type);  // Генерирует новый UUID

    // Доступ к данным
    [[nodiscard]] const std::string& uuid() const { return uuid_; }
    [[nodiscard]] Type type() const { return type_; }
    [[nodiscard]] bool is_valid() const { return type_ != Type::Invalid; }

    // Сравнение
    [[nodiscard]] bool operator==(const EntityId& other) const;
    [[nodiscard]] bool operator!=(const EntityId& other) const;
    [[nodiscard]] bool operator<(const EntityId& other) const;  // Для std::map

    // Сериализация
    [[nodiscard]] std::string to_string() const;
    static std::optional<EntityId> from_string(const std::string& str);

    // JSON сериализация (для nlohmann/json)
    [[nodiscard]] std::string to_json() const;
    static std::optional<EntityId> from_json(const std::string& json_str);

    // Логирование
    [[nodiscard]] std::string debug_str() const;

private:
    std::string uuid_;  // 36 символов: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    Type type_ = Type::Invalid;

    static std::string generate_uuid();
    [[nodiscard]] static Type parse_type(uint8_t type_byte);
};

// Шаблонные типы для type-safety (опционально, для будущих версий)
// using PartId = EntityId;
// using InstanceId = EntityId;
// etc.

// Специализация std::hash для использования в std::unordered_map
}  // namespace bendcore::document

namespace std {
template <>
struct hash<bendcore::document::EntityId> {
    size_t operator()(const bendcore::document::EntityId& id) const;
};
}  // namespace std
