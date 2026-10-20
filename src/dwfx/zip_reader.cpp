// Чтение ZIP-контейнера DWFx. Логика повторяет src/import/dwfx/zip-directory.mjs и zip-entry-extractor.mjs
// из BendPipe-Pro: поиск EOCD с хвостовым комментарием до 64 КБ, центральный каталог, локальные заголовки.
#include "bendcore/dwfx/zip_reader.hpp"

#include <cstring>
#include <stdexcept>

#include <zlib.h>

namespace bendcore::dwfx {
namespace {

constexpr std::uint32_t kEocdSig = 0x06054b50;
constexpr std::uint32_t kCentralSig = 0x02014b50;
constexpr std::uint32_t kLocalSig = 0x04034b50;
constexpr std::size_t kEocdSize = 22;
constexpr std::size_t kCentralFixed = 46;
constexpr std::size_t kLocalFixed = 30;

[[noreturn]] void fail(const std::string& what) { throw std::invalid_argument("DWFx ZIP: " + what); }

std::uint16_t u16(const std::vector<std::uint8_t>& b, std::size_t at) {
    if (at + 2 > b.size()) fail("выход за конец данных");
    return static_cast<std::uint16_t>(b[at] | (b[at + 1] << 8));
}

std::uint32_t u32(const std::vector<std::uint8_t>& b, std::size_t at) {
    if (at + 4 > b.size()) fail("выход за конец данных");
    return static_cast<std::uint32_t>(b[at]) | (static_cast<std::uint32_t>(b[at + 1]) << 8) |
           (static_cast<std::uint32_t>(b[at + 2]) << 16) | (static_cast<std::uint32_t>(b[at + 3]) << 24);
}

// Поиск EOCD с конца: сигнатура может быть перед комментарием длиной до 65535 байт.
std::size_t find_eocd(const std::vector<std::uint8_t>& b) {
    if (b.size() < kEocdSize) fail("файл короче записи конца каталога");
    const std::size_t lowest = b.size() > kEocdSize + 0xffff ? b.size() - kEocdSize - 0xffff : 0;
    for (std::size_t i = b.size() - kEocdSize + 1; i-- > lowest;) {
        if (u32(b, i) == kEocdSig) return i;
    }
    fail("запись конца каталога не найдена");
}

}  // namespace

ZipArchive::ZipArchive(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {
    const std::size_t eocd = find_eocd(bytes_);
    const std::uint16_t count = u16(bytes_, eocd + 10);
    const std::uint32_t dir_size = u32(bytes_, eocd + 12);
    const std::uint32_t dir_offset = u32(bytes_, eocd + 16);
    if (count == 0xffff || dir_size == 0xffffffffu || dir_offset == 0xffffffffu) fail("ZIP64 не поддерживается");
    if (static_cast<std::uint64_t>(dir_offset) + dir_size > bytes_.size()) fail("центральный каталог за концом файла");

    std::size_t p = dir_offset;
    entries_.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i) {
        if (u32(bytes_, p) != kCentralSig) fail("неверная сигнатура записи каталога");
        ZipEntry e;
        const std::uint16_t flags = u16(bytes_, p + 8);
        e.method = u16(bytes_, p + 10);
        e.crc32 = u32(bytes_, p + 16);
        e.compressed_size = u32(bytes_, p + 20);
        e.uncompressed_size = u32(bytes_, p + 24);
        const std::uint16_t name_len = u16(bytes_, p + 28);
        const std::uint16_t extra_len = u16(bytes_, p + 30);
        const std::uint16_t comment_len = u16(bytes_, p + 32);
        e.local_header_offset = u32(bytes_, p + 42);
        const std::size_t name_at = p + kCentralFixed;
        if (name_at + name_len > bytes_.size()) fail("имя записи за концом файла");
        e.path.assign(reinterpret_cast<const char*>(bytes_.data() + name_at), name_len);
        e.encrypted = (flags & 0x1) != 0;
        e.directory = !e.path.empty() && e.path.back() == '/';
        entries_.push_back(std::move(e));
        p = name_at + name_len + extra_len + comment_len;
    }
}

const ZipEntry* ZipArchive::find(const std::string& path) const {
    for (const auto& e : entries_) {
        if (e.path == path) return &e;
    }
    return nullptr;
}

std::vector<std::uint8_t> ZipArchive::extract(const ZipEntry& entry) const {
    if (entry.encrypted) fail("запись '" + entry.path + "' зашифрована");
    if (entry.method != 0 && entry.method != 8) fail("метод сжатия " + std::to_string(entry.method) + " не поддерживается");

    const std::size_t lh = entry.local_header_offset;
    if (u32(bytes_, lh) != kLocalSig) fail("неверная локальная сигнатура для '" + entry.path + "'");
    const std::size_t data_at = lh + kLocalFixed + u16(bytes_, lh + 26) + u16(bytes_, lh + 28);
    if (data_at + entry.compressed_size > bytes_.size()) fail("данные записи '" + entry.path + "' за концом файла");
    const std::uint8_t* src = bytes_.data() + data_at;

    std::vector<std::uint8_t> out;
    if (entry.method == 0) {
        if (entry.compressed_size != entry.uncompressed_size) fail("размеры хранимой записи не совпадают");
        out.assign(src, src + entry.compressed_size);
    } else {
        out.resize(entry.uncompressed_size);
        z_stream zs{};
        if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) fail("inflateInit2");
        zs.next_in = const_cast<Bytef*>(src);
        zs.avail_in = entry.compressed_size;
        zs.next_out = out.data();
        zs.avail_out = static_cast<uInt>(out.size());
        const int rc = inflate(&zs, Z_FINISH);
        const uLong produced = zs.total_out;
        inflateEnd(&zs);
        if (rc != Z_STREAM_END || produced != entry.uncompressed_size) {
            fail("распаковка записи '" + entry.path + "' не дала ожидаемый размер");
        }
    }

    const uLong crc = crc32(0L, out.data(), static_cast<uInt>(out.size()));
    if (static_cast<std::uint32_t>(crc) != entry.crc32) fail("CRC-32 записи '" + entry.path + "' не совпадает");
    return out;
}

}  // namespace bendcore::dwfx
