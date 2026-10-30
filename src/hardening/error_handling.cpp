#include "bendcore/hardening/error_handling.hpp"
#include <thread>
#include <chrono>

namespace bendcore::hardening {

// RetryStrategy
RetryStrategy::RetryStrategy(int max_retries, int delay_ms)
    : max_retries_(max_retries), delay_ms_(delay_ms), retry_count_(0) {}

bool RetryStrategy::execute() {
    retry_count_ = 0;
    for (int i = 0; i < max_retries_; ++i) {
        retry_count_++;
        if (delay_ms_ > 0 && delay_ms_ <= 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms_));
        }
    }
    return true;
}

int RetryStrategy::getRetryCount() const { return retry_count_; }
std::string RetryStrategy::name() const { return "Retry"; }

// RollbackStrategy
void RollbackStrategy::addCheckpoint(std::function<void()> fn) {
    checkpoints_.push_back(std::move(fn));
}
int RollbackStrategy::getCheckpointCount() const {
    return static_cast<int>(checkpoints_.size());
}
bool RollbackStrategy::execute() {
    if (checkpoints_.empty()) return false;
    for (auto& cp : checkpoints_) cp();
    return true;
}
std::string RollbackStrategy::name() const { return "Rollback"; }

// ThrottleStrategy
ThrottleStrategy::ThrottleStrategy(int max_requests, int window_ms)
    : max_requests_(max_requests), window_ms_(window_ms), current_requests_(0) {}

bool ThrottleStrategy::allowRequest() {
    if (current_requests_ >= max_requests_) return false;
    current_requests_++;
    return true;
}
bool ThrottleStrategy::execute() { return allowRequest(); }
std::string ThrottleStrategy::name() const { return "Throttle"; }

// CircuitBreaker
CircuitBreaker::CircuitBreaker(int failure_threshold, int timeout_ms)
    : failure_threshold_(failure_threshold), timeout_ms_(timeout_ms),
      failure_count_(0), state_(State::CLOSED) {}

bool CircuitBreaker::call(std::function<bool()> fn) {
    if (state_ == State::OPEN) return false;
    try {
        bool result = fn();
        return result;
    } catch (...) {
        failure_count_++;
        if (failure_count_ >= failure_threshold_) {
            state_ = State::OPEN;
        }
        return false;
    }
}
CircuitBreaker::State CircuitBreaker::getState() const { return state_; }
int CircuitBreaker::getFailureCount() const { return failure_count_; }

// ErrorHandler
ErrorHandler::ErrorHandler() {}

void ErrorHandler::reportError(ErrorCode code, ErrorLevel level,
                               const std::string& message,
                               const char* file, int line) {
    std::lock_guard<std::mutex> lock(mutex_);
    errors_.push_back({code, level, message, file ? file : "", line});
}

int ErrorHandler::getErrorCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(errors_.size());
}

ErrorInfo* ErrorHandler::getLastError() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (errors_.empty()) return nullptr;
    return &errors_.back();
}

int ErrorHandler::getErrorCountByLevel(ErrorLevel level) const {
    std::lock_guard<std::mutex> lock(mutex_);
    int count = 0;
    for (const auto& e : errors_) {
        if (e.level == level) count++;
    }
    return count;
}

void ErrorHandler::addRecoveryStrategy(std::unique_ptr<IRecoveryStrategy> strategy) {
    strategies_.push_back(std::move(strategy));
}

bool ErrorHandler::executeRecoveryStrategies() {
    if (strategies_.empty()) return false;
    bool any = false;
    for (auto& s : strategies_) {
        if (s->execute()) any = true;
    }
    return any;
}

void ErrorHandler::clearHistory() {
    std::lock_guard<std::mutex> lock(mutex_);
    errors_.clear();
}

}  // namespace bendcore::hardening
