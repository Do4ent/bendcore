// Fault guard of the kernel readers (src/kernel/occt_fault_guard.*, docs/kernel/OCCT_BUILD.md,
// раздел 6): a hardware fault inside runConvertingFaults() becomes a Standard_Failure; outside it
// the process still dies with the original signal.

#include "../../src/kernel/occt_fault_guard.hpp"

#include <gtest/gtest.h>

#include <OSD_SIGSEGV.hxx>
#include <Standard_Failure.hxx>
#include <Standard_NumericError.hxx>
#include <Standard_OutOfRange.hxx>

#include <csignal>
#include <stdexcept>
#include <string>

namespace {

using bendcore::kernel::detail::callConvertingFaults;
using bendcore::kernel::detail::faultConversionAvailable;
using bendcore::kernel::detail::runConvertingFaults;

// Reads through a null pointer the compiler cannot prove null.
int readNull() {
    int* volatile p = nullptr;
    volatile int value = *p;
    return value;
}

int divideByZero() {
    volatile int zero = 0;
    volatile int one = 1;
    volatile int result = one / zero;  // stored: an unused quotient could be optimised away
    return result;
}

std::string faultMessage(const std::function<void()>& fn) {
    try {
        runConvertingFaults(fn);
    } catch (const Standard_Failure& failure) {
        return std::string(failure.ExceptionType()) + ": " + failure.what();
    }
    return "no exception";
}

TEST(KernelFaultGuard, AvailableInNativeBuilds) {
    // This test binary is native POSIX; WASM has no signals (the guard is a plain call there).
    EXPECT_TRUE(faultConversionAvailable());
}

TEST(KernelFaultGuard, NullReadBecomesSigsegvFailure) {
    const std::string m = faultMessage([] { (void)readNull(); });
    EXPECT_NE(m.find("OSD_SIGSEGV"), std::string::npos) << m;
    EXPECT_NE(m.find("invalid memory access"), std::string::npos) << m;
}

TEST(KernelFaultGuard, IntegerDivisionByZeroBecomesNumericError) {
    const std::string m = faultMessage([] { (void)divideByZero(); });
    EXPECT_NE(m.find("Standard_NumericError"), std::string::npos) << m;
    EXPECT_NE(m.find("SIGFPE"), std::string::npos) << m;
}

TEST(KernelFaultGuard, RepeatedFaultsAreAllConverted) {
    // The signal must be unblocked after each longjmp, otherwise the second fault kills the process.
    for (int i = 0; i < 50; ++i) {
        const std::string m = faultMessage([] { (void)readNull(); });
        ASSERT_NE(m.find("OSD_SIGSEGV"), std::string::npos) << "iteration " << i << ": " << m;
    }
}

TEST(KernelFaultGuard, NestedGuardCatchesInnermostFault) {
    std::string inner;
    int after = 0;
    runConvertingFaults([&] {
        inner = faultMessage([] { (void)readNull(); });
        after = 42;  // the outer guarded call continues normally
    });
    EXPECT_NE(inner.find("OSD_SIGSEGV"), std::string::npos) << inner;
    EXPECT_EQ(after, 42);
}

TEST(KernelFaultGuard, ExceptionsAndValuesPassThroughUnchanged) {
    EXPECT_THROW(runConvertingFaults([] { throw std::runtime_error("plain"); }), std::runtime_error);
    EXPECT_THROW(runConvertingFaults([] { throw Standard_OutOfRange("index"); }), Standard_OutOfRange);
    EXPECT_EQ(callConvertingFaults<int>([] { return 7 * 6; }), 42);
}

TEST(KernelFaultGuard, FaultOutsideTheGuardStillKillsTheProcess) {
    // Install the handler first, then fault outside a guarded call: the default action must run.
    runConvertingFaults([] {});
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_EXIT((void)readNull(), ::testing::KilledBySignal(SIGSEGV), "");
    EXPECT_EXIT((void)divideByZero(), ::testing::KilledBySignal(SIGFPE), "");
}

}  // namespace
