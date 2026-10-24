#include "bendcore/dwg_export.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <numbers>
#include <sstream>
#include <algorithm>

namespace bendcore::dwg {
namespace {

constexpr double kPi = std::numbers::pi;

// Преобразовать LineStyle в DWGLineType
DWGLineType style_to_linetype(drawing::LineStyle style) {
    switch (style) {
        case drawing::LineStyle::Contour:
            return DWGLineType::Continuous;
        case drawing::LineStyle::Thin:
            return DWGLineType::Continuous;
        case drawing::LineStyle::Axis:
            return DWGLineType::Dashed;
        case drawing::LineStyle::Cut:
            return DWGLineType::DashDot;
        case drawing::LineStyle::Hidden:
            return DWGLineType::Dotted;
    }
    return DWGLineType::Continuous;
}

// Получить цвет слоя
int get_color(LayerType layer) {
    switch (layer) {
        case LayerType::Contour:
            return 30;  // синий
        case LayerType::Dimensions:
            return 1;   // красный
        case LayerType::Axis:
            return 3;   // зелёный
        case LayerType::Markup:
            return 8;   // серый
        case LayerType::Construction:
            return 256; // по слою
    }
    return 7;  // белый (по умолчанию)
}

// Получить имя слоя
std::string get_layer_name_str(LayerType layer) {
    switch (layer) {
        case LayerType::Contour:
            return "Contour";
        case LayerType::Dimensions:
            return "Dimensions";
        case LayerType::Axis:
            return "Axis";
        case LayerType::Markup:
            return "Markup";
        case LayerType::Construction:
            return "Construction";
    }
    return "Default";
}

// Преобразовать LayerType в строку для обратного преобразования
LayerType linestyle_to_layer(drawing::LineStyle style) {
    switch (style) {
        case drawing::LineStyle::Contour:
        case drawing::LineStyle::Thin:
            return LayerType::Contour;
        case drawing::LineStyle::Axis:
            return LayerType::Axis;
        case drawing::LineStyle::Cut:
        case drawing::LineStyle::Hidden:
            return LayerType::Markup;
    }
    return LayerType::Contour;
}

// Функция для записи DXF заголовка
std::string write_dxf_header(const DWGExportOptions& opts) {
    std::ostringstream os;

    // SECTION HEADER
    os << "  0\nSECTION\n";
    os << "  2\nHEADER\n";

    // Variable: $ACADVER (AutoCAD version)
    os << "  9\n$ACADVER\n  1\nAC1021\n";  // AutoCAD 2000+

    // Variable: $UNITS
    os << "  9\n$INSUNITS\n 70\n";
    if (opts.units == "MM") os << "4\n";      // millimeters
    else if (opts.units == "CM") os << "5\n"; // centimeters
    else if (opts.units == "INCH") os << "1\n"; // inches
    else os << "4\n";

    // End of HEADER
    os << "  0\nENDSEC\n";

    return os.str();
}

// Функция для записи таблицы слоёв DXF
std::string write_dxf_layers(const std::map<LayerType, DWGLayer>& layers) {
    std::ostringstream os;

    os << "  0\nSECTION\n";
    os << "  2\nTABLES\n";

    // Таблица LAYER
    os << "  0\nTABLE\n";
    os << "  2\nLAYER\n";
    os << " 70\n" << (layers.size() + 1) << "\n";  // количество слоёв

    // Слой 0 (обязательный)
    os << "  0\nLAYER\n";
    os << "  2\n0\n";
    os << " 70\n0\n";
    os << " 62\n7\n";
    os << "  6\nCONTINUOUS\n";

    // Остальные слои
    for (const auto& [layer_type, layer] : layers) {
        os << "  0\nLAYER\n";
        os << "  2\n" << layer.name << "\n";
        os << " 70\n0\n";  // не заморожен
        os << " 62\n" << layer.color << "\n";  // цвет

        // Тип линии
        std::string linetype = "CONTINUOUS";
        switch (layer.linetype) {
            case DWGLineType::Dashed:
                linetype = "DASHED";
                break;
            case DWGLineType::Dotted:
                linetype = "DOTTED";
                break;
            case DWGLineType::DashDot:
                linetype = "DASHDOT";
                break;
            case DWGLineType::Continuous:
                linetype = "CONTINUOUS";
                break;
        }
        os << "  6\n" << linetype << "\n";
    }

    os << "  0\nENDTAB\n";
    os << "  0\nENDSEC\n";

    return os.str();
}

// Функция для записи сущностей DXF
std::string write_dxf_entities(const std::vector<DWGEntity>& entities) {
    std::ostringstream os;

    os << "  0\nSECTION\n";
    os << "  2\nENTITIES\n";

    for (const auto& entity : entities) {
        switch (entity.type) {
            case DWGEntity::Type::Line: {
                if (entity.points.size() >= 2) {
                    os << "  0\nLINE\n";
                    os << "  8\n" << get_layer_name_str(entity.layer) << "\n";
                    os << " 62\n" << entity.color << "\n";
                    os << " 10\n" << std::fixed << std::setprecision(6) << entity.points[0][0] << "\n";
                    os << " 20\n" << entity.points[0][1] << "\n";
                    os << " 11\n" << entity.points[1][0] << "\n";
                    os << " 21\n" << entity.points[1][1] << "\n";
                }
                break;
            }
            case DWGEntity::Type::Circle: {
                os << "  0\nCIRCLE\n";
                os << "  8\n" << get_layer_name_str(entity.layer) << "\n";
                os << " 62\n" << entity.color << "\n";
                if (!entity.points.empty()) {
                    os << " 10\n" << std::fixed << std::setprecision(6) << entity.points[0][0] << "\n";
                    os << " 20\n" << entity.points[0][1] << "\n";
                }
                os << " 40\n" << std::fixed << std::setprecision(6) << entity.radius << "\n";
                break;
            }
            case DWGEntity::Type::Arc: {
                os << "  0\nARC\n";
                os << "  8\n" << get_layer_name_str(entity.layer) << "\n";
                os << " 62\n" << entity.color << "\n";
                if (!entity.points.empty()) {
                    os << " 10\n" << std::fixed << std::setprecision(6) << entity.points[0][0] << "\n";
                    os << " 20\n" << entity.points[0][1] << "\n";
                }
                os << " 40\n" << std::fixed << std::setprecision(6) << entity.radius << "\n";
                os << " 50\n" << std::fixed << std::setprecision(2) << entity.start_angle << "\n";
                os << " 51\n" << std::fixed << std::setprecision(2) << entity.end_angle << "\n";
                break;
            }
            case DWGEntity::Type::Text: {
                os << "  0\nTEXT\n";
                os << "  8\n" << get_layer_name_str(entity.layer) << "\n";
                os << " 62\n" << entity.color << "\n";
                os << " 10\n" << std::fixed << std::setprecision(6) << entity.text_pos[0] << "\n";
                os << " 20\n" << entity.text_pos[1] << "\n";
                os << " 40\n" << std::fixed << std::setprecision(6) << entity.text_height << "\n";
                os << "  1\n" << entity.text << "\n";

                // Alignment
                int align_h = 0;  // left = 0
                if (entity.text_anchor == "middle") align_h = 1;
                else if (entity.text_anchor == "end") align_h = 2;
                os << " 72\n" << align_h << "\n";
                break;
            }
            case DWGEntity::Type::Polyline: {
                os << "  0\nLWPOLYLINE\n";
                os << "  8\n" << get_layer_name_str(entity.layer) << "\n";
                os << " 62\n" << entity.color << "\n";
                os << " 70\n" << (entity.closed ? 1 : 0) << "\n";  // флаг замкнутости
                os << " 90\n" << entity.points.size() << "\n";  // количество вершин

                for (const auto& pt : entity.points) {
                    os << " 10\n" << std::fixed << std::setprecision(6) << pt[0] << "\n";
                    os << " 20\n" << pt[1] << "\n";
                }
                break;
            }
            case DWGEntity::Type::Point: {
                // Экспортировать точку как маленький круг (DXF POINT)
                if (!entity.points.empty()) {
                    os << "  0\nPOINT\n";
                    os << "  8\n" << get_layer_name_str(entity.layer) << "\n";
                    os << " 62\n" << entity.color << "\n";
                    os << " 10\n" << std::fixed << std::setprecision(6) << entity.points[0][0] << "\n";
                    os << " 20\n" << entity.points[0][1] << "\n";
                    // Размер точки (в DXF это параметр PDSIZE)
                    os << " 39\n" << std::fixed << std::setprecision(2) << 0.5 << "\n";
                }
                break;
            }
            case DWGEntity::Type::Dimension: {
                // Экспортировать размер как DIMENSION сущность (или LINE + TEXT для совместимости)
                // В DXF версии 1 используем комбинацию LINE (размерная линия) + TEXT (текст размера)
                if (entity.points.size() >= 2) {
                    // Размерная линия
                    os << "  0\nLINE\n";
                    os << "  8\n" << get_layer_name_str(entity.layer) << "\n";
                    os << " 62\n" << entity.color << "\n";
                    os << " 10\n" << std::fixed << std::setprecision(6) << entity.points[0][0] << "\n";
                    os << " 20\n" << entity.points[0][1] << "\n";
                    os << " 11\n" << entity.points[1][0] << "\n";
                    os << " 21\n" << entity.points[1][1] << "\n";
                }

                // Текст размера
                os << "  0\nTEXT\n";
                os << "  8\n" << get_layer_name_str(entity.layer) << "\n";
                os << " 62\n" << entity.color << "\n";
                os << " 10\n" << std::fixed << std::setprecision(6) << entity.text_pos[0] << "\n";
                os << " 20\n" << entity.text_pos[1] << "\n";
                os << " 40\n" << std::fixed << std::setprecision(6) << entity.text_height << "\n";
                os << "  1\n" << entity.text << "\n";

                // Выравнивание по центру
                int align_h = 1;  // center
                os << " 72\n" << align_h << "\n";
                break;
            }
        }
    }

    os << "  0\nENDSEC\n";
    return os.str();
}

}  // namespace

// Инициализировать слои по умолчанию
std::map<LayerType, DWGLayer> create_default_layers() {
    std::map<LayerType, DWGLayer> layers;

    layers[LayerType::Contour] = {
        "Contour",      // name
        30,             // color (синий)
        DWGLineType::Continuous,
        0.35
    };

    layers[LayerType::Dimensions] = {
        "Dimensions",
        1,              // color (красный)
        DWGLineType::Continuous,
        0.35
    };

    layers[LayerType::Axis] = {
        "Axis",
        3,              // color (зелёный)
        DWGLineType::Dashed,
        0.35
    };

    layers[LayerType::Markup] = {
        "Markup",
        8,              // color (серый)
        DWGLineType::Continuous,
        0.35
    };

    layers[LayerType::Construction] = {
        "Construction",
        7,              // color (белый)
        DWGLineType::Dotted,
        0.35
    };

    return layers;
}

DWGLineType linestyle_to_dwg(drawing::LineStyle style) {
    return style_to_linetype(style);
}

int get_layer_color(LayerType layer) {
    return get_color(layer);
}

std::string get_layer_name(LayerType layer) {
    return get_layer_name_str(layer);
}

// Преобразовать Geometry в DWG сущности
GeometryToDWGResult geometry_to_dwg_entities(
    const drawing::Geometry& geom,
    const DWGExportOptions& opts) {

    GeometryToDWGResult result;

    // Преобразовать линии (Polyline2 -> LINE entities)
    for (const auto& line : geom.lines) {
        if (line.pts.size() >= 2) {
            // Если это одиночный отрезок или ломаная - создать как LWPOLYLINE
            DWGEntity entity;
            entity.type = DWGEntity::Type::Polyline;
            entity.layer = linestyle_to_layer(line.style);
            entity.points = line.pts;
            entity.closed = line.closed;
            entity.color = get_color(entity.layer);
            entity.linetype = style_to_linetype(line.style);

            result.entities.push_back(entity);
        }
    }

    // Преобразовать окружности (Circle2 -> CIRCLE entities)
    for (const auto& circle : geom.circles) {
        DWGEntity entity;
        entity.type = DWGEntity::Type::Circle;
        entity.layer = linestyle_to_layer(circle.style);
        entity.points.push_back(circle.c);
        entity.radius = circle.r;
        entity.color = get_color(entity.layer);
        entity.linetype = style_to_linetype(circle.style);

        result.entities.push_back(entity);
    }

    // Преобразовать текст (Text2 -> TEXT entities)
    if (opts.include_text) {
        for (const auto& text : geom.texts) {
            DWGEntity entity;
            entity.type = DWGEntity::Type::Text;
            entity.layer = LayerType::Markup;
            entity.text = text.text;
            entity.text_pos = text.p;
            entity.text_height = text.height;
            entity.text_anchor = text.anchor;
            entity.color = get_color(entity.layer);

            result.entities.push_back(entity);
        }
    }

    // Преобразовать размеры (Dim2 -> TEXT entities для размера)
    // В версии 1 размеры экспортируются как текст на соответствующей позиции
    if (opts.include_dimensions) {
        for (const auto& dim : geom.dims) {
            // Основной размер - как TEXT
            DWGEntity entity;
            entity.type = DWGEntity::Type::Text;
            entity.layer = LayerType::Dimensions;
            entity.text = dim.text;
            // Позиция текста - середина между a и b, смещённая на offset
            entity.text_pos[0] = (dim.a[0] + dim.b[0]) / 2.0;
            entity.text_pos[1] = (dim.a[1] + dim.b[1]) / 2.0 - dim.offset;
            entity.text_height = 3.5;
            entity.text_anchor = "middle";
            entity.color = get_color(entity.layer);

            result.entities.push_back(entity);

            // Линия размера - как LINE
            DWGEntity line;
            line.type = DWGEntity::Type::Line;
            line.layer = LayerType::Dimensions;
            line.points = {dim.a, dim.b};
            line.color = get_color(line.layer);
            line.linetype = DWGLineType::Continuous;

            result.entities.push_back(line);
        }
    }

    return result;
}

// Записать DWG/DXF файл
DWGWriteResult write_dwg_file(
    const std::string& filename,
    const std::vector<DWGEntity>& entities,
    const DWGExportOptions& opts) {

    DWGWriteResult result;
    result.filename = filename;

    try {
        // Используем DXF формат (версия 1)
        result.format = "DXF";

        // Инициализировать слои
        auto layers = create_default_layers();

        // Открыть файл для записи
        std::ofstream file(filename, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            result.error = "Cannot open file: " + filename;
            return result;
        }

        // Записать DXF структуру
        file << write_dxf_header(opts);
        file << write_dxf_layers(layers);
        file << write_dxf_entities(entities);

        // Записать EOF
        file << "  0\nEOF\n";

        file.close();

        result.success = true;
        result.entity_count = entities.size();
        result.layer_count = layers.size();

    } catch (const std::exception& e) {
        result.error = e.what();
    }

    return result;
}

// Сохранить чертёж в DWG/DXF формат
DWGSaveResult save_drawing_as_dwg(
    const drawing::DrawingSheet& sheet,
    const DWGExportOptions& opts) {

    DWGSaveResult result;
    result.filename = opts.filename;

    // Преобразовать все виды на листе в DWG сущности
    std::vector<DWGEntity> all_entities;
    double offset_x = 0, offset_y = 0;

    for (const auto& view : sheet.views) {
        // Преобразовать геометрию вида
        auto [entities, errors, warnings] = geometry_to_dwg_entities(view.geom, opts);

        // Применить масштаб и сдвиг
        for (auto& entity : entities) {
            // Применить origin и scale вида
            for (auto& pt : entity.points) {
                pt[0] = view.origin[0] + pt[0] * view.scale;
                pt[1] = view.origin[1] + pt[1] * view.scale;
            }
            // Масштабировать radius и text_height
            if (entity.type == DWGEntity::Type::Circle || entity.type == DWGEntity::Type::Arc) {
                entity.radius *= view.scale;
            }
            entity.text_height *= view.scale;

            all_entities.push_back(entity);
        }

        // Добавить логирование ошибок/предупреждений
        result.warnings.insert(result.warnings.end(), warnings.begin(), warnings.end());
    }

    // Записать файл DXF
    auto write_result = write_dwg_file(opts.filename, all_entities, opts);

    result.success = write_result.success;
    result.error = write_result.error;
    result.format = write_result.format;

    // Получить размер файла
    if (result.success) {
        std::ifstream file(opts.filename, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            result.file_size = file.tellg();
            file.close();
        }
    }

    return result;
}

}  // namespace bendcore::dwg
