#include "bendcore/hardening/logging_monitoring.hpp"
#include <sstream>
#include <numeric>
#include <algorithm>

namespace bendcore::hardening {

// SimpleFormatter
std::string SimpleFormatter::format(const LogEntry& entry) {
    std::ostringstream oss;
    oss << "[" << entry.component << "] " << entry.message;
    return oss.str();
}

// JSONFormatter
std::string JSONFormatter::format(const LogEntry& entry) {
    std::ostringstream oss;
    oss << "{\"message\":\"" << entry.message
        << "\",\"component\":\"" << entry.component << "\"}";
    return oss.str();
}

// StructuredLogger
StructuredLogger::StructuredLogger()
    : min_level_(LogLevel::TRACE), next_id_(1),
      formatter_(std::make_unique<SimpleFormatter>()) {}

void StructuredLogger::setLogLevel(LogLevel level) { min_level_ = level; }

void StructuredLogger::addEntry(LogLevel level, const std::string& msg,
                                 const std::string& component) {
    if (level < min_level_) return;
    LogEntry e;
    e.id = next_id_++;
    e.level = level;
    e.source = LogSource::CORE;
    e.message = msg;
    e.component = component;
    e.timestamp = 0;
    e.sequence = static_cast<int>(logs_.size()) + 1;
    e.extra = "";
    logs_.push_back(e);
}

void StructuredLogger::trace(const std::string& msg, const std::string& comp) {
    addEntry(LogLevel::TRACE, msg, comp);
}
void StructuredLogger::debug(const std::string& msg, const std::string& comp) {
    addEntry(LogLevel::DEBUG, msg, comp);
}
void StructuredLogger::info(const std::string& msg, const std::string& comp) {
    addEntry(LogLevel::INFO, msg, comp);
}
void StructuredLogger::warning(const std::string& msg, const std::string& comp) {
    addEntry(LogLevel::WARNING, msg, comp);
}
void StructuredLogger::error(const std::string& msg, const std::string& comp) {
    addEntry(LogLevel::ERROR, msg, comp);
}
void StructuredLogger::critical(const std::string& msg, const std::string& comp) {
    addEntry(LogLevel::CRITICAL, msg, comp);
}

int StructuredLogger::getLogCount() const {
    return static_cast<int>(logs_.size());
}

const LogEntry* StructuredLogger::getLogEntry(int n) const {
    // 1-based
    if (n < 1 || n > static_cast<int>(logs_.size())) return nullptr;
    return &logs_[n - 1];
}

const std::vector<LogEntry>& StructuredLogger::getLogHistory() const {
    return logs_;
}

void StructuredLogger::setFormatter(std::unique_ptr<ILogFormatter> formatter) {
    formatter_ = std::move(formatter);
}

// MetricsAggregator
MetricsAggregator::MetricsAggregator() {}

void MetricsAggregator::recordMetric(const std::string& name, double value) {
    metrics_[name].push_back(value);
}

std::map<std::string, std::vector<double>> MetricsAggregator::getAllMetrics() const {
    return metrics_;
}

double MetricsAggregator::getMetricAverage(const std::string& name) const {
    auto it = metrics_.find(name);
    if (it == metrics_.end() || it->second.empty()) return 0.0;
    double sum = 0.0;
    for (double v : it->second) sum += v;
    return sum / static_cast<double>(it->second.size());
}

double MetricsAggregator::getMetricMax(const std::string& name) const {
    auto it = metrics_.find(name);
    if (it == metrics_.end() || it->second.empty()) return 0.0;
    return *std::max_element(it->second.begin(), it->second.end());
}

double MetricsAggregator::getMetricMin(const std::string& name) const {
    auto it = metrics_.find(name);
    if (it == metrics_.end() || it->second.empty()) return 0.0;
    return *std::min_element(it->second.begin(), it->second.end());
}

void MetricsAggregator::incrementCounter(const std::string& name, int64_t delta) {
    counters_[name] += delta;
}

int64_t MetricsAggregator::getCounter(const std::string& name) const {
    auto it = counters_.find(name);
    if (it == counters_.end()) return 0;
    return it->second;
}

// LogAggregator
LogAggregator::LogAggregator() {}

void LogAggregator::addLog(const LogEntry& entry) {
    logs_.push_back(entry);
}

std::vector<LogEntry> LogAggregator::getLogsByLevel(LogLevel level) const {
    std::vector<LogEntry> result;
    for (const auto& e : logs_) {
        if (e.level == level) result.push_back(e);
    }
    return result;
}

int LogAggregator::getLogCountByLevel(LogLevel level) const {
    int count = 0;
    for (const auto& e : logs_) {
        if (e.level == level) count++;
    }
    return count;
}

std::vector<LogEntry> LogAggregator::getLogsBySource(LogSource source) const {
    std::vector<LogEntry> result;
    for (const auto& e : logs_) {
        if (e.source == source) result.push_back(e);
    }
    return result;
}

// MonitoringSystem
MonitoringSystem::MonitoringSystem() : monitoring_(false) {}

void MonitoringSystem::startMonitoring() { monitoring_ = true; }
void MonitoringSystem::stopMonitoring() { monitoring_ = false; }
bool MonitoringSystem::isMonitoring() const { return monitoring_; }
StructuredLogger& MonitoringSystem::getLogger() { return logger_; }

}  // namespace bendcore::hardening
