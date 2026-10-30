// Реализация редактора параметров чертежа
#include "bendcore/drawing_editor.hpp"
#include "bendcore/formula.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>

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

namespace {

using nlohmann::json;
using Param = DrawingParameterEditor::EditableParameter;
using State = DrawingParameterEditor::DrawingState;

json to_json(const State& st) {
    json params = json::array();
    for (const auto& p : st.parameters) {
        params.push_back({{"id", p.id}, {"name", p.name}, {"value", p.current_value},
                          {"min", p.min_value}, {"max", p.max_value}, {"unit", p.unit},
                          {"is_formula", p.is_formula}, {"formula", p.formula_text},
                          {"description", p.description}});
    }
    return {{"format", "bendcore-drawing-params"}, {"version", 1}, {"drawing_id", st.drawing_id},
            {"last_modified_at", st.last_modified_at}, {"modified_by", st.modified_by},
            {"metadata", st.metadata}, {"parameters", params}};
}

State from_json(const json& j, const std::string& expected_id) {
    if (!j.is_object() || j.value("format", "") != "bendcore-drawing-params") {
        throw std::runtime_error("не файл параметров чертежа BendCore");
    }
    State st;
    st.drawing_id = j.at("drawing_id").get<std::string>();
    if (st.drawing_id != expected_id) {
        throw std::runtime_error("файл принадлежит другому чертежу: " + st.drawing_id);
    }
    st.last_modified_at = j.value("last_modified_at", "");
    st.modified_by = j.value("modified_by", "");
    if (j.contains("metadata")) st.metadata = j.at("metadata").get<std::map<std::string, std::string>>();
    for (const auto& jp : j.at("parameters")) {
        Param p{};
        p.id = jp.at("id").get<std::string>();
        p.name = jp.value("name", "");
        p.current_value = jp.at("value").get<double>();
        p.min_value = jp.value("min", 0.0);
        p.max_value = jp.value("max", 0.0);
        p.unit = jp.value("unit", "");
        p.is_formula = jp.value("is_formula", false);
        p.formula_text = jp.value("formula", "");
        p.description = jp.value("description", "");
        st.parameters.push_back(std::move(p));
    }
    return st;
}

}  // namespace

void DrawingParameterEditor::set_storage_dir(const std::string& dir) { storage_dir_ = dir; }

void DrawingParameterEditor::set_variable(const std::string& name, double value) {
    variables_[name] = value;
}

std::string DrawingParameterEditor::storage_path(const std::string& drawing_id) const {
    // Идентификатор становится именем файла: запрещаем разделители путей и «..».
    if (drawing_id.empty() || drawing_id.find_first_of("/\\") != std::string::npos ||
        drawing_id.find("..") != std::string::npos) {
        throw std::invalid_argument("недопустимый идентификатор чертежа: " + drawing_id);
    }
    return (std::filesystem::path(storage_dir_) / (drawing_id + ".json")).string();
}

void DrawingParameterEditor::load_drawing(const std::string& drawing_id) {
    State loaded;
    loaded.drawing_id = drawing_id;

    if (!storage_dir_.empty()) {
        const std::string path = storage_path(drawing_id);
        std::ifstream in(path, std::ios::binary);
        if (in) {
            json j;
            try {
                in >> j;
            } catch (const json::exception& e) {
                throw std::runtime_error("повреждён файл чертежа " + path + ": " + e.what());
            }
            try {
                loaded = from_json(j, drawing_id);
            } catch (const json::exception& e) {
                throw std::runtime_error("неверная структура файла " + path + ": " + e.what());
            }
        }
    }

    current_state_ = loaded;
    last_saved_state_ = loaded;
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
    if (!storage_dir_.empty() && !current_state_.drawing_id.empty()) {
        namespace fs = std::filesystem;
        const std::string path = storage_path(current_state_.drawing_id);
        std::error_code ec;
        fs::create_directories(storage_dir_, ec);
        const std::string tmp = path + ".tmp";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out) throw std::runtime_error("не удалось открыть для записи: " + tmp);
            out << to_json(current_state_).dump(2);
            out.flush();
            if (!out) throw std::runtime_error("ошибка записи: " + tmp);
        }
        fs::rename(tmp, path, ec);
        if (ec) {
            fs::remove(tmp, ec);
            throw std::runtime_error("не удалось сохранить " + path);
        }
    }
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
    if (formula.empty()) return std::nullopt;
    formula::EvaluationContext ctx;
    ctx.variables = variables_;
    try {
        const double v = formula::FormulaEvaluator::evaluate_formula(formula, ctx);
        if (!std::isfinite(v)) return std::nullopt;
        return v;
    } catch (const std::exception&) {
        return std::nullopt;  // синтаксическая ошибка или неизвестная переменная
    }
}

} // namespace bendcore::drawing
