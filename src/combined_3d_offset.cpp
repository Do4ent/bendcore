#include "bendcore/combined_3d_offset.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace bendcore {

// ============================================================================
// Конструктор
// ============================================================================

Combined3DOffsetCalculator::Combined3DOffsetCalculator() = default;

// ============================================================================
// Валидация входных параметров
// ============================================================================

bool Combined3DOffsetCalculator::validate(const Parameters& params,
                                          std::vector<std::string>& errors) {
  errors.clear();

  // Проверка 3D расстояния
  double dist_3d = distance_3d(params.start_point, params.end_point);
  if (dist_3d < MIN_DISTANCE_3D) {
    errors.push_back("3D расстояние слишком малое (< " +
                    std::to_string(MIN_DISTANCE_3D) + " мм)");
  }
  if (dist_3d > MAX_DISTANCE_3D) {
    errors.push_back("3D расстояние слишком большое (> " +
                    std::to_string(MAX_DISTANCE_3D) + " мм)");
  }

  // Проверка диаметра
  if (params.outer_diameter <= params.wall_thickness * 2) {
    errors.push_back("Внешний диаметр должен быть > 2 × толщина стенки");
  }
  if (params.outer_diameter < 1.0 || params.outer_diameter > 500.0) {
    errors.push_back("Внешний диаметр должен быть 1-500 мм");
  }

  // Проверка толщины стенки
  if (params.wall_thickness < 0.1 || params.wall_thickness > 50.0) {
    errors.push_back("Толщина стенки должна быть 0.1-50 мм");
  }

  // Проверка радиуса гибки
  double min_radius = params.outer_diameter * MIN_BEND_RADIUS_FACTOR;
  if (params.bend_radius < min_radius) {
    errors.push_back("Радиус гибки слишком малый (< " +
                    std::to_string(min_radius) + " мм)");
  }
  if (params.bend_radius > 1000.0) {
    errors.push_back("Радиус гибки слишком большой (> 1000 мм)");
  }

  // Проверка количества гибок
  if (params.num_bends < 2 || params.num_bends > 5) {
    errors.push_back("Количество гибок должно быть 2-5");
  }

  // Проверка типа распределения
  if (params.distribution != "equal" && params.distribution != "custom") {
    errors.push_back("Тип распределения должен быть 'equal' или 'custom'");
  }

  // Проверка custom позиций
  if (params.distribution == "custom") {
    if (params.custom_bend_positions.size() != params.num_bends) {
      errors.push_back("Количество custom позиций должно соответствовать num_bends");
    }
    for (double pos : params.custom_bend_positions) {
      if (pos < 0.0 || pos > 1.0) {
        errors.push_back("Custom позиции должны быть в диапазоне 0.0-1.0");
      }
    }
  }

  // Проверка угла наклона
  if (params.inclination_angle < -MAX_INCLINATION_ANGLE ||
      params.inclination_angle > MAX_INCLINATION_ANGLE) {
    errors.push_back("Угол наклона должен быть ±" +
                    std::to_string(MAX_INCLINATION_ANGLE) + " градусов");
  }

  // Проверка материала
  if (params.material != "steel" && params.material != "aluminum" &&
      params.material != "copper") {
    errors.push_back("Материал должен быть 'steel', 'aluminum' или 'copper'");
  }

  return errors.empty();
}

// ============================================================================
// Основной метод расчёта
// ============================================================================

Combined3DOffsetCalculator::Result Combined3DOffsetCalculator::calculate(
    const Parameters& params) {
  Result result;
  result.success = false;

  // Валидация
  std::vector<std::string> errors;
  if (!validate(params, errors)) {
    result.error_message = "Ошибка валидации: " + errors[0];
    return result;
  }

  try {
    // Расчёт 3D расстояния и направления
    double total_3d_distance = distance_3d(params.start_point, params.end_point);
    result.total_distance_3d = total_3d_distance;

    // Распределение гибок вдоль траектории
    std::vector<Point3D> bend_positions = distribute_bends(
      params.start_point,
      params.end_point,
      params.num_bends,
      params.distribution,
      params.custom_bend_positions
    );

    // Расчёт геометрии гибок
    calculate_bends_geometry(
      params.start_point,
      params.end_point,
      bend_positions,
      params.outer_diameter,
      params.wall_thickness,
      params.springback_correction,
      params.material,
      result.bends
    );

    // Расчёт прямых участков
    calculate_sections(
      params.start_point,
      params.end_point,
      result.bends,
      result.sections
    );

    // Расчёт общей длины
    result.total_length = 0.0;
    for (const auto& section : result.sections) {
      result.total_length += section.length;
    }
    for (const auto& bend : result.bends) {
      // Длина гибки = радиус × угол (в радианах)
      double bend_length = bend.radius * 3.14159265359 / 2.0; // Предполагаем 90° гибку
      result.total_length += bend_length;
    }

    // Генерация точек траектории для визуализации
    result.trajectory_3d = generate_trajectory_points(
      result.bends,
      params.start_point,
      params.end_point
    );

    // Проверка feasibility (возможность производства)
    result.feasibility_score = check_feasibility(params, result.bends);

    // Генерация G-code
    result.cnc_code = generate_cnc_code(result);

    // Добавление предупреждений
    if (result.feasibility_score < 60.0) {
      result.warnings.push_back("Низкая оценка feasibility: " +
                               std::to_string(static_cast<int>(result.feasibility_score)) + "%");
    }
    if (params.inclination_angle > 45.0) {
      result.warnings.push_back("Высокий угол наклона может потребовать специальной оснастки");
    }

    result.success = true;
    result.error_message = "";

  } catch (const std::exception& e) {
    result.error_message = std::string("Исключение: ") + e.what();
  }

  return result;
}

