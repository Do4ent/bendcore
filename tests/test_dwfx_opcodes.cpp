// Декодер опкодов HSF/W3D: синтетические потоки (сегменты, теги, полигон, матрица, сетка EdgeBreaker)
// и настоящий ресурс из source.dwfx, если задан BENDCORE_DWFX_SAMPLE. Эталон — JS-версия из BendPipe-Pro
// (decodeHsfOpcodePrefix на 80004806): число сущностей по видам, хэш последовательности позиций и суммы
// по сеткам, полилиниям и полигонам.
#include "bendcore/dwfx/hsf_envelope.hpp"
#include "bendcore/dwfx/hsf_opcodes.hpp"
#include "bendcore/dwfx/zip_reader.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using namespace bendcore::dwfx;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}

static void put_f32(std::vector<std::uint8_t>& out, float v) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof bits);
    put_u32(out, bits);
}

// Сбор сводки по потоку: число сущностей по видам, хэш позиций, суммы по геометрии.
struct Summary {
    std::map<std::string, std::size_t> by_kind;
    std::uint64_t sequence_hash = 0xcbf29ce484222325ULL;
    std::size_t mesh_vertices = 0;
    std::size_t mesh_faces = 0;      // грани EdgeBreaker (треугольники)
    std::size_t mesh_face_list = 0;  // элементы несжатого списка граней (счётчик + индексы)
    std::size_t mesh_decoded = 0;
    std::size_t polyline_points = 0;
    std::size_t polygon_points = 0;
    std::vector<hsf::Entity> kept;  // только для синтетических проверок (мало сущностей)
};

static void mix(std::uint64_t& h, std::uint64_t v) {
    h ^= v;
    h *= 0x100000001b3ULL;
}

static hsf::OpcodeResult summarize(const std::vector<std::uint8_t>& stream, const std::string& version,
                                   Summary& summary, bool keep) {
    return hsf::decode_opcodes(stream, version, [&](const hsf::Entity& e) {
        summary.by_kind[hsf::kind_name(e.kind)] += 1;
        mix(summary.sequence_hash, e.source_offset);
        mix(summary.sequence_hash, std::strlen(hsf::kind_name(e.kind)));
        if (e.kind == hsf::EntityKind::TriangleMesh) {
            summary.mesh_vertices += e.points.size();
            summary.mesh_faces += e.faces.size();
            summary.mesh_face_list += e.face_list.size();
            if (e.connectivity_status == "decoded") summary.mesh_decoded += 1;
        }
        if (e.kind == hsf::EntityKind::Polyline) summary.polyline_points += e.points.size();
        if (e.kind == hsf::EntityKind::Polygon) summary.polygon_points += e.points.size();
        if (keep) summary.kept.push_back(e);
    });
}

static void check_segments_tags_and_scope() {
    // open "a", tag, geometry open, tag, termination (закрывает область), close, stop.
    const std::vector<std::uint8_t> stream = {0x28, 0x01, 'a', 0x71, 0x3a, 0x71, 0x00, 0x29, 0x7a};
    Summary s;
    const hsf::OpcodeResult r = summarize(stream, "14.50", s, true);
    check(r.complete_prefix && r.next_offset == 9, "сегменты: поток завершён на 'z', позиция 9");
    check(r.entity_count == 6, "сегменты: 6 сущностей");
    check(s.by_kind["segment"] == 2 && s.by_kind["tag"] == 2 && s.by_kind["geometry_scope"] == 2,
          "сегменты: по 2 сегмента, тега и области");
    check(s.kept.size() == 6 && s.kept[0].name == "a" && s.kept[0].action == hsf::SegmentAction::Open,
          "сегменты: имя открытого сегмента 'a'");
    check(s.kept[1].tag_index == 0 && s.kept[3].tag_index == 1, "теги: сквозная нумерация 0, 1");
}

static void check_unsupported_termination() {
    const std::vector<std::uint8_t> stream = {0x00};
    Summary s;
    const hsf::OpcodeResult r = summarize(stream, "14.50", s, false);
    check(!r.complete_prefix && r.next_offset == 0, "завершение вне области: остановка на 0");
    check(r.unsupported_opcode.has_value() && *r.unsupported_opcode == 0x00, "завершение вне области: опкод 0x00");
    check(!r.unsupported_variant.empty(), "завершение вне области: пояснение");
}

