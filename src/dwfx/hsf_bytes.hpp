// Внутренние помощники чтения little-endian полей с проверкой границ (только для src/dwfx).
// Ошибки повреждённого потока — std::invalid_argument, как в остальной части DWFx.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace bendcore::dwfx::hsf::bytes {

[[noreturn]] inline void fail(const std::string& message) { throw std::invalid_argument(message); }

struct View {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
};

// Проверка, что на позиции off есть len байт.
inline void need(const View& v, std::size_t off, std::size_t len, const char* what) {
    if (off > v.size || len > v.size - off) fail(std::string("truncated ") + what);
}

inline std::uint8_t u8(const View& v, std::size_t off, const char* what) {
    need(v, off, 1, what);
    return v.data[off];
}

inline std::uint16_t u16(const View& v, std::size_t off, const char* what) {
    need(v, off, 2, what);
    return static_cast<std::uint16_t>(v.data[off] | (v.data[off + 1] << 8));
}

inline std::uint32_t u32(const View& v, std::size_t off, const char* what) {
    need(v, off, 4, what);
    return static_cast<std::uint32_t>(v.data[off]) | (static_cast<std::uint32_t>(v.data[off + 1]) << 8) |
           (static_cast<std::uint32_t>(v.data[off + 2]) << 16) | (static_cast<std::uint32_t>(v.data[off + 3]) << 24);
}

inline std::int32_t i32(const View& v, std::size_t off, const char* what) {
    return static_cast<std::int32_t>(u32(v, off, what));
}

inline float f32(const View& v, std::size_t off, const char* what) {
    const std::uint32_t bits = u32(v, off, what);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

inline std::string ascii(const View& v, std::size_t begin, std::size_t end) {
    return std::string(reinterpret_cast<const char*>(v.data + begin), end - begin);
}

}  // namespace bendcore::dwfx::hsf::bytes