// ============================================================================
// Вспомогательные методы геометрии
// ============================================================================

double Combined3DOffsetCalculator::distance_3d(const Point3D& p1,
                                               const Point3D& p2) const {
  Vector3D diff = p2 - p1;
  return diff.magnitude();
}

void Combined3DOffsetCalculator::calculate_angles(const Vector3D& direction,
                                                  double& angle_xy,
                                                  double& angle_z) const {
  // Угол в XY плоскости (azimuth)
  angle_xy = std::atan2(direction.y, direction.x) * 180.0 / 3.14159265359;
  if (angle_xy < 0) angle_xy += 360.0;

  // Угол наклона (elevation)
  double horizontal = std::sqrt(direction.x * direction.x + direction.y * direction.y);
  angle_z = std::atan2(direction.z, horizontal) * 180.0 / 3.14159265359;
}

std::vector<Point3D> Combined3DOffsetCalculator::distribute_bends(
    const Point3D& start,
    const Point3D& end,
    int num_bends,
    const std::string& distribution,
    const std::vector<double>& custom_positions) {

  std::vector<Point3D> result;
  Vector3D direction = end - start;

  if (distribution == "equal") {
    // Равномерное распределение гибок
    for (int i = 1; i <= num_bends; ++i) {
      double t = static_cast<double>(i) / (num_bends + 1);
      Point3D bend_pos = start + direction * t;
      result.push_back(bend_pos);
    }
  } else if (distribution == "custom") {
    // Custom распределение
    for (double t : custom_positions) {
      Point3D bend_pos = start + direction * t;
      result.push_back(bend_pos);
    }
  }

  return result;
}

double Combined3DOffsetCalculator::get_springback_correction(
    const std::string& material,
    double radius) const {
  double base_correction = SPRINGBACK_STEEL;

  if (material == "aluminum") {
    base_correction = SPRINGBACK_ALUMINUM;
  } else if (material == "copper") {
    base_correction = SPRINGBACK_COPPER;
  }

  // Коррекция зависит от радиуса гибки
  return base_correction * (1.0 + radius / 100.0);
}

void Combined3DOffsetCalculator::calculate_bends_geometry(
    const Point3D& start,
    const Point3D& end,
    const std::vector<Point3D>& bend_positions,
    double outer_diameter,
    double wall_thickness,
    bool springback_correction,
    const std::string& material,
    std::vector<BendDetail>& result_bends) {

  Vector3D overall_direction = end - start;
  double total_distance = overall_direction.magnitude();

  for (size_t i = 0; i < bend_positions.size(); ++i) {
    BendDetail bend;
    bend.position = bend_positions[i];
    bend.bend_index = static_cast<int>(i) + 1;

    // Расчёт углов
    double angle_xy, angle_z;
    calculate_angles(overall_direction, angle_xy, angle_z);
    bend.angle_xy = angle_xy;
    bend.angle_z = angle_z;

    // Расчёт длины до этой гибки
    Vector3D from_start = bend.position - start;
    bend.length = from_start.magnitude();

    // Радиус гибки
    bend.radius = outer_diameter / 2.0 + wall_thickness;

    // Коррекция пружинения
    if (springback_correction) {
      bend.springback_correction = get_springback_correction(material, bend.radius);
      bend.radius *= (1.0 + bend.springback_correction);
    } else {
      bend.springback_correction = 0.0;
    }

    result_bends.push_back(bend);
  }
}

