#pragma once

// Чтение DXF (ASCII): заголовок ($ACADVER, $INSUNITS, $DWGCODEPAGE), таблица слоёв и
// примитивы LINE, ARC, CIRCLE, LWPOLYLINE (с выпуклостью), POLYLINE/VERTEX (2D), TEXT,
// MTEXT раздела ENTITIES. Координаты возвращаются в единицах файла; перевод в мм —
// scaleToMillimeters. Описание, ограничения и правила: docs/drawing/DXF_IMPORT.md.

#include <bendcore/kernel/result.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bendcore::dxf {

using kernel::Error;
using kernel::ErrorCode;
using kernel::Result;

struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

// Общие свойства примитива. color — ACI: 256 = BYLAYER, 0 = BYBLOCK.
struct EntityProps {
    std::string layer = "0";
    int color = 256;
    std::string linetype = "BYLAYER";
    bool paper_space = false;  // группа 67 = 1
    std::string handle;        // группа 5
};

struct Line {
    EntityProps props;
    Point2D start;
    Point2D end;
};

struct Circle {
    EntityProps props;
    Point2D center;
    double radius = 0.0;
};

// Дуга против часовой стрелки (в мировой СК, вид сверху) от start_deg до end_deg.
struct Arc {
    EntityProps props;
    Point2D center;
    double radius = 0.0;
    double start_deg = 0.0;
    double end_deg = 0.0;
};

// Вершина полилинии. bulge — выпуклость сегмента от этой вершины к следующей:
// tg(θ/4), θ — центральный угол дуги, знак «+» — против часовой стрелки; 0 — отрезок.
struct PolylineVertex {
    Point2D point;
    double bulge = 0.0;
};

struct Polyline {
    EntityProps props;
    std::vector<PolylineVertex> vertices;
    bool closed = false;
    bool lightweight = true;  // LWPOLYLINE (true) или POLYLINE/VERTEX (false)
};

struct Text {
    EntityProps props;
    bool multiline = false;  // MTEXT
    // Текст в UTF-8: \U+XXXX и коды %%c/%%d/%%p раскрыты; в MTEXT абзацы \P → '\n',
    // коды форматирования удалены.
    std::string value;
    Point2D insertion;            // группа 10
    std::optional<Point2D> alignment;  // TEXT: группа 11 (только при выравнивании ≠ влево/по базе)
    double height = 0.0;          // группа 40
    double rotation_deg = 0.0;    // TEXT: 50; MTEXT: 50 или направление 11/21
    double width_factor = 1.0;    // TEXT: 41
    double oblique_deg = 0.0;     // TEXT: 51
    int h_justify = 0;            // TEXT: 72
    int v_justify = 0;            // TEXT: 73
    int attachment = 1;           // MTEXT: 71 (1 — верх-лево … 9 — низ-право)
    std::string style = "STANDARD";
};

struct Layer {
    std::string name;
    int color = 7;  // ACI; отрицательный — слой выключен
    std::string linetype = "CONTINUOUS";
    bool frozen = false;  // флаг 70 & 1
    bool locked = false;  // флаг 70 & 4
};

struct Document {
    std::string version;   // $ACADVER, например "AC1015"
    std::string codepage;  // $DWGCODEPAGE
    int insunits = 0;      // $INSUNITS (0 — без единиц, если переменной нет)
    std::vector<Layer> layers;
    std::vector<Line> lines;
    std::vector<Circle> circles;
    std::vector<Arc> arcs;
    std::vector<Polyline> polylines;
    std::vector<Text> texts;
    // Примитивы раздела ENTITIES, которые не читаются: тип → число (например
    // "SPLINE", "INSERT", "ARC (OCS)" — дуга в наклонной плоскости).
    std::map<std::string, int> skipped;
    // Отклонения, при которых данные прочитаны с потерей (например, Z ≠ 0 отброшена).
    std::vector<std::string> warnings;

    std::size_t entity_count() const {
        return lines.size() + circles.size() + arcs.size() + polylines.size() + texts.size();
    }
    const Layer* find_layer(const std::string& name) const;
};

// Разбор текста DXF. Ошибки: ParseError (нечётное число строк, нечисловое значение
// числовой группы, незакрытая секция, нет SECTION), Unsupported (двоичный DXF).
Result<Document> parseDxf(std::string_view text);

// Чтение файла: IoError, если файл не открывается; затем parseDxf.
Result<Document> readDxfFile(const std::string& path);

// Расширение .dxf (без учёта регистра).
bool isDxfPath(const std::string& path);

// Миллиметров в одной единице $INSUNITS; пусто для 0 (без единиц) и неизвестных кодов.
std::optional<double> millimetersPerUnit(int insunits);

// Копия документа в мм: координаты, радиусы, высоты текста умножены на коэффициент
// millimetersPerUnit; insunits = 4. Ошибка InvalidInput, если единицы не заданы.
Result<Document> scaleToMillimeters(const Document& doc);

// Дуга сегмента полилинии p1 → p2 с выпуклостью bulge ≠ 0.
struct BulgeArc {
    Point2D center;
    double radius = 0.0;
    double start_deg = 0.0;  // угол точки p1 от центра, [0, 360)
    double sweep_deg = 0.0;  // > 0 — против часовой стрелки, < 0 — по часовой
};
// Ошибка InvalidInput: bulge = 0 или p1 совпадает с p2.
Result<BulgeArc> bulgeToArc(Point2D p1, Point2D p2, double bulge);

}  // namespace bendcore::dxf
