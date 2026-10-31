#include "bendcore/production/logging_monitoring.hpp"
#include <chrono>
#include <thread>
#include <fstream>

namespace bendcore::production {

StructuredLogger::StructuredLogger() : min_level_(LogLevel::INFO) {}

StructuredLogger::~StructuredLogger() {}

void StructuredLogger::log(LogLevel level,
                          const std::string& component,
                          const std::string& message,
                          const char* file,
                          int line,
                          const char* function) {
  if (static_cast<int>(level) < static_cast<int>(min_level_)) {
    return;
  }

  {
    std::lock_guard<std::shared_mutex> lock(mutex_);
    LogEntry entry{
      .timestamp = std::chrono::system_clock::now(),
      .level = level,
      .component = component,
      .message = message,
      .file = file ? file : "",
      .line = line,
      .function = function ? function : "",
      .metadata = current_metadata_
    };

    log_buffer_.push_back(entry);
    if (log_buffer_.size() > MAX_BUFFER_SIZE) {
      log_buffer_.erase(log_buffer_.begin());
    }

    for (const auto& handler : handlers_) {
      handler(entry);
    }
  }
}

void StructuredLogger::add_metadata(const std::string& key, const std::string& value) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  current_metadata_[key] = value;
}

void StructuredLogger::register_handler(LogCallback handler) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  handlers_.push_back(handler);
}

std::vector<LogEntry> StructuredLogger::get_recent_logs(size_t count) const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  if (log_buffer_.size() <= count) {
    return log_buffer_;
  }
  return std::vector<LogEntry>(log_buffer_.end() - count, log_buffer_.end());
}

std::vector<LogEntry> StructuredLogger::get_component_logs(const std::string& component) const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  std::vector<LogEntry> result;
  for (const auto& entry : log_buffer_) {
    if (entry.component == component) {
      result.push_back(entry);
    }
  }
  return result;
}

void StructuredLogger::clear_logs() {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  log_buffer_.clear();
}

bool StructuredLogger::export_logs(const std::string& filepath) const {
  std::ofstream file(filepath);
  if (!file.is_open()) return false;

  {
    std::lock_guard<std::shared_mutex> lock(mutex_);
    for (const auto& entry : log_buffer_) {
      file << entry.component << ": " << entry.message << "\n";
    }
  }
  return true;
}

MetricsCollector::MetricsCollector() {}

void MetricsCollector::record_metric(const std::string& name,
                                    double value,
                                    const std::map<std::string, std::string>&) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  metrics_[name].push_back(value);
}

void MetricsCollector::increment_counter(const std::string& name, int delta) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  counters_[name] += delta;
}

void MetricsCollector::set_gauge(const std::string& name, double value) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  metrics_[name] = {value};
}

std::map<std::string, double> MetricsCollector::get_metric_stats(const std::string& name) const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  std::map<std::string, double> stats;
  auto it = metrics_.find(name);
  if (it != metrics_.end() && !it->second.empty()) {
    stats["count"] = it->second.size();
    stats["sum"] = 0;
    for (double v : it->second) stats["sum"] += v;
    stats["mean"] = stats["sum"] / stats["count"];
    stats["min"] = *std::min_element(it->second.begin(), it->second.end());
    stats["max"] = *std::max_element(it->second.begin(), it->second.end());
  }
  return stats;
}

std::map<std::string, std::vector<double>> MetricsCollector::get_all_metrics() const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  return metrics_;
}

void MetricsCollector::reset_metrics() {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  metrics_.clear();
  counters_.clear();
}

LogAggregator::LogAggregator() {}

void LogAggregator::add_entry(const LogEntry& entry) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  entries_.push_back(entry);
}

std::vector<LogEntry> LogAggregator::query(
    const std::string& component,
    LogLevel min_level,
    std::chrono::seconds time_range) const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  std::vector<LogEntry> result;
  auto cutoff = std::chrono::system_clock::now() - time_range;

  for (const auto& entry : entries_) {
    if (entry.component == component &&
        static_cast<int>(entry.level) >= static_cast<int>(min_level) &&
        entry.timestamp >= cutoff) {
      result.push_back(entry);
    }
  }
  return result;
}

std::map<std::string, int> LogAggregator::get_statistics() const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  std::map<std::string, int> stats;
  for (const auto& entry : entries_) {
    stats[entry.component]++;
  }
  return stats;
}

bool LogAggregator::archive_logs(const std::string&, std::chrono::hours) {
  // Placeholder implementation
  return true;
}

} // namespace bendcore::production
