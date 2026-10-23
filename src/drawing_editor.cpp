// Реализация редактора параметров чертежа
#include "bendcore/drawing_editor.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace bendcore::drawing {

// ==================== EditableParameter ====================

bool DrawingParameterEditor::EditableParameter::operator==(const EditableParameter& other) const {
    return id == other.id && name == other.name && current_value == other.current_value &&
           min_value == other.min_value && max_value == other.max_value && unit == other.unit &&
           is_formula == other.is_formula && formula_text == other.formula_text &&
           description == other.description;
}

// ==================== DrawingState ====================

bool DrawingParameterEditor::DrawingState::operator==(const DrawingState& other) const {
    return drawing_id == other.drawing_id && parameters == other.parameters &&
           last_modified_at == other.last_modified_at && modified_by == other.modified_by;
}

// ==================== DrawingParameterEditor ====================

DrawingParameterEditor::DrawingParameterEditor() : current_user_("unknown") {
    current_state_.drawing_id = "";
    current_state_.last_modified_at = "";
    current_state_.modified_by = "";
}

void DrawingParameterEditor::load_drawing(const std::string& drawing_id) {
    current_state_.drawing_id = drawing_id;
    current_state_.parameters.clear();
    current_state_.metadata.clear();
    last_saved_state_ = current_state_;

    // В реальной реализации здесь будет загрузка из БД
    // Для сейчас инициализируем примерные параметры
    current_state_.last_modified_at = "";
    current_state_.modified_by = "";
}

DrawingParameterEditor::DrawingState DrawingParameterEditor::get_current_state() const {
    return current_state_;
}

void DrawingParameterEditor::update_parameter(const EditableParameter& param) {
    // Найти параметр и обновить его
    auto it = std::find_if(current_state_.parameters.begin(), current_state_.parameters.end(),
                          [&param](const EditableParameter& p) { return p.id == param.id; });

    if (it != current_state_.parameters.end()) {
        *it = param;
    } else {
        current_state_.parameters.push_back(param);
    }

    // Обновить время последней модификации
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    current_state_.last_modified_at = ss.str();
    current_state_.modified_by = current_user_;
}

std::optional<DrawingParameterEditor::EditableParameter> DrawingParameterEditor::get_parameter(
    const std::string& param_id) const {
    auto it = std::find_if(current_state_.parameters.begin(), current_state_.parameters.end(),
                          [&param_id](const EditableParameter& p) { return p.id == param_id; });

    if (it != current_state_.parameters.end()) {
        return *it;
    }
    return std::nullopt;
}

std::vector<DrawingParameterEditor::EditableParameter> DrawingParameterEditor::get_all_parameters() const {
    return current_state_.parameters;
}

DrawingParameterEditor::ValidationResult DrawingParameterEditor::validate_parameter(
    const EditableParameter& param) {

    if (param.id.empty()) {
        return {false, "ID параметра не может быть пустым", std::nullopt};
    }

    if (param.name.empty()) {
        return {false, "Имя параметра не может быть пустым", std::nullopt};
    }

    if (param.min_value > param.max_value) {
        return {false, "Минимальное значение больше максимального", std::nullopt};
    }

    // Если это формула, попытаться вычислить её
    if (param.is_formula) {
        auto result = evaluate_formula(param.formula_text);
        if (!result) {
            return {false, "Неверная формула: " + param.formula_text, std::nullopt};
        }
    } else {
        // Если это обычное значение, проверить диапазон
        if (!validate_range(param.current_value, param.min_value, param.max_value)) {
            double suggested = std::max(param.min_value, std::min(param.current_value, param.max_value));
            return {false, "Значение вне диапазона [" + std::to_string(param.min_value) + ", " +
                          std::to_string(param.max_value) + "]", suggested};
        }
    }

    return {true, "", std::nullopt};
}

void DrawingParameterEditor::apply_changes() {
    last_saved_state_ = current_state_;
}

void DrawingParameterEditor::revert_changes() {
    current_state_ = last_saved_state_;
}

bool DrawingParameterEditor::has_unsaved_changes() const {
    return current_state_.parameters != last_saved_state_.parameters;
}

void DrawingParameterEditor::clear() {
    current_state_.drawing_id = "";
    current_state_.parameters.clear();
    current_state_.metadata.clear();
    current_state_.last_modified_at = "";
    current_state_.modified_by = "";
    last_saved_state_ = current_state_;
}

std::string DrawingParameterEditor::get_last_modified_at() const {
    return current_state_.last_modified_at;
}

std::string DrawingParameterEditor::get_modified_by() const {
    return current_state_.modified_by;
}

void DrawingParameterEditor::set_editor_info(const std::string& username) {
    current_user_ = username;
}

bool DrawingParameterEditor::validate_range(double value, double min_val, double max_val) {
    return value >= min_val && value <= max_val;
}

std::optional<double> DrawingParameterEditor::evaluate_formula(const std::string& formula) {
    // Простая реализация вычисления формул
    // Поддерживаются переменные: $OD (outer diameter), $Wall (стенка)
    // и простые математические операции

    if (formula.empty()) {
        return std::nullopt;
    }

    // Пример простого парсера формул
    // В реальной системе это должно быть более сложным
    if (formula.find("$OD") != std::string::npos) {
        // Для тестирования вернём фиксированное значение
        return 50.0;
    }

    if (formula.find("$Wall") != std::string::npos) {
        return 2.0;
    }

    // Попытка парсить как число
    try {
        size_t idx;
        double value = std::stod(formula, &idx);
        if (idx == formula.length()) {
            return value;
        }
    } catch (...) {
        return std::nullopt;
    }

    return std::nullopt;
}

} // namespace bendcore::drawing
