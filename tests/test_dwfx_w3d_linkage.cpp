// W3D-связи: экземпляры из content.xml/sections → теги (tag_index) → сегменты → Include Library → сетки.
// Проверка: покрытие тегов, принадлежность сеток, сирот (orphan meshes).
// Запуск: BENDCORE_DWFX_SAMPLE=/path/to/sample.dwfx test_dwfx_w3d_linkage
#include "bendcore/dwfx/dwfx_structure.hpp"
#include "bendcore/dwfx/hsf_envelope.hpp"
#include "bendcore/dwfx/hsf_opcodes.hpp"
#include "bendcore/dwfx/zip_reader.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace bendcore::dwfx;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static std::string find_entry(const ZipArchive& zip, const std::string& suffix) {
    for (const auto& e : zip.entries()) {
        if (e.path.size() >= suffix.size() && e.path.compare(e.path.size() - suffix.size(), suffix.size(), suffix) == 0) {
            const auto bytes = zip.extract(e);
            return std::string(bytes.begin(), bytes.end());
        }
    }
    return {};
}

static std::vector<std::uint8_t> find_entry_binary(const ZipArchive& zip, const std::string& suffix) {
    for (const auto& e : zip.entries()) {
        if (e.path.size() >= suffix.size() && e.path.compare(e.path.size() - suffix.size(), suffix.size(), suffix) == 0) {
            return zip.extract(e);
        }
    }
    return {};
}

// Карта: tag_index → имя сегмента (из разбора потока опкодов).
using TagMap = std::unordered_map<std::int64_t, std::string>;

// Карта: имя сегмента Include → мес.нахождение в дереве (для отладки).
using IncludeMap = std::unordered_map<std::string, std::string>;

// Счётчики владения.
struct Ownership {
    std::size_t total_instances = 0;
    std::size_t instances_with_include = 0;
    std::size_t instances_without_include = 0;
    std::size_t total_meshes = 0;
    std::size_t claimed_meshes = 0;
    std::size_t unclaimed_meshes = 0;
    std::size_t tags_seen = 0;
    std::size_t segment_opens = 0;
    std::set<std::string> include_libraries;
};

// Контекст разбора: отслеживание вложенности сегментов и их владения.
struct SegmentContext {
    std::vector<std::string> segment_stack;  // стек открытых сегментов
    std::unordered_set<std::int64_t> meshes_in_current;  // сетки в текущем наборе сегментов
    std::unordered_map<std::string, std::unordered_set<std::int64_t>> library_meshes;  // Include → сетки
};

static void check_sample(const char* path) {
    std::printf("W3D-связи: %s\n", path);
    const std::vector<std::uint8_t> zipped = read_file(path);
    ZipArchive zip(zipped);

    // Структура: content.xml, разделы.
    const std::string content_xml = find_entry(zip, ".content.xml");
    check(!content_xml.empty(), "W3D: content.xml найден");
    if (content_xml.empty()) return;

    const DwfContent content = parse_dwf_content(content_xml);
    std::printf("  объектов в content.xml: %zu\n", content.objects.size());

    // Разделы: экземпляры и виды.
    DwfSection section_instances;
    for (const auto& e : zip.entries()) {
        if (e.path.find("/sections/") == std::string::npos) continue;
        const auto bytes = zip.extract(e);
        const std::string text(bytes.begin(), bytes.end());
        if (text.find("<dwf:Instances>") != std::string::npos) {
            section_instances = parse_dwf_section(text);
            break;
        }
    }
    std::printf("  экземпляров в разделах: %zu\n", section_instances.instances.size());

    // W3D: разбор потока опкодов.
    const std::vector<std::uint8_t> w3d = find_entry_binary(zip, ".w3d");
    check(!w3d.empty(), "W3D: ресурс .w3d найден");
    if (w3d.empty()) return;

    const hsf::Envelope env = hsf::decode_envelope(w3d);
    std::printf("  HSF версия: %s\n", env.hsf_version.c_str());

    // Первый проход: собрать тег_индекс → имя сегмента и счёт сеток.
    TagMap tag_map;
    Ownership own;
    SegmentContext ctx;
    std::int64_t mesh_counter = 0;

    hsf::decode_opcodes(env.opcode_stream, env.hsf_version, [&](const hsf::Entity& e) {
        if (e.kind == hsf::EntityKind::Tag) {
            tag_map[e.tag_index] = e.name;
            own.tags_seen += 1;
        }
        if (e.kind == hsf::EntityKind::Segment) {
            if (e.action == hsf::SegmentAction::Open) {
                own.segment_opens += 1;
                ctx.segment_stack.push_back(e.name);
            } else if (e.action == hsf::SegmentAction::Close && !ctx.segment_stack.empty()) {
                ctx.segment_stack.pop_back();
            } else if (e.action == hsf::SegmentAction::Include && e.name.find("Include Library") != std::string::npos) {
                own.include_libraries.insert(e.name);
            }
        }
        if (e.kind == hsf::EntityKind::TriangleMesh) {
            own.total_meshes += 1;
            ctx.meshes_in_current.insert(mesh_counter);
            // Если мы внутри Include сегмента, добавим сетку туда.
            for (const auto& seg : ctx.segment_stack) {
                if (seg.find("Include Library") != std::string::npos) {
                    ctx.library_meshes[seg].insert(mesh_counter);
                }
            }
            mesh_counter += 1;
        }
    });

    std::printf("  тегов (tag_index): %zu\n", own.tags_seen);
    std::printf("  открытых сегментов: %zu\n", own.segment_opens);
    std::printf("  Include библиотек: %zu\n", own.include_libraries.size());
    std::printf("  всего сеток: %zu\n", own.total_meshes);

    // Подсчёт сеток в каждой библиотеке.
    std::size_t total_library_meshes = 0;
    for (const auto& [lib, meshes] : ctx.library_meshes) {
        total_library_meshes += meshes.size();
    }
    std::printf("  сеток в Include библиотеках: %zu\n", total_library_meshes);

    // Второй проход: для каждого экземпляра, проверить есть ли Tag с его node.
    own.total_instances = section_instances.instances.size();
    std::unordered_set<std::uint32_t> covered_nodes;

    for (const auto& inst : section_instances.instances) {
        const auto it = tag_map.find(inst.node);
        if (it != tag_map.end()) {
            covered_nodes.insert(inst.node);
            own.instances_with_include += 1;
        } else {
            own.instances_without_include += 1;
        }
    }

    std::printf("  экземпляров с Tag: %zu\n", own.instances_with_include);
    std::printf("  экземпляров без Tag: %zu\n", own.instances_without_include);

    // Подсчёт orphan meshes (сетки, которые не привязаны ни к какой библиотеке).
    own.unclaimed_meshes = own.total_meshes - total_library_meshes;
    std::printf("  orphan meshes (не в библиотеках): %zu\n", own.unclaimed_meshes);

    // Выводы.
    if (own.instances_without_include > 0) {
        std::printf("  ⚠️  %zu экземпляров без связи к тегу (node не в tag_map)\n", own.instances_without_include);
    }
    if (own.unclaimed_meshes > 0) {
        std::printf("  ⚠️  %zu orphan meshes\n", own.unclaimed_meshes);
    }
}

int main() {
    if (const char* sample = std::getenv("BENDCORE_DWFX_SAMPLE")) {
        check_sample(sample);
    } else {
        std::printf("задайте BENDCORE_DWFX_SAMPLE=/path/to/sample.dwfx\n");
        return 1;
    }
    if (failures == 0) std::printf("OK dwfx_w3d_linkage\n");
    return failures == 0 ? 0 : 1;
}
