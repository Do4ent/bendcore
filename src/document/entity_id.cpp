#include "bendcore/document/entity_id.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <random>
#include <sstream>

namespace bendcore::document {

// Генерация UUID v4
std::string EntityId::generate_uuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    for (int i = 0; i < 8; i++) ss << std::hex << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << std::hex << dis(gen);
    ss << "-4";  // version 4
    for (int i = 0; i < 3; i++) ss << std::hex << dis(gen);
    ss << "-";
    ss << std::hex << dis2(gen);
    for (int i = 0; i < 3; i++) ss << std::hex << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << std::hex << dis(gen);

    return ss.str();
}

EntityId::Type EntityId::parse_type(uint8_t type_byte) {
    if (type_byte > static_cast<uint8_t>(Type::CustomProperty)) {
        return Type::Invalid;
    }
    return static_cast<Type>(type_byte);
}

// Конструкторы
EntityId::EntityId(const std::string& uuid_str, Type type) : uuid_(uuid_str), type_(type) {
    // Валидация UUID формата: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    if (uuid_.length() != 36 ||
        uuid_[8] != '-' || uuid_[13] != '-' || uuid_[18] != '-' || uuid_[23] != '-') {
        uuid_.clear();
        type_ = Type::Invalid;
    }
}

EntityId::EntityId(Type type) : type_(type) {
    if (type != Type::Invalid) {
        uuid_ = generate_uuid();
    }
}

// Сравнение
bool EntityId::operator==(const EntityId& other) const {
    return uuid_ == other.uuid_ && type_ == other.type_;
}

bool EntityId::operator!=(const EntityId& other) const {
    return !(*this == other);
}

bool EntityId::operator<(const EntityId& other) const {
    if (type_ != other.type_) return static_cast<uint8_t>(type_) < static_cast<uint8_t>(other.type_);
    return uuid_ < other.uuid_;
}

// Сериализация
std::string EntityId::to_string() const {
    if (!is_valid()) return "";
    std::stringstream ss;
    ss << static_cast<int>(type_) << ":" << uuid_;
    return ss.str();
}

std::optional<EntityId> EntityId::from_string(const std::string& str) {
    auto colon_pos = str.find(':');
    if (colon_pos == std::string::npos || colon_pos == 0 || colon_pos == str.length() - 1) {
        return std::nullopt;
    }

    try {
        int type_int = std::stoi(str.substr(0, colon_pos));
        Type type = parse_type(static_cast<uint8_t>(type_int));
        if (type == Type::Invalid) return std::nullopt;

        std::string uuid = str.substr(colon_pos + 1);
        return EntityId(uuid, type);
    } catch (...) {
        return std::nullopt;
    }
}

// JSON сериализация
std::string EntityId::to_json() const {
    if (!is_valid()) return "{}";
    std::stringstream ss;
    ss << "{\"type\":" << static_cast<int>(type_) << ",\"uuid\":\"" << uuid_ << "\"}";
    return ss.str();
}

std::optional<EntityId> EntityId::from_json(const std::string& json_str) {
    // Простой парсер для нашего формата: {"type":N,"uuid":"..."}
    if (json_str.length() < 20 || json_str[0] != '{') {
        return std::nullopt;
    }

    try {
        // Найти "type": и прочитать число
        auto type_start = json_str.find("\"type\":");
        if (type_start == std::string::npos) return std::nullopt;
        type_start += 7;  // длина "\"type\":"

        // Пропустить пробелы
        while (type_start < json_str.length() && json_str[type_start] == ' ') type_start++;

        // Найти конец числа (запятая или закрывающий бра)
        auto type_end = type_start;
        while (type_end < json_str.length() && std::isdigit(json_str[type_end])) type_end++;

        if (type_start == type_end) return std::nullopt;

        int type_int = std::stoi(json_str.substr(type_start, type_end - type_start));
        Type type = parse_type(static_cast<uint8_t>(type_int));
        if (type == Type::Invalid) return std::nullopt;

        // Найти "uuid":"..."
        auto uuid_start = json_str.find("\"uuid\":\"");
        if (uuid_start == std::string::npos) return std::nullopt;
        uuid_start += 8;  // длина "\"uuid\":\""

        auto uuid_end = json_str.find("\"", uuid_start);
        if (uuid_end == std::string::npos || uuid_end - uuid_start != 36) {
            return std::nullopt;
        }

        std::string uuid = json_str.substr(uuid_start, uuid_end - uuid_start);
        return EntityId(uuid, type);
    } catch (...) {
        return std::nullopt;
    }
}

// Логирование
std::string EntityId::debug_str() const {
    if (!is_valid()) return "EntityId(INVALID)";

    std::string type_name;
    switch (type_) {
        case Type::Part: type_name = "Part"; break;
        case Type::Instance: type_name = "Instance"; break;
        case Type::Body: type_name = "Body"; break;
        case Type::Sketch: type_name = "Sketch"; break;
        case Type::Feature: type_name = "Feature"; break;
        case Type::Assembly: type_name = "Assembly"; break;
        case Type::Parameter: type_name = "Parameter"; break;
        case Type::Dimension: type_name = "Dimension"; break;
        case Type::Reference: type_name = "Reference"; break;
        case Type::Drawing: type_name = "Drawing"; break;
        case Type::Annotation: type_name = "Annotation"; break;
        case Type::CustomProperty: type_name = "CustomProperty"; break;
        case Type::Invalid: type_name = "Invalid"; break;
    }

    // Показать только первые 8 символов UUID
    std::string short_uuid = uuid_.substr(0, 8);
    return type_name + "(" + short_uuid + "...)";
}

}  // namespace bendcore::document

// Специализация std::hash
namespace std {
size_t hash<bendcore::document::EntityId>::operator()(
    const bendcore::document::EntityId& id) const {
    auto h1 = std::hash<std::string>{}(id.uuid());
    auto h2 = std::hash<int>{}(static_cast<int>(id.type()));
    return h1 ^ (h2 << 1);
}
}  // namespace std
