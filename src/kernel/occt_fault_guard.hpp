#pragma once

// Private helper of bendcore_kernel: runs an OCCT reader so that a hardware fault inside it
// (SIGSEGV, SIGBUS, SIGFPE on the calling thread) becomes a Standard_Failure exception instead of
// killing the process (docs/kernel/KERNEL_CORE.md, "Повреждённые файлы").
//
// Mechanism (native Linux/macOS): the same one OCCT uses in Draw and inside its own translators.
// OCCT is built with OCC_CONVERT_SIGNALS; its readers contain OCC_CATCH_SIGNALS points
// (Standard_ErrorHandler + setjmp). runConvertingFaults() adds one more such point around the
// call and installs, once per process, a SIGSEGV/SIGBUS/SIGFPE handler that longjmps to the
// innermost point (Standard_ErrorHandler::Abort) — but only while the faulting thread is inside
// runConvertingFaults(). A fault anywhere else is passed to the previously installed handler
// (ASan, libFuzzer, a crash reporter) or to the default action, so ordinary crashes keep their
// core dumps.
//
// Costs and limits: the longjmp skips destructors of the frames between the fault and the
// catch point, so the objects of the aborted read leak; a fault inside a section that holds a
// lock or corrupts the heap is not recoverable (heap corruption usually ends in SIGABRT from
// the allocator, which is not converted). Use only around readers of untrusted data whose
// state is local to the call. Stack overflow is not converted (no alternate signal stack).
//
// WebAssembly has no signals: the call runs as is; a null access reads linear memory at 0 and
// an out-of-bounds access is a trap (JS RuntimeError) that leaves the module unusable. There,
// isolation is the Web Worker that runs the import (docs/kernel/OCCT_BUILD.md, раздел 6).

#include <functional>
#include <optional>
#include <utility>

namespace bendcore::kernel::detail {

// True when this build converts faults (native POSIX); false in WASM and on Windows.
bool faultConversionAvailable();

// Calls fn(); a SIGSEGV/SIGBUS/SIGFPE raised by fn() on this thread is rethrown as OSD_SIGSEGV /
// OSD_SIGBUS / Standard_NumericError (all derive from Standard_Failure). Exceptions of fn()
// propagate unchanged. Re-entrant.
void runConvertingFaults(const std::function<void()>& fn);

// The same for a callable that returns a value (for example Result<T>).
template <typename R, typename Fn>
R callConvertingFaults(Fn&& fn) {
    std::optional<R> out;
    runConvertingFaults([&] { out.emplace(std::forward<Fn>(fn)()); });
    return std::move(*out);
}

}  // namespace bendcore::kernel::detail
