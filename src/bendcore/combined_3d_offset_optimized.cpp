#include "bendcore/combined_3d_offset_optimized.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace bendcore {

// ============================================================================
// Конструктор
// ============================================================================

Combined3DOffsetCalculatorOptimized::Combined3DOffsetCalculatorOptimized()
    : Combined3DOffsetCalculator() {
  springback_cache_.count = 0;
  opt_stats_.total_allocations = 0;
}

// ============================================================================
// Основной метод расчёта (оптимизированный)
// ============================================================================

Combined3DOffsetCalculator::Result
Combined3DOffsetCalculatorOptimized::calculate(const Parameters& params) {  // Note: intentionally named same as base
  Result result;
  result.success = false;

  // Валидация
  std::vector<std::string> errors;
  if (!validate(params, errors)) {
    result.error_message = "Ошибка валидации: " + errors[0];
    return result;
  }

  try {
    // Очистить пулы перед использованием
    allocation_pool_.clear();

    // Расчёт 3D расстояния и направления (используем оптимизированную версию)
    double total_3d_distance = distance_3d_fast(params.start_point,
                                               params.end_point);
    result.total_distance_3d = total_3d_distance;

    // Распределение гибок вдоль траектории (быстрая версия)
    distribute_bends_fast(
        params.start_point,
        params.end_point,
        params.num_bends,
        params.distribution,
        params.custom_bend_positions,
        allocation_pool_.bend_positions_pool);

    // Расчёт геометрии гибок (оптимизированная версия)
    calculate_bends_geometry_fast(
        params.start_point,
        params.end_point,
        allocation_pool_.bend_positions_pool,
        params.outer_diameter,
        params.wall_thickness,
        params.springback_correction,
        params.material,
        allocation_pool_.bend_pool);

    // Расчёт прямых участков (оптимизированная версия)
    calculate_sections_fast(
        params.start_point,
        params.end_point,
        allocation_pool_.bend_pool,
        allocation_pool_.section_pool);

    // Копировать результаты в output (минимум копирований)
    result.bends = allocation_pool_.bend_pool;
    result.sections = allocation_pool_.section_pool;

    // Расчёт общей длины (без лишних итераций)
    result.total_length = 0.0;
    for (const auto& section : result.sections) {
      result.total_length += section.length;
    }
    for (const auto& bend : result.bends) {
      double bend_length = bend.radius * 3.14159265359 / 2.0;
      result.total_length += bend_length;
    }

    // Генерация точек траектории
    result.trajectory_3d.clear();
    result.trajectory_3d.reserve(result.bends.size() + 2);
    result.trajectory_3d.push_back(params.start_point);
    for (const auto& bend : result.bends) {
      result.trajectory_3d.push_back(bend.position);
    }
    result.trajectory_3d.push_back(params.end_point);

    // Проверка feasibility
    result.feasibility_score = check_feasibility(params, result.bends);

    // Генерация G-code (lazy - только если нужен)
    result.cnc_code = generate_cnc_code_fast(result);

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
// Кешированная коррекция пружинения
// ============================================================================

double Combined3DOffsetCalculatorOptimized::get_cached_springback_correction(
    const std::string& material, double radius) {
  // Проверить кеш
  for (int i = 0; i < springback_cache_.count; ++i) {
    if (springback_cache_.entries[i].material == material &&
        std::abs(springback_cache_.entries[i].radius - radius) < 1.0) {
      opt_stats_.springback_cache_hits++;
      return springback_cache_.entries[i].correction;
    }
  }

  // Расчёт базовой коррекции
  double base_correction = 0.02;  // Steel по умолчанию
  if (material == "aluminum") {
    base_correction = 0.03;
  } else if (material == "copper") {
    base_correction = 0.025;
  }

  // Коррекция зависит от радиуса гибки
  double correction = base_correction * (1.0 + radius / 100.0);

  // Добавить в кеш
  if (springback_cache_.count < SpringbackCache::MAX_ENTRIES) {
    springback_cache_.entries[springback_cache_.count].material = material;
    springback_cache_.entries[springback_cache_.count].radius = radius;
    springback_cache_.entries[springback_cache_.count].correction = correction;
    springback_cache_.count++;
  }

  return correction;
}

// ============================================================================
// Быстрое распределение гибок
// ============================================================================

void Combined3DOffsetCalculatorOptimized::distribute_bends_fast(
    const Point3D& start, const Point3D& end, int num_bends,
    const std::string& distribution,
    const std::vector<double>& custom_positions,
    std::vector<Point3D>& result) {
  result.clear();
  result.reserve(num_bends);

  Vector3D direction = end - start;

  if (distribution == "equal") {
    for (int i = 1; i <= num_bends; ++i) {
      double t = static_cast<double>(i) / (num_bends + 1);
      result.push_back(start + direction * t);
    }
  } else if (distribution == "custom") {
    for (double t : custom_positions) {
      result.push_back(start + direction * t);
    }
  }
}

// ============================================================================
// Быстрый расчёт геометрии гибок
// ============================================================================

void Combined3DOffsetCalculatorOptimized::calculate_bends_geometry_fast(
    const Point3D& start, const Point3D& end,
    const std::vector<Point3D>& bend_positions, double outer_diameter,
    double wall_thickness, bool springback_correction,
    const std::string& material, std::vector<BendDetail>& result_bends) {

  result_bends.clear();
  result_bends.reserve(bend_positions.size());

  Vector3D overall_direction = end - start;

  for (size_t i = 0; i < bend_positions.size(); ++i) {
    BendDetail bend;
    bend.position = bend_positions[i];
    bend.bend_index = static_cast<int>(i) + 1;

    // Расчёт углов (оптимизированная версия)
    calculate_angles_fast(overall_direction, bend.angle_xy, bend.angle_z);

    // Расчёт длины до этой гибки (используем fast distance)
    Vector3D from_start = bend.position - start;
    bend.length = from_start.magnitude();

    // Радиус гибки (baseline без коррекции)
    bend.radius = outer_diameter / 2.0 + wall_thickness;

    // Коррекция пружинения (с кешированием)
    if (springback_correction) {
      bend.springback_correction =
          get_cached_springback_correction(material, bend.radius);
      bend.radius *= (1.0 + bend.springback_correction);
    } else {
      bend.springback_correction = 0.0;
    }

    result_bends.push_back(bend);
    opt_stats_.vector_reuses++;
  }
}

// ============================================================================
// Быстрый расчёт секций
// ============================================================================

void Combined3DOffsetCalculatorOptimized::calculate_sections_fast(
    const Point3D& start, const Point3D& end,
    const std::vector<BendDetail>& bends,
    std::vector<SectionDetail>& result_sections) {

  result_sections.clear();
  result_sections.reserve(bends.size() + 1);

  std::vector<Point3D> all_points;
  all_points.reserve(bends.size() + 2);
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
    double horizontal =
        std::sqrt(diff.x * diff.x + diff.y * diff.y);
    section.slope_percent = (horizontal > 0.0)
                               ? (vertical / horizontal) * 100.0
                               : 0.0;

    result_sections.push_back(section);
  }
}

// ============================================================================
// Быстрая генерация G-code
// ============================================================================

std::string Combined3DOffsetCalculatorOptimized::generate_cnc_code_fast(
    const Result& result) const {
  std::ostringstream ss;
  ss.reserve(2048);  // Pre-allocate строку

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

  // Гибки
  for (size_t i = 0; i < result.bends.size(); ++i) {
    const auto& bend = result.bends[i];
    ss << "; ========== Гибка " << (i + 1) << " ==========\n";
    ss << "; Позиция: (" << std::fixed << std::setprecision(1) << bend.position.x
       << ", " << bend.position.y << ", " << bend.position.z << ")\n";
    ss << "; Углы: XY=" << std::fixed << std::setprecision(1) << bend.angle_xy
       << "° Z=" << bend.angle_z << "°\n";
    ss << "; Радиус: " << std::fixed << std::setprecision(2) << bend.radius
       << " мм\n";

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

// ============================================================================
// Управление кешами
// ============================================================================

void Combined3DOffsetCalculatorOptimized::clear_caches() {
  springback_cache_.count = 0;
  opt_stats_.total_allocations = 0;
  opt_stats_.springback_cache_hits = 0;
  opt_stats_.vector_reuses = 0;
}

}  // namespace bendcore
