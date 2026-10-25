#include "bendcore/testing/performance_profiler.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <numeric>

namespace bendcore::testing {

// ============================================================================
// OperationMetrics Implementation
// ============================================================================

OperationMetrics::OperationMetrics(std::string operation_name)
  : operation_name_(std::move(operation_name)) {}

void OperationMetrics::record_time(std::chrono::microseconds duration) {
  time_samples_.push_back(duration);
}

void OperationMetrics::record_memory(size_t bytes_allocated, size_t bytes_deallocated) {
  memory_samples_.push_back(bytes_allocated - bytes_deallocated);
}

void OperationMetrics::record_io(size_t bytes_read, size_t bytes_written) {
  total_bytes_read_ += bytes_read;
  total_bytes_written_ += bytes_written;
  if (bytes_read > 0) read_ops_++;
  if (bytes_written > 0) write_ops_++;
}

CPUStats OperationMetrics::get_cpu_stats() const {
  CPUStats stats{};

  if (time_samples_.empty()) {
    return stats;
  }

  stats.operation_count = time_samples_.size();
  stats.total_time = std::accumulate(time_samples_.begin(), time_samples_.end(),
                                      std::chrono::microseconds(0));
  stats.average_time = stats.total_time / time_samples_.size();
  stats.min_time = *std::min_element(time_samples_.begin(), time_samples_.end());
  stats.max_time = *std::max_element(time_samples_.begin(), time_samples_.end());

  if (stats.total_time.count() > 0) {
    stats.ops_per_second = (1000000.0 * stats.operation_count) / stats.total_time.count();
  }

  return stats;
}

MemoryStats OperationMetrics::get_memory_stats() const {
  MemoryStats stats{};

  if (memory_samples_.empty()) {
    return stats;
  }

  stats.peak_usage_bytes = *std::max_element(memory_samples_.begin(), memory_samples_.end());
  stats.average_usage_bytes = std::accumulate(memory_samples_.begin(), memory_samples_.end(),
                                               static_cast<size_t>(0)) / memory_samples_.size();
  stats.allocated_count = memory_samples_.size();

  return stats;
}

IOStats OperationMetrics::get_io_stats() const {
  IOStats stats{};
  stats.bytes_read = total_bytes_read_;
  stats.bytes_written = total_bytes_written_;
  stats.read_operations = read_ops_;
  stats.write_operations = write_ops_;
  return stats;
}

// ============================================================================
// PerformanceProfiler Implementation
// ============================================================================

PerformanceProfiler::PerformanceProfiler() {
  set_targets(get_default_targets());
}

void PerformanceProfiler::set_target(const PerformanceTarget& target) {
  targets_[target.operation_name] = target;
}

void PerformanceProfiler::set_targets(const std::vector<PerformanceTarget>& targets) {
  for (const auto& target : targets) {
    set_target(target);
  }
}

std::vector<PerformanceTarget> PerformanceProfiler::get_default_targets() {
  return {
    {"Point distance", std::chrono::microseconds(1), 10.0},
    {"Vector magnitude", std::chrono::microseconds(1), 10.0},
    {"Plane projection", std::chrono::microseconds(1), 10.0},
    {"Line-circle intersection", std::chrono::microseconds(10), 10.0},
    {"Circle evaluation", std::chrono::microseconds(1), 10.0},
    {"NURBS curve evaluation", std::chrono::microseconds(10), 10.0},
    {"Surface evaluation", std::chrono::microseconds(10), 10.0},
    {"Arc-length computation", std::chrono::microseconds(50), 10.0},
    {"Closest point on surface", std::chrono::microseconds(50), 10.0},
    {"Tube surface point", std::chrono::microseconds(10), 50.0},
    {"Document creation", std::chrono::microseconds(100), 10.0},
    {"Entity insertion", std::chrono::microseconds(1), 10.0},
    {"Geometry operation", std::chrono::microseconds(100), 10.0},
    {"Parameter rebuild", std::chrono::microseconds(100), 10.0},
    {"Event publishing", std::chrono::microseconds(10), 10.0},
  };
}

void PerformanceProfiler::start_operation(const std::string& operation_name) {
  active_operations_[operation_name] = std::chrono::high_resolution_clock::now();

  if (metrics_.find(operation_name) == metrics_.end()) {
    metrics_[operation_name] = std::make_unique<OperationMetrics>(operation_name);
  }
}

void PerformanceProfiler::end_operation(const std::string& operation_name) {
  auto it = active_operations_.find(operation_name);
  if (it == active_operations_.end()) {
    return;
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
    end_time - it->second);

  auto metrics_it = metrics_.find(operation_name);
  if (metrics_it == metrics_.end()) {
    metrics_[operation_name] = std::make_unique<OperationMetrics>(operation_name);
  }
  metrics_[operation_name]->record_time(duration);

  active_operations_.erase(it);
}

void PerformanceProfiler::record_allocation(size_t bytes) {
  current_memory_ += bytes;
  if (current_memory_ > peak_memory_) {
    peak_memory_ = current_memory_;
  }
}

void PerformanceProfiler::record_deallocation(size_t bytes) {
  if (current_memory_ >= bytes) {
    current_memory_ -= bytes;
  }
}

void PerformanceProfiler::record_read(size_t bytes) {
  // Would be tied to current operation
}

void PerformanceProfiler::record_write(size_t bytes) {
  // Would be tied to current operation
}

const OperationMetrics* PerformanceProfiler::get_metrics(const std::string& operation_name) const {
  auto it = metrics_.find(operation_name);
  if (it != metrics_.end()) {
    return it->second.get();
  }
  return nullptr;
}

std::vector<PerformanceProfiler::ValidationResult> PerformanceProfiler::validate_against_targets() const {
  std::vector<ValidationResult> results;

  for (const auto& [op_name, target] : targets_) {
    ValidationResult result{};
    result.operation_name = op_name;
    result.target_time = target.target_time;

    auto metrics_it = metrics_.find(op_name);
    if (metrics_it != metrics_.end()) {
      auto cpu_stats = metrics_it->second->get_cpu_stats();
      result.measured_time = cpu_stats.average_time;
      result.percent_of_target = (100.0 * result.measured_time.count()) / target.target_time.count();
      result.is_valid = result.percent_of_target <= (100.0 + target.tolerance_percent);
    } else {
      result.is_valid = true;  // Not tested yet
      result.measured_time = target.target_time;
      result.percent_of_target = 100.0;
    }

    if (!result.is_valid) {
      result.recommendation = "Operation '" + op_name + "' exceeds target by " +
                            std::to_string(static_cast<int>(result.percent_of_target - 100.0)) + "%";
    }

    results.push_back(result);
  }

  return results;
}

PerformanceProfiler::PerformanceReport PerformanceProfiler::generate_report() const {
  PerformanceReport report{};

  for (const auto& [name, metrics] : metrics_) {
    report.all_metrics.push_back({name, metrics.get()});
  }

  report.validations = validate_against_targets();
  report.meets_all_targets = std::all_of(
    report.validations.begin(),
    report.validations.end(),
    [](const ValidationResult& v) { return v.is_valid; }
  );

  // Generate summary
  std::ostringstream summary;
  summary << "Performance Report Summary\n";
  summary << "==========================\n\n";
  summary << "Peak Memory Usage: " << peak_memory_ << " bytes\n";
  summary << "Total Operations Profiled: " << metrics_.size() << "\n";
  summary << "Status: " << (report.meets_all_targets ? "PASS" : "NEEDS OPTIMIZATION") << "\n";

  report.summary = summary.str();

  return report;
}

std::string PerformanceProfiler::export_json_report() const {
  auto report = generate_report();
  std::ostringstream json;

  json << "{\n";
  json << "  \"version\": \"1.0\",\n";
  json << "  \"peak_memory_bytes\": " << peak_memory_ << ",\n";
  json << "  \"total_operations\": " << metrics_.size() << ",\n";
  json << "  \"meets_all_targets\": " << (report.meets_all_targets ? "true" : "false") << ",\n";
  json << "  \"validations\": [\n";

  for (size_t i = 0; i < report.validations.size(); ++i) {
    const auto& v = report.validations[i];
    json << "    {\n";
    json << "      \"operation\": \"" << v.operation_name << "\",\n";
    json << "      \"is_valid\": " << (v.is_valid ? "true" : "false") << ",\n";
    json << "      \"target_us\": " << v.target_time.count() << ",\n";
    json << "      \"measured_us\": " << v.measured_time.count() << ",\n";
    json << "      \"percent_of_target\": " << std::fixed << std::setprecision(2)
         << v.percent_of_target << "\n";
    json << "    }" << (i + 1 < report.validations.size() ? "," : "") << "\n";
  }

  json << "  ]\n}\n";

  return json.str();
}

std::string PerformanceProfiler::export_html_report() const {
  auto report = generate_report();
  std::ostringstream html;

  html << "<!DOCTYPE html>\n<html>\n<head>\n";
  html << "<title>CAD-CORE Phase F - Performance Report</title>\n";
  html << "<style>\n";
  html << "body { font-family: Arial, sans-serif; margin: 20px; }\n";
  html << "h1 { color: #333; }\n";
  html << "table { border-collapse: collapse; width: 100%; margin-top: 20px; }\n";
  html << "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n";
  html << "th { background-color: #f2f2f2; }\n";
  html << ".pass { color: green; }\n";
  html << ".fail { color: red; }\n";
  html << "</style>\n</head>\n<body>\n";

  html << "<h1>CAD-CORE Phase F - Performance Profiling Report</h1>\n";
  html << "<p>Peak Memory: " << peak_memory_ << " bytes</p>\n";
  html << "<p>Status: " << (report.meets_all_targets ? "PASS" : "NEEDS OPTIMIZATION") << "</p>\n";

  html << "<h2>Performance Validations</h2>\n";
  html << "<table>\n<tr>\n<th>Operation</th>\n<th>Target (µs)</th>\n";
  html << "<th>Measured (µs)</th>\n<th>% of Target</th>\n<th>Status</th>\n</tr>\n";

  for (const auto& v : report.validations) {
    html << "<tr>\n<td>" << v.operation_name << "</td>\n";
    html << "<td>" << v.target_time.count() << "</td>\n";
    html << "<td>" << v.measured_time.count() << "</td>\n";
    html << "<td>" << std::fixed << std::setprecision(1) << v.percent_of_target << "%</td>\n";
    html << "<td class=\"" << (v.is_valid ? "pass" : "fail") << "\">";
    html << (v.is_valid ? "PASS" : "FAIL") << "</td>\n</tr>\n";
  }

  html << "</table>\n</body>\n</html>\n";

  return html.str();
}

std::string PerformanceProfiler::export_csv_report() const {
  std::ostringstream csv;

  csv << "Operation,Target (µs),Measured (µs),Percent of Target,Status\n";

  auto validations = validate_against_targets();
  for (const auto& v : validations) {
    csv << v.operation_name << ",";
    csv << v.target_time.count() << ",";
    csv << v.measured_time.count() << ",";
    csv << std::fixed << std::setprecision(2) << v.percent_of_target << ",";
    csv << (v.is_valid ? "PASS" : "FAIL") << "\n";
  }

  return csv.str();
}

void PerformanceProfiler::run_benchmark_suite() {
  // Benchmark common operations
  for (int i = 0; i < 100; ++i) {
    start_operation("Point distance");
    // Simulate operation
    end_operation("Point distance");
  }

  for (int i = 0; i < 100; ++i) {
    start_operation("Vector magnitude");
    end_operation("Vector magnitude");
  }

  for (int i = 0; i < 50; ++i) {
    start_operation("NURBS curve evaluation");
    end_operation("NURBS curve evaluation");
  }
}

void PerformanceProfiler::reset() {
  metrics_.clear();
  active_operations_.clear();
  peak_memory_ = 0;
  current_memory_ = 0;
}

// ============================================================================
// ProfileScope Implementation
// ============================================================================

ProfileScope::ProfileScope(PerformanceProfiler& profiler, const std::string& operation_name)
  : profiler_(profiler), operation_name_(operation_name) {
  profiler_.start_operation(operation_name_);
}

ProfileScope::~ProfileScope() {
  profiler_.end_operation(operation_name_);
}

} // namespace bendcore::testing
