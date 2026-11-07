#pragma once

// STEP exchange through memory buffers only (no file system). Shapes inside bendcore are
// always in millimetres; the file unit is converted on read and chosen on write.

#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/shape.hpp>

#include <string>
#include <string_view>

namespace bendcore::kernel {

enum class StepLengthUnit { Millimetre, Metre, Inch };

struct StepWriteOptions {
    StepLengthUnit fileUnit = StepLengthUnit::Millimetre;
};

// All transferable roots; several roots are returned as one compound.
Result<Shape> readStep(std::string_view data);

// AP214 IS, STEPControl_AsIs representation.
Result<std::string> writeStep(const Shape& shape, const StepWriteOptions& options = {});

}  // namespace bendcore::kernel
