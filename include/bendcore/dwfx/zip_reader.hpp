// Чтение ZIP-контейнера DWFx (OPC): центральный каталог и распаковка записей (хранение и deflate, CRC-32).
// Шифрование и ZIP64 не поддерживаются. Реализация: src/dwfx/zip_reader.cpp, нужен zlib.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bendcore::dwfx {

struct ZipEntry {
    std::string path;                    // имя записи как в архиве (UTF-8 байты), разделитель '/'
    std::uint16_t method = 0;            // 0 — хранится, 8 — deflate
    std::uint32_t crc32 = 0;
    std::uint32_t compressed_size = 0;
    std::uint32_t uncompressed_size = 0;
    std::uint32_t local_header_offset = 0;
    bool directory = false;              // имя оканчивается на '/'
    bool encrypted = false;
};

class ZipArchive {
public:
    // Разбирает центральный каталог. Исключение std::invalid_argument, если архив не читается.
    explicit ZipArchive(std::vector<std::uint8_t> bytes);

    const std::vector<ZipEntry>& entries() const { return entries_; }

    // Запись по точному пути или nullptr.
    const ZipEntry* find(const std::string& path) const;

    // Распаковывает запись и проверяет размер и CRC-32. Исключение std::invalid_argument при ошибке.
    std::vector<std::uint8_t> extract(const ZipEntry& entry) const;

private:
    std::vector<std::uint8_t> bytes_;
    std::vector<ZipEntry> entries_;
};

}  // namespace bendcore::dwfx
