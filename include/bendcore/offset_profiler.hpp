#pragma once

#include <string>
#include <map>
#include <chrono>
#include <vector>
#include <iostream>
#include <iomanip>

namespace bendcore {

/**
 * Класс для профилирования производительности Offset modules
 * Измеряет время выполнения функций и предоставляет статистику
 */
class OffsetProfiler {
public:
  struct TimingStats {
    int call_count = 0;
    double total_time_ms = 0.0;      // Полное время в миллисекундах
    double min_time_ms = 1e9;        // Минимальное время
    double max_time_ms = 0.0;        // Максимальное время
    double avg_time_ms = 0.0;        // Среднее время

    void update() {
      if (call_count > 0) {
        avg_time_ms = total_time_ms / call_count;
      }
    }
  };

  OffsetProfiler() = default;
  ~OffsetProfiler() = default;

  /**
   * Начать профилирование секции
   */
  void start(const std::string& section);

  /**
   * Закончить профилирование секции
   */
  void stop(const std::string& section);

  /**
   * Получить статистику для конкретной секции
   */
  TimingStats get_stats(const std::string& section) const;

  /**
   * Вывести полный отчёт о производительности
   */
  void report(std::ostream& os = std::cout) const;

  /**
   * Вывести краткий отчёт (только основные метрики)
   */
  void report_compact(std::ostream& os = std::cout) const;

  /**
   * Сбросить все статистики
   */
  void reset();

  /**
   * Получить общее время профилирования
   */
  double get_total_time_ms() const;

private:
  struct SectionTiming {
    std::chrono::high_resolution_clock::time_point start_time;
    TimingStats stats;
  };

  std::map<std::string, SectionTiming> sections_;
  std::map<std::string, std::vector<double>> history_;  // История всех измерений
};

/**
 * RAII помощник для автоматического профилирования блока кода
 * Использование: { ProfileScope scope(profiler, "my_function"); ... }
 */
class ProfileScope {
public:
  ProfileScope(OffsetProfiler& profiler, const std::string& section)
      : profiler_(profiler), section_(section) {
    profiler_.start(section_);
  }

  ~ProfileScope() {
    profiler_.stop(section_);
  }

private:
  OffsetProfiler& profiler_;
  std::string section_;
};

}  // namespace bendcore
