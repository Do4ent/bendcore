// Чтение ZIP DWFx: самодостаточная проверка на архиве, собранном в памяти (хранение и deflate, CRC, ошибки),
// и необязательная проверка на настоящем файле, если задан BENDCORE_DWFX_SAMPLE (путь к source.dwfx).
#include "bendcore/dwfx/zip_reader.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include <zlib.h>

using namespace bendcore::dwfx;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static void put16(std::vector<std::uint8_t>& b, std::uint16_t v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}

static void put32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    put16(b, static_cast<std::uint16_t>(v));
    put16(b, static_cast<std::uint16_t>(v >> 16));
}

static std::vector<std::uint8_t> deflate_raw(const std::vector<std::uint8_t>& in) {
    z_stream zs{};
    deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    std::vector<std::uint8_t> out(deflateBound(&zs, in.size()));
    zs.next_in = const_cast<Bytef*>(in.data());
    zs.avail_in = static_cast<uInt>(in.size());
    zs.next_out = out.data();
    zs.avail_out = static_cast<uInt>(out.size());
    deflate(&zs, Z_FINISH);
    out.resize(zs.total_out);
    deflateEnd(&zs);
    return out;
}

struct Item {
    std::string name;
    std::vector<std::uint8_t> data;
    bool compress;
};

// Собирает ZIP: локальные заголовки, центральный каталог и запись конца каталога.
static std::vector<std::uint8_t> build_zip(const std::vector<Item>& items, bool corrupt_crc = false) {
    std::vector<std::uint8_t> zip, central;
    for (const auto& it : items) {
        const std::vector<std::uint8_t> packed = it.compress ? deflate_raw(it.data) : it.data;
        const std::uint16_t method = it.compress ? 8 : 0;
        std::uint32_t crc = static_cast<std::uint32_t>(crc32(0L, it.data.data(), static_cast<uInt>(it.data.size())));
        if (corrupt_crc) crc ^= 0x1u;
        const std::uint32_t offset = static_cast<std::uint32_t>(zip.size());
        put32(zip, 0x04034b50);
        put16(zip, 20);
        put16(zip, 0);
        put16(zip, method);
        put16(zip, 0);
        put16(zip, 0);
        put32(zip, crc);
        put32(zip, static_cast<std::uint32_t>(packed.size()));
        put32(zip, static_cast<std::uint32_t>(it.data.size()));
        put16(zip, static_cast<std::uint16_t>(it.name.size()));
        put16(zip, 0);
        zip.insert(zip.end(), it.name.begin(), it.name.end());
        zip.insert(zip.end(), packed.begin(), packed.end());

        put32(central, 0x02014b50);
        put16(central, 20);
        put16(central, 20);
        put16(central, 0);
        put16(central, method);
        put16(central, 0);
        put16(central, 0);
        put32(central, crc);
        put32(central, static_cast<std::uint32_t>(packed.size()));
        put32(central, static_cast<std::uint32_t>(it.data.size()));
        put16(central, static_cast<std::uint16_t>(it.name.size()));
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put32(central, 0);
        put32(central, offset);
        central.insert(central.end(), it.name.begin(), it.name.end());
    }
    const std::uint32_t dir_offset = static_cast<std::uint32_t>(zip.size());
    zip.insert(zip.end(), central.begin(), central.end());
    put32(zip, 0x06054b50);
    put16(zip, 0);
    put16(zip, 0);
    put16(zip, static_cast<std::uint16_t>(items.size()));
    put16(zip, static_cast<std::uint16_t>(items.size()));
    put32(zip, static_cast<std::uint32_t>(central.size()));
    put32(zip, dir_offset);
    put16(zip, 0);
    return zip;
}

static void check_synthetic() {
    std::string text;
    for (int i = 0; i < 200; ++i) text += "PATH 12.5 3.0 L" + std::to_string(i) + "\n";
    const std::vector<std::uint8_t> big(text.begin(), text.end());
    const std::vector<std::uint8_t> small = {'h', 'e', 'l', 'l', 'o'};
    const std::vector<std::uint8_t> zip = build_zip({{"Documents/hello.txt", small, false}, {"Pages/1.fpage", big, true}});

    ZipArchive archive(zip);
    check(archive.entries().size() == 2, "два входа в каталоге");
    const ZipEntry* hello = archive.find("Documents/hello.txt");
    const ZipEntry* page = archive.find("Pages/1.fpage");
    check(hello && page, "записи найдены по пути");
    if (hello && page) {
        check(archive.extract(*hello) == small, "хранимая запись распаковывается без изменений");
        const std::vector<std::uint8_t> got = archive.extract(*page);
        check(got == big, "deflate-запись распаковывается без изменений");
        check(page->method == 8 && page->uncompressed_size == big.size(), "метод и размер deflate-записи");
    }
    check(archive.find("missing") == nullptr, "несуществующий путь даёт nullptr");

    // Испорченная CRC-32 должна отвергаться при распаковке.
    const std::vector<std::uint8_t> bad = build_zip({{"a.bin", big, true}}, /*corrupt_crc=*/true);
    bool threw = false;
    try {
        ZipArchive broken(bad);
        broken.extract(broken.entries()[0]);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "неверная CRC-32 вызывает исключение");

    // Не ZIP и обрезанный архив — исключение в конструкторе.
    const std::vector<std::uint8_t> junk(64, 0x41);
    threw = false;
    try {
        ZipArchive x(junk);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "не ZIP — исключение");
    std::vector<std::uint8_t> cut(zip.begin(), zip.begin() + static_cast<std::ptrdiff_t>(zip.size() / 2));
    threw = false;
    try {
        ZipArchive y(cut);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "обрезанный архив — исключение");
}

// Настоящий файл из BendPipe-Pro: 30 записей, все распаковываются с проверкой CRC.
static void check_sample(const char* file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        std::printf("FAIL: не открыть %s\n", file);
        ++failures;
        return;
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ZipArchive archive(bytes);
    std::printf("образец: записей %zu\n", archive.entries().size());
    check(archive.entries().size() == 30, "образец 80004806: 30 записей");
    const ZipEntry* rels = archive.find("_rels/.rels");
    check(rels && rels->uncompressed_size == 807 && rels->compressed_size == 371 && rels->method == 8,
          "образец: _rels/.rels 807/371, deflate");
    std::size_t total = 0;
    for (const auto& e : archive.entries()) {
        if (e.directory) continue;
        total += archive.extract(e).size();
    }
    std::printf("образец: распаковано байт %zu\n", total);
    check(total > 0, "образец: содержимое распаковано");
}

int main() {
    check_synthetic();
    if (const char* sample = std::getenv("BENDCORE_DWFX_SAMPLE")) {
        check_sample(sample);
    } else {
        std::printf("пропуск образца: задайте BENDCORE_DWFX_SAMPLE\n");
    }
    if (failures == 0) std::printf("OK dwfx_zip\n");
    return failures == 0 ? 0 : 1;
}
