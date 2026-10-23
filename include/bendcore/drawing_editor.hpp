// Редактор параметров чертежа (Q4 Task 1 Subtask 1)
// Позволяет редактировать параметры трубы: длины, углы, формулы и т.д.
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <map>
#include <ctime>

namespace bendcore::drawing {

class DrawingParameterEditor {
public:
    // Редактируемый параметр с поддержкой формул
    struct EditableParameter {
        std::string id;              // Уникальный идентификатор ("length_1", "bend_angle_1")
        std::string name;            // Отображаемое имя ("Длина 1", "Угол изгиба 1")
        double current_value;        // Текущее значение
        double min_value;            // Минимальное значение
        double max_value;            // Максимальное значение
        std::string unit;            // Единица измерения ("mm", "deg", "rad")
        bool is_formula;             // Является ли значение формулой
        std::string formula_text;    // Текст формулы ($OD, $Wall, и т.д.)
        std::string description;     // Описание параметра

        bool operator==(const EditableParameter& other) const;
    };

    // Состояние чертежа с историей изменений
    struct DrawingState {
        std::string drawing_id;
        std::vector<EditableParameter> parameters;
        std::string last_modified_at;
        std::string modified_by;
        std::map<std::string, std::string> metadata;

        bool operator==(const DrawingState& other) const;
    };

    // Результат валидации параметра
    struct ValidationResult {
        bool valid;
        std::string error_message;
        std::optional<double> suggested_value;
    };

    DrawingParameterEditor();
    ~DrawingParameterEditor() = default;

    // Загрузить чертёж со всеми параметрами
    void load_drawing(const std::string& drawing_id);

    // Получить текущее состояние чертежа
    DrawingState get_current_state() const;

    // Обновить параметр (в реальном времени)
    void update_parameter(const EditableParameter& param);

    // Получить параметр по ID
    std::optional<EditableParameter> get_parameter(const std::string& param_id) const;

    // Получить все параметры
    std::vector<EditableParameter> get_all_parameters() const;

    // Валидировать параметр (проверка min/max, формула и т.д.)
    ValidationResult validate_parameter(const EditableParameter& param);

    // Применить изменения (сохранить)
    void apply_changes();

    // Отменить изменения (вернуться к последнему сохранённому состоянию)
    void revert_changes();

    // Проверить наличие несохранённых изменений
    bool has_unsaved_changes() const;

    // Очистить редактор
    void clear();

    // Получить последнее время модификации
    std::string get_last_modified_at() const;

    // Получить того, кто последним редактировал
    std::string get_modified_by() const;

    // Установить информацию о редакторе
    void set_editor_info(const std::string& username);

private:
    DrawingState current_state_;
    DrawingState last_saved_state_;
    std::string current_user_;

    // Применить валидацию значения к параметру
    bool validate_range(double value, double min_val, double max_val);

    // Вычислить формулу (если параметр является формулой)
    std::optional<double> evaluate_formula(const std::string& formula);
};

} // namespace bendcore::drawing
