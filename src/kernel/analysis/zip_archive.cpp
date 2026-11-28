// ZIP writer and reader of the 3MF export (docs/kernel/ANALYSIS.md, "ZIP и zlib").
//
// CRC-32 is computed here (table-driven, the ZIP polynomial) in every build. Deflate and
// inflate use zlib where the build links it (BENDCORE_ANALYSIS_HAS_ZLIB, native builds);
// without zlib (WebAssembly) entries are stored and a deflated entry cannot be read.

#include <bendcore/kernel/mesh_export.hpp>

#include <array>
#include <bit>
#include <cstring>

#if BENDCORE_ANALYSIS_HAS_ZLIB
#include <zlib.h>
#endif

static_assert(std::endian::native == std::endian::little, "ZIP headers are copied in little-endian byte order");

namespace bendcore::kernel {

namespace {

// Table of the reflected CRC-32 (polynomial 0x04C11DB7 bit-reversed = 0xEDB88320): entry n
// is the remainder of the byte n shifted through eight division steps.
constexpr std::array<std::uint32_t, 256> makeCrcTable() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t n = 0; n < 256; ++n) {
        std::uint32_t c = n;
        for (int k = 0; k < 8; ++k) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        table[n] = c;
    }
    return table;
}
constexpr std::array<std::uint32_t, 256> kCrcTable = makeCrcTable();

void appendLe32(std::string& out, std::uint32_t v) {
    char b[4];
    std::memcpy(b, &v, 4);
    out.append(b, 4);
}
void appendLe16(std::string& out, std::uint16_t v) {
    char b[2];
    std::memcpy(b, &v, 2);
    out.append(b, 2);
}
std::uint32_t readLe32(std::string_view s, std::size_t at) {
    std::uint32_t v;
    std::memcpy(&v, s.data() + at, 4);
    return v;
}
std::uint16_t readLe16(std::string_view s, std::size_t at) {
    std::uint16_t v;
    std::memcpy(&v, s.data() + at, 2);
    return v;
}

#if BENDCORE_ANALYSIS_HAS_ZLIB
Result<std::string> deflateRaw(std::string_view data) {
    z_stream zs{};
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return Result<std::string>::failure(ErrorCode::OperationFailed, "zlib deflateInit2 failed");
    std::string out(deflateBound(&zs, static_cast<uLong>(data.size())), '\0');
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());
    zs.next_out = reinterpret_cast<Bytef*>(out.data());
    zs.avail_out = static_cast<uInt>(out.size());
    const int rc = deflate(&zs, Z_FINISH);
    const auto produced = zs.total_out;
    deflateEnd(&zs);
    if (rc != Z_STREAM_END) return Result<std::string>::failure(ErrorCode::OperationFailed, "zlib deflate failed");
    out.resize(produced);
    return out;
}

Result<std::string> inflateRaw(std::string_view data, std::size_t expectedSize) {
    z_stream zs{};
    if (inflateInit2(&zs, -MAX_WBITS) != Z_OK)
        return Result<std::string>::failure(ErrorCode::OperationFailed, "zlib inflateInit2 failed");
    std::string out(expectedSize, '\0');
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());
    zs.next_out = reinterpret_cast<Bytef*>(out.data());
    zs.avail_out = static_cast<uInt>(out.size());
    const int rc = inflate(&zs, Z_FINISH);
    const auto produced = zs.total_out;
    inflateEnd(&zs);
    if (rc != Z_STREAM_END || produced != expectedSize)
        return Result<std::string>::failure(ErrorCode::ParseError, "zip: corrupt deflate stream");
    return out;
}
#else
Result<std::string> deflateRaw(std::string_view) {
    return Result<std::string>::failure(ErrorCode::Unsupported, "zip: deflate needs zlib, which this build does not link");
}
Result<std::string> inflateRaw(std::string_view, std::size_t) {
    return Result<std::string>::failure(ErrorCode::Unsupported,
                                        "zip: deflated entry cannot be read, this build does not link zlib");
}
#endif

}  // namespace