static void check_polygon_and_transform() {
    std::vector<std::uint8_t> stream = {0x47, 3, 0, 0, 0};
    const float pts[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    for (float v : pts) put_f32(stream, v);
    stream.push_back(0x25);
    for (int i = 1; i <= 12; ++i) put_f32(stream, static_cast<float>(i));
    stream.push_back(0x7a);

    Summary s;
    const hsf::OpcodeResult r = summarize(stream, "14.50", s, true);
    check(r.complete_prefix, "полигон и матрица: поток завершён");
    check(s.kept.size() == 2, "полигон и матрица: две сущности");
    if (s.kept.size() == 2) {
        check(s.kept[0].kind == hsf::EntityKind::Polygon && s.kept[0].points.size() == 3, "полигон: 3 точки");
        check(s.kept[0].points[2].z == 9.0f, "полигон: последняя точка (7,8,9)");
        const auto& m = s.kept[1].matrix;
        check(m[12] == 10.0f && m[13] == 11.0f && m[14] == 12.0f, "матрица: перенос из элементов 9..11");
        check(m[3] == 0.0f && m[7] == 0.0f && m[11] == 0.0f && m[15] == 1.0f, "матрица: нижняя строка 0 0 0 1");
    }

    std::vector<std::uint8_t> truncated = {0x47, 3, 0, 0, 0};
    put_f32(truncated, 1.0f);
    bool threw = false;
    try {
        summarize(truncated, "14.50", s, false);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "полигон с обрывом — исключение");
}

// Одна грань: три ребра граничной компоненты и опкод CASE_E. Сетка TKE_Shell со сжатой связностью
// (EdgeBreaker) и несжатыми вершинами (версия 14.50).
static void check_shell_edgebreaker() {
    std::vector<std::uint8_t> workspace;
    workspace.push_back(2);  // scheme EdgeBreaker
    workspace.push_back(0);  // mtable scheme
    workspace.push_back(0);  // points scheme
    workspace.push_back(0);  // normals scheme
    put_u32(workspace, 1);   // ops length
    put_u32(workspace, 4);   // mtable length
    put_u32(workspace, 0);   // packed points length
    put_u32(workspace, 3);   // point count
    put_u32(workspace, 0);   // normals length
    workspace.push_back(2);  // CASE_E
    workspace.push_back(0);
    workspace.push_back(0);
    workspace.push_back(0);
    put_u32(workspace, 0);   // MTable: без необязательных блоков
    check(workspace.size() == 32, "EB: рабочая область 32 байта");

    std::vector<std::uint8_t> stream = {0x53, 0x50, 0x00, 0x05};  // shell: EB-связность, без сжатых точек
    put_u32(stream, static_cast<std::uint32_t>(workspace.size()));
    stream.insert(stream.end(), workspace.begin(), workspace.end());
    const float verts[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    for (float v : verts) put_f32(stream, v);
    stream.push_back(0x7a);

    Summary s;
    const hsf::OpcodeResult r = summarize(stream, "14.50", s, true);
    check(r.complete_prefix, "EB: поток завершён");
    check(s.kept.size() == 1 && s.kept[0].kind == hsf::EntityKind::TriangleMesh, "EB: одна сетка");
    if (s.kept.size() == 1) {
        const hsf::Entity& mesh = s.kept[0];
        check(mesh.connectivity_status == "decoded", "EB: связность декодирована");
        check(mesh.points.size() == 3 && mesh.point_count == 3, "EB: три вершины");
        check(mesh.faces.size() == 1 && mesh.faces[0][0] == 0 && mesh.faces[0][1] == 1 && mesh.faces[0][2] == 2,
              "EB: одна грань (0, 1, 2)");
    }
}

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
    const hsf::Envelope env = hsf::decode_envelope(resource);

    Summary s;
    const hsf::OpcodeResult r = summarize(env.opcode_stream, env.hsf_version, s, false);
    std::printf("образец: сущностей %zu, позиция %zu из %zu\n", r.entity_count, r.next_offset,
                env.opcode_stream.size());
    check(r.complete_prefix, "образец: поток разобран до 'z'");
    check(r.next_offset == 20552707 && env.opcode_stream.size() == 20552707, "образец: позиция конца 20552707");
    check(r.entity_count == 610122, "образец: всего 610122 сущности (JS)");

    const std::map<std::string, std::size_t> golden = {
        {"segment", 240838},        {"tag", 120048},          {"color", 118523},
        {"curve_candidate", 97024}, {"polyline", 27530},      {"polygon", 2605},
        {"triangle_mesh", 768},     {"transform", 580},       {"visibility", 204},
        {"texture", 364},           {"texture_matrix", 111},  {"rendering_options", 108},
        {"line_weight", 11},        {"geometry_scope", 4},    {"light", 2},
        {"pause", 2},               {"heuristics", 12},       {"hw3d_image", 12},
        {"bounds", 1},              {"view", 1},              {"user_options", 1374},
    };
    for (const auto& [kind, expected] : golden) {
        const std::size_t got = s.by_kind.count(kind) ? s.by_kind.at(kind) : 0;
        if (got != expected) {
            std::printf("FAIL: вид %s: %zu, ожидалось %zu\n", kind.c_str(), got, expected);
            ++failures;
        }
    }
    check(s.by_kind.size() == golden.size(), "образец: набор видов совпадает с JS");

    char hash[32];
    std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(s.sequence_hash));
    check(std::string(hash) == "cbc1d22798da68e4", "образец: хэш последовательности позиций совпадает с JS");

    check(s.mesh_decoded == 768, "образец: все 768 сеток со связностью");
    check(s.mesh_vertices == 892233, "образец: вершины сеток 892233");
    std::printf("образец: сетки %zu вершин %zu граней EB %zu, элементов списка %zu\n", s.mesh_decoded,
                s.mesh_vertices, s.mesh_faces, s.mesh_face_list);
    // В JS connectivity.faces для несжатого списка — сырой список (счётчики и индексы), поэтому сверяем сумму.
    check(s.mesh_faces + s.mesh_face_list == 1165154, "образец: грани сеток (EB + список) 1165154");
    check(s.polyline_points == 118565, "образец: точки полилиний 118565");
    check(s.polygon_points == 7815, "образец: точки полигонов 7815");
}