void Combined3DOffsetCalculator::calculate_sections(
    const Point3D& start,
    const Point3D& end,
    const std::vector<BendDetail>& bends,
    std::vector<SectionDetail>& result_sections) {

  std::vector<Point3D> all_points;
  all_points.push_back(start);

  for (const auto& bend : bends) {
    all_points.push_back(bend.position);
  }

  all_points.push_back(end);

  // Расчёт секций между последовательными точками
  for (size_t i = 0; i < all_points.size() - 1; ++i) {
    SectionDetail section;
    section.start = all_points[i];
    section.end = all_points[i + 1];
    section.section_index = static_cast<int>(i);

    Vector3D diff = section.end - section.start;
    section.length = diff.magnitude();

    // Расчёт наклона
    double vertical = std::abs(diff.z);
    double horizontal = std::sqrt(diff.x * diff.x + diff.y * diff.y);
    section.slope_percent = (horizontal > 0.0) ? (vertical / horizontal) * 100.0 : 0.0;

    result_sections.push_back(section);
  }
}

double Combined3DOffsetCalculator::check_feasibility(
    const Parameters& params,
    const std::vector<BendDetail>& bends) const {

  double score = 100.0;

  // Проверка минимального радиуса
  double min_radius = params.outer_diameter * MIN_BEND_RADIUS_FACTOR;
  for (const auto& bend : bends) {
    if (bend.radius < min_radius) {
      score -= 20.0;
    }
  }

  // Проверка угла наклона
  if (std::abs(params.inclination_angle) > 60.0) {
    score -= 15.0;
  }

  // Проверка материала и толщины стенки
  if (params.wall_thickness > params.outer_diameter / 3.0) {
    score -= 10.0;
  }

  // Проверка количества гибок
  if (params.num_bends > 4) {
    score -= 5.0;
  }

  return std::max(0.0, score);
}

std::vector<Point3D> Combined3DOffsetCalculator::generate_trajectory_points(
    const std::vector<BendDetail>& bends,
    const Point3D& start_point,
    const Point3D& end_point) const {

  std::vector<Point3D> trajectory;
  trajectory.push_back(start_point);

  for (const auto& bend : bends) {
    trajectory.push_back(bend.position);
  }

  trajectory.push_back(end_point);

  return trajectory;
}

// ============================================================================
// Генерация G-code для ЧПУ
// ============================================================================

std::string Combined3DOffsetCalculator::generate_cnc_code(const Result& result) {
  std::ostringstream ss;

  // G-code заголовок
  ss << "G20  ; Дюймы\n";
  ss << "G90  ; Абсолютные координаты\n";
  ss << "G00 Z5.0  ; Поднять инструмент\n\n";

  ss << "; Программа гибки трубопровода\n";
  ss << "; Общая длина: " << std::fixed << std::setprecision(2)
     << result.total_length << " мм\n";
  ss << "; Feasibility: " << std::fixed << std::setprecision(1)
     << result.feasibility_score << "%\n\n";

  // Перемещение в первую точку
  if (!result.trajectory_3d.empty()) {
    const auto& first = result.trajectory_3d[0];
    ss << "; Перемещение в стартовую позицию\n";
    ss << "G00 X" << std::fixed << std::setprecision(2) << first.x << " ";
    ss << "Y" << first.y << " Z" << first.z << "\n";
    ss << "M03 S500  ; Включить шпиндель\n\n";
  }

  // Gибки
  for (size_t i = 0; i < result.bends.size(); ++i) {
    const auto& bend = result.bends[i];
    ss << "; ========== Гибка " << (i + 1) << " ==========\n";
    ss << "; Позиция: (" << std::fixed << std::setprecision(1)
       << bend.position.x << ", " << bend.position.y << ", "
       << bend.position.z << ")\n";
    ss << "; Углы: XY=" << std::fixed << std::setprecision(1) << bend.angle_xy
       << "° Z=" << bend.angle_z << "°\n";
    ss << "; Радиус: " << std::fixed << std::setprecision(2)
       << bend.radius << " мм\n";

    // G-code команда для гибки (F код может быть 8xx для изгиба)
    ss << "G01 X" << std::fixed << std::setprecision(2) << bend.position.x << " ";
    ss << "Y" << bend.position.y << " Z" << bend.position.z;
    ss << " F100  ; Гибка\n\n";
  }

  // Финальная позиция
  ss << "; Перемещение в конечную позицию\n";
  if (!result.trajectory_3d.empty()) {
    const auto& last = result.trajectory_3d.back();
    ss << "G00 X" << std::fixed << std::setprecision(2) << last.x << " ";
    ss << "Y" << last.y << " Z" << last.z << "\n";
  }

  ss << "M05  ; Выключить шпиндель\n";
  ss << "G00 Z10.0  ; Поднять инструмент\n";
  ss << "M30  ; Конец программы\n";

  return ss.str();
}

}  // namespace bendcore
