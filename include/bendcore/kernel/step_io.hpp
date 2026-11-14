#pragma once

// STEP exchange through memory buffers only (no file system). Shapes inside bendcore are
// always in millimetres; the file unit is converted on read and chosen on write.

#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/shape.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace bendcore::kernel {

enum class StepLengthUnit { Millimetre, Metre, Inch };

struct StepWriteOptions {
    StepLengthUnit fileUnit = StepLengthUnit::Millimetre;
};

// Default limit of a STEP input: the reader needs about ten times the file size in memory
// (docs/qa/PERFORMANCE.md), and 256 MiB is the largest file that stays within the 4 GiB of a
// 32-bit WebAssembly module (docs/kernel/KERNEL_CORE.md, "Повреждённые файлы").
inline constexpr std::size_t kStepDefaultMaxInputBytes = std::size_t(256) << 20;

struct StepReadOptions {
    // Larger input is refused with InvalidInput before it is parsed.
    std::size_t maxInputBytes = kStepDefaultMaxInputBytes;
};

// All transferable roots; several roots are returned as one compound. Before OCCT parses the
// text, a size limit and a lexical check (characters no ISO 10303-21 token has outside strings)
// reject damaged files; OCCT faults while reading become ParseError (docs/kernel/KERNEL_CORE.md).
Result<Shape> readStep(std::string_view data, const StepReadOptions& options = {});

// AP214 IS, STEPControl_AsIs representation.
Result<std::string> writeStep(const Shape& shape, const StepWriteOptions& options = {});

}  // namespace bendcore::kernel
