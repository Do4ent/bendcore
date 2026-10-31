#ifndef BENDCORE_PRODUCTION_ERROR_HANDLING_HPP
#define BENDCORE_PRODUCTION_ERROR_HANDLING_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <exception>
#include <optional>

namespace bendcore::production {

/**
 * Standardized error codes for BendCore
 */
enum class ErrorCode {
  SUCCESS = 0,
  INVALID_INPUT = 1001,
  FILE_NOT_FOUND = 1002,
  ACCESS_DENIED = 1003,
  INVALID_STATE = 1004,
  OPERATION_FAILED = 1005,
  TIMEOUT = 1006,
  OUT_OF_MEMORY = 1007,
  INVALID_PARAMETER = 1008,
  INITIALIZATION_FAILED = 1009,
  RESOURCE_EXHAUSTED = 1010,
  NETWORK_ERROR = 2001,
  DATABASE_ERROR = 2002,
  SERIALIZATION_ERROR = 2003,
  CONCURRENT_MODIFICATION = 3001,
  UNKNOWN_ERROR = 9999
};

/**
 * Error severity level
 */
enum class ErrorSeverity {
  INFO,
  WARNING,
  ERROR,
  CRITICAL
};

/**
 * Exception context with recovery information
 */
struct ExceptionContext {
  ErrorCode code;
  ErrorSeverity severity;
  std::string message;
  std::string file;
  int line;
  std::string function;
  std::string context;
  std::map<std::string, std::string> metadata;
  std::optional<std::string> recovery_hint;
};

/**
 * Recovery strategy for exceptions
 */
enum class RecoveryStrategy {
  RETRY,        // Retry the operation
  FALLBACK,     // Use fallback implementation
  GRACEFUL_DEGRADE, // Degrade functionality
  TERMINATE,    // Terminate operation
  SKIP          // Skip and continue
};

/**
 * Exception handler with recovery support
 */
class ExceptionHandler {
 public:
  using RecoveryCallback = std::function<bool(const ExceptionContext&)>;
  using ErrorCallback = std::function<void(const ExceptionContext&)>;

  ExceptionHandler();
  ~ExceptionHandler() = default;

  // Register recovery handler
  void register_recovery_handler(ErrorCode code, RecoveryCallback handler);

  // Register error callback for logging/monitoring
  void register_error_callback(ErrorCallback callback);

  // Handle exception with context
  RecoveryStrategy handle_exception(const std::exception& e,
                                   const std::string& context);

  // Handle with explicit context
  RecoveryStrategy handle_error(ErrorCode code,
                               ErrorSeverity severity,
                               const std::string& message,
                               const std::string& context);

  // Get error statistics
  std::map<ErrorCode, int> get_error_statistics() const;

  // Clear statistics
  void clear_statistics();

 private:
  std::map<ErrorCode, RecoveryCallback> recovery_handlers_;
  std::vector<ErrorCallback> error_callbacks_;
  std::map<ErrorCode, int> error_count_;
  mutable std::shared_mutex mutex_;
};

/**
 * Production exception class
 */
class ProductionException : public std::exception {
 public:
  ProductionException(ErrorCode code,
                     const std::string& message,
                     const std::string& context = "");

  const char* what() const noexcept override;
  ErrorCode code() const { return code_; }
  const std::string& context() const { return context_; }

 private:
  ErrorCode code_;
  std::string message_;
  std::string context_;
  std::string what_message_;
};

/**
 * Error handler utilities
 */
namespace error_utils {
  std::string error_code_to_string(ErrorCode code);
  std::string severity_to_string(ErrorSeverity severity);
  ErrorCode error_from_errno();
}

} // namespace bendcore::production

#endif // BENDCORE_PRODUCTION_ERROR_HANDLING_HPP
