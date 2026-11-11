#include "occt_fault_guard.hpp"

// OCC_CATCH_SIGNALS and Standard_ErrorHandler::Abort behave as in OCCT itself only when this
// macro is set: OCCT's CMake defines it for every GCC/Clang build on POSIX (adm/cmake/
// occt_defs_flags.cmake), and the inline parts of Standard_ErrorHandler.hxx must match.
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#ifndef OCC_CONVERT_SIGNALS
#define OCC_CONVERT_SIGNALS
#endif
#define BENDCORE_KERNEL_CONVERT_FAULTS 1
#else
#define BENDCORE_KERNEL_CONVERT_FAULTS 0
#endif

#include <Standard_ErrorHandler.hxx>

#if BENDCORE_KERNEL_CONVERT_FAULTS
#include <OSD_SIGBUS.hxx>
#include <OSD_SIGSEGV.hxx>
#include <Standard_NumericError.hxx>

#include <csignal>
#include <cstdio>
#include <mutex>

#include <signal.h>
#endif

namespace bendcore::kernel::detail {

#if BENDCORE_KERNEL_CONVERT_FAULTS

namespace {

// Nesting depth of runConvertingFaults() on this thread; the handler converts only when > 0.
thread_local int t_guardDepth = 0;

constexpr int kSignals[] = {SIGSEGV, SIGBUS, SIGFPE};
struct sigaction g_previous[3];
std::once_flag g_installed;

int slotOf(int sig) {
    for (int i = 0; i < 3; ++i) {
        if (kSignals[i] == sig) return i;
    }
    return -1;
}

// Fault outside a guarded call: behave as if this handler did not exist.
void passOn(int sig, siginfo_t* info, void* context) {
    const int slot = slotOf(sig);
    const struct sigaction& prev = g_previous[slot < 0 ? 0 : slot];
    if ((prev.sa_flags & SA_SIGINFO) != 0 && prev.sa_sigaction != nullptr) {
        prev.sa_sigaction(sig, info, context);
        return;
    }
    if ((prev.sa_flags & SA_SIGINFO) == 0 && prev.sa_handler != SIG_DFL && prev.sa_handler != SIG_IGN &&
        prev.sa_handler != nullptr) {
        prev.sa_handler(sig);
        return;
    }
    // Default action: restore it and return; the faulting instruction runs again and the
    // kernel terminates the process with the original signal (core dump preserved).
    struct sigaction dfl {};
    dfl.sa_handler = SIG_DFL;
    sigemptyset(&dfl.sa_mask);
    sigaction(sig, &dfl, nullptr);
}

void onFault(int sig, siginfo_t* info, void* context) {
    if (t_guardDepth <= 0 || Standard_ErrorHandler::FindHandler() == nullptr) {
        passOn(sig, info, context);
        return;
    }
    // The handler leaves by longjmp: unblock the signal so that the next fault is delivered.
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, sig);
    sigprocmask(SIG_UNBLOCK, &set, nullptr);
    char message[128];
    void* address = info != nullptr ? info->si_addr : nullptr;
    switch (sig) {
        case SIGSEGV:
            std::snprintf(message, sizeof(message), "SIGSEGV (invalid memory access at %p) inside OCCT", address);
            Standard_ErrorHandler::Abort(OSD_SIGSEGV(message));
            break;
        case SIGBUS:
            std::snprintf(message, sizeof(message), "SIGBUS (bus error at %p) inside OCCT", address);
            Standard_ErrorHandler::Abort(OSD_SIGBUS(message));
            break;
        default:
            std::snprintf(message, sizeof(message), "SIGFPE (arithmetic fault at %p) inside OCCT", address);
            Standard_ErrorHandler::Abort(Standard_NumericError(message));
            break;
    }
}

void install() {
    for (int i = 0; i < 3; ++i) {
        struct sigaction action {};
        action.sa_sigaction = onFault;
        // SA_NODEFER: the handler exits by longjmp, the signal must not stay blocked.
        action.sa_flags = SA_SIGINFO | SA_NODEFER | SA_ONSTACK;
        sigemptyset(&action.sa_mask);
        sigaction(kSignals[i], &action, &g_previous[i]);
    }
}

struct DepthScope {
    DepthScope() { ++t_guardDepth; }
    ~DepthScope() { --t_guardDepth; }
    DepthScope(const DepthScope&) = delete;
    DepthScope& operator=(const DepthScope&) = delete;
};

}  // namespace

bool faultConversionAvailable() { return true; }

void runConvertingFaults(const std::function<void()>& fn) {
    std::call_once(g_installed, install);
    DepthScope depth;
    // Standard_ErrorHandler + setjmp: a fault in fn() lands here and is thrown as a C++ exception.
    OCC_CATCH_SIGNALS
    fn();
}

#else

bool faultConversionAvailable() { return false; }

void runConvertingFaults(const std::function<void()>& fn) { fn(); }

#endif

}  // namespace bendcore::kernel::detail
