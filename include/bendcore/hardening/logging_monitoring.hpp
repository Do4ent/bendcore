#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <cstdint>

namespace bendcore::hardening {

// ==============================================================================
// LOGGING & MONITORING - GAP-090
// ==============================================================================

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

enum class LogSource {
    CORE,
    IO,
    GEOMETRY,
    NETWORK,
    UI,
    PLUGIN
};

struct LogEntry {
    uint64_t id;
    LogLevel level;
    LogSource source;
    std::string message;
    std::string component;
    int64_t timestamp;
    int sequence;
    std::string extra;
};

class ILogFormatter {
public:
    virtual ~ILogFormatter() = default;
    virtual std::string format(const LogEntry& entry) = 0;
};

class SimpleFormatter : public ILogFormatter {
public:
    std::string format(const LogEntry& entry) override;
};

class JSONFormatter : public ILogFormatter {
public:
    std::string format(const LogEntry& entry) override;
};

class StructuredLogger {
public:
    StructuredLogger();
    void setLogLevel(LogLevel level);
    void trace(const std::string& msg, const std::string& component = "");
    void debug(const std::string& msg, const std::string& component = "");
    void info(const std::string& msg, const std::string& component = "");
    void warning(const std::string& msg, const std::string& component = "");
    void error(const std::string& msg, const std::string& component = "");
    void critical(const std::string& msg, const std::string& component = "");
    int getLogCount() const;
    const LogEntry* getLogEntry(int n) const;  // 1-based
    const std::vector<LogEntry>& getLogHistory() const;
    void setFormatter(std::unique_ptr<ILogFormatter> formatter);
private:
    void addEntry(LogLevel level, const std::string& msg, const std::string& component);
    LogLevel min_level_;
    std::vector<LogEntry> logs_;
    std::unique_ptr<ILogFormatter> formatter_;
    uint64_t next_id_;
};

class MetricsAggregator {
public:
    MetricsAggregator();
    void recordMetric(const std::string& name, double value);
    std::map<std::string, std::vector<double>> getAllMetrics() const;
    double getMetricAverage(const std::string& name) const;
    double getMetricMax(const std::string& name) const;
    double getMetricMin(const std::string& name) const;
    void incrementCounter(const std::string& name, int64_t delta = 1);
    int64_t getCounter(const std::string& name) const;
private:
    std::map<std::string, std::vector<double>> metrics_;
    std::map<std::string, int64_t> counters_;
};

class LogAggregator {
public:
    LogAggregator();
    void addLog(const LogEntry& entry);
    std::vector<LogEntry> getLogsByLevel(LogLevel level) const;
    int getLogCountByLevel(LogLevel level) const;
    std::vector<LogEntry> getLogsBySource(LogSource source) const;
private:
    std::vector<LogEntry> logs_;
};

class MonitoringSystem {
public:
    MonitoringSystem();
    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const;
    StructuredLogger& getLogger();
private:
    bool monitoring_;
    StructuredLogger logger_;
};

}  // namespace bendcore::hardening

namespace bendcore {
    namespace production = hardening;
}
