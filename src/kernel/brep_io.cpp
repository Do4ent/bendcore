#include <bendcore/kernel/brep_io.hpp>

#include "memory_stream.hpp"
#include "occt_failure.hpp"

#include <exception>
#include <istream>
#include <sstream>

#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BinTools.hxx>
#include <BinTools_FormatVersion.hxx>
#include <Standard_Failure.hxx>
#include <TopTools_FormatVersion.hxx>

namespace bendcore::kernel {

using detail::failureText;

namespace {

template <typename ReadFn>
Result<Shape> readFromMemory(const char* data, std::size_t size, const char* format, ReadFn read) {
    using R = Result<Shape>;
    const std::string prefix = std::string(format) + " BRep input";
    if (data == nullptr || size == 0) {
        return R::failure(ErrorCode::ParseError, prefix + " is empty");
    }
    try {
        detail::MemoryStreamBuf buffer(data, size);
        std::istream in(&buffer);
        detail::throwOnFailedExtraction(in);
        TopoDS_Shape result;
        read(result, in);
        if (result.IsNull()) {
            return R::failure(ErrorCode::ParseError, prefix + " holds no shape");
        }
        return Shape(result);
    } catch (const std::ios_base::failure&) {
        return R::failure(ErrorCode::ParseError, prefix + " is truncated or malformed: reading stopped at "
                                                     "a value that could not be extracted");
    } catch (const Standard_Failure& failure) {
        return R::failure(ErrorCode::ParseError, prefix + " is malformed: " + failureText(failure));
    } catch (const std::exception& e) {
        return R::failure(ErrorCode::ParseError, prefix + " is malformed: " + e.what());
    }
}

}  // namespace

Result<std::vector<std::uint8_t>> writeBrepBinary(const Shape& shape, const BrepWriteOptions& options) {
    using R = Result<std::vector<std::uint8_t>>;
    if (shape.isNull()) {
        return R::failure(ErrorCode::InvalidInput, "shape is null");
    }
    try {
        std::ostringstream out(std::ios::out | std::ios::binary);
        BinTools::Write(shape.occt(), out, options.withTriangles, false, BinTools_FormatVersion_CURRENT);
        if (!out) {
            return R::failure(ErrorCode::IoError, "BinTools::Write failed to write the stream");
        }
        const std::string bytes = out.str();
        return std::vector<std::uint8_t>(bytes.begin(), bytes.end());
    } catch (const Standard_Failure& failure) {
        return R::failure(ErrorCode::OperationFailed, failureText(failure));
    } catch (const std::exception& e) {
        return R::failure(ErrorCode::OperationFailed, e.what());
    }
}

Result<Shape> readBrepBinary(const std::uint8_t* data, std::size_t size) {
    return readFromMemory(reinterpret_cast<const char*>(data), size, "binary",
                          [](TopoDS_Shape& shape, std::istream& in) { BinTools::Read(shape, in); });
}

Result<std::string> writeBrepText(const Shape& shape, const BrepWriteOptions& options) {
    using R = Result<std::string>;
    if (shape.isNull()) {
        return R::failure(ErrorCode::InvalidInput, "shape is null");
    }
    try {
        std::ostringstream out;
        BRepTools::Write(shape.occt(), out, options.withTriangles, false, TopTools_FormatVersion_CURRENT);
        if (!out) {
            return R::failure(ErrorCode::IoError, "BRepTools::Write failed to write the stream");
        }
        return out.str();
    } catch (const Standard_Failure& failure) {
        return R::failure(ErrorCode::OperationFailed, failureText(failure));
    } catch (const std::exception& e) {
        return R::failure(ErrorCode::OperationFailed, e.what());
    }
}

Result<Shape> readBrepText(std::string_view text) {
    return readFromMemory(text.data(), text.size(), "text", [](TopoDS_Shape& shape, std::istream& in) {
        BRep_Builder builder;
        BRepTools::Read(shape, in, builder);
    });
}

}  // namespace bendcore::kernel
