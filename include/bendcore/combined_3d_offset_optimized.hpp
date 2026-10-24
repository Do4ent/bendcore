#pragma once

#include "combined_3d_offset.hpp"
#include <array>
#include <memory>

namespace bendcore {

/**
 * Оптимизированная версия Combined3DOffsetCalculator
 *
 * Оптимизации:
 * 1. Pre-allocation векторов для избегания reallocations
 * 2. SIMD операции для Vector3D (используя встроенные операции)
 * 3. Кешированные springback коррекции
 * 4. Минимизация временных объектов
 * 5. Inline критические функции
 * 6. Lazy evaluation для G-code генерации
 */
class Combined3DOffsetCalculatorOptimized : public Combined3DOffsetCalculator {
public:
  Combined3DOffsetCalculatorOptimized();
  ~Combined3DOffsetCalculatorOptimized() = default;

  /**
   * Оптимизированный расчёт
   */
  Result calculate(const Parameters& params);

  /**
   * Очистить кеши
   */
  void clear_caches();

  /**
   * Статистика оптимизации
   */
  struct OptimizationStats {
    int vector_reuses = 0;        // Сколько раз переиспользован вектор
    int springback_cache_hits = 0; // Попадания в кеш пружинения
    int total_allocations = 0;     // Всего выделений памяти
  };

  OptimizationStats get_optimization_stats() const { return opt_stats_; }

private:
  // ========== ALLOCATION POOLS ==========

  // Pre-allocated буферы для избегания reallocations
  struct AllocationPool {
    std::vector<BendDetail> bend_pool;      // Переиспользуемый пул для гибок
    std::vector<SectionDetail> section_pool;  // Пул для секций
    std::vector<Point3D> trajectory_pool;    // Пул для траектории
    std::vector<Point3D> bend_positions_pool; // Пул для позиций гибок

    AllocationPool() {
      bend_pool.reserve(5);           // Max 5 гибок
      section_pool.reserve(6);        // Max 6 секций (5 гибок + 1)
      trajectory_pool.reserve(10);    // Запас для интерполяции
      bend_positions_pool.reserve(5);
    }

    void clear() {
      bend_pool.clear();
      section_pool.clear();
      trajectory_pool.clear();
      bend_positions_pool.clear();
    }
  } allocation_pool_;

  // ========== CACHE STRUCTURES ==========

  // Кеш springback коррекций по материалам и радиусам
  struct SpringbackCache {
    static constexpr int MAX_ENTRIES = 32;
    struct CacheEntry {
      std::string material;
      double radius;
      double correction;
    };
    std::array<CacheEntry, MAX_ENTRIES> entries;
    int count = 0;
  } springback_cache_;

  OptimizationStats opt_stats_;

  // ========== INLINE OPTIMIZATIONS ==========

  /**
   * Оптимизированное вычисление 3D расстояния (inline)
   */
  inline double distance_3d_fast(const Point3D& p1, const Point3D& p2) const {
    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    double dz = p2.z - p1.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
  }

  /**
   * Быстрый расчёт углов (минимизирует atan2 вызовы)
   */
  inline void calculate_angles_fast(const Vector3D& direction,
                                   double& angle_xy,
                                   double& angle_z) const {
    // Используем atan2 один раз (встроенная функция оптимизирована)
    angle_xy = std::atan2(direction.y, direction.x) * 180.0 / 3.14159265359;
    if (angle_xy < 0) angle_xy += 360.0;

    double horizontal = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    angle_z = std::atan2(direction.z, horizontal) * 180.0 / 3.14159265359;
  }

  /**
   * Кешированная коррекция пружинения
   */
  double get_cached_springback_correction(const std::string& material,
                                         double radius);

  /**
   * Быстрое распределение гибок (без промежуточных vectors)
   */
  void distribute_bends_fast(const Point3D& start,
                            const Point3D& end,
                            int num_bends,
                            const std::string& distribution,
                            const std::vector<double>& custom_positions,
                            std::vector<Point3D>& result);

  /**
   * Быстрый расчёт геометрии гибок
   */
  void calculate_bends_geometry_fast(
      const Point3D& start,
      const Point3D& end,
      const std::vector<Point3D>& bend_positions,
      double outer_diameter,
      double wall_thickness,
      bool springback_correction,
      const std::string& material,
      std::vector<BendDetail>& result_bends);

  /**
   * Быстрый расчёт секций
   */
  void calculate_sections_fast(
      const Point3D& start,
      const Point3D& end,
      const std::vector<BendDetail>& bends,
      std::vector<SectionDetail>& result_sections);

  /**
   * Быстрая генерация G-code (lazy evaluation - только если нужен)
   */
  std::string generate_cnc_code_fast(const Result& result) const;
};

}  // namespace bendcore
