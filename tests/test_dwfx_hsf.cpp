// Оболочка HSF/W3D: самодостаточная проверка на синтетическом ресурсе (zlib в памяти) и проверка
// на настоящем ресурсе из source.dwfx, если задан BENDCORE_DWFX_SAMPLE. Эталон — JS-версия из BendPipe-Pro.
#include "bendcore/dwfx/hsf_envelope.hpp"
#include "bendcore/dwfx/zip_reader.hpp"

#include <cstdio>
#include <cstdlib>
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

static std::vector<std::uint8_t> zlib_pack(const std::vector<std::uint8_t>& in) {
    uLongf n = compressBound(static_cast<uLong>(in.size()));
    std::vector<std::uint8_t> out(n);
    compress2(out.data(), &n, in.data(), static_cast<uLong>(in.size()), Z_BEST_COMPRESSION);
    out.resize(n);
    return out;
}

static bool throws_on(const std::vector<std::uint8_t>& bytes) {
    try {
        hsf::decode_envelope(bytes);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static void check_synthetic() {
    // Заголовок, один блок 'I' с флагами, один комментарий, затем 'Z' + zlib{ сегмент "a", 'z' }.
    std::vector<std::uint8_t> bytes;
    const std::string head = ";; HSF V14.50\n";
    bytes.insert(bytes.end(), head.begin(), head.end());
    bytes.push_back(0x49);  // 'I'
    for (std::uint8_t b : {0x0a, 0x00, 0x00, 0x00}) bytes.push_back(b);  // flags = 10
    const std::string comment = ";W3D V01.00\n";
    bytes.insert(bytes.end(), comment.begin(), comment.end());
    bytes.push_back(0x5a);  // 'Z'
    const std::vector<std::uint8_t> stream = {0x28, 0x01, 'a', 0x29, 0x7a};  // open "a", close, stop
    const std::vector<std::uint8_t> packed = zlib_pack(stream);
    bytes.insert(bytes.end(), packed.begin(), packed.end());

    const hsf::Envelope env = hsf::decode_envelope(bytes);
    check(env.hsf_version == "14.50", "синтетика: версия 14.50");
    check(env.header_end_offset == 13, "синтетика: терминатор заголовка на 13");
    check(env.file_info.size() == 1 && env.file_info[0].flags == 10, "синтетика: блок 'I' с флагами 10");
    check(env.comments.size() == 1 && env.comments[0].text == "W3D V01.00", "синтетика: комментарий");
    check(env.compression.opcode_offset == 13 + 1 + 5 + comment.size(), "синтетика: позиция 'Z'");
    check(env.opcode_stream == stream, "синтетика: поток опкодов распакован без изменений");
    check(env.compression.decompressed_bytes == stream.size(), "синтетика: размер распакованного потока");

    // Нарушения формата должны отвергаться.
    std::vector<std::uint8_t> no_version = bytes;
    no_version[3] = 'X';
    check(throws_on(no_version), "синтетика: без заголовка HSF — исключение");

    std::vector<std::uint8_t> no_z = bytes;
    no_z[13 + 1 + 5 + comment.size()] = 0x41;
    check(throws_on(no_z), "синтетика: без 'Z' — исключение");

    // Поток без TKE_Stop_Compression.
    const std::vector<std::uint8_t> unstopped = {0x28, 0x01, 'a', 0x29};
    std::vector<std::uint8_t> bad = bytes;
    bad.resize(13 + 1 + 5 + comment.size() + 1);
    const std::vector<std::uint8_t> p2 = zlib_pack(unstopped);
    bad.insert(bad.end(), p2.begin(), p2.end());
    check(throws_on(bad), "синтетика: поток без 'z' — исключение");
}

// Настоящий W3D из source.dwfx 80004806: значения совпадают с JS-версией (decodeHsfEnvelope).
static void check_sample(const char* file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        std::printf("FAIL: не открыть %s\n", file);
        ++failures;
        return;
    }
    std::vector<std::uint8_t> zipped((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ZipArchive archive(zipped);
    const ZipEntry* w3d = nullptr;
    for (const auto& e : archive.entries()) {
        const std::string& p = e.path;
        if (p.size() >= 4 && p.compare(p.size() - 4, 4, ".w3d") == 0) {
            w3d = &e;
            break;
        }
    }
    check(w3d != nullptr, "образец: есть ресурс .w3d");
    if (!w3d) return;
    const std::vector<std::uint8_t> resource = archive.extract(*w3d);
    check(resource.size() == 7514659, "образец: размер ресурса 7514659");

    const hsf::Envelope env = hsf::decode_envelope(resource);
    std::printf("образец: HSF %s, распаковано %zu байт, блоков I %zu, комментариев %zu\n", env.hsf_version.c_str(),
                env.compression.decompressed_bytes, env.file_info.size(), env.comments.size());
    check(env.hsf_version == "14.50", "образец: HSF 14.50");
    check(env.header_end_offset == 14, "образец: терминатор заголовка 14");
    check(env.compression.opcode_offset == 37 && env.compression.compressed_offset == 38, "образец: позиция 'Z' 37");
    check(env.compression.decompressed_bytes == 20552707, "образец: распаковано 20552707 байт");
    check(env.file_info.size() == 2 && env.file_info[0].flags == 39430 && env.file_info[0].source_offset == 15,
          "образец: блок I на 15 с флагами 39430");
    check(env.comments.size() == 1 && env.comments[0].text == "W3D V01.00", "образец: комментарий W3D V01.00");
    check(env.opcode_stream.size() == 20552707 && env.opcode_stream.back() == 0x7a, "образец: поток кончается на 'z'");
    check(env.opcode_stream[0] == 40 && env.opcode_stream[1] == 37 && env.opcode_stream[2] == 63,
          "образец: первые опкоды 40, 37, 63");
}

int main() {
    check_synthetic();
    if (const char* sample = std::getenv("BENDCORE_DWFX_SAMPLE")) {
        check_sample(sample);
    } else {
        std::printf("пропуск образца: задайте BENDCORE_DWFX_SAMPLE\n");
    }
    if (failures == 0) std::printf("OK dwfx_hsf\n");
    return failures == 0 ? 0 : 1;
}
