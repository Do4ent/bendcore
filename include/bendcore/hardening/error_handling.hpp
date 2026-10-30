#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <cstdint>

namespace bendcore::hardening {

// ==============================================================================
// ERROR HANDLING - GAP-088
// ==============================================================================

enum class ErrorCode {
    SUCCESS,
    INFO,
    INVALID_ARGUMENT,
    UNKNOWN_ERROR,
    RESOURCE_EXHAUSTED,
    TIMEOUT,
    NETWORK_ERROR
};

enum class ErrorLevel {
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

struct ErrorInfo {
    ErrorCode code;
    ErrorLevel level;
    std::string message;
    std::string file;
    int line;
};

class IRecoveryStrategy {
public:
    virtual ~IRecoveryStrategy() = default;
    virtual bool execute() = 0;
    virtual std::string name() const = 0;
};

class RetryStrategy : public IRecoveryStrategy {
public:
    RetryStrategy(int max_retries, int delay_ms);
    bool execute() override;
    int getRetryCount() const;
    std::string name() const override;
private:
    int max_retries_;
    int delay_ms_;
    int retry_count_;
};

class RollbackStrategy : public IRecoveryStrategy {
public:
    void addCheckpoint(std::function<void()> fn);
    int getCheckpointCount() const;
    bool execute() override;
    std::string name() const override;
private:
    std::vector<std::function<void()>> checkpoints_;
};

class ThrottleStrategy : public IRecoveryStrategy {
public:
    ThrottleStrategy(int max_requests, int window_ms);
    bool allowRequest();
    bool execute() override;
    std::string name() const override;
private:
    int max_requests_;
    int window_ms_;
    int current_requests_;
};

class CircuitBreaker {
public:
    enum class State { CLOSED, OPEN, HALF_OPEN };
    CircuitBreaker(int failure_threshold, int timeout_ms);
    bool call(std::function<bool()> fn);
    State getState() const;
    int getFailureCount() const;
private:
    int failure_threshold_;
    int timeout_ms_;
    int failure_count_;
    State state_;
};

class ErrorHandler {
public:
    ErrorHandler();
    void reportError(ErrorCode code, ErrorLevel level, const std::string& message,
                     const char* file = "", int line = 0);
    int getErrorCount() const;
    ErrorInfo* getLastError();
    int getErrorCountByLevel(ErrorLevel level) const;
    void addRecoveryStrategy(std::unique_ptr<IRecoveryStrategy> strategy);
    bool executeRecoveryStrategies();
    void clearHistory();
private:
    std::vector<ErrorInfo> errors_;
    std::vector<std::unique_ptr<IRecoveryStrategy>> strategies_;
    mutable std::mutex mutex_;
};

}  // namespace bendcore::hardening

namespace bendcore {
    namespace production = hardening;
}
