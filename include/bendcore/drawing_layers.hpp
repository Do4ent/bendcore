// Система слоёв и аннотаций для чертежей (Q4 Task 1 Subtask 3)
// Управление видимостью слоёв, аннотациями, размерами и метками
#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>

namespace bendcore::drawing {

class DrawingLayerManager {
public:
    // Слой чертежа
    struct Layer {
        std::string id;                    // Уникальный идентификатор
        std::string name;                  // Имя слоя ("Контуры", "Размеры", и т.д.)
        bool visible = true;               // Видимость слоя
        bool editable = true;              // Редактируемость слоя
        bool locked = false;               // Заблокирован ли слой
        std::vector<std::string> element_ids;  // ID элементов в этом слое
        std::string parent_layer_id;       // ID родительского слоя (для групп)

        bool operator==(const Layer& other) const;
    };

    // Аннотация (размер, текст, метка)
    struct Annotation {
        std::string id;                    // Уникальный идентификатор
        std::string type;                  // "dimension", "note", "label", "arrow"
        std::string text;                  // Текст аннотации
        double x = 0.0;                    // X позиция на чертеже (в мм)
        double y = 0.0;                    // Y позиция на чертеже (в мм)
        std::string layer_id;              // ID слоя, к которому относится аннотация
        std::map<std::string, std::string> properties;  // Дополнительные свойства

        bool operator==(const Annotation& other) const;
    };

    // Группа слоёв
    struct LayerGroup {
        std::string id;
        std::string name;
        std::vector<std::string> layer_ids;
        bool expanded = true;
    };

    DrawingLayerManager();
    ~DrawingLayerManager() = default;

    // ===== Управление слоями =====

    // Добавить новый слой
    void add_layer(const Layer& layer);

    // Удалить слой по ID
    void remove_layer(const std::string& layer_id);

    // Получить слой по ID
    std::optional<Layer> get_layer(const std::string& layer_id) const;

    // Получить все слои
    std::vector<Layer> get_all_layers() const;

    // Обновить слой
    void update_layer(const Layer& layer);

    // Переименовать слой
    void rename_layer(const std::string& layer_id, const std::string& new_name);

    // Переключить видимость слоя
    void toggle_layer_visibility(const std::string& layer_id);

    // Установить видимость слоя
    void set_layer_visibility(const std::string& layer_id, bool visible);

    // Блокировать/разблокировать слой
    void set_layer_locked(const std::string& layer_id, bool locked);

    // Установить редактируемость слоя
    void set_layer_editable(const std::string& layer_id, bool editable);

    // ===== Управление элементами в слоях =====

    // Добавить элемент в слой
    void add_element_to_layer(const std::string& layer_id, const std::string& element_id);

    // Удалить элемент из слоя
    void remove_element_from_layer(const std::string& layer_id, const std::string& element_id);

    // Получить все элементы в слое
    std::vector<std::string> get_elements_in_layer(const std::string& layer_id) const;

    // Получить слой, содержащий элемент
    std::optional<std::string> get_layer_for_element(const std::string& element_id) const;

    // ===== Управление аннотациями =====

    // Добавить аннотацию
    void add_annotation(const Annotation& annot);

    // Удалить аннотацию
    void remove_annotation(const std::string& annot_id);

    // Получить аннотацию по ID
    std::optional<Annotation> get_annotation(const std::string& annot_id) const;

    // Получить все аннотации в слое
    std::vector<Annotation> get_annotations_for_layer(const std::string& layer_id) const;

    // Получить все аннотации определённого типа
    std::vector<Annotation> get_annotations_by_type(const std::string& type) const;

    // Обновить аннотацию
    void update_annotation(const Annotation& annot);

    // Переместить аннотацию
    void move_annotation(const std::string& annot_id, double x, double y);

    // ===== Управление группами слоёв =====

    // Создать группу слоёв
    void create_layer_group(const LayerGroup& group);

    // Удалить группу слоёв
    void remove_layer_group(const std::string& group_id);

    // Получить группу по ID
    std::optional<LayerGroup> get_layer_group(const std::string& group_id) const;

    // ===== Общие операции =====

    // Очистить все слои и аннотации
    void clear();

    // Получить количество слоёв
    size_t layer_count() const;

    // Получить количество аннотаций
    size_t annotation_count() const;

private:
    std::map<std::string, Layer> layers_;
    std::map<std::string, Annotation> annotations_;
    std::map<std::string, LayerGroup> layer_groups_;

    // Проверить, существует ли слой
    bool layer_exists(const std::string& layer_id) const;

    // Создать уникальный ID для слоя
    std::string generate_layer_id();
};

} // namespace bendcore::drawing
