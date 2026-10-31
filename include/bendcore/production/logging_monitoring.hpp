#ifndef BENDCORE_PRODUCTION_LOGGING_MONITORING_HPP
#define BENDCORE_PRODUCTION_LOGGING_MONITORING_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <chrono>
#include <iostream>

namespace bendcore::production {

/**
 * Log levels
 */
enum class LogLevel {
  DEBUG = 0,
  INFO = 1,
  WARNING = 2,
  ERROR = 3,
  CRITICAL = 4,
  TRACE = 5
};

/**
 * Log entry structure
 */
struct LogEntry {
  std::chrono::system_clock::time_point timestamp;
  LogLevel level;
  std::string component;
  std::string message;
  std::string file;
  int line;
  std::string function;
  std::string thread_id;
  std::map<std::string, std::string> metadata;
};

/**
 * Structured logging system
 */
class StructuredLogger {
 public:
  using LogCallback = std::function<void(const LogEntry&)>;

  StructuredLogger();
  ~StructuredLogger();

  // Log message with context
  void log(LogLevel level,
          const std::string& component,
          const std::string& message,
          const char* file = "",
          int line = 0,
          const char* function = "");

  // Add metadata to log entry
  void add_metadata(const std::string& key, const std::string& value);

  // Register log handler
  void register_handler(LogCallback handler);

  // Set minimum log level
  void set_level(LogLevel level) { min_level_ = level; }

  // Get recent logs
  std::vector<LogEntry> get_recent_logs(size_t count = 100) const;

  // Get logs by component
  std::vector<LogEntry> get_component_logs(const std::string& component) const;

  // Clear logs
  void clear_logs();

  // Export logs to file
  bool export_logs(const std::string& filepath) const;

 private:
  LogLevel min_level_;
  std::vector<LogEntry> log_buffer_;
  std::vector<LogCallback> handlers_;
  std::map<std::string, std::string> current_metadata_;
  mutable std::shared_mutex mutex_;
  static constexpr size_t MAX_BUFFER_SIZE = 10000;
};

/**
 * Metrics monitoring system
 */
class MetricsCollector {
 public:
  MetricsCollector();
  ~MetricsCollector() = default;

  // Record metric
  void record_metric(const std::string& name,
                    double value,
                    const std::map<std::string, std::string>& tags = {});

  // Record counter increment
  void increment_counter(const std::string& name, int delta = 1);

  // Record gauge value
  void set_gauge(const std::string& name, double value);

  // Get metric statistics
  std::map<std::string, double> get_metric_stats(const std::string& name) const;

  // Get all metrics
  std::map<std::string, std::vector<double>> get_all_metrics() const;

  // Reset metrics
  void reset_metrics();

 private:
  std::map<std::string, std::vector<double>> metrics_;
  std::map<std::string, int> counters_;
  mutable std::shared_mutex mutex_;
};

/**
 * Log aggregator for centralized log management
 */
class LogAggregator {
 public:
  LogAggregator();
  ~LogAggregator() = default;

  // Add log entry
  void add_entry(const LogEntry& entry);

  // Query logs
  std::vector<LogEntry> query(
    const std::string& component,
    LogLevel min_level,
    std::chrono::seconds time_range) const;

  // Get statistics
  std::map<std::string, int> get_statistics() const;

  // Archive old logs
  bool archive_logs(const std::string& archive_path,
                   std::chrono::hours age_threshold);

 private:
  std::vector<LogEntry> entries_;
  mutable std::shared_mutex mutex_;
};

} // namespace bendcore::production

#endif // BENDCORE_PRODUCTION_LOGGING_MONITORING_HPP
