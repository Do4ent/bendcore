#include "bendcore/dwfx/hsf_opcodes.hpp"

#include "bendcore/dwfx/hsf_edgebreaker.hpp"
#include "hsf_bytes.hpp"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>

namespace bendcore::dwfx::hsf {
namespace {

using bytes::View;
using bytes::fail;

constexpr std::int64_t kMaxCount = 1000000;      // полигоны, полилинии, NURBS
constexpr std::int64_t kMaxShellPoints = 10000000;

// Версия HSF как число (как parseFloat в JS); NaN, если версии нет или она не разбирается.
double parse_version(const std::string& text) {
    const char* begin = text.c_str();
    char* end = nullptr;
    const double value = std::strtod(begin, &end);
    if (end == begin) return std::numeric_limits<double>::quiet_NaN();
    return value;
}

bool finite(float value) { return std::isfinite(value); }

std::int64_t align4_(std::int64_t value) { return value + ((4 - (value % 4)) % 4); }

Entity make(EntityKind kind, std::size_t source_offset) {
    Entity entity;
    entity.kind = kind;
    entity.source_offset = source_offset;
    return entity;
}

class Decoder {
public:
    Decoder(const std::vector<std::uint8_t>& stream, const std::string& hsf_version, const EntitySink& sink)
        : view_{stream.data(), stream.size()}, version_(parse_version(hsf_version)), sink_(sink) {}

    OpcodeResult run();

private:
    View view_;
    double version_;
    const EntitySink& sink_;
    std::size_t entity_count_ = 0;
    std::int64_t tag_index_ = 0;
    std::size_t geometry_depth_ = 0;

    void emit(const Entity& entity) {
        entity_count_ += 1;
        sink_(entity);
    }

    OpcodeResult stop(std::size_t next, std::optional<std::uint8_t> opcode, std::string variant = {}) const {
        OpcodeResult result;
        result.complete_prefix = false;
        result.next_offset = next;
        result.unsupported_opcode = opcode;
        result.unsupported_variant = std::move(variant);
        result.entity_count = entity_count_;
        return result;
    }

    bool version_known() const { return std::isfinite(version_); }

