#include "bendcore/parallel_offset.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <map>

namespace bendcore {

constexpr double PI = 3.14159265358979323846;
constexpr double DEG_TO_RAD = PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / PI;

// Коэффициенты для разных материалов
// Влияют на минимальный радиус гибки и припуски
const std::map<std::string, double> MATERIAL_FACTORS = {
  {"Steel", 1.0},
  {"Stainless Steel", 1.15},
  {"Aluminum", 0.85},
  {"Copper", 0.95},
  {"Titanium", 1.25},
  {"Brass", 0.90}
};

// Минимальный радиус гибки (как кратный внешнему диаметру)
const std::map<std::string, double> MIN_BEND_RADIUS_FACTOR = {
  {"Steel", 2.5},
  {"Stainless Steel", 3.0},
  {"Aluminum", 2.0},
  {"Copper", 2.5},
  {"Titanium", 4.0},
  {"Brass", 2.3}
};

ParallelOffsetCalculator::Result ParallelOffsetCalculator::calculate(
    const Parameters& params) {
  Result result;
  result.is_valid = false;
  validation_errors_.clear();

  // Валидация входных параметров
  if (!validate(params)) {
    result.warnings = get_error_message();
    return result;
  }

  // Преобразование угла в радианы
  double bend_angle_rad = params.bend_angle * DEG_TO_RAD;

  // Расчёт коэффициента нейтрального слоя
  double k_factor = calculate_k_factor(params.outer_diameter,
                                       params.wall_thickness);

  // Расчёт длины первой гибки
  result.bend_1_length = calculate_bend_arc_length(params.bend_radius,
                                                   bend_angle_rad);

  // Геометрический расчёт для второй гибки
  // При параллельном смещении вторая гибка имеет смещённый радиус
  double bend_2_radius = std::sqrt(
    params.bend_radius * params.bend_radius +
    params.offset_distance * params.offset_distance
  );

  // Расчёт угла второй гибки (в общем случае отличается от первого)
  double bend_2_angle = std::atan2(params.offset_distance, params.bend_radius);
  bend_2_angle = (bend_2_angle > bend_angle_rad) ? bend_angle_rad : bend_2_angle;

  result.bend_2_length = calculate_bend_arc_length(bend_2_radius, bend_2_angle);

  // Расчёт прямого участка между гибками
  // Зависит от параллельного смещения и угла
  double offset_factor = params.offset_distance / (params.bend_radius +
                                                    params.outer_diameter / 2.0);
  result.straight_section = std::max(0.0,
    params.offset_distance * (1.0 - std::sin(bend_angle_rad)) /
    std::cos(bend_angle_rad * 0.5)
  );

  // Убедимся, что прямой участок не отрицательный
  if (result.straight_section < 0) {
    result.straight_section = 0.0;
  }

  // Расчёт углов гибок
  result.bend_angles[0] = params.bend_angle;
  result.bend_angles[1] = params.bend_angle *
    (bend_2_radius / params.bend_radius);

  // Расчёт 3D координат первой гибки
  result.bend_1_pos = calculate_bend_position(0.0, bend_angle_rad, 0.0, 1);

  // Расчёт 3D координат второй гибки
  // Вторая гибка находится в смещённой плоскости
  double second_bend_distance = result.bend_1_length + result.straight_section;
  result.bend_2_pos = calculate_bend_position(
    second_bend_distance,
    bend_angle_rad,
    params.offset_distance,
    2
  );

  // Расчёт общей длины с припусками
  result.total_length = calculate_developed_length(
    result.bend_1_length,
    result.straight_section,
    result.bend_2_length,
    k_factor
  );

  // Расчёт уверенности расчёта (зависит от экстремальности параметров)
  double confidence = 95.0;

  // Штраф за очень малое смещение
  if (params.offset_distance < params.outer_diameter * 0.1) {
    confidence -= 5.0;
  }

  // Штраф за очень близкие к экстремуму углы
  if (params.bend_angle < 10.0 || params.bend_angle > 170.0) {
    confidence -= 10.0;
  }

  // Штраф за смещение больше радиуса гибки
  if (params.offset_distance > params.bend_radius) {
    confidence -= 8.0;
  }

  result.confidence = std::max(50.0, confidence);

  // Проверка на предупреждения
  std::stringstream warnings;

  if (result.bend_2_angle > params.bend_angle * 1.1) {
    warnings << "Вторая гибка может быть более сложной. ";
  }

  if (result.straight_section < params.outer_diameter) {
    warnings << "Прямой участок очень короткий. ";
  }

  if (params.bend_radius < params.outer_diameter * 2.0) {
    warnings << "Радиус гибки близок к минимально допустимому. ";
  }

  result.warnings = warnings.str();
  result.is_valid = true;

  return result;
}

bool ParallelOffsetCalculator::validate(const Parameters& params) {
  validation_errors_.clear();
  bool all_valid = true;

  if (!validate_diameter(params.outer_diameter)) {
    all_valid = false;
  }

  if (!validate_wall_thickness(params.outer_diameter, params.wall_thickness)) {
    all_valid = false;
  }

  if (!validate_bend_angle(params.bend_angle)) {
    all_valid = false;
  }

  if (!validate_offset_distance(params.offset_distance,
                               params.outer_diameter)) {
    all_valid = false;
  }

  if (!validate_bend_radius(params.bend_radius, params.outer_diameter,
                           params.wall_thickness, params.material)) {
    all_valid = false;
  }

  if (all_valid) {
    error_message_ = "";
  } else {
    std::stringstream ss;
    for (size_t i = 0; i < validation_errors_.size(); ++i) {
      ss << validation_errors_[i].message;
      if (i < validation_errors_.size() - 1) {
        ss << "; ";
      }
    }
    error_message_ = ss.str();
  }

  return all_valid;
}

std::string ParallelOffsetCalculator::get_error_message() const {
  return error_message_;
}

std::vector<ParallelOffsetCalculator::ValidationError>
ParallelOffsetCalculator::get_validation_errors() const {
  return validation_errors_;
}

double ParallelOffsetCalculator::get_material_factor(
    const std::string& material) {
  auto it = MATERIAL_FACTORS.find(material);
  if (it != MATERIAL_FACTORS.end()) {
    return it->second;
  }
  return 1.0; // Стандартный фактор
}

bool ParallelOffsetCalculator::can_bend(double od, double wall_thickness,
                                        double bend_radius,
                                        const std::string& material) {
  auto it = MIN_BEND_RADIUS_FACTOR.find(material);
  double min_factor = (it != MIN_BEND_RADIUS_FACTOR.end()) ? it->second : 2.5;
  double min_radius = od * min_factor;

  return bend_radius >= min_radius;
}

double ParallelOffsetCalculator::calculate_bend_arc_length(
    double bend_radius, double bend_angle_rad) const {
  // Длина дуги = R * θ, где θ в радианах
  return bend_radius * bend_angle_rad;
}

std::array<double, 3> ParallelOffsetCalculator::calculate_bend_position(
    double distance, double angle_rad, double offset_distance,
    int bend_number) const {
  // Расчёт 3D координат гибки
  // X - продольная ось трубы
  // Y - направление смещения
  // Z - вертикаль

  double x = distance;
  double y = offset_distance * bend_number;
  double z = 0.0;

  if (bend_number == 2) {
    // Для второй гибки добавляем высоту из-за углового перегиба
    z = offset_distance * (1.0 - std::cos(angle_rad));
  }

  return {x, y, z};
}

double ParallelOffsetCalculator::calculate_k_factor(double od,
                                                     double wall_thickness) const {
  // K-фактор определяет положение нейтрального слоя
  // K = (расстояние до нейтрального слоя) / толщина стенки

  // Стандартная формула: K ≈ 0.3...0.5 в зависимости от материала
  // Упрощённо: K ≈ (t + 0.2d) / t

  if (wall_thickness <= 0) {
    return 0.4; // Значение по умолчанию
  }

  double ratio = wall_thickness / od;

  if (ratio > 0.1) {
    return 0.3; // Толстостенная труба
  } else if (ratio > 0.05) {
    return 0.4; // Среднестенная
  } else {
    return 0.5; // Тонкостенная
  }
}

double ParallelOffsetCalculator::calculate_developed_length(
    double bend_1_length, double straight_section, double bend_2_length,
    double k_factor) const {
  // Развёрнутая длина - это фактическая длина трубы с учётом припусков
  // Припуск на гибку зависит от положения нейтрального слоя

  // Коррекция по K-фактору
  double correction_factor = 1.0 + k_factor * 0.1;

  return (bend_1_length + bend_2_length) * correction_factor + straight_section;
}

bool ParallelOffsetCalculator::validate_diameter(double od) {
  if (od < 3.0) {
    add_validation_error("DIAMETER_TOO_SMALL",
                         "Диаметр трубы не может быть меньше 3 мм",
                         "outer_diameter");
    return false;
  }

  if (od > 500.0) {
    add_validation_error("DIAMETER_TOO_LARGE",
                         "Диаметр трубы не может быть больше 500 мм",
                         "outer_diameter");
    return false;
  }

  return true;
}

bool ParallelOffsetCalculator::validate_wall_thickness(double od,
                                                        double wall) {
  if (wall <= 0.0) {
    add_validation_error("INVALID_WALL_THICKNESS",
                         "Толщина стенки должна быть положительной",
                         "wall_thickness");
    return false;
  }

  if (wall >= od / 2.0) {
    add_validation_error("WALL_TOO_THICK",
                         "Толщина стенки не может быть ≥ половине диаметра",
                         "wall_thickness");
    return false;
  }

  if (wall < od * 0.01) {
    add_validation_error("WALL_TOO_THIN",
                         "Стенка слишком тонкая (< 1% диаметра)",
                         "wall_thickness");
    return false;
  }

  return true;
}

bool ParallelOffsetCalculator::validate_bend_angle(double angle) {
  if (angle < 5.0) {
    add_validation_error("ANGLE_TOO_SMALL",
                         "Угол гибки не может быть меньше 5°",
                         "bend_angle");
    return false;
  }

  if (angle > 180.0) {
    add_validation_error("ANGLE_TOO_LARGE",
                         "Угол гибки не может быть больше 180°",
                         "bend_angle");
    return false;
  }

  return true;
}

bool ParallelOffsetCalculator::validate_offset_distance(double offset,
                                                         double od) {
  if (offset <= 0.0) {
    add_validation_error("INVALID_OFFSET",
                         "Смещение должно быть положительным",
                         "offset_distance");
    return false;
  }

  if (offset > od * 10.0) {
    add_validation_error("OFFSET_TOO_LARGE",
                         "Смещение слишком велико (максимум 10*OD)",
                         "offset_distance");
    return false;
  }

  return true;
}

bool ParallelOffsetCalculator::validate_bend_radius(
    double radius, double od, double wall, const std::string& material) {

  if (radius <= 0.0) {
    add_validation_error("INVALID_RADIUS",
                         "Радиус гибки должен быть положительным",
                         "bend_radius");
    return false;
  }

  if (!can_bend(od, wall, radius, material)) {
    std::stringstream ss;
    auto it = MIN_BEND_RADIUS_FACTOR.find(material);
    double min_factor = (it != MIN_BEND_RADIUS_FACTOR.end()) ? it->second : 2.5;
    double min_radius = od * min_factor;

    ss << "Радиус гибки слишком мал для " << material
       << " (минимум " << min_radius << " мм)";

    add_validation_error("RADIUS_TOO_SMALL", ss.str(), "bend_radius");
    return false;
  }

  return true;
}

void ParallelOffsetCalculator::add_validation_error(
    const std::string& code, const std::string& message,
    const std::string& parameter) {
  validation_errors_.push_back({code, message, parameter});
}

}  // namespace bendcore
