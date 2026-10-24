#include "bendcore/parallel_offset_optimized.hpp"
#include <cmath>
#include <algorithm>

namespace bendcore {

constexpr double PI = 3.14159265358979323846;
constexpr double DEG_TO_RAD = PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / PI;

// ============================================================================
// TrigCache инициализация
// ============================================================================

ParallelOffsetCalculatorOptimized::TrigCache::TrigCache() {
  precompute();
}

void ParallelOffsetCalculatorOptimized::TrigCache::precompute() {
  // Pre-compute sin/cos для всех целых градусов
  for (int i = 0; i < MAX_ANGLES; ++i) {
    double angle_rad = i * DEG_TO_RAD;
    sin_cache[i] = std::sin(angle_rad);
    cos_cache[i] = std::cos(angle_rad);
  }
}

// ============================================================================
// Конструктор
// ============================================================================

ParallelOffsetCalculatorOptimized::ParallelOffsetCalculatorOptimized()
    : ParallelOffsetCalculator() {
  // Материальный кеш инициализируется пусто
  material_cache_.count = 0;
  k_factor_cache_.entries = 0;
}

// ============================================================================
// Основной метод расчёта (оптимизированный)
// ============================================================================

ParallelOffsetCalculator::Result
ParallelOffsetCalculatorOptimized::calculate(const Parameters& params) {  // Note: intentionally named same as base
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

  // Оптимизированный расчёт K-фактора
  double k_factor = get_cached_k_factor(params.outer_diameter,
                                        params.wall_thickness);

  // Расчёт длины первой гибки (inline)
  result.bend_1_length = calculate_bend_arc_length_fast(params.bend_radius,
                                                       bend_angle_rad);

  // Геометрический расчёт для второй гибки
  // Используем pre-computed sqrt для скорости
  double radius_sq = params.bend_radius * params.bend_radius;
  double offset_sq = params.offset_distance * params.offset_distance;
  double bend_2_radius = fast_sqrt(radius_sq + offset_sq);

  // Расчёт угла второй гибки (минимизация atan2 вызовов)
  double bend_2_angle_raw = params.offset_distance / params.bend_radius;
  double bend_2_angle =
      (bend_2_angle_raw > bend_angle_rad) ? bend_angle_rad : bend_2_angle_raw;

  result.bend_2_length = calculate_bend_arc_length_fast(bend_2_radius,
                                                       bend_2_angle);

  // Расчёт прямого участка между гибками (оптимизирован)
  double offset_factor = params.offset_distance / (params.bend_radius +
                                                    params.outer_diameter / 2.0);

  // Используем кешированные sin/cos
  double sin_angle = cached_sin(bend_angle_rad);
  double cos_angle = cached_cos(bend_angle_rad);

  result.straight_section = std::max(0.0,
      params.offset_distance * (1.0 - sin_angle) / cos_angle);

  // Убедимся, что прямой участок не отрицательный
  if (result.straight_section < 0) {
    result.straight_section = 0.0;
  }

  // Расчёт углов гибок
  result.bend_angles[0] = params.bend_angle;
  result.bend_angles[1] = params.bend_angle * (bend_2_radius / params.bend_radius);

  // Расчёт 3D координат (используем исходный метод)
  result.bend_1_pos = calculate_bend_position(0.0, bend_angle_rad, 0.0, 1);

  double second_bend_distance = result.bend_1_length + result.straight_section;
  result.bend_2_pos = calculate_bend_position(
      second_bend_distance,
      bend_angle_rad,
      params.offset_distance,
      2);

  // Расчёт общей длины с припусками (inline оптимизированная версия)
  result.total_length = calculate_developed_length_fast(
      result.bend_1_length,
      result.straight_section,
      result.bend_2_length,
      k_factor);

  // Расчёт уверенности расчёта
  double confidence = 95.0;

  if (params.offset_distance < params.outer_diameter * 0.1) {
    confidence -= 5.0;
  }

  if (params.bend_angle < 10.0 || params.bend_angle > 170.0) {
    confidence -= 10.0;
  }

  if (params.offset_distance > params.bend_radius) {
    confidence -= 8.0;
  }

  result.confidence = std::max(50.0, confidence);

  // Проверка на предупреждения (минимизирован number of string operations)
  if (result.bend_2_angle > params.bend_angle * 1.1) {
    if (result.warnings.empty()) {
      result.warnings = "Вторая гибка может быть более сложной. ";
    }
  }

  if (result.straight_section < params.outer_diameter) {
    result.warnings += "Прямой участок очень короткий. ";
  }

  if (params.bend_radius < params.outer_diameter * 2.0) {
    result.warnings += "Радиус гибки близок к минимально допустимому. ";
  }

  result.is_valid = true;

  return result;
}

// ============================================================================
// Кешированный K-фактор
// ============================================================================

double ParallelOffsetCalculatorOptimized::get_cached_k_factor(
    double od, double wall_thickness) {
  // Используем соотношение стенки как ключ
  double ratio = wall_thickness / od;

  // Проверить в кеше
  for (int i = 0; i < k_factor_cache_.entries; ++i) {
    if (std::abs(k_factor_cache_.ratio_keys[i] - ratio) < 0.001) {
      return k_factor_cache_.cache[i];
    }
  }

  // Расчёт K-фактора
  double k_factor;
  if (ratio > 0.1) {
    k_factor = 0.3;  // Толстостенная труба
  } else if (ratio > 0.05) {
    k_factor = 0.4;  // Среднестенная
  } else {
    k_factor = 0.5;  // Тонкостенная
  }

  // Добавить в кеш
  if (k_factor_cache_.entries < KFactorCache::CACHE_SIZE) {
    k_factor_cache_.ratio_keys[k_factor_cache_.entries] = ratio;
    k_factor_cache_.cache[k_factor_cache_.entries] = k_factor;
    k_factor_cache_.entries++;
  }

  return k_factor;
}

// ============================================================================
// Управление кешами
// ============================================================================

void ParallelOffsetCalculatorOptimized::clear_caches() {
  material_cache_.count = 0;
  k_factor_cache_.entries = 0;
}

}  // namespace bendcore
