// Оболочка HSF/W3D (ресурс DWFx): версия в заголовке, блоки 'I' (флаги) и ';' (комментарии),
// затем один zlib-поток, начинающийся с 'Z' и заканчивающийся байтом 'z' (TKE_Stop_Compression).
// Разбор только оболочки; опкоды внутри потока разбираются отдельно. Логика повторяет
// src/import/dwfx/hsf-envelope.mjs из BendPipe-Pro (decodeHsfEnvelope).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bendcore::dwfx::hsf {

struct FileInfo {
    std::size_t source_offset = 0;
    std::uint32_t flags = 0;
};

struct Comment {
    std::size_t source_offset = 0;
    std::string text;
};

struct Compression {
    std::size_t opcode_offset = 0;        // позиция байта 'Z'
    std::size_t compressed_offset = 0;    // начало zlib-потока
    std::size_t compressed_input_bytes = 0;
    std::size_t decompressed_bytes = 0;
};

struct Envelope {
    std::string hsf_version;              // например "14.50"
    std::size_t header_end_offset = 0;    // позиция терминатора заголовка (0 или перевод строки)
    std::vector<FileInfo> file_info;
    std::vector<Comment> comments;
    Compression compression;
    std::vector<std::uint8_t> opcode_stream;  // распакованный поток опкодов, заканчивается на 'z'
};

// Исключение std::invalid_argument, если заголовок, блоки или поток повреждены.
Envelope decode_envelope(const std::vector<std::uint8_t>& bytes);

}  // namespace bendcore::dwfx::hsf
