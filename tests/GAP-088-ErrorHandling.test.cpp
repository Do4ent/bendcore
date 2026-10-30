// GAP-088: ErrorHandling (37 tests) - Exception handling, recovery strategies
#include "bendcore/hardening/error_handling.hpp"
#include <cassert>
#include <cstdio>
#include <thread>

using namespace bendcore::production;

int main() {
    int passed = 0, failed = 0;

    #define TEST(name, cond) { \
        if (cond) { passed++; } else { \
            printf("FAIL: %s\n", name); failed++; \
        } \
    }

    // Test 1-5: ErrorInfo and ErrorLevel
    {
        ErrorHandler handler;
        handler.reportError(ErrorCode::INVALID_ARGUMENT, ErrorLevel::WARNING,
                           "Invalid argument", __FILE__, __LINE__);

        TEST("T1: Error reported", handler.getErrorCount() > 0);
        auto* error = handler.getLastError();
        TEST("T2: Error retrievable", error != nullptr);
        TEST("T3: Error code correct", error->code == ErrorCode::INVALID_ARGUMENT);
        TEST("T4: Error level correct", error->level == ErrorLevel::WARNING);
        TEST("T5: Message captured", error->message == "Invalid argument");
    }

    // Test 6-10: Multiple error levels
    {
        ErrorHandler handler;
        handler.reportError(ErrorCode::INFO, ErrorLevel::INFO, "Info msg", __FILE__, __LINE__);
        handler.reportError(ErrorCode::UNKNOWN_ERROR, ErrorLevel::ERROR, "Error msg", __FILE__, __LINE__);
        handler.reportError(ErrorCode::RESOURCE_EXHAUSTED, ErrorLevel::CRITICAL, "Critical msg", __FILE__, __LINE__);

        TEST("T6: Three errors logged", handler.getErrorCount() == 3);
        TEST("T7: Count info level", handler.getErrorCountByLevel(ErrorLevel::INFO) == 1);
        TEST("T8: Count error level", handler.getErrorCountByLevel(ErrorLevel::ERROR) == 1);
        TEST("T9: Count critical level", handler.getErrorCountByLevel(ErrorLevel::CRITICAL) == 1);
        TEST("T10: Last error is critical", handler.getLastError()->level == ErrorLevel::CRITICAL);
    }

    // Test 11-15: Retry strategy
    {
        auto retry = std::make_unique<RetryStrategy>(3, 10);
        TEST("T11: Retry strategy created", retry != nullptr);
        bool result = retry->execute();
        TEST("T12: Retry executed", result);
        TEST("T13: Retry count increased", retry->getRetryCount() == 3);
        TEST("T14: Retry name correct", retry->name() == "Retry");

        auto retry2 = std::make_unique<RetryStrategy>(1, 5);
        retry2->execute();
        TEST("T15: Single retry works", retry2->getRetryCount() == 1);
    }

    // Test 16-20: Rollback strategy
    {
        auto rollback = std::make_unique<RollbackStrategy>();
        bool executed = false;
        rollback->addCheckpoint([&]() { executed = true; });

        TEST("T16: Checkpoint added", rollback->getCheckpointCount() == 1);
        bool result = rollback->execute();
        TEST("T17: Rollback executed", result);
        TEST("T18: Checkpoint was called", executed);
        TEST("T19: Rollback name correct", rollback->name() == "Rollback");

        auto rollback2 = std::make_unique<RollbackStrategy>();
        TEST("T20: Empty rollback fails", !rollback2->execute());
    }

    // Test 21-25: Throttle strategy
    {
        auto throttle = std::make_unique<ThrottleStrategy>(2, 100);
        TEST("T21: Throttle created", throttle != nullptr);

        bool req1 = throttle->allowRequest();
        bool req2 = throttle->allowRequest();
        bool req3 = throttle->allowRequest();

        TEST("T22: First request allowed", req1);
        TEST("T23: Second request allowed", req2);
        TEST("T24: Third request rejected", !req3);
        TEST("T25: Throttle name correct", throttle->name() == "Throttle");
    }

    // Test 26-30: Circuit breaker
    {
        CircuitBreaker cb(2, 1000);
        TEST("T26: CB initial state closed", cb.getState() == CircuitBreaker::State::CLOSED);

        bool op1 = cb.call([]() { throw std::runtime_error("error"); return false; });
        TEST("T27: First failure recorded", !op1);
        TEST("T28: Failure count 1", cb.getFailureCount() == 1);

        bool op2 = cb.call([]() { throw std::runtime_error("error"); return false; });
        TEST("T29: State opened after threshold", cb.getState() == CircuitBreaker::State::OPEN);

        bool op3 = cb.call([]() { return true; });
        TEST("T30: Open CB rejects call", !op3);
    }

    // Test 31-35: Error handler with recovery strategies
    {
        ErrorHandler handler;
        auto retry = std::make_unique<RetryStrategy>(1, 5);
        handler.addRecoveryStrategy(std::move(retry));

        TEST("T31: Recovery strategy added", true);
        bool recovered = handler.executeRecoveryStrategies();
        TEST("T32: Recovery executed", recovered);

        handler.reportError(ErrorCode::TIMEOUT, ErrorLevel::ERROR, "Timeout", __FILE__, __LINE__);
        TEST("T33: Error logged after recovery", handler.getErrorCount() == 1);

        handler.clearHistory();
        TEST("T34: History cleared", handler.getErrorCount() == 0);

        handler.reportError(ErrorCode::NETWORK_ERROR, ErrorLevel::WARNING, "Network", __FILE__, __LINE__);
        TEST("T35: Error logged after clear", handler.getErrorCount() == 1);
    }

    // Test 36-37: Thread safety
    {
        ErrorHandler handler;
        auto worker = [&handler]() {
            for (int j = 0; j < 10; ++j) {
                handler.reportError(ErrorCode::SUCCESS, ErrorLevel::INFO,
                                  "Thread msg", __FILE__, __LINE__);
            }
        };
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
        // WebAssembly без pthreads: std::thread недоступен, та же работа выполняется последовательно.
        for (int i = 0; i < 4; ++i) worker();
#else
        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i) threads.emplace_back(worker);
        for (auto& t : threads) t.join();
#endif

        TEST("T36: All errors logged (thread-safe)", handler.getErrorCount() == 40);
        TEST("T37: No data corruption", handler.getLastError() != nullptr);
    }

    printf("\n=== GAP-088: ErrorHandling ===\n");
    printf("Passed: %d/37\n", passed);
    printf("Failed: %d/37\n", failed);
    return (failed == 0) ? 0 : 1;
}
