#include "bendcore/dwfx/hsf_edgebreaker.hpp"

#include "hsf_bytes.hpp"

#include <map>
#include <set>
#include <string>
#include <utility>

namespace bendcore::dwfx::hsf {
namespace {

using bytes::View;
using bytes::fail;

constexpr std::uint8_t CASE_C = 0;
constexpr std::uint8_t CASE_L = 1;
constexpr std::uint8_t CASE_E = 2;
constexpr std::uint8_t CASE_R = 3;
constexpr std::uint8_t CASE_S = 4;
constexpr std::uint8_t CASE_M = 5;
constexpr std::uint8_t CASE_M2 = 6;

constexpr std::uint32_t MTABLE_HAS_LENGTHS = 0x01;
constexpr std::uint32_t MTABLE_HAS_M2STACKOFFSETS = 0x02;
constexpr std::uint32_t MTABLE_HAS_M2GATEOFFSETS = 0x04;
constexpr std::uint32_t MTABLE_HAS_DUMMIES = 0x08;
constexpr std::uint32_t MTABLE_HAS_PATCHES = 0x10;
constexpr std::uint32_t MTABLE_HAS_BOUNDING = 0x20;
constexpr std::uint32_t MTABLE_HAS_QUANTIZATION = 0x40;
constexpr std::uint32_t MTABLE_HAS_QUANTIZATION_NORMALS = 0x80;

constexpr std::int64_t kMaxIntArray = 10000000;

std::int64_t align4(std::int64_t value) { return value + ((4 - (value % 4)) % 4); }

struct MTable {
    std::vector<std::int64_t> dummies;                              // абсолютные индексы
    std::vector<std::pair<std::int64_t, std::int64_t>> patches;     // (старая вершина, новая вершина)
};

MTable parse_mtable(const View& v) {
    std::size_t cur = 0;
    const std::uint32_t flags = bytes::u32(v, cur, "EdgeBreaker MTable flags");
    cur += 4;

    std::uint32_t lengths_count = 0;
    std::uint32_t m2_count = 0;
    std::uint32_t dummies_count = 0;
    std::uint32_t patches_count = 0;

    if (flags & MTABLE_HAS_LENGTHS) {
        lengths_count = bytes::u32(v, cur, "EdgeBreaker MTable lengths count");
        cur += 4;
    }
    if (flags & MTABLE_HAS_M2STACKOFFSETS) {
        if ((flags & MTABLE_HAS_M2GATEOFFSETS) == 0) {
            fail("EdgeBreaker MTable has stack offsets without gate offsets");
        }
        m2_count = bytes::u32(v, cur, "EdgeBreaker MTable m2 count");
        cur += 4;
    }
    if (flags & MTABLE_HAS_DUMMIES) {
        dummies_count = bytes::u32(v, cur, "EdgeBreaker MTable dummies count");
        cur += 4;
    }
    if (flags & MTABLE_HAS_PATCHES) {
        patches_count = bytes::u32(v, cur, "EdgeBreaker MTable patches count");
        cur += 4;
        if ((patches_count & 1) != 0) fail("EdgeBreaker patch table length must be even");
    }

    auto read_ints = [&](std::uint32_t count, const char* label) {
        if (static_cast<std::int64_t>(count) > kMaxIntArray) {
            fail(std::string("EdgeBreaker ") + label + " count is unreasonable");
        }
        std::vector<std::int32_t> out(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            out[i] = bytes::i32(v, cur, "EdgeBreaker MTable array");
            cur += 4;
        }
        return out;
    };

    (void)read_ints(lengths_count, "length");
    (void)read_ints(m2_count, "m2 stack offset");
    (void)read_ints(m2_count, "m2 gate offset");

    const std::vector<std::int32_t> dummy_deltas = read_ints(dummies_count, "dummy");
    MTable table;
    std::int64_t previous_dummy = 0;
    for (std::int32_t delta : dummy_deltas) {
        if (delta < 0) fail("EdgeBreaker dummy delta must be non-negative");
        previous_dummy += delta;
        table.dummies.push_back(previous_dummy);
    }

    const std::vector<std::int32_t> patch_raw = read_ints(patches_count, "patch");
    std::int64_t previous_patch = 0;
    for (std::size_t i = 0; i + 1 < patch_raw.size(); i += 2) {
        previous_patch += patch_raw[i];
        table.patches.emplace_back(previous_patch, static_cast<std::int64_t>(patch_raw[i + 1]));
    }

    if (flags & MTABLE_HAS_BOUNDING) {
        bytes::need(v, cur, 24, "EdgeBreaker MTable bounding");
        cur += 24;
    }
    if (flags & MTABLE_HAS_QUANTIZATION) {
        bytes::need(v, cur, 12, "EdgeBreaker MTable quantization");
        cur += 12;
    }
    if (flags & MTABLE_HAS_QUANTIZATION_NORMALS) {
        bytes::need(v, cur, 12, "EdgeBreaker MTable normal quantization");
        cur += 12;
    }

    if (cur != v.size) {
        fail("EdgeBreaker MTable length mismatch: decoded " + std::to_string(cur) + " of " +
             std::to_string(v.size) + " bytes");
    }
    return table;
}

struct ComponentPre {
    std::int64_t edge_count = 0;
    std::vector<std::int64_t> split_offsets;  // -1 — не разрешено
    std::size_t end_offset = 0;
};

// Первый проход по компоненте: число граничных рёбер и смещения разбиений до конца компоненты.
ComponentPre preprocess_component(const std::uint8_t* ops, std::size_t n, std::size_t start) {
    std::int64_t edge_count = 0;
    std::int64_t split_count = 0;
    std::vector<std::int64_t> edge_stack;
    std::vector<std::int64_t> split_stack;
    std::vector<std::int64_t> split_offsets;
    std::size_t cur = start;

    while (cur < n) {
        const std::uint8_t op = ops[cur++];
        switch (op) {
            case CASE_C:
                edge_count -= 1;
                break;
            case CASE_L:
            case CASE_R:
                edge_count += 1;
                break;
            case CASE_E:
                edge_count += 3;
                if (!edge_stack.empty()) {
                    const std::int64_t split_index = split_stack.back();
                    split_stack.pop_back();
                    const std::int64_t saved_edge_count = edge_stack.back();
                    edge_stack.pop_back();
                    if (static_cast<std::int64_t>(split_offsets.size()) <= split_index) {
                        split_offsets.resize(static_cast<std::size_t>(split_index + 1), -1);
                    }
                    split_offsets[static_cast<std::size_t>(split_index)] = edge_count - 2 - saved_edge_count;
                } else {
                    if (edge_count <= 0) fail("EdgeBreaker component has invalid initial boundary");
                    return ComponentPre{edge_count, split_offsets, cur};
                }
                break;
            case CASE_S:
                edge_count -= 1;
                split_stack.push_back(split_count++);
                edge_stack.push_back(edge_count);
                break;
            case CASE_M:
            case CASE_M2:
                fail("EdgeBreaker merge opcode " + std::to_string(op) +
                     " is not decoded by the conservative connectivity path");
            default:
                fail("unknown EdgeBreaker topology opcode " + std::to_string(op));
        }
    }
    fail("unterminated EdgeBreaker connected component");
}

struct Topology {
    std::int64_t raw_point_count = 0;
    std::vector<std::array<std::int64_t, 3>> faces;
};

// Второй проход: восстановление треугольников по опкодам компонент.
Topology decode_topology(const std::uint8_t* ops, std::size_t n) {
    Topology out;
    std::int64_t vertex_count = 0;
    std::size_t cur = 0;

    auto ensure = [](std::vector<std::int64_t>& array, std::int64_t index) {
        if (static_cast<std::int64_t>(array.size()) <= index) {
            array.resize(static_cast<std::size_t>(index + 1), 0);
        }
    };
    auto pop_available = [](std::vector<std::int64_t>& available) {
        if (available.empty()) fail("EdgeBreaker topology has no free slot");
        const std::int64_t slot = available.back();
        available.pop_back();
        return slot;
    };

    while (cur < n) {
        const ComponentPre pre = preprocess_component(ops, n, cur);
        const std::int64_t edge_count = pre.edge_count;

        std::vector<std::int64_t> prev(static_cast<std::size_t>(edge_count));
        std::vector<std::int64_t> next(static_cast<std::size_t>(edge_count));
        std::vector<std::int64_t> start(static_cast<std::size_t>(edge_count));
        for (std::int64_t i = 0; i < edge_count; ++i) {
            prev[static_cast<std::size_t>(i)] = (i - 1 + edge_count) % edge_count;
            next[static_cast<std::size_t>(i)] = (i + 1) % edge_count;
            start[static_cast<std::size_t>(i)] = vertex_count + i;
        }
        vertex_count += edge_count;

        std::int64_t hash_used = edge_count;
        std::vector<std::int64_t> available;
        std::vector<std::int64_t> gate_stack;
        std::int64_t gate = 0;
        std::size_t split_index = 0;
        bool terminated = false;

        while (!terminated) {
            if (cur >= n) fail("truncated EdgeBreaker topology stream");
            if (available.empty()) {
                available.push_back(hash_used);
                ensure(prev, hash_used);
                ensure(next, hash_used);
                ensure(start, hash_used);
                hash_used += 1;
            }

            const std::uint8_t op = ops[cur++];
            const std::int64_t a = start.at(static_cast<std::size_t>(gate));
            const std::int64_t b = start.at(static_cast<std::size_t>(next.at(static_cast<std::size_t>(gate))));
            std::int64_t c = 0;

            switch (op) {
                case CASE_C: {
                    c = vertex_count;
                    const std::int64_t slot = pop_available(available);
                    const std::int64_t before = prev.at(static_cast<std::size_t>(gate));
                    next.at(static_cast<std::size_t>(before)) = slot;
                    prev.at(static_cast<std::size_t>(slot)) = before;
                    next.at(static_cast<std::size_t>(slot)) = gate;
                    prev.at(static_cast<std::size_t>(gate)) = slot;
                    start.at(static_cast<std::size_t>(slot)) = start.at(static_cast<std::size_t>(gate));
                    start.at(static_cast<std::size_t>(gate)) = vertex_count;
                    vertex_count += 1;
                    break;
                }
                case CASE_L: {
                    const std::int64_t before = prev.at(static_cast<std::size_t>(gate));
                    c = start.at(static_cast<std::size_t>(before));
                    start.at(static_cast<std::size_t>(gate)) = start.at(static_cast<std::size_t>(before));
                    available.push_back(before);
                    prev.at(static_cast<std::size_t>(gate)) = prev.at(static_cast<std::size_t>(before));
                    next.at(static_cast<std::size_t>(prev.at(static_cast<std::size_t>(gate)))) = gate;
                    break;
                }
                case CASE_E: {
                    const std::int64_t before = prev.at(static_cast<std::size_t>(gate));
                    const std::int64_t after = next.at(static_cast<std::size_t>(gate));
                    c = start.at(static_cast<std::size_t>(before));
                    available.push_back(gate);
                    available.push_back(before);
                    available.push_back(after);
                    if (!gate_stack.empty()) {
                        gate = gate_stack.back();
                        gate_stack.pop_back();
                    } else {
                        terminated = true;
                    }
                    break;
                }
                case CASE_R: {
                    const std::int64_t after = next.at(static_cast<std::size_t>(gate));
                    const std::int64_t after_after = next.at(static_cast<std::size_t>(after));
                    c = start.at(static_cast<std::size_t>(after_after));
                    available.push_back(after);
                    next.at(static_cast<std::size_t>(gate)) = after_after;
                    prev.at(static_cast<std::size_t>(after_after)) = gate;
                    break;
                }
                case CASE_S: {
                    if (split_index >= pre.split_offsets.size()) fail("EdgeBreaker split offset is unresolved");
                    const std::int64_t split_offset = pre.split_offsets[split_index++];
                    if (split_offset < 0) fail("EdgeBreaker split offset is unresolved");
                    std::int64_t boundary = gate;
                    for (std::int64_t i = 0; i <= split_offset; ++i) {
                        boundary = next.at(static_cast<std::size_t>(boundary));
                    }
                    c = start.at(static_cast<std::size_t>(next.at(static_cast<std::size_t>(boundary))));
                    const std::int64_t slot = pop_available(available);
                    gate_stack.push_back(slot);

                    const std::int64_t before_gate = prev.at(static_cast<std::size_t>(gate));
                    const std::int64_t after_boundary = next.at(static_cast<std::size_t>(boundary));
                    next.at(static_cast<std::size_t>(before_gate)) = slot;
                    prev.at(static_cast<std::size_t>(slot)) = before_gate;
                    next.at(static_cast<std::size_t>(slot)) = after_boundary;
                    prev.at(static_cast<std::size_t>(after_boundary)) = slot;
                    start.at(static_cast<std::size_t>(slot)) = start.at(static_cast<std::size_t>(gate));
                    start.at(static_cast<std::size_t>(gate)) = start.at(static_cast<std::size_t>(after_boundary));
                    prev.at(static_cast<std::size_t>(gate)) = boundary;
                    next.at(static_cast<std::size_t>(boundary)) = gate;
                    break;
                }
                case CASE_M:
                case CASE_M2:
                    fail("EdgeBreaker merge opcode " + std::to_string(op) +
                         " is not decoded by the conservative connectivity path");
                default:
                    fail("unknown EdgeBreaker topology opcode " + std::to_string(op));
            }

            if (a == b || b == c || c == a) {
                fail("invalid EdgeBreaker triangle at topology offset " + std::to_string(cur - 1));
            }
            out.faces.push_back({a, b, c});
        }

        if (cur != pre.end_offset) {
            fail("EdgeBreaker component boundary mismatch: preprocess=" + std::to_string(pre.end_offset) +
                 ", decode=" + std::to_string(cur));
        }
    }

    out.raw_point_count = vertex_count;
    return out;
}

EdgeBreakerConnectivity remove_dummy_vertices(const Topology& topology, const MTable& mtable,
                                              std::int64_t expected_point_count) {
    const std::int64_t raw_count = topology.raw_point_count;
    std::set<std::int64_t> dummy_set;
    for (std::int64_t index : mtable.dummies) {
        if (index < 0 || index >= raw_count) {
            fail("EdgeBreaker dummy vertex " + std::to_string(index) + " is outside raw point array");
        }
        if (!dummy_set.insert(index).second) {
            fail("duplicate EdgeBreaker dummy vertex " + std::to_string(index));
        }
    }

    // Патчи HOOPS — алиасы «старая вершина → новая вершина», применяются после восстановления
    // псевдомногообразия; старые индексы удаляются из итогового массива точек, как фиктивные.
    std::map<std::int64_t, std::int64_t> aliases;
    for (const auto& [old_index, new_index] : mtable.patches) {
        if (old_index < 0 || old_index >= raw_count) {
            fail("EdgeBreaker patch old vertex " + std::to_string(old_index) + " is outside raw point array");
        }
        if (new_index < 0 || new_index >= raw_count) {
            fail("EdgeBreaker patch new vertex " + std::to_string(new_index) + " is outside raw point array");
        }
        if (old_index == new_index) fail("EdgeBreaker patch " + std::to_string(old_index) + " aliases itself");
        if (aliases.count(old_index) != 0) {
            fail("duplicate EdgeBreaker patch old vertex " + std::to_string(old_index));
        }
        aliases.emplace(old_index, new_index);
    }

    auto resolve_alias = [&](std::int64_t index) {
        std::int64_t value = index;
        std::size_t steps = 0;
        while (aliases.count(value) != 0) {
            if (++steps > aliases.size()) fail("EdgeBreaker patch alias cycle");
            value = aliases.at(value);
        }
        return value;
    };

    std::set<std::int64_t> removed_set = dummy_set;
    for (const auto& alias : aliases) removed_set.insert(alias.first);

    std::vector<std::int64_t> remap(static_cast<std::size_t>(raw_count), -1);
    std::int64_t removed = 0;
    for (std::int64_t i = 0; i < raw_count; ++i) {
        if (removed_set.count(i) != 0) {
            remap[static_cast<std::size_t>(i)] = -1;
            removed += 1;
        } else {
            remap[static_cast<std::size_t>(i)] = i - removed;
        }
    }

    const std::int64_t point_count = raw_count - removed;
    if (point_count != expected_point_count) {
        fail("EdgeBreaker point count mismatch after patch/dummy removal: decoded " + std::to_string(point_count) +
             ", header " + std::to_string(expected_point_count));
    }

    EdgeBreakerConnectivity out;
    out.raw_point_count = static_cast<std::uint32_t>(raw_count);
    out.point_count = static_cast<std::uint32_t>(point_count);
    for (const auto& tri : topology.faces) {
        std::array<std::int64_t, 3> resolved{};
        for (std::size_t k = 0; k < 3; ++k) {
            resolved[k] = resolve_alias(tri[k]);
            if (resolved[k] != tri[k]) out.patched_face_references += 1;
        }

        bool touches_dummy = false;
        for (std::int64_t index : resolved) {
            if (dummy_set.count(index) != 0) touches_dummy = true;
        }
        if (touches_dummy) {
            out.dropped_dummy_faces += 1;
            continue;
        }

        std::array<std::int64_t, 3> mapped{};
        for (std::size_t k = 0; k < 3; ++k) {
            mapped[k] = remap.at(static_cast<std::size_t>(resolved[k]));
            if (mapped[k] < 0 || mapped[k] >= point_count) {
                fail("EdgeBreaker face remap escaped final point array");
            }
        }
        if (std::set<std::int64_t>(mapped.begin(), mapped.end()).size() < 3) {
            out.dropped_dummy_faces += 1;
            continue;
        }
        out.faces.push_back({static_cast<std::int32_t>(mapped[0]), static_cast<std::int32_t>(mapped[1]),
                             static_cast<std::int32_t>(mapped[2])});
    }
    return out;
}

}  // namespace

EdgeBreakerConnectivity decode_edgebreaker_connectivity(const std::uint8_t* data, std::size_t size) {
    if (size < 24) fail("EdgeBreaker workspace is too short");
    const View v{data, size};

    const std::uint8_t scheme = data[0];
    const std::uint8_t mtable_scheme = data[1];
    const std::int32_t ops_length = bytes::i32(v, 4, "EdgeBreaker ops length");
    const std::int32_t mtable_length = bytes::i32(v, 8, "EdgeBreaker mtable length");
    const std::int32_t packed_points_length = bytes::i32(v, 12, "EdgeBreaker points length");
    const std::int32_t expected_point_count = bytes::i32(v, 16, "EdgeBreaker point count");
    const std::int32_t normals_length = bytes::i32(v, 20, "EdgeBreaker normals length");

    if (scheme != 2) fail("unsupported EdgeBreaker scheme " + std::to_string(scheme));
    if (mtable_scheme != 0) fail("unsupported EdgeBreaker MTable scheme " + std::to_string(mtable_scheme));
    const std::pair<const char*, std::int32_t> lengths[] = {
        {"ops", ops_length},
        {"mtable", mtable_length},
        {"points", packed_points_length},
        {"normals", normals_length},
        {"point_count", expected_point_count},
    };
    for (const auto& [label, value] : lengths) {
        if (value < 0) fail(std::string("invalid EdgeBreaker ") + label + " length/count");
    }

    const std::int64_t ops_start = 24;
    const std::int64_t ops_end = ops_start + ops_length;
    const std::int64_t mtable_start = ops_start + align4(ops_length);
    const std::int64_t mtable_end = mtable_start + mtable_length;
    const std::int64_t points_start = mtable_start + align4(mtable_length);
    const std::int64_t points_end = points_start + packed_points_length;
    const std::int64_t expected_workspace = points_start + align4(packed_points_length);

    if (expected_workspace != static_cast<std::int64_t>(size)) {
        fail("EdgeBreaker workspace length mismatch: header implies " + std::to_string(expected_workspace) +
             ", got " + std::to_string(size));
    }
    if (ops_end > static_cast<std::int64_t>(size) || mtable_end > static_cast<std::int64_t>(size) ||
        points_end > static_cast<std::int64_t>(size)) {
        fail("truncated EdgeBreaker workspace sections");
    }

    for (std::int64_t i = ops_start; i < ops_end; ++i) {
        const std::uint8_t op = data[i];
        if (op > CASE_S) {
            if (op == CASE_M || op == CASE_M2) {
                fail("EdgeBreaker merge opcode " + std::to_string(op) + " requires the extended connectivity path");
            }
            fail("unknown EdgeBreaker topology opcode " + std::to_string(op));
        }
    }

    try {
        const MTable mtable = parse_mtable(View{data + mtable_start, static_cast<std::size_t>(mtable_length)});
        const Topology topology =
            decode_topology(data + ops_start, static_cast<std::size_t>(ops_length));
        return remove_dummy_vertices(topology, mtable, expected_point_count);
    } catch (const std::out_of_range& error) {
        fail(std::string("EdgeBreaker index out of range: ") + error.what());
    }
}

}  // namespace bendcore::dwfx::hsf
