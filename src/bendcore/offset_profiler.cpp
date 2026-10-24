#include "bendcore/offset_profiler.hpp"
#include <iomanip>
#include <algorithm>

namespace bendcore {

void OffsetProfiler::start(const std::string& section) {
  sections_[section].start_time = std::chrono::high_resolution_clock::now();
}

void OffsetProfiler::stop(const std::string& section) {
  auto end_time = std::chrono::high_resolution_clock::now();

  if (sections_.find(section) == sections_.end()) {
    return;  // Раздел не был запущен
  }

  auto start_time = sections_[section].start_time;
  auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
      end_time - start_time);

  double elapsed_ms = duration.count() / 1e6;

  auto& stats = sections_[section].stats;
  stats.call_count++;
  stats.total_time_ms += elapsed_ms;
  stats.min_time_ms = std::min(stats.min_time_ms, elapsed_ms);
  stats.max_time_ms = std::max(stats.max_time_ms, elapsed_ms);
  stats.update();

  // Добавить в историю
  history_[section].push_back(elapsed_ms);
}

OffsetProfiler::TimingStats OffsetProfiler::get_stats(
    const std::string& section) const {
  auto it = sections_.find(section);
  if (it != sections_.end()) {
    return it->second.stats;
  }
  return TimingStats();
}

void OffsetProfiler::report(std::ostream& os) const {
  os << "\n";
  os << "╔════════════════════════════════════════════════════════════════╗\n";
  os << "║           OFFSET MODULES PROFILING REPORT                      ║\n";
  os << "╚════════════════════════════════════════════════════════════════╝\n\n";

  os << std::left << std::setw(30) << "Function/Section"
     << std::right << std::setw(12) << "Calls"
     << std::setw(14) << "Total (ms)"
     << std::setw(14) << "Avg (ms)"
     << std::setw(14) << "Min (ms)"
     << std::setw(14) << "Max (ms)\n";

  os << std::string(98, '─') << "\n";

  double total_time = 0.0;

  for (const auto& [section, timing] : sections_) {
    const auto& stats = timing.stats;
    if (stats.call_count > 0) {
      total_time += stats.total_time_ms;

      os << std::left << std::setw(30) << section
         << std::right << std::setw(12) << stats.call_count
         << std::fixed << std::setprecision(3)
         << std::setw(14) << stats.total_time_ms
         << std::setw(14) << stats.avg_time_ms
         << std::setw(14) << stats.min_time_ms
         << std::setw(14) << stats.max_time_ms << "\n";
    }
  }

  os << std::string(98, '─') << "\n";
  os << std::left << std::setw(30) << "TOTAL"
     << std::right << std::fixed << std::setprecision(3)
     << std::setw(52) << std::right << std::setw(14) << total_time << " ms\n\n";
}

void OffsetProfiler::report_compact(std::ostream& os) const {
  os << "\n══ PERFORMANCE SUMMARY ══\n";

  for (const auto& [section, timing] : sections_) {
    const auto& stats = timing.stats;
    if (stats.call_count > 0) {
      os << "  " << section << ": "
         << std::fixed << std::setprecision(2)
         << stats.avg_time_ms << "ms avg ("
         << stats.call_count << " calls)\n";
    }
  }

  os << "\n";
}

void OffsetProfiler::reset() {
  sections_.clear();
  history_.clear();
}

double OffsetProfiler::get_total_time_ms() const {
  double total = 0.0;
  for (const auto& [section, timing] : sections_) {
    total += timing.stats.total_time_ms;
  }
  return total;
}

}  // namespace bendcore
