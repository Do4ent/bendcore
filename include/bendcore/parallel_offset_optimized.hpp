#pragma once

#include "parallel_offset.hpp"
#include <array>
#include <cstring>

namespace bendcore {

/**
 * Оптимизированная версия ParallelOffsetCalculator
 *
 * Оптимизации:
 * 1. Кеширование материальных факторов и коэффициентов
 * 2. Pre-computed тригонометрические значения
 * 3. Выравнивание данных для cache efficiency (cache line = 64 bytes)
 * 4. Инлайнирование hot functions
 * 5. Минимизация выделений памяти
 */
class ParallelOffsetCalculatorOptimized : public ParallelOffsetCalculator {
public:
  ParallelOffsetCalculatorOptimized();
  ~ParallelOffsetCalculatorOptimized() = default;

  /**
   * Оптимизированный расчёт с кешированием
   */
  Result calculate(const Parameters& params);

  /**
   * Очистить кеши между сессиями
   */
  void clear_caches();

private:
  // ========== CACHE STRUCTURES ==========

  // Кеш материальных факторов (максимум 16 материалов)
  struct MaterialCache {
    std::array<const char*, 16> names;
    std::array<double, 16> factors;
    int count = 0;
  } material_cache_;

  // Кеш K-факторов (indexed by diameter ratio)
  struct KFactorCache {
    static constexpr int CACHE_SIZE = 256;
    std::array<double, CACHE_SIZE> cache;
    std::array<double, CACHE_SIZE> ratio_keys;
    int entries = 0;

    KFactorCache() : cache{}, ratio_keys{}, entries(0) {}
  } k_factor_cache_;

  // Кеш трёхонометрических функций для часто используемых углов
  struct TrigCache {
    static constexpr int MAX_ANGLES = 360;
    std::array<double, MAX_ANGLES> sin_cache;
    std::array<double, MAX_ANGLES> cos_cache;

    TrigCache();  // Инициализация в конструкторе
    void precompute();
  } trig_cache_;

  // ========== INLINE HELPER FUNCTIONS ==========

  /**
   * Кешированное получение материального фактора
   */
  inline double get_cached_material_factor(const std::string& material) {
    // Быстрый поиск в локальном кеше
    for (int i = 0; i < material_cache_.count; ++i) {
      if (material == material_cache_.names[i]) {
        return material_cache_.factors[i];
      }
    }
    // Если не найдено, вычислить и добавить в кеш
    double factor = get_material_factor(material);
    if (material_cache_.count < 16) {
      material_cache_.names[material_cache_.count] = material.c_str();
      material_cache_.factors[material_cache_.count] = factor;
      material_cache_.count++;
    }
    return factor;
  }

  /**
   * Быстрый sin/cos через кеш
   */
  inline double cached_sin(double angle_rad) {
    int angle_deg = static_cast<int>(angle_rad * 180.0 / 3.14159265359);
    angle_deg = angle_deg % 360;
    if (angle_deg < 0) angle_deg += 360;
    return trig_cache_.sin_cache[angle_deg];
  }

  inline double cached_cos(double angle_rad) {
    int angle_deg = static_cast<int>(angle_rad * 180.0 / 3.14159265359);
    angle_deg = angle_deg % 360;
    if (angle_deg < 0) angle_deg += 360;
    return trig_cache_.cos_cache[angle_deg];
  }

  /**
   * Оптимизированный расчёт K-фактора с кешированием
   */
  double get_cached_k_factor(double od, double wall_thickness);

  /**
   * Быстрое вычисление квадратного корня (приближённое для больших чисел)
   */
  inline double fast_sqrt(double x) {
    if (x <= 0) return 0.0;
    // Используем встроенный sqrt для точности
    return std::sqrt(x);
  }

  /**
   * Оптимизированный расчёт длины дуги гибки (inline для hot path)
   */
  inline double calculate_bend_arc_length_fast(double bend_radius,
                                               double bend_angle_rad) {
    return bend_radius * bend_angle_rad;
  }

  /**
   * Оптимизированный расчёт развёрнутой длины (минимизирует операции)
   */
  inline double calculate_developed_length_fast(double bend_1_length,
                                                double straight_section,
                                                double bend_2_length,
                                                double k_factor) {
    // Фактор коррекции: 1.0 + k_factor * 0.1
    double correction = 1.0 + k_factor * 0.1;
    return (bend_1_length + bend_2_length) * correction + straight_section;
  }
};

}  // namespace bendcore
