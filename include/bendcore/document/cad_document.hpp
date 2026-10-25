#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "bendcore/document/entity_id.hpp"

namespace bendcore::document {

// Базовая сущность документа
struct Entity {
    EntityId id;
    EntityId parent_id;  // родитель в иерархии (если есть)
    std::string name;
    bool deleted = false;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;

    virtual ~Entity() = default;
};

// Типы сущностей в документе
struct Part : Entity {
    std::string designation;
    std::vector<EntityId> instance_ids;  // экземпляры этой детали
    EntityId primary_instance;  // главный экземпляр
};

struct Instance : Entity {
    EntityId part_id;  // какой Part это экземпляр
    bool adaptive = false;
    // Placement будет в отдельной системе
};

struct Parameter : Entity {
    enum class Type { Double, String, Reference };
    Type type = Type::Double;

    double value_double = 0.0;
    std::string value_string;
    EntityId value_reference;  // для Reference типов

    std::optional<std::string> formula;  // если есть формула
    std::vector<EntityId> dependencies;  // параметры, от которых это зависит
};

struct Feature : Entity {
    std::string feature_type;  // "Pad", "Pocket", "Hole", etc.
    std::map<std::string, std::string> parameters;  // ключ-значение параметры
    EntityId input_geometry;  // входящая геометрия
    EntityId output_geometry;  // результирующая геометрия
};

struct Body : Entity {
    std::vector<EntityId> feature_ids;  // операции в порядке построения
};

struct Sketch : Entity {
    EntityId attached_to;  // лицо, плоскость, или ребро, к которому привязан эскиз
    std::string sketch_data;  // сериализованные эскизы (PlaneGCS)
};

struct Assembly : Entity {
    std::vector<EntityId> component_ids;  // экземпляры компонентов
};

struct Drawing : Entity {
    std::vector<EntityId> view_ids;
    std::string layout_data;
};

// Единый документ CAD
class CadDocument {
public:
    CadDocument();
    ~CadDocument() = default;

    // Идентификация документа
    [[nodiscard]] EntityId id() const { return document_id_; }
    [[nodiscard]] int revision() const { return revision_; }
    [[nodiscard]] const std::string& file_path() const { return file_path_; }
    void set_file_path(const std::string& path) { file_path_ = path; }

    // Управление сущностями
    template <typename T>
    EntityId add_entity(const T& entity);

    template <typename T>
    T* get_entity(EntityId id);

    template <typename T>
    const T* get_entity(EntityId id) const;

    bool remove_entity(EntityId id);

    // Иерархия
    [[nodiscard]] bool set_parent(EntityId child_id, EntityId parent_id);
    [[nodiscard]] EntityId parent_of(EntityId id) const;
    [[nodiscard]] std::vector<EntityId> children_of(EntityId id) const;
    [[nodiscard]] std::vector<EntityId> tree_order() const;

    // Проверка целостности
    [[nodiscard]] bool is_valid_hierarchy() const;
    [[nodiscard]] bool has_cycles() const;

    // Версионирование
    void increment_revision() { revision_++; }

    // Получение всех сущностей определённого типа
    [[nodiscard]] std::vector<EntityId> all_parts() const;
    [[nodiscard]] std::vector<EntityId> all_instances() const;
    [[nodiscard]] std::vector<EntityId> all_parameters() const;
    [[nodiscard]] std::vector<EntityId> all_features() const;

    // Проверка существования
    [[nodiscard]] bool contains(EntityId id) const;
    [[nodiscard]] EntityId::Type entity_type(EntityId id) const;

private:
    EntityId document_id_;
    int revision_ = 0;
    std::string file_path_;

    // Хранилище сущностей по ID
    std::map<EntityId, std::shared_ptr<Entity>> entities_;

    // Иерархия: дочерние элементы для каждого ID
    std::map<EntityId, std::vector<EntityId>> hierarchy_;

    // Порядок в дереве (для вывода)
    std::vector<EntityId> tree_order_;

    // Вспомогательные методы
    bool validate_parent_child(EntityId parent, EntityId child) const;
    void rebuild_tree_order();
};

// Специализации для каждого типа сущности
template <>
EntityId CadDocument::add_entity<Part>(const Part& entity);

template <>
EntityId CadDocument::add_entity<Instance>(const Instance& entity);

template <>
EntityId CadDocument::add_entity<Parameter>(const Parameter& entity);

template <>
Part* CadDocument::get_entity<Part>(EntityId id);

template <>
const Part* CadDocument::get_entity<Part>(EntityId id) const;

}  // namespace bendcore::document