    // Возвращает true, если остановка уже выставлена (неподдержанный вариант).
    bool decode_color(std::size_t& off);
    bool decode_texture(std::size_t& off);
    bool decode_shell(std::size_t& off, OpcodeResult& stop_result);
    bool decode_nurbs(std::size_t& off);
    bool decode_circular_arc(std::size_t& off, OpcodeResult& stop_result);
    bool decode_rendering_options(std::size_t& off, OpcodeResult& stop_result);
    bool decode_heuristics(std::size_t& off, OpcodeResult& stop_result);
    void decode_view_or_bounds(std::size_t& off, OpcodeResult& stop_result, bool& stopped);
    void decode_color_rgb(std::size_t& off);
};

OpcodeResult Decoder::run() {
    std::size_t off = 0;
    while (off < view_.size) {
        const std::uint8_t opcode = view_.data[off];
        OpcodeResult stopped;

        switch (opcode) {
            case 0x7a:  // TKE_Stop_Compression
                return OpcodeResult{true, off + 1, std::nullopt, {}, entity_count_};

            case 0x28: {  // TKE_Open_Segment: длина (1 байт) и имя
                bytes::need(view_, off, 2, "TKE_Open_Segment");
                const std::size_t end = off + 2 + view_.data[off + 1];
                if (end > view_.size) fail("truncated TKE_Open_Segment name");
                Entity entity = make(EntityKind::Segment, off);
                entity.action = SegmentAction::Open;
                entity.name = bytes::ascii(view_, off + 2, end);
                emit(entity);
                off = end;
                break;
            }

            case 0x29: {  // TKE_Close_Segment
                Entity entity = make(EntityKind::Segment, off);
                entity.action = SegmentAction::Close;
                emit(entity);
                off += 1;
                break;
            }

            case 0x3a: {  // TKE_Geometry_Attributes: без операндов
                Entity entity = make(EntityKind::GeometryScope, off);
                entity.action = SegmentAction::Open;
                emit(entity);
                geometry_depth_ += 1;
                off += 1;
                break;
            }

            case 0x00: {  // TKE_Termination
                if (geometry_depth_ > 0) {
                    geometry_depth_ -= 1;
                    Entity entity = make(EntityKind::GeometryScope, off);
                    entity.action = SegmentAction::Close;
                    emit(entity);
                    off += 1;
                    break;
                }
                return stop(off, opcode, "TKE_Termination outside geometry-attributes scope");
            }

            case 0x71: {  // TKE_Tag: без операндов, порядковый номер
                Entity entity = make(EntityKind::Tag, off);
                entity.tag_index = tag_index_;
                tag_index_ += 1;
                emit(entity);
                off += 1;
                break;
            }

            case 0x64: {  // TKE_Distant_Light: направление (3 float32)
                bytes::need(view_, off, 13, "TKE_Distant_Light");
                emit(make(EntityKind::Light, off));
                off += 13;
                break;
            }

            case 0x01:  // TKE_Pause
                emit(make(EntityKind::Pause, off));
                off += 1;
                break;

            case 0x74:  // TKE_Texture
                if (!decode_texture(off)) break;
                break;

            case 0xe0: {  // TKE_HW3D_Image
                std::size_t cur = off + 1;
                const std::uint8_t name_length = bytes::u8(view_, cur, "TKE_HW3D_Image name length");
                cur += 1;
                const std::size_t name_end = cur + name_length;
                if (name_end > view_.size) fail("truncated TKE_HW3D_Image name");
                Entity entity = make(EntityKind::Hw3dImage, off);
                entity.name = bytes::ascii(view_, cur, name_end);
                cur = name_end;
                bytes::need(view_, cur, 8, "TKE_HW3D_Image size");
                cur += 8;
                bytes::u8(view_, cur, "TKE_HW3D_Image bit depth");
                cur += 1;
                emit(entity);
                off = cur;
                break;
            }

            case 0x48:  // TKE_Heuristics
                if (!decode_heuristics(off, stopped)) return stopped;
                break;

            case 0x55: {  // TKE_User_Options: длина (2 байта, 0xffff — 4 байта) и строка
                std::size_t cur = off + 1;
                std::uint32_t length = bytes::u16(view_, cur, "TKE_User_Options length");
                cur += 2;
                if (length == 0xffff) {
                    length = bytes::u32(view_, cur, "TKE_User_Options extended length");
                    cur += 4;
                }
                const std::size_t end = cur + length;
                if (end > view_.size) fail("truncated TKE_User_Options string");
                emit(make(EntityKind::UserOptions, off));
                off = end;
                break;
            }

            case 0x22:  // TKE_Color
                if (!decode_color(off)) break;
                break;

            case 0x3d: {  // TKE_Line_Weight
                bytes::need(view_, off, 5, "TKE_Line_Weight");
                const float weight = bytes::f32(view_, off + 1, "TKE_Line_Weight");
                if (!finite(weight)) fail("non-finite TKE_Line_Weight");
                std::size_t cur = off + 5;
                if (weight < 0.0f) {
                    bytes::u8(view_, cur, "TKE_Line_Weight units");
                    cur += 1;
                }
                Entity entity = make(EntityKind::LineWeight, off);
                entity.value = weight;
                emit(entity);
                off = cur;
                break;
            }

            case 0x25: {  // TKE_Modelling_Matrix: 12 float32 (3×4)
                bytes::need(view_, off, 49, "TKE_Modelling_Matrix");
                float compact[12];
                for (std::size_t i = 0; i < 12; ++i) {
                    compact[i] = bytes::f32(view_, off + 1 + i * 4, "TKE_Modelling_Matrix element");
                    if (!finite(compact[i])) fail("non-finite TKE_Modelling_Matrix element");
                }
                Entity entity = make(EntityKind::Transform, off);
                entity.matrix = {compact[0], compact[1], compact[2], 0.0f,
                                 compact[3], compact[4], compact[5], 0.0f,
                                 compact[6], compact[7], compact[8], 0.0f,
                                 compact[9], compact[10], compact[11], 1.0f};
                emit(entity);
                off += 49;
                break;
            }

            case 0x24: {  // TKE_Texture_Matrix
                bytes::need(view_, off, 49, "TKE_Texture_Matrix");
                for (std::size_t i = 0; i < 12; ++i) {
                    const float element = bytes::f32(view_, off + 1 + i * 4, "TKE_Texture_Matrix element");
                    if (!finite(element)) fail("non-finite TKE_Texture_Matrix element");
                }
                emit(make(EntityKind::TextureMatrix, off));
                off += 49;
                break;
            }

            case 0x52:  // TKE_Rendering_Options
                if (!decode_rendering_options(off, stopped)) return stopped;
                break;

            case 0x3c:    // TKE_Include_Segment
            case 0x7b: {  // TKE_Style_Segment
                bytes::need(view_, off, 2, "TKE_Include_Segment");
                const std::size_t end = off + 2 + view_.data[off + 1];
                if (end > view_.size) fail("truncated segment name");
                Entity entity = make(EntityKind::Segment, off);
                entity.action = opcode == 0x3c ? SegmentAction::Include : SegmentAction::Style;
                entity.name = bytes::ascii(view_, off + 2, end);
                emit(entity);
                off = end;
                break;
            }

            case 0x56: {  // TKE_Visibility
                bytes::need(view_, off, 3, "TKE_Visibility");
                std::size_t cur = off + 1;
                const std::uint8_t mask_low = view_.data[cur++];
                cur += 1;  // value_low
                if (mask_low & 0x80) {
                    bytes::need(view_, cur, 4, "TKE_Visibility extended mask");
                    cur += 4;
                }
                emit(make(EntityKind::Visibility, off));
                off = cur;
                break;
            }

            case 0x7e:  // TKE_Color_RGB
                decode_color_rgb(off);
                break;

            case 0x47: {  // TKE_Polygon
                const std::int64_t point_count = bytes::i32(view_, off + 1, "TKE_Polygon point count");
                if (point_count < 3 || point_count > kMaxCount) {
                    fail("invalid TKE_Polygon point count " + std::to_string(point_count));
                }
                const std::size_t end = off + 5 + static_cast<std::size_t>(point_count) * 12;
                if (end > view_.size) fail("truncated TKE_Polygon points");
                Entity entity = make(EntityKind::Polygon, off);
                entity.points.reserve(static_cast<std::size_t>(point_count));
                for (std::int64_t i = 0; i < point_count; ++i) {
                    const std::size_t base = off + 5 + static_cast<std::size_t>(i) * 12;
                    Vec3 point{bytes::f32(view_, base, "TKE_Polygon point"),
                               bytes::f32(view_, base + 4, "TKE_Polygon point"),
                               bytes::f32(view_, base + 8, "TKE_Polygon point")};
                    if (!finite(point.x) || !finite(point.y) || !finite(point.z)) {
                        fail("non-finite TKE_Polygon point");
                    }
                    entity.points.push_back(point);
                }
                emit(entity);
                off = end;
                break;
            }

            case 0x4e:  // TKE_NURBS_Curve
                if (!decode_nurbs(off)) break;
                break;

            case 0x6c: {  // TKE_Line: две точки
                bytes::need(view_, off, 25, "TKE_Line");
                Vec3 start{bytes::f32(view_, off + 1, "TKE_Line"), bytes::f32(view_, off + 5, "TKE_Line"),
                           bytes::f32(view_, off + 9, "TKE_Line")};
                Vec3 end{bytes::f32(view_, off + 13, "TKE_Line"), bytes::f32(view_, off + 17, "TKE_Line"),
                         bytes::f32(view_, off + 21, "TKE_Line")};
                if (!finite(start.x) || !finite(start.y) || !finite(start.z) || !finite(end.x) ||
                    !finite(end.y) || !finite(end.z)) {
                    fail("non-finite TKE_Line coordinate");
                }
                emit(make(EntityKind::CurveCandidate, off));
                off += 25;
                break;
            }

            case 0x63:  // TKE_Circular_Arc
                if (!decode_circular_arc(off, stopped)) return stopped;
                break;

            case 0x53:  // TKE_Shell
                if (!decode_shell(off, stopped)) return stopped;
                break;

            case 0x42: {  // TKE_Bounding_Info: 0 — ящик, 1 — сфера
                bytes::need(view_, off, 2, "TKE_Bounding_Info");
                const std::uint8_t type = view_.data[off + 1];
                if (type == 0) {
                    bytes::need(view_, off, 26, "cuboid TKE_Bounding_Info");
                    Entity entity = make(EntityKind::Bounds, off);
                    Vec3 lo{bytes::f32(view_, off + 2, "bounds"), bytes::f32(view_, off + 6, "bounds"),
                            bytes::f32(view_, off + 10, "bounds")};
                    Vec3 hi{bytes::f32(view_, off + 14, "bounds"), bytes::f32(view_, off + 18, "bounds"),
                            bytes::f32(view_, off + 22, "bounds")};
                    entity.points = {lo, hi};
                    emit(entity);
                    off += 26;
                    break;
                }
                if (type == 1) {
                    bytes::need(view_, off, 18, "sphere TKE_Bounding_Info");
                    Entity entity = make(EntityKind::Bounds, off);
                    entity.points = {Vec3{bytes::f32(view_, off + 2, "bounds"), bytes::f32(view_, off + 6, "bounds"),
                                          bytes::f32(view_, off + 10, "bounds")}};
                    entity.value = bytes::f32(view_, off + 14, "bounds radius");
                    emit(entity);
                    off += 18;
                    break;
                }
                fail("unsupported TKE_Bounding_Info type " + std::to_string(type));
            }

            case 0x7d: {  // TKE_View
                bool stopped_view = false;
                decode_view_or_bounds(off, stopped, stopped_view);
                if (stopped_view) return stopped;
                break;
            }

            case 0x4c: {  // TKE_Polyline
                const std::uint32_t point_count = bytes::u32(view_, off + 1, "TKE_Polyline point count");
                if (point_count < 2 || point_count > kMaxCount) {
                    fail("invalid TKE_Polyline point count " + std::to_string(point_count));
                }
                const std::size_t end = off + 5 + static_cast<std::size_t>(point_count) * 12;
                if (end > view_.size) fail("truncated TKE_Polyline points");
                Entity entity = make(EntityKind::Polyline, off);
                entity.points.reserve(point_count);
                for (std::uint32_t i = 0; i < point_count; ++i) {
                    const std::size_t base = off + 5 + static_cast<std::size_t>(i) * 12;
                    Vec3 point{bytes::f32(view_, base, "TKE_Polyline point"),
                               bytes::f32(view_, base + 4, "TKE_Polyline point"),
                               bytes::f32(view_, base + 8, "TKE_Polyline point")};
                    if (!finite(point.x) || !finite(point.y) || !finite(point.z)) {
                        fail("non-finite TKE_Polyline point");
                    }
                    entity.points.push_back(point);
                }
                emit(entity);
                off = end;
                break;
            }

            default:
                return stop(off, opcode);
        }
    }

    return stop(off, std::nullopt);
}

bool Decoder::decode_texture(std::size_t& off) {
    std::size_t cur = off + 1;
    auto read_string = [&](const char* label) {
        const std::uint8_t first = bytes::u8(view_, cur, label);
        cur += 1;
        std::int64_t length = first;
        if (first == 0xff) {
            length = bytes::i32(view_, cur, label);
            cur += 4;
            if (length < 0) fail(std::string("negative TKE_Texture ") + label + " length");
        }
        const std::size_t end = cur + static_cast<std::size_t>(length);
        if (end > view_.size) fail(std::string("truncated TKE_Texture ") + label);
        std::string text = bytes::ascii(view_, cur, end);
        cur = end;
        return text;
    };

    Entity entity = make(EntityKind::Texture, off);
    entity.name = read_string("name");
    read_string("image name");

    std::uint32_t flags = bytes::u16(view_, cur, "TKE_Texture flags");
    cur += 2;
    if (flags & 0x8000) {
        flags = flags | (static_cast<std::uint32_t>(bytes::u16(view_, cur, "TKE_Texture extended flags")) << 16);
        cur += 2;
    }

    // Однобайтовые параметры по битам 0x1…0x200 (в порядке битов).
    for (std::uint32_t bit : {0x1u, 0x2u, 0x4u, 0x8u, 0x10u, 0x20u, 0x40u, 0x80u, 0x100u, 0x200u}) {
        if (flags & bit) {
            bytes::u8(view_, cur, "TKE_Texture option");
            cur += 1;
        }
    }
    if (flags & 0x800) {
        bytes::need(view_, cur, 8, "TKE_Texture value scale");
        cur += 8;
    }
    if (flags & 0x400) {
        const std::uint8_t length = bytes::u8(view_, cur, "TKE_Texture transform length");
        cur += 1;
        const std::size_t end = cur + length;
        if (end > view_.size) fail("truncated TKE_Texture transform");
        cur = end;
    }
    if (flags & (0x10000u | 0x20000u)) {
        bytes::u8(view_, cur, "TKE_Texture apply mode");
        cur += 1;
    }
    if (flags & 0x40000) {
        bytes::u8(view_, cur, "TKE_Texture parameter offset");
        cur += 1;
    }

    emit(entity);
    off = cur;
    return true;
}

bool Decoder::decode_color(std::size_t& off) {
    std::size_t cur = off + 1;
    std::uint32_t geometry_mask = bytes::u8(view_, cur, "TKE_Color geometry mask");
    cur += 1;
    if (geometry_mask & 0x80) {
        geometry_mask |= static_cast<std::uint32_t>(bytes::u8(view_, cur, "TKE_Color extended geometry mask")) << 8;
        cur += 1;
    }
    if (geometry_mask & 0x00008000) {
        geometry_mask |= static_cast<std::uint32_t>(bytes::u8(view_, cur, "TKE_Color extended color mask")) << 16;
        cur += 1;
    }
    if (geometry_mask & 0x00800000) {
        geometry_mask |= static_cast<std::uint32_t>(bytes::u8(view_, cur, "TKE_Color secondary geometry mask")) << 24;
        cur += 1;
    }

    std::uint32_t channels_mask = bytes::u8(view_, cur, "TKE_Color channels mask");
    cur += 1;
    if (channels_mask & 0x80) {
        channels_mask |= static_cast<std::uint32_t>(bytes::u8(view_, cur, "TKE_Color extended channels mask")) << 8;
        cur += 1;
    }

    auto read_value = [&](const char* label, bool extended_length, bool rgb_allowed) {
        std::uint32_t length = bytes::u8(view_, cur, label);
        cur += 1;
        if (extended_length && length == 255) {
            length = bytes::u32(view_, cur, label);
            cur += 4;
        }
        if (rgb_allowed && length == 0) {
            bytes::need(view_, cur, 3, label);
            cur += 3;
            return;
        }
        const std::size_t end = cur + length;
        if (end > view_.size) fail(std::string("truncated TKE_Color ") + label);
        cur = end;
    };

    if (channels_mask & 0x0001) read_value("diffuse", true, true);
    if (channels_mask & 0x0002) read_value("specular", false, true);
    if (channels_mask & 0x0004) read_value("mirror", false, true);
    if (channels_mask & 0x0008) read_value("transmission", false, true);
    if (channels_mask & 0x0010) read_value("emission", false, true);
    if (channels_mask & 0x0020) {
        bytes::need(view_, cur, 4, "TKE_Color gloss");
        cur += 4;
    }
    if (channels_mask & 0x0040) {
        bytes::need(view_, cur, 4, "TKE_Color index");
        cur += 4;
    }
    if (channels_mask & 0x0100) read_value("environment", false, false);
    if (channels_mask & 0x0200) read_value("bump", false, false);

    emit(make(EntityKind::Color, off));
    off = cur;
    return true;
}

void Decoder::decode_color_rgb(std::size_t& off) {
    bytes::need(view_, off, 5, "TKE_Color_RGB");
    std::size_t cur = off + 1;
    std::uint32_t geometry_mask = bytes::u8(view_, cur, "TKE_Color_RGB geometry mask");
    cur += 1;
    if (geometry_mask & 0x00000080) {
        geometry_mask |= static_cast<std::uint32_t>(bytes::u8(view_, cur, "TKE_Color_RGB extended mask")) << 8;
        cur += 1;
    }
    if (geometry_mask & 0x00008000) {
        geometry_mask |= static_cast<std::uint32_t>(bytes::u8(view_, cur, "TKE_Color_RGB color mask")) << 16;
        cur += 1;
    }
    if (geometry_mask & 0x00800000) {
        geometry_mask |= static_cast<std::uint32_t>(bytes::u8(view_, cur, "TKE_Color_RGB secondary mask")) << 24;
        cur += 1;
    }
    bytes::need(view_, cur, 3, "TKE_Color_RGB value");
    cur += 3;
    emit(make(EntityKind::Color, off));
    off = cur;
}

bool Decoder::decode_heuristics(std::size_t& off, OpcodeResult& stop_result) {
    std::size_t cur = off + 1;
    std::uint32_t mask = bytes::u16(view_, cur, "TKE_Heuristics mask");
    cur += 2;
    if (mask & 0x8000) {
        mask = mask | (static_cast<std::uint32_t>(bytes::u16(view_, cur, "TKE_Heuristics extended mask")) << 16);
        cur += 2;
    }
    std::uint32_t value = bytes::u16(view_, cur, "TKE_Heuristics value");
    cur += 2;
    if (mask & 0x8000) {
        value = value | (static_cast<std::uint32_t>(bytes::u16(view_, cur, "TKE_Heuristics extended value")) << 16);
        cur += 2;
    }

    const std::uint32_t enabled = mask & value;
    const std::uint32_t payload_bits = 0x00000040 | 0x00000080 | 0x0000000c | 0x00010000 | 0x00200000 |
                                       0x00400000 | 0x01000000;
    if ((enabled & payload_bits) != 0 || (mask & 0x00200000) != 0 || (mask & 0x01000000) != 0) {
        stop_result = stop(off, 0x48, "TKE_Heuristics optional payload requires version-aware decoding");
        return false;
    }
    emit(make(EntityKind::Heuristics, off));
    off = cur;
    return true;
}

bool Decoder::decode_rendering_options(std::size_t& off, OpcodeResult& stop_result) {
    const std::size_t start = off;
    std::size_t cur = off + 1;
    const std::uint32_t mask = bytes::u32(view_, cur, "TKE_Rendering_Options mask");
    cur += 4;
    if (mask & 0x80000000u) {
        stop_result = stop(start, 0x52, "extended TKE_Rendering_Options mask is not decoded yet");
        return false;
    }
    const std::uint32_t value = bytes::u32(view_, cur, "TKE_Rendering_Options value");
    cur += 4;

    constexpr std::uint32_t kAttributeLock = 0x00100000;
    constexpr std::uint32_t kInterpTextureFaces = 0x00000001;
    constexpr std::uint32_t kSupportedTopLevel = kAttributeLock | kInterpTextureFaces;
    if ((mask & ~kSupportedTopLevel) != 0) {
        stop_result = stop(start, 0x52, "TKE_Rendering_Options mask contains unsupported options");
        return false;
    }

    if ((mask & value & kAttributeLock) != 0) {
        const std::uint32_t lock_mask = bytes::u32(view_, cur, "TKE_Rendering_Options lock mask");
        cur += 4;
        const std::uint32_t lock_value = bytes::u32(view_, cur, "TKE_Rendering_Options lock value");
        cur += 4;

        constexpr std::uint32_t kLockColor = 0x00000004;
        constexpr std::uint32_t kLockVisibility = 0x08000000;
        if ((lock_mask & lock_value & kLockVisibility) != 0) {
            stop_result = stop(start, 0x52, "TKE_Rendering_Options visibility lock payload is not decoded yet");
            return false;
        }
        if ((lock_mask & lock_value & kLockColor) != 0) {
            const std::uint32_t color_mask = bytes::u32(view_, cur, "TKE_Rendering_Options color mask");
            cur += 4;
            const std::uint32_t color_value = bytes::u32(view_, cur, "TKE_Rendering_Options color value");
            cur += 4;
            (void)color_mask;

            constexpr std::uint32_t kFaceOrFront = 0x00001001;
            if ((color_value & ~kFaceOrFront) != 0) {
                stop_result = stop(start, 0x52, "TKE_Rendering_Options color-lock value requires unsupported per-geometry payloads");
                return false;
            }
            if ((color_value & kFaceOrFront) != 0) {
                bytes::need(view_, cur, 4, "TKE_Rendering_Options face color lock");
                cur += 4;
            }
        }
    }

    emit(make(EntityKind::RenderingOptions, start));
    off = cur;
    return true;
}

bool Decoder::decode_nurbs(std::size_t& off) {
    std::size_t cur = off + 1;
    bytes::need(view_, cur, 6, "TKE_NURBS_Curve header");
    const std::uint8_t options = view_.data[cur];
    const std::uint8_t degree = view_.data[cur + 1];
    const std::int32_t control_count = bytes::i32(view_, cur + 2, "TKE_NURBS_Curve control count");
    cur += 6;
    if (degree < 1 || control_count < 2 || control_count > kMaxCount || degree >= control_count) {
        fail("invalid TKE_NURBS_Curve degree/count: degree=" + std::to_string(degree) +
             ", count=" + std::to_string(control_count));
    }

    for (std::int32_t i = 0; i < control_count; ++i) {
        bytes::need(view_, cur, 12, "TKE_NURBS_Curve control points");
        const float x = bytes::f32(view_, cur, "TKE_NURBS_Curve control point");
        const float y = bytes::f32(view_, cur + 4, "TKE_NURBS_Curve control point");
        const float z = bytes::f32(view_, cur + 8, "TKE_NURBS_Curve control point");
        if (!finite(x) || !finite(y) || !finite(z)) fail("non-finite TKE_NURBS_Curve control point");
        cur += 12;
    }
    if (options & 0x01) {
        for (std::int32_t i = 0; i < control_count; ++i) {
            bytes::need(view_, cur, 4, "TKE_NURBS_Curve weights");
            if (!finite(bytes::f32(view_, cur, "TKE_NURBS_Curve weight"))) fail("non-finite TKE_NURBS_Curve weight");
            cur += 4;
        }
    }
    if (options & 0x02) {
        const std::int64_t knot_count = static_cast<std::int64_t>(control_count) + degree + 1;
        for (std::int64_t i = 0; i < knot_count; ++i) {
            bytes::need(view_, cur, 4, "TKE_NURBS_Curve knots");
            if (!finite(bytes::f32(view_, cur, "TKE_NURBS_Curve knot"))) fail("non-finite TKE_NURBS_Curve knot");
            cur += 4;
        }
    }
    float start_parameter = 0.0f;
    float end_parameter = 1.0f;
    if (options & 0x04) {
        start_parameter = bytes::f32(view_, cur, "TKE_NURBS_Curve start parameter");
        cur += 4;
    }
    if (options & 0x08) {
        end_parameter = bytes::f32(view_, cur, "TKE_NURBS_Curve end parameter");
        cur += 4;
    }
    if (!finite(start_parameter) || !finite(end_parameter)) fail("non-finite TKE_NURBS_Curve parameter range");

    emit(make(EntityKind::CurveCandidate, off));
    off = cur;
    return true;
}

bool Decoder::decode_circular_arc(std::size_t& off, OpcodeResult& stop_result) {
    bytes::need(view_, off, 37, "TKE_Circular_Arc");
    for (std::size_t base : {off + 1, off + 13, off + 25}) {
        for (std::size_t k = 0; k < 3; ++k) {
            if (!finite(bytes::f32(view_, base + 4 * k, "TKE_Circular_Arc point"))) {
                fail("non-finite TKE_Circular_Arc coordinate");
            }
        }
    }
    std::size_t cur = off + 37;
    if (!version_known()) {
        stop_result = stop(off, 0x63, "TKE_Circular_Arc requires HSF version to resolve post-12.15 flags");
        return false;
    }
    if (version_ >= 12.15) {
        const std::uint8_t flags = bytes::u8(view_, cur, "TKE_Circular_Arc flags");
        cur += 1;
        if (flags & 0x01) {
            bytes::need(view_, cur, 12, "TKE_Circular_Arc center");
            cur += 12;
        }
    }
    emit(make(EntityKind::CurveCandidate, off));
    off = cur;
    return true;
}

void Decoder::decode_view_or_bounds(std::size_t& off, OpcodeResult& stop_result, bool& stopped) {
    bytes::need(view_, off, 47, "TKE_View");
    const std::uint8_t projection = view_.data[off + 1];
    if (projection != 0) {
        stop_result = stop(off, 0x7d, "TKE_View projection " + std::to_string(projection));
        stopped = true;
        return;
    }
    const std::uint8_t name_length = view_.data[off + 46];
    const std::size_t end = off + 47 + name_length;
    if (end > view_.size) fail("truncated TKE_View name");
    Entity entity = make(EntityKind::View, off);
    entity.name = bytes::ascii(view_, off + 47, end);
    emit(entity);
    off = end;
}

bool Decoder::decode_shell(std::size_t& off, OpcodeResult& stop_result) {
    const std::size_t shell_offset = off;
    std::size_t cur = off + 1;
    const std::uint8_t suboptions = bytes::u8(view_, cur, "TKE_Shell suboptions");
    cur += 1;

    std::uint32_t suboptions2 = 0;
    if (suboptions & 0x80) {
        suboptions2 = bytes::u16(view_, cur, "TKE_Shell expanded suboptions");
        cur += 2;
        if ((suboptions2 & 0x000b) != 0) {
            stop_result = stop(shell_offset, 0x53, "TKE_Shell expanded suboptions are not decoded yet");
            return false;
        }
    }
    if (suboptions & 0x20) {
        stop_result = stop(shell_offset, 0x53, "TKE_Shell bounding-only representation is not decoded yet");
        return false;
    }
    if ((suboptions & 0x10) == 0) {
        bytes::i32(view_, cur, "TKE_Shell refinement index");  // индекс уточнения, в результат не выводится
        cur += 4;
    }
    const std::uint8_t lod = bytes::u8(view_, cur, "TKE_Shell LOD");
    (void)lod;
    cur += 1;

    const bool has_compressed_points = (suboptions & 0x01) != 0;
    const bool has_connectivity_compression = (suboptions & 0x40) != 0;

    std::int64_t point_count = 0;
    std::vector<Vec3> vertices;
    std::optional<EdgeBreakerConnectivity> edge_breaker;
    std::vector<std::int64_t> face_list;
    bool has_face_list = false;

    if (has_compressed_points || has_connectivity_compression) {
        const std::uint8_t compression_scheme = bytes::u8(view_, cur, "TKE_Shell compression scheme");
        cur += 1;
        if (compression_scheme != 0x05) {
            stop_result = stop(shell_offset, 0x53,
                               "TKE_Shell compression scheme " + std::to_string(compression_scheme) + " is not decoded yet");
            return false;
        }

        const std::int64_t workspace_length = bytes::i32(view_, cur, "TKE_Shell EdgeBreaker workspace length");
        cur += 4;
        if (workspace_length < 24 || static_cast<std::int64_t>(cur) + workspace_length > static_cast<std::int64_t>(view_.size)) {
            fail("invalid or truncated TKE_Shell EdgeBreaker workspace");
        }
        const std::size_t ws_start = cur;
        const std::size_t ws_end = ws_start + static_cast<std::size_t>(workspace_length);

        const std::uint8_t edge_scheme = view_.data[ws_start];
        const std::uint8_t mtable_scheme = view_.data[ws_start + 1];
        const std::int32_t ops_length = bytes::i32(view_, ws_start + 4, "EdgeBreaker ops length");
        const std::int32_t mtable_length = bytes::i32(view_, ws_start + 8, "EdgeBreaker mtable length");
        const std::int32_t packed_points_length = bytes::i32(view_, ws_start + 12, "EdgeBreaker points length");
        point_count = bytes::i32(view_, ws_start + 16, "EdgeBreaker point count");
        const std::int32_t normals_length = bytes::i32(view_, ws_start + 20, "EdgeBreaker normals length");

        if (edge_scheme != 2 || mtable_scheme != 0 || ops_length < 0 || mtable_length < 0 ||
            packed_points_length < 0 || normals_length < 0 || point_count < 0 || point_count > kMaxShellPoints) {
            stop_result = stop(shell_offset, 0x53, "TKE_Shell EdgeBreaker header variant is not decoded yet");
            return false;
        }

        const std::int64_t expected_workspace =
            24 + align4_(ops_length) + align4_(mtable_length) + align4_(packed_points_length);
        if (expected_workspace != workspace_length) {
            stop_result = stop(shell_offset, 0x53,
                               "TKE_Shell EdgeBreaker workspace length mismatch: header implies " +
                                   std::to_string(expected_workspace) + ", stream has " +
                                   std::to_string(workspace_length));
            return false;
        }

        try {
            edge_breaker = decode_edgebreaker_connectivity(view_.data + ws_start, ws_end - ws_start);
        } catch (const std::invalid_argument& error) {
            stop_result = stop(shell_offset, 0x53, std::string("TKE_Shell EdgeBreaker connectivity blocked: ") + error.what());
            return false;
        }
        cur = ws_end;

        if (!has_compressed_points) {
            if (!version_known()) {
                stop_result = stop(shell_offset, 0x53, "TKE_Shell requires HSF version to resolve post-EdgeBreaker point layout");
                return false;
            }
            if (version_ >= 6.51) {
                const std::size_t point_bytes = static_cast<std::size_t>(point_count) * 12;
                bytes::need(view_, cur, point_bytes, "TKE_Shell uncompressed point array");
                vertices.reserve(static_cast<std::size_t>(point_count));
                for (std::int64_t i = 0; i < point_count; ++i) {
                    const std::size_t base = cur + static_cast<std::size_t>(i) * 12;
                    Vec3 point{bytes::f32(view_, base, "TKE_Shell vertex"), bytes::f32(view_, base + 4, "TKE_Shell vertex"),
                               bytes::f32(view_, base + 8, "TKE_Shell vertex")};
                    if (!finite(point.x) || !finite(point.y) || !finite(point.z)) fail("non-finite TKE_Shell vertex");
                    vertices.push_back(point);
                }
                cur += point_bytes;
            }
        }
    } else {
        point_count = bytes::i32(view_, cur, "TKE_Shell point count");
        cur += 4;
        if (point_count < 0 || point_count > kMaxShellPoints) {
            fail("invalid TKE_Shell point count " + std::to_string(point_count));
        }
        const std::size_t point_bytes = static_cast<std::size_t>(point_count) * 12;
        bytes::need(view_, cur, point_bytes, "TKE_Shell uncompressed point array");
        vertices.reserve(static_cast<std::size_t>(point_count));
        for (std::int64_t i = 0; i < point_count; ++i) {
            const std::size_t base = cur + static_cast<std::size_t>(i) * 12;
            Vec3 point{bytes::f32(view_, base, "TKE_Shell vertex"), bytes::f32(view_, base + 4, "TKE_Shell vertex"),
                       bytes::f32(view_, base + 8, "TKE_Shell vertex")};
            if (!finite(point.x) || !finite(point.y) || !finite(point.z)) fail("non-finite TKE_Shell vertex");
            vertices.push_back(point);
        }
        cur += point_bytes;

        const std::uint8_t face_scheme = bytes::u8(view_, cur, "TKE_Shell face compression scheme");
        cur += 1;
        if (face_scheme != 0x01) {
            stop_result = stop(shell_offset, 0x53,
                               "TKE_Shell face compression scheme " + std::to_string(face_scheme) + " is not decoded yet");
            return false;
        }
        const std::int64_t face_workspace_length = bytes::i32(view_, cur, "TKE_Shell face workspace length");
        cur += 4;
        if (face_workspace_length < 1 || static_cast<std::int64_t>(cur) + face_workspace_length >
                                             static_cast<std::int64_t>(view_.size)) {
            fail("invalid or truncated TKE_Shell face workspace");
        }
        const std::size_t face_end = cur + static_cast<std::size_t>(face_workspace_length);
        const std::uint8_t bits_per_sample = bytes::u8(view_, cur, "TKE_Shell face bits per sample");
        cur += 1;
        if (bits_per_sample != 8 && bits_per_sample != 16 && bits_per_sample != 32) {
            fail("unsupported TKE_Shell face bits_per_sample " + std::to_string(bits_per_sample));
        }
        const std::size_t bytes_per_sample = bits_per_sample / 8;
        if ((face_workspace_length - 1) % static_cast<std::int64_t>(bytes_per_sample) != 0) {
            fail("misaligned TKE_Shell trivial face workspace");
        }
        const bool signed_faces = (suboptions2 & 0x0004) != 0;
        while (cur < face_end) {
            std::int64_t value = 0;
            if (bytes_per_sample == 1) {
                const std::uint8_t raw = view_.data[cur];
                value = signed_faces ? static_cast<std::int8_t>(raw) : raw;
            } else if (bytes_per_sample == 2) {
                const std::uint16_t raw = bytes::u16(view_, cur, "TKE_Shell face index");
                value = signed_faces ? static_cast<std::int16_t>(raw) : raw;
            } else {
                const std::uint32_t raw = bytes::u32(view_, cur, "TKE_Shell face index");
                value = signed_faces ? static_cast<std::int64_t>(static_cast<std::int32_t>(raw))
                                     : static_cast<std::int64_t>(raw);
            }
            face_list.push_back(value);
            cur += bytes_per_sample;
        }

        std::size_t face_cursor = 0;
        while (face_cursor < face_list.size()) {
            const std::int64_t count_value = face_list[face_cursor];
            if (count_value == 0) fail("zero-length face in TKE_Shell face list");
            const std::size_t vertex_count = static_cast<std::size_t>(count_value < 0 ? -count_value : count_value);
            const std::size_t end = face_cursor + 1 + vertex_count;
            if (end > face_list.size()) fail("truncated TKE_Shell face list");
            for (std::size_t i = face_cursor + 1; i < end; ++i) {
                const std::int64_t index = face_list[i];
                if (index < 0 || index >= point_count) {
                    fail("TKE_Shell face index " + std::to_string(index) + " outside point array");
                }
            }
            face_cursor = end;
        }
        has_face_list = true;
    }

    std::uint32_t optional_count = 0;
    if (suboptions & 0x08) {
        bool terminated = false;
        while (cur < view_.size) {
            const std::size_t optional_offset = cur;
            const std::uint8_t optional_opcode = view_.data[cur];
            cur += 1;
            if (optional_opcode == 0x00) {
                terminated = true;
                break;
            }
            if (optional_opcode == 0x01) {  // OPT_ALL_NORMALS_COMPRESSED
                if (!version_known() || version_ < 6.50) {
                    stop_result = stop(optional_offset, 0x53,
                                       "OPT_ALL_NORMALS_COMPRESSED pre-6.50 layout is not decoded yet");
                    return false;
                }
                if (cur + 6 > view_.size) fail("truncated OPT_ALL_NORMALS_COMPRESSED header");
                const std::uint8_t normal_scheme = view_.data[cur];
                cur += 2;  // схема и число бит
                if (normal_scheme != 0x01 && normal_scheme != 0x03 && normal_scheme != 0x07) {
                    stop_result = stop(optional_offset, 0x53,
                                       "OPT_ALL_NORMALS_COMPRESSED scheme " + std::to_string(normal_scheme) +
                                           " is not decoded yet");
                    return false;
                }
                const std::int64_t normal_workspace = bytes::i32(view_, cur, "OPT_ALL_NORMALS_COMPRESSED workspace");
                cur += 4;
                if (normal_workspace < 0 || static_cast<std::int64_t>(cur) + normal_workspace >
                                                static_cast<std::int64_t>(view_.size)) {
                    fail("invalid or truncated OPT_ALL_NORMALS_COMPRESSED workspace");
                }
                cur += static_cast<std::size_t>(normal_workspace);
                optional_count += 1;
                continue;
            }
            if (optional_opcode == 0x1c) {  // OPT_ALL_PARAMETERS
                const std::uint8_t width = bytes::u8(view_, cur, "OPT_ALL_PARAMETERS width");
                cur += 1;
                if (width < 1 || width > 4) fail("invalid OPT_ALL_PARAMETERS width " + std::to_string(width));
                const std::size_t byte_count = static_cast<std::size_t>(point_count) * width * 4;
                bytes::need(view_, cur, byte_count, "OPT_ALL_PARAMETERS values");
                cur += byte_count;
                optional_count += 1;
                continue;
            }
            stop_result = stop(optional_offset, 0x53,
                               "TKE_Shell optional opcode 0x" + std::to_string(optional_opcode) + " is not decoded yet");
            return false;
        }
        if (!terminated) fail("unterminated TKE_Shell optional attributes");
    }

    Entity entity = make(EntityKind::TriangleMesh, shell_offset);
    entity.point_count = static_cast<std::size_t>(point_count);
    entity.points = std::move(vertices);
    entity.optional_count = optional_count;
    if (has_connectivity_compression) {
        entity.connectivity_status = edge_breaker ? "decoded" : "compressed_unresolved";
        if (edge_breaker) entity.faces = edge_breaker->faces;
    } else {
        entity.connectivity_status = has_face_list ? "decoded" : "unresolved";
        entity.face_list = std::move(face_list);
    }
    emit(entity);
    off = cur;
    return true;
}

}  // namespace

const char* kind_name(EntityKind kind) {
    switch (kind) {
        case EntityKind::Segment: return "segment";
        case EntityKind::GeometryScope: return "geometry_scope";
        case EntityKind::Tag: return "tag";
        case EntityKind::Light: return "light";
        case EntityKind::Pause: return "pause";
        case EntityKind::Texture: return "texture";
        case EntityKind::Hw3dImage: return "hw3d_image";
        case EntityKind::Heuristics: return "heuristics";
        case EntityKind::UserOptions: return "user_options";
        case EntityKind::Color: return "color";
        case EntityKind::LineWeight: return "line_weight";
        case EntityKind::Transform: return "transform";
        case EntityKind::TextureMatrix: return "texture_matrix";
        case EntityKind::RenderingOptions: return "rendering_options";
        case EntityKind::Visibility: return "visibility";
        case EntityKind::Polygon: return "polygon";
        case EntityKind::CurveCandidate: return "curve_candidate";
        case EntityKind::Polyline: return "polyline";
        case EntityKind::TriangleMesh: return "triangle_mesh";
        case EntityKind::Bounds: return "bounds";
        case EntityKind::View: return "view";
    }
    return "unknown";
}

OpcodeResult decode_opcodes(const std::vector<std::uint8_t>& stream, const std::string& hsf_version,
                            const EntitySink& sink) {
    Decoder decoder(stream, hsf_version, sink);
    return decoder.run();
}

}  // namespace bendcore::dwfx::hsf
