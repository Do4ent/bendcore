#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <cstdint>

namespace bendcore::hardening {

// ==============================================================================
// ERROR HANDLING - GAP-088
// ==============================================================================
// Comprehensive exception handling, error recovery, graceful degradation

enum class ErrorSeverity {
    FATAL,
    CRITICAL,
    ERROR,
    WARNING,
    INFO
};

enum class RecoveryStrategy {
    RETRY,
    ROLLBACK,
    FALLBACK,
    GRACEFUL_DEGRADATION,
    ABORT
};

struct ErrorContext {
    std::string errorId;
    std::string message;
    ErrorSeverity severity;
    std::string component;
    std::string functionName;
    int32_t lineNumber;
    std::string stackTrace;
    int64_t timestamp;
    std::map<std::string, std::string> contextData;
};

struct RecoveryAction {
    std::string actionId;
    RecoveryStrategy strategy;
    std::string description;
    std::function<bool()> action;
    int32_t maxRetries;
    int32_t retryCount;
};

class ErrorHandlingSystem {
public:
    ErrorHandlingSystem();
    ~ErrorHandlingSystem();

    // Error logging and reporting
    bool logError(const ErrorContext& error);
    std::vector<ErrorContext> getErrorLog();
    std::vector<ErrorContext> getErrorsByComponent(const std::string& component);
    std::vector<ErrorContext> getErrorsBySeverity(ErrorSeverity severity);

    // Exception handling
    bool handleException(const std::exception& e, ErrorSeverity severity);
    bool handleCustomException(const std::string& message, ErrorSeverity severity);
    bool registerExceptionHandler(std::function<void(const ErrorContext&)> handler);

    // Error recovery
    bool registerRecoveryAction(const std::string& errorType, const RecoveryAction& action);
    bool attemptRecovery(const ErrorContext& error);
    bool executeRecoveryAction(const RecoveryAction& action);
    std::vector<RecoveryAction> getAvailableRecoveryActions();

    // Retry logic
    bool executeWithRetry(
        std::function<bool()> operation,
        int32_t maxRetries,
        int32_t delayMs
    );
    bool setRetryPolicy(int32_t maxRetries, int32_t initialDelayMs, float backoffFactor);

    // Fallback handling
    bool registerFallback(const std::string& operationName, std::function<bool()> fallback);
    bool executeFallback(const std::string& operationName);

    // Graceful degradation
    bool enableGracefulDegradation(bool enable);
    bool isGracefulDegradationEnabled() const;
    bool reduceFeatureSet();
    bool restoreFullFeatureSet();

    // Error propagation
    bool propagateError(const ErrorContext& error);
    bool suppressError(const std::string& errorId);
    bool muteErrorType(const std::string& errorType);

    // Error analytics
    int32_t getErrorCount() const;
    int32_t getErrorCountByComponent(const std::string& component);
    std::map<std::string, int32_t> getErrorFrequency();
    float getErrorRate() const;

    // Error reporting
    std::string generateErrorReport();
    std::string generateErrorReportByComponent(const std::string& component);
    bool exportErrorLog(const std::string& outputPath);

    // Circuit breaker pattern
    bool enableCircuitBreaker(const std::string& name, int32_t failureThreshold);
    bool isCircuitBreakerOpen(const std::string& name);
    bool resetCircuitBreaker(const std::string& name);

    // Health monitoring
    bool isSystemHealthy() const;
    float getSystemHealthScore();
    std::vector<std::string> getHealthIssues();

private:
    std::vector<ErrorContext> errorLog;
    std::map<std::string, RecoveryAction> recoveryActions;
    std::map<std::string, std::function<bool()>> fallbacks;
    std::vector<std::function<void(const ErrorContext&)>> exceptionHandlers;
    bool gracefulDegradationEnabled;
};

}  // namespace bendcore::hardening
