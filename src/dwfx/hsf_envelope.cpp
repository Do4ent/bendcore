// Оболочка HSF/W3D. Логика из src/import/dwfx/hsf-envelope.mjs (decodeHsfEnvelope).
// Отличие: zlib распаковывается нативно и завершается на Z_STREAM_END; хвостовые байты после потока
// игнорируются (как и в zlib), поэтому повтор без последнего нулевого байта из JS не нужен.
#include "bendcore/dwfx/hsf_envelope.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>

#include <zlib.h>

namespace bendcore::dwfx::hsf {
namespace {

[[noreturn]] void fail(const std::string& what) { throw std::invalid_argument("HSF: " + what); }

std::uint32_t u32le(const std::vector<std::uint8_t>& b, std::size_t at) {
    if (at + 4 > b.size()) fail("выход за конец данных");
    return static_cast<std::uint32_t>(b[at]) | (static_cast<std::uint32_t>(b[at + 1]) << 8) |
           (static_cast<std::uint32_t>(b[at + 2]) << 16) | (static_cast<std::uint32_t>(b[at + 3]) << 24);
}

// Версия из начала файла: ";; HSF V" + "<цифры>.<две цифры>". Возвращает пусто, если формат другой.
bool parse_version(const std::vector<std::uint8_t>& b, std::string& version, std::size_t& prefix_len) {
    static const char kPrefix[] = ";; HSF V";
    const std::size_t plen = std::strlen(kPrefix);
    if (b.size() < plen) return false;
    if (std::memcmp(b.data(), kPrefix, plen) != 0) return false;
    std::size_t i = plen;
    std::string ver;
    while (i < b.size() && std::isdigit(b[i])) ver.push_back(static_cast<char>(b[i++]));
    if (ver.empty() || i >= b.size() || b[i] != '.') return false;
    ver.push_back('.');
    ++i;
    for (int k = 0; k < 2; ++k) {
        if (i >= b.size() || !std::isdigit(b[i])) return false;
        ver.push_back(static_cast<char>(b[i++]));
    }
    version = ver;
    prefix_len = plen + (ver.size());  // позиция сразу после версии
    return true;
}

// Терминатор заголовка: первый 0 или перевод строки в пределах 64 байт от конца версии.
std::size_t find_header_terminator(const std::vector<std::uint8_t>& b, std::size_t start) {
    const std::size_t end = std::min(b.size(), start + 64);
    for (std::size_t i = start; i < end; ++i) {
        if (b[i] == 0 || b[i] == 10) return i;
    }
    return static_cast<std::size_t>(-1);
}

// Распаковка zlib-потока. Заканчивается на Z_STREAM_END; неполный поток — ошибка.
std::vector<std::uint8_t> inflate_zlib(const std::uint8_t* src, std::size_t n) {
    z_stream zs{};
    if (inflateInit(&zs) != Z_OK) fail("inflateInit");
    zs.next_in = const_cast<Bytef*>(src);
    zs.avail_in = static_cast<uInt>(n);
    std::vector<std::uint8_t> out(std::max<std::size_t>(n * 4, 1024));
    int rc = Z_OK;
    for (;;) {
        if (zs.total_out == out.size()) out.resize(out.size() * 2);
        zs.next_out = out.data() + zs.total_out;
        zs.avail_out = static_cast<uInt>(out.size() - zs.total_out);
        rc = inflate(&zs, Z_NO_FLUSH);
        if (rc == Z_STREAM_END) break;
        if (rc != Z_OK) {
            inflateEnd(&zs);
            fail("повреждён zlib-поток");
        }
        if (zs.avail_in == 0 && zs.avail_out != 0) {
            inflateEnd(&zs);
            fail("zlib-поток обрезан");
        }
    }
    out.resize(zs.total_out);
    inflateEnd(&zs);
    return out;
}

}  // namespace

Envelope decode_envelope(const std::vector<std::uint8_t>& bytes) {
    Envelope env;
    std::size_t prefix_len = 0;
    if (!parse_version(bytes, env.hsf_version, prefix_len)) fail("ресурс не начинается с заголовка HSF");

    const std::size_t term = find_header_terminator(bytes, prefix_len);
    if (term == static_cast<std::size_t>(-1)) fail("терминатор заголовка HSF не найден");
    env.header_end_offset = term;
    std::size_t offset = term + 1;

    while (offset < bytes.size()) {
        const std::uint8_t op = bytes[offset];
        if (op == 0x49) {  // 'I': флаги u32
            env.file_info.push_back(FileInfo{offset, u32le(bytes, offset + 1)});
            offset += 5;
            continue;
        }
        if (op == 0x3b) {  // ';': комментарий до перевода строки
            std::size_t nl = offset + 1;
            while (nl < bytes.size() && bytes[nl] != 10) ++nl;
            if (nl >= bytes.size()) fail("незавершённый комментарий HSF перед блоком сжатия");
            env.comments.push_back(Comment{offset, std::string(bytes.begin() + static_cast<std::ptrdiff_t>(offset + 1),
                                                               bytes.begin() + static_cast<std::ptrdiff_t>(nl))});
            offset = nl + 1;
            continue;
        }
        break;
    }

    if (offset >= bytes.size() || bytes[offset] != 0x5a) {  // 'Z': TKE_Start_Compression
        fail("ожидался TKE_Start_Compression на смещении " + std::to_string(offset));
    }
    env.compression.opcode_offset = offset;
    env.compression.compressed_offset = offset + 1;
    env.compression.compressed_input_bytes = bytes.size() - (offset + 1);

    env.opcode_stream = inflate_zlib(bytes.data() + offset + 1, bytes.size() - (offset + 1));
    env.compression.decompressed_bytes = env.opcode_stream.size();
    if (env.opcode_stream.empty() || env.opcode_stream.back() != 0x7a) {
        fail("в распакованном потоке нет TKE_Stop_Compression");
    }
    return env;
}

}  // namespace bendcore::dwfx::hsf