// Дуги TKE_Elliptical_Arc (0x65) в образцах, где они есть: 80005311 — 188 дуг, 80005313 — 376.
// Поток разбирается до конца, оси (точка − центр) перпендикулярны, end > start. Формат выведен по данным.
static void check_elliptical_arcs(const char* path, std::size_t expected) {
    std::ifstream in(path, std::ios::binary);
    check(in.good(), "дуги: файл открыт");
    if (!in.good()) return;
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
    if (!w3d) {
        check(false, "дуги: есть ресурс .w3d");
        return;
    }
    const std::vector<std::uint8_t> resource = archive.extract(*w3d);
    const hsf::Envelope env = hsf::decode_envelope(resource);

    std::size_t arcs = 0, perpendicular = 0, ordered = 0;
    const hsf::OpcodeResult r = hsf::decode_opcodes(env.opcode_stream, env.hsf_version, [&](const hsf::Entity& e) {
        if (e.kind != hsf::EntityKind::EllipticalArc) return;
        ++arcs;
        const hsf::Vec3 c = e.points[0];
        const float a[3] = {e.points[1].x - c.x, e.points[1].y - c.y, e.points[1].z - c.z};
        const float b[3] = {e.points[2].x - c.x, e.points[2].y - c.y, e.points[2].z - c.z};
        const double la = std::sqrt(double(a[0]) * a[0] + double(a[1]) * a[1] + double(a[2]) * a[2]);
        const double lb = std::sqrt(double(b[0]) * b[0] + double(b[1]) * b[1] + double(b[2]) * b[2]);
        const double dot = double(a[0]) * b[0] + double(a[1]) * b[1] + double(a[2]) * b[2];
        if (la > 1e-6 && lb > 1e-6 && std::fabs(dot) / (la * lb) < 1e-3) ++perpendicular;
        if (e.angles[1] > e.angles[0]) ++ordered;
    });
    std::printf("дуги %s: %zu, поток до %zu из %zu, перпендикулярны %zu, end>start %zu\n", path, arcs,
                r.next_offset, env.opcode_stream.size(), perpendicular, ordered);
    check(r.complete_prefix && r.next_offset == env.opcode_stream.size(), "дуги: поток разобран до конца");
    check(arcs == expected, "дуги: число дуг совпадает с проверкой на данных");
    check(perpendicular == expected, "дуги: оси перпендикулярны у всех дуг");
    check(ordered == expected, "дуги: end > start у всех дуг");
}

int main() {
    check_segments_tags_and_scope();
    check_unsupported_termination();
    check_polygon_and_transform();
    check_shell_edgebreaker();
    if (const char* sample = std::getenv("BENDCORE_DWFX_SAMPLE")) {
        const std::string path = sample;
        if (path.find("80005311") != std::string::npos) check_elliptical_arcs(sample, 188);
        if (path.find("80005313") != std::string::npos) check_elliptical_arcs(sample, 376);
        if (path.find("80004806") != std::string::npos) check_sample(sample);
    } else {
        std::printf("пропуск образца: задайте BENDCORE_DWFX_SAMPLE\n");
    }
    if (failures == 0) std::printf("OK dwfx_opcodes\n");
    return failures == 0 ? 0 : 1;
}
