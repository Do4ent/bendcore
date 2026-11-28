#include <bendcore/drawing_v2/common.hpp>

#include <string>

namespace bendcore::drawing_v2 {

Result<std::u32string> decodeUtf8(std::string_view text) {
    std::u32string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        const auto b0 = static_cast<unsigned char>(text[i]);
        int len = 0;
        char32_t cp = 0;
        if (b0 < 0x80) {
            len = 1;
            cp = b0;
        } else if ((b0 & 0xE0) == 0xC0) {
            len = 2;
            cp = b0 & 0x1F;
        } else if ((b0 & 0xF0) == 0xE0) {
            len = 3;
            cp = b0 & 0x0F;
        } else if ((b0 & 0xF8) == 0xF0) {
            len = 4;
            cp = b0 & 0x07;
        } else {
            return Error{ErrorCode::InvalidInput,
                         "invalid UTF-8 lead byte at offset " + std::to_string(i)};
        }
        if (i + std::size_t(len) > text.size()) {
            return Error{ErrorCode::InvalidInput, "truncated UTF-8 sequence"};
        }
        for (int k = 1; k < len; ++k) {
            const auto b = static_cast<unsigned char>(text[i + std::size_t(k)]);
            if ((b & 0xC0) != 0x80) {
                return Error{ErrorCode::InvalidInput,
                             "invalid UTF-8 continuation byte at offset " +
                                 std::to_string(i + std::size_t(k))};
            }
            cp = (cp << 6) | (b & 0x3F);
        }
        static constexpr char32_t kMin[5] = {0, 0, 0x80, 0x800, 0x10000};
        if (cp < kMin[len] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            return Error{ErrorCode::InvalidInput,
                         "invalid UTF-8 code point at offset " + std::to_string(i)};
        }
        out.push_back(cp);
        i += std::size_t(len);
    }
    return out;
}

std::string encodeUtf8(std::u32string_view text) {
    std::string out;
    for (char32_t cp : text) {
        if (cp < 0x80) {
            out.push_back(char(cp));
        } else if (cp < 0x800) {
            out.push_back(char(0xC0 | (cp >> 6)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(char(0xE0 | (cp >> 12)));
            out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(char(0xF0 | (cp >> 18)));
            out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

}  // namespace bendcore::drawing_v2
