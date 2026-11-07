#pragma once

// In-memory BRep serialization: binary (BinTools) and text (BRepTools).

#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/shape.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bendcore::kernel {

struct BrepWriteOptions {
    bool withTriangles = false;  // also store face triangulations / edge polygons
};

Result<std::vector<std::uint8_t>> writeBrepBinary(const Shape& shape, const BrepWriteOptions& options = {});
Result<Shape> readBrepBinary(const std::uint8_t* data, std::size_t size);
inline Result<Shape> readBrepBinary(const std::vector<std::uint8_t>& data) {
    return readBrepBinary(data.data(), data.size());
}

Result<std::string> writeBrepText(const Shape& shape, const BrepWriteOptions& options = {});
Result<Shape> readBrepText(std::string_view text);

}  // namespace bendcore::kernel
