#include <bendcore/kernel/step_io.hpp>

#include "occt_failure.hpp"
#include "memory_stream.hpp"
#include "occt_fault_guard.hpp"
#include "step_preflight.hpp"

#include <exception>
#include <optional>
#include <sstream>

#include <DESTEP_Parameters.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <StepData_StepModel.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>
#include <UnitsMethods_LengthUnit.hxx>

namespace bendcore::kernel {

using detail::failureText;

namespace {

// Millimetres per unit of the bendcore model space.
constexpr double kSystemUnitMm = 1.0;

UnitsMethods_LengthUnit toOcctUnit(StepLengthUnit unit) {
    switch (unit) {
        case StepLengthUnit::Millimetre: return UnitsMethods_LengthUnit_Millimeter;
        case StepLengthUnit::Metre: return UnitsMethods_LengthUnit_Meter;
        case StepLengthUnit::Inch: return UnitsMethods_LengthUnit_Inch;
    }
    return UnitsMethods_LengthUnit_Millimeter;
}

}  // namespace

Result<Shape> readStep(std::string_view data, const StepReadOptions& options) {
    using R = Result<Shape>;
    if (data.empty()) {
        return R::failure(ErrorCode::ParseError, "STEP input is empty");
    }
    if (data.size() > options.maxInputBytes) {
        return R::failure(ErrorCode::InvalidInput, *detail::preflightStep(data, options.maxInputBytes));
    }
    if (auto invalid = detail::findInvalidStepCharacter(data)) {
        return R::failure(ErrorCode::ParseError, std::move(*invalid));
    }
    try {
        // Parsing and transfer run under the fault guard: corrupted entity references can leave
        // null handles that OCCT dereferences without a check (docs/kernel/OCCT_BUILD.md, раздел 6).
        std::optional<R> failure;
        TopoDS_Shape shape;
        detail::runConvertingFaults([&] {
            // Read in place, without a copy of the text (the input may be hundreds of MiB).
            detail::MemoryStreamBuf buffer(data.data(), data.size());
            std::istream in(&buffer);
            STEPControl_Reader reader;
            const IFSelect_ReturnStatus status = reader.ReadStream("memory.step", in);
            if (status != IFSelect_RetDone) {
                failure = R::failure(ErrorCode::ParseError, "STEP input could not be parsed (IFSelect status " +
                                                                std::to_string(status) + ")");
                return;
            }
            reader.SetSystemLengthUnit(kSystemUnitMm);
            if (reader.NbRootsForTransfer() <= 0) {
                failure = R::failure(ErrorCode::ParseError, "STEP input has no transferable shape roots");
                return;
            }
            const int transferred = reader.TransferRoots();
            if (transferred <= 0 || reader.NbShapes() <= 0) {
                failure = R::failure(ErrorCode::OperationFailed, "STEP roots produced no shape on transfer");
                return;
            }
            shape = reader.OneShape();
        });
        if (failure) {
            return std::move(*failure);
        }
        if (shape.IsNull()) {
            return R::failure(ErrorCode::OperationFailed, "STEP transfer returned a null shape");
        }
        if (!TopExp_Explorer(shape, TopAbs_VERTEX).More()) {
            // Every root failed to translate (OCCT records the failures and returns an empty
            // compound), e.g. after a reference to an entity of the wrong type.
            return R::failure(ErrorCode::OperationFailed, "STEP transfer produced an empty shape (no vertices)");
        }
        return Shape(shape);
    } catch (const Standard_Failure& failure) {
        return R::failure(detail::faultOr(failure, ErrorCode::ParseError), "STEP input is malformed: " + failureText(failure));
    } catch (const std::exception& e) {
        return R::failure(ErrorCode::ParseError, std::string("STEP input is malformed: ") + e.what());
    }
}

Result<std::string> writeStep(const Shape& shape, const StepWriteOptions& options) {
    using R = Result<std::string>;
    if (shape.isNull()) {
        return R::failure(ErrorCode::InvalidInput, "shape is null");
    }
    try {
        STEPControl_Writer writer;
        writer.Model()->SetLocalLengthUnit(kSystemUnitMm);
        DESTEP_Parameters params;
        params.WriteUnit = toOcctUnit(options.fileUnit);
        params.WriteSchema = DESTEP_Parameters::WriteMode_StepSchema_AP214IS;
        const IFSelect_ReturnStatus transferStatus = writer.Transfer(shape.occt(), STEPControl_AsIs, params);
        if (transferStatus != IFSelect_RetDone) {
            return R::failure(ErrorCode::OperationFailed,
                              "STEP transfer failed (IFSelect status " + std::to_string(transferStatus) + ")");
        }
        std::ostringstream out;
        const IFSelect_ReturnStatus writeStatus = writer.WriteStream(out);
        if (writeStatus != IFSelect_RetDone || !out) {
            return R::failure(ErrorCode::IoError,
                              "STEP write failed (IFSelect status " + std::to_string(writeStatus) + ")");
        }
        return out.str();
    } catch (const Standard_Failure& failure) {
        return R::failure(ErrorCode::OperationFailed, failureText(failure));
    } catch (const std::exception& e) {
        return R::failure(ErrorCode::OperationFailed, e.what());
    }
}

}  // namespace bendcore::kernel
