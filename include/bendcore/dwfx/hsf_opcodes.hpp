// Декодер опкодов HSF/W3D (поток после распаковки zlib, оканчивается на 'z'): сегменты, теги, цвета,
// текстуры, матрицы, полилинии, полигоны, кривые, сетки TKE_Shell (EdgeBreaker или список граней).
// Порт decodeHsfOpcodePrefix из src/import/dwfx/hsf-envelope.mjs (BendPipe-Pro).
//
// Сущности отдаются через обратный вызов по одной, без накопления в памяти. Поля сущности заполнены
// там, где их использует дальнейший импорт (сегменты, теги, матрицы, сетки, полилинии, полигоны, границы);
// остальные опкоды разбираются по длине, чтобы сохранить позицию в потоке.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace bendcore::dwfx::hsf {

enum class EntityKind {
    Segment,
    GeometryScope,
    Tag,
    Light,
    Pause,
    Texture,
    Hw3dImage,
    Heuristics,
    UserOptions,
    Color,
    LineWeight,
    Transform,
    TextureMatrix,
    RenderingOptions,
    Visibility,
    Polygon,
    CurveCandidate,
    Polyline,
    TriangleMesh,
    Bounds,
    View,
    EllipticalArc,  // TKE_Elliptical_Arc, опкод 0x65
};

// Имя вида как в JS-версии (поле kind): используется для сверки счётчиков.
const char* kind_name(EntityKind kind);

enum class SegmentAction { None, Open, Close, Include, Style };

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Entity {
    EntityKind kind = EntityKind::Segment;
    std::size_t source_offset = 0;            // позиция опкода в потоке

    SegmentAction action = SegmentAction::None;  // для сегментов и геометрических областей
    std::string name;                         // имя сегмента, текстуры, вида
    std::int64_t tag_index = -1;              // для тегов: порядковый номер объекта

    // Полезные геометрические поля:
    //  polygon, polyline — точки;
    //  triangle_mesh — вершины (пусто, если точки сжаты и не декодированы);
    //  bounds — {min, max} для ящика или {center} для сферы (radius в value);
    //  view — {position, target, up};
    //  elliptical_arc — {center, axis_a_end, axis_b_end}: концы осей заданы абсолютными точками,
    //  оси — разности «точка − центр» (проверено на данных: перпендикулярны).
    std::vector<Vec3> points;
    std::array<float, 16> matrix{};           // transform: 4×4 по строкам, как в JS
    float value = 0.0f;                       // line_weight: вес; bounds (сфера): радиус
    std::array<float, 2> angles{};            // elliptical_arc: start, end — доля полного оборота, t = 2π·значение
                                              // (проверено: концы дуг совпадают с вершинами кривых при допуске 0.005 мм)

    // triangle_mesh:
    std::size_t point_count = 0;              // заявленное число вершин
    std::string connectivity_status;          // "decoded" | "compressed_unresolved" | "unresolved"
    std::vector<std::array<std::int32_t, 3>> faces;  // треугольники EdgeBreaker
    std::vector<std::int64_t> face_list;      // список граней без сжатия: {count, индексы...}
    std::uint32_t optional_count = 0;         // число необязательных блоков (нормали, параметры)
};

using EntitySink = std::function<void(const Entity&)>;

struct OpcodeResult {
    bool complete_prefix = false;             // встречен 'z' (конец потока)
    std::size_t next_offset = 0;              // позиция, на которой разбор остановился
    std::optional<std::uint8_t> unsupported_opcode;
    std::string unsupported_variant;          // пояснение, если разбор остановлен на неподдержанном варианте
    std::size_t entity_count = 0;             // сколько сущностей передано в обратный вызов
};

// Разбор потока опкодов. Исключение std::invalid_argument при повреждённом потоке
// (обрыв операнда, недопустимые размеры). Неподдержанный вариант не бросает исключение:
// разбор останавливается, результат содержит unsupported_*.
OpcodeResult decode_opcodes(const std::vector<std::uint8_t>& stream, const std::string& hsf_version,
                            const EntitySink& sink);

}  // namespace bendcore::dwfx::hsf