bool zipDeflateAvailable() noexcept { return BENDCORE_ANALYSIS_HAS_ZLIB != 0; }

ZipMethod zipDefaultMethod() noexcept { return zipDeflateAvailable() ? ZipMethod::Deflate : ZipMethod::Stored; }

std::uint32_t crc32(std::string_view data, std::uint32_t crc) noexcept {
    std::uint32_t c = crc ^ 0xFFFFFFFFu;
    for (const char ch : data) c = kCrcTable[(c ^ static_cast<unsigned char>(ch)) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

Result<std::string> writeZip(const std::vector<ZipEntry>& entries, ZipMethod method) {
    constexpr std::uint16_t kDosTime = 0;
    constexpr std::uint16_t kDosDate = (0 << 9) | (1 << 5) | 1;
    // Version needed to extract: 1.0 for stored entries, 2.0 for deflate (APPNOTE 4.4.3).
    const std::uint16_t version = method == ZipMethod::Deflate ? 20 : 10;
    const auto methodCode = static_cast<std::uint16_t>(method);
    std::string out;
    std::string central;
    for (const ZipEntry& e : entries) {
        if (e.name.size() > 0xFFFF) return Result<std::string>::failure(ErrorCode::InvalidInput, "zip: entry name too long");
        std::string packedStorage;
        std::string_view packed = e.data;
        if (method == ZipMethod::Deflate) {
            auto deflated = deflateRaw(e.data);
            if (!deflated) return deflated.error();
            packedStorage = std::move(deflated).value();
            packed = packedStorage;
        }
        if (e.data.size() > 0xFFFFFFFEull || packed.size() > 0xFFFFFFFEull)
            return Result<std::string>::failure(ErrorCode::Unsupported, "zip: entry " + e.name + " needs ZIP64");
        const std::uint32_t crc = crc32(e.data);
        const auto offset = static_cast<std::uint32_t>(out.size());
        appendLe32(out, 0x04034b50);
        appendLe16(out, version);
        appendLe16(out, 0);  // flags
        appendLe16(out, methodCode);
        appendLe16(out, kDosTime);
        appendLe16(out, kDosDate);
        appendLe32(out, crc);
        appendLe32(out, static_cast<std::uint32_t>(packed.size()));
        appendLe32(out, static_cast<std::uint32_t>(e.data.size()));
        appendLe16(out, static_cast<std::uint16_t>(e.name.size()));
        appendLe16(out, 0);
        out += e.name;
        out += packed;

        appendLe32(central, 0x02014b50);
        appendLe16(central, 20);  // made by: MS-DOS / FAT, 2.0
        appendLe16(central, version);
        appendLe16(central, 0);
        appendLe16(central, methodCode);
        appendLe16(central, kDosTime);
        appendLe16(central, kDosDate);
        appendLe32(central, crc);
        appendLe32(central, static_cast<std::uint32_t>(packed.size()));
        appendLe32(central, static_cast<std::uint32_t>(e.data.size()));
        appendLe16(central, static_cast<std::uint16_t>(e.name.size()));
        appendLe16(central, 0);  // extra
        appendLe16(central, 0);  // comment
        appendLe16(central, 0);  // disk
        appendLe16(central, 0);  // internal attributes
        appendLe32(central, 0);  // external attributes
        appendLe32(central, offset);
        central += e.name;
        if (out.size() > 0xFFFFFFFEull)
            return Result<std::string>::failure(ErrorCode::Unsupported, "zip: archive needs ZIP64");
    }
    if (out.size() + central.size() > 0xFFFFFFFFull || entries.size() > 0xFFFF)
        return Result<std::string>::failure(ErrorCode::Unsupported, "zip: archive needs ZIP64");
    const auto centralOffset = static_cast<std::uint32_t>(out.size());
    out += central;
    appendLe32(out, 0x06054b50);
    appendLe16(out, 0);
    appendLe16(out, 0);
    appendLe16(out, static_cast<std::uint16_t>(entries.size()));
    appendLe16(out, static_cast<std::uint16_t>(entries.size()));
    appendLe32(out, static_cast<std::uint32_t>(central.size()));
    appendLe32(out, centralOffset);
    appendLe16(out, 0);
    return out;
}

Result<std::vector<ZipEntry>> readZip(std::string_view z) {
    using R = Result<std::vector<ZipEntry>>;
    if (z.size() < 22) return R::failure(ErrorCode::ParseError, "zip: too short");
    std::size_t eocd = std::string_view::npos;
    const std::size_t lowest = z.size() >= 22 + 0xFFFF ? z.size() - 22 - 0xFFFF : 0;
    for (std::size_t at = z.size() - 22 + 1; at-- > lowest;) {
        if (readLe32(z, at) == 0x06054b50) {
            eocd = at;
            break;
        }
    }
    if (eocd == std::string_view::npos) return R::failure(ErrorCode::ParseError, "zip: end of central directory not found");
    const std::uint16_t count = readLe16(z, eocd + 10);
    const std::uint32_t cdSize = readLe32(z, eocd + 12);
    std::size_t cd = readLe32(z, eocd + 16);
    if (cd + cdSize > eocd) return R::failure(ErrorCode::ParseError, "zip: central directory out of range");
    std::vector<ZipEntry> entries;
    for (std::uint16_t i = 0; i < count; ++i) {
        if (cd + 46 > eocd || readLe32(z, cd) != 0x02014b50)
            return R::failure(ErrorCode::ParseError, "zip: bad central directory header");
        const std::uint16_t flags = readLe16(z, cd + 8);
        const std::uint16_t method = readLe16(z, cd + 10);
        const std::uint32_t crc = readLe32(z, cd + 16);
        const std::uint32_t csize = readLe32(z, cd + 20);
        const std::uint32_t usize = readLe32(z, cd + 24);
        const std::uint16_t nameLen = readLe16(z, cd + 28);
        const std::uint16_t extraLen = readLe16(z, cd + 30);
        const std::uint16_t commentLen = readLe16(z, cd + 32);
        const std::uint32_t local = readLe32(z, cd + 42);
        if (cd + 46 + nameLen > eocd) return R::failure(ErrorCode::ParseError, "zip: name out of range");
        ZipEntry e;
        e.name = std::string(z.substr(cd + 46, nameLen));
        if (flags & 0x1) return R::failure(ErrorCode::Unsupported, "zip: encrypted entry " + e.name);
        if (csize == 0xFFFFFFFFu || usize == 0xFFFFFFFFu || local == 0xFFFFFFFFu)
            return R::failure(ErrorCode::Unsupported, "zip: ZIP64 entry " + e.name);
        if (std::size_t(local) + 30 > z.size() || readLe32(z, local) != 0x04034b50)
            return R::failure(ErrorCode::ParseError, "zip: bad local header of " + e.name);
        const std::size_t dataAt = std::size_t(local) + 30 + readLe16(z, local + 26) + readLe16(z, local + 28);
        if (dataAt + csize > z.size()) return R::failure(ErrorCode::ParseError, "zip: data of " + e.name + " out of range");
        const std::string_view packed = z.substr(dataAt, csize);
        if (method == 0) {
            if (csize != usize) return R::failure(ErrorCode::ParseError, "zip: stored size mismatch in " + e.name);
            e.data = std::string(packed);
        } else if (method == 8) {
            auto data = inflateRaw(packed, usize);
            if (!data) return Error{data.error().code, data.error().message + " (" + e.name + ")"};
            e.data = std::move(data).value();
        } else {
            return R::failure(ErrorCode::Unsupported, "zip: compression method " + std::to_string(method) + " in " + e.name);
        }
        if (crc32(e.data) != crc) return R::failure(ErrorCode::ParseError, "zip: CRC-32 mismatch in " + e.name);
        entries.push_back(std::move(e));
        cd += 46 + nameLen + extraLen + commentLen;
    }
    return entries;
}

}  // namespace bendcore::kernel
