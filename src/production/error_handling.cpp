#include "bendcore/production/error_handling.hpp"
#include <sstream>
#include <iostream>

namespace bendcore::production {

ExceptionHandler::ExceptionHandler() {}

void ExceptionHandler::register_recovery_handler(ErrorCode code, RecoveryCallback handler) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  recovery_handlers_[code] = handler;
}

void ExceptionHandler::register_error_callback(ErrorCallback callback) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  error_callbacks_.push_back(callback);
}

RecoveryStrategy ExceptionHandler::handle_exception(const std::exception& e,
                                                   const std::string& context) {
  ExceptionContext ctx{
    .code = ErrorCode::UNKNOWN_ERROR,
    .severity = ErrorSeverity::ERROR,
    .message = e.what(),
    .context = context
  };
  return handle_error(ctx.code, ctx.severity, ctx.message, context);
}

RecoveryStrategy ExceptionHandler::handle_error(ErrorCode code,
                                               ErrorSeverity severity,
                                               const std::string& message,
                                               const std::string& context) {
  {
    std::lock_guard<std::shared_mutex> lock(mutex_);
    error_count_[code]++;

    ExceptionContext ctx{
      .code = code,
      .severity = severity,
      .message = message,
      .context = context
    };

    for (const auto& callback : error_callbacks_) {
      callback(ctx);
    }
  }

  {
    std::lock_guard<std::shared_mutex> lock(mutex_);
    auto it = recovery_handlers_.find(code);
    if (it != recovery_handlers_.end()) {
      ExceptionContext ctx{
        .code = code,
        .severity = severity,
        .message = message,
        .context = context
      };
      if (it->second(ctx)) {
        return RecoveryStrategy::RETRY;
      }
    }
  }

  return RecoveryStrategy::TERMINATE;
}

std::map<ErrorCode, int> ExceptionHandler::get_error_statistics() const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  return error_count_;
}

void ExceptionHandler::clear_statistics() {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  error_count_.clear();
}

ProductionException::ProductionException(ErrorCode code,
                                        const std::string& message,
                                        const std::string& context)
    : code_(code), message_(message), context_(context) {
  std::ostringstream oss;
  oss << error_utils::error_code_to_string(code) << ": " << message;
  if (!context.empty()) {
    oss << " [" << context << "]";
  }
  what_message_ = oss.str();
}

const char* ProductionException::what() const noexcept {
  return what_message_.c_str();
}

namespace error_utils {

std::string error_code_to_string(ErrorCode code) {
  switch (code) {
    case ErrorCode::SUCCESS: return "SUCCESS";
    case ErrorCode::INVALID_INPUT: return "INVALID_INPUT";
    case ErrorCode::FILE_NOT_FOUND: return "FILE_NOT_FOUND";
    case ErrorCode::ACCESS_DENIED: return "ACCESS_DENIED";
    case ErrorCode::INVALID_STATE: return "INVALID_STATE";
    case ErrorCode::OPERATION_FAILED: return "OPERATION_FAILED";
    case ErrorCode::TIMEOUT: return "TIMEOUT";
    case ErrorCode::OUT_OF_MEMORY: return "OUT_OF_MEMORY";
    case ErrorCode::INVALID_PARAMETER: return "INVALID_PARAMETER";
    case ErrorCode::INITIALIZATION_FAILED: return "INITIALIZATION_FAILED";
    case ErrorCode::RESOURCE_EXHAUSTED: return "RESOURCE_EXHAUSTED";
    case ErrorCode::NETWORK_ERROR: return "NETWORK_ERROR";
    case ErrorCode::DATABASE_ERROR: return "DATABASE_ERROR";
    case ErrorCode::SERIALIZATION_ERROR: return "SERIALIZATION_ERROR";
    case ErrorCode::CONCURRENT_MODIFICATION: return "CONCURRENT_MODIFICATION";
    case ErrorCode::UNKNOWN_ERROR: return "UNKNOWN_ERROR";
    default: return "UNKNOWN_ERROR";
  }
}

std::string severity_to_string(ErrorSeverity severity) {
  switch (severity) {
    case ErrorSeverity::INFO: return "INFO";
    case ErrorSeverity::WARNING: return "WARNING";
    case ErrorSeverity::ERROR: return "ERROR";
    case ErrorSeverity::CRITICAL: return "CRITICAL";
    default: return "UNKNOWN";
  }
}

ErrorCode error_from_errno() {
  // Map system errno to ErrorCode
  extern int errno;
  switch (errno) {
    case ENOENT: return ErrorCode::FILE_NOT_FOUND;
    case EACCES: return ErrorCode::ACCESS_DENIED;
    case ENOMEM: return ErrorCode::OUT_OF_MEMORY;
    case EINVAL: return ErrorCode::INVALID_PARAMETER;
    case ETIMEDOUT: return ErrorCode::TIMEOUT;
    default: return ErrorCode::OPERATION_FAILED;
  }
}

} // namespace error_utils

} // namespace bendcore::production
