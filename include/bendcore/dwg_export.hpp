// DWG/DXF экспорт (раздел 5): преобразование Geometry в DWG/DXF форматы
// Совместимость с AutoCAD 2020+
// Сохранение линий, размеров, текста, слоёв
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "bendcore/drawing.hpp"

namespace bendcore::dwg {

// Типы слоёв в DWG (LayerType)
enum class LayerType {
    Contour,      // основной контур трубы (синий, сплошная линия)
    Dimensions,   // размеры (красный)
    Axis,         // оси симметрии (зелёный, штриховая)
    Markup,       // вспомогательная разметка (серый)
    Construction  // вспомогательные линии
};

// Тип линии в DWG
enum class DWGLineType {
    Continuous,  // сплошная (CONTINUOUS)
    Dashed,      // штриховая (DASHED)
    Dotted,      // пунктирная (DOTTED)
    DashDot      // штрих-пунктирная (DASHDOT)
};

// Описание слоя DWG
struct DWGLayer {
    std::string name;
    int color = 7;              // AutoCAD цвет (1-256, 7 = белый)
    DWGLineType linetype = DWGLineType::Continuous;
    double line_width = 0.35;   // мм
};

// Опции экспорта в DWG/DXF
struct DWGExportOptions {
    std::string filename = "drawing.dxf";
    double scale = 1.0;                  // масштаб (1.0 = 1:1)
    std::string units = "MM";            // MM, CM, INCH
    bool use_dwg = false;                // true = DWG, false = DXF (для v1 используем DXF)
    std::map<LayerType, DWGLayer> layers;  // описание слоёв
    bool include_dimensions = true;
    bool include_text = true;
    bool include_hatches = false;        // версия 1: без штриховок
};

// Сущность DWG (примитив)
struct DWGEntity {
    enum class Type {
        Line,
        Circle,
        Arc,
        Text,
        Dimension,
        Polyline,
        Point
    };

    Type type;
    LayerType layer;
    std::vector<drawing::Vec2> points;  // координаты (для Line, Polyline)
    double radius = 0.0;                // для Circle, Arc
    double start_angle = 0.0;           // для Arc (градусы)
    double end_angle = 0.0;             // для Arc (градусы)
    std::string text;                   // для Text, Dimension
    drawing::Vec2 text_pos;             // позиция текста
    double text_height = 3.5;           // высота текста (мм)
    std::string text_anchor = "left";   // start | middle | end
    bool closed = false;                // для Polyline
    int color = 7;
    DWGLineType linetype = DWGLineType::Continuous;
    double line_width = 0.35;
};

// Результат преобразования Geometry в DWG сущности
struct GeometryToDWGResult {
    std::vector<DWGEntity> entities;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// Преобразовать Geometry в набор DWG сущностей
GeometryToDWGResult geometry_to_dwg_entities(
    const drawing::Geometry& geom,
    const DWGExportOptions& opts);

// Записать DWG/DXF файл с сущностями
struct DWGWriteResult {
    bool success = false;
    std::string filename;
    std::string format = "DXF";  // "DWG" или "DXF"
    std::string error;
    std::size_t entity_count = 0;
    std::size_t layer_count = 0;
};

DWGWriteResult write_dwg_file(
    const std::string& filename,
    const std::vector<DWGEntity>& entities,
    const DWGExportOptions& opts);

// Сохранить чертёж в DWG/DXF формат
struct DWGSaveResult {
    bool success = false;
    std::string filename;
    std::string format;  // "DWG" или "DXF"
    std::size_t file_size = 0;
    std::string error;
    std::vector<std::string> warnings;
};

DWGSaveResult save_drawing_as_dwg(
    const drawing::DrawingSheet& sheet,
    const DWGExportOptions& opts);

// Вспомогательные функции для конвертации

// Преобразовать LineStyle из drawing в DWGLineType
DWGLineType linestyle_to_dwg(drawing::LineStyle style);

// Получить цвет слоя по типу
int get_layer_color(LayerType layer);

// Получить имя слоя по типу
std::string get_layer_name(LayerType layer);

// Инициализировать слои по умолчанию
std::map<LayerType, DWGLayer> create_default_layers();

}  // namespace bendcore::dwg
