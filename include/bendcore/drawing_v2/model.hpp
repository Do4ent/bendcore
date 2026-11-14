#pragma once

// 2D-модель чертежа: лист, слои, примитивы. Единицы — мм листа.

#include <bendcore/drawing_v2/common.hpp>
#include <bendcore/drawing_v2/line_types.hpp>
#include <bendcore/drawing_v2/sheet.hpp>

#include <array>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace bendcore::drawing_v2 {

struct LineSeg {
    Point2 a;
    Point2 b;
};

// Дуга окружности против часовой стрелки от startDeg до endDeg (градусы, от оси X).
struct Arc {
    Point2 center;
    double radius = 0.0;
    double startDeg = 0.0;
    double endDeg = 90.0;
};

struct Circle {
    Point2 center;
    double radius = 0.0;
};

struct Polyline {
    std::vector<Point2> points;
    bool closed = false;
};

enum class HAlign { Left, Center, Right };
// Baseline — по линии основания строки; Middle — середина высоты прописных; Top — верх
// прописных букв.
enum class VAlign { Baseline, Middle, Top };

// Высоты шрифта по ГОСТ 2.304-81 (размер шрифта h — высота прописных букв, мм).
inline constexpr std::array<double, 7> kGostTextHeights = {2.5, 3.5, 5.0, 7.0, 10.0, 14.0, 20.0};

// Угол наклона букв шрифта с наклоном к основанию строки (ГОСТ 2.304).
inline constexpr double kGostTextSlantDeg = 75.0;

struct Text {
    Point2 position;  // точка привязки (по h/v выравниванию)
    std::string utf8;
    double height = 3.5;       // h из kGostTextHeights
    double rotationDeg = 0.0;  // поворот строки против часовой стрелки
    bool slanted = true;       // наклон 75° к основанию строки
    // Коэффициент ширины букв 0,5…1,0 (сжатие по горизонтали; DXF — группа 41,
    // PDF — масштаб в Tm, SVG — scale). Наклон 75° при сжатии сохраняется.
    double widthFactor = 1.0;
    HAlign hAlign = HAlign::Left;
    VAlign vAlign = VAlign::Baseline;
};

// Штриховка замкнутого контура (ГОСТ 2.306-68): параллельные линии под углом angleDeg
// с шагом spacing. Контуры обрабатываются по правилу чётности (вложенный контур — отверстие).
struct Hatch {
    std::vector<std::vector<Point2>> loops;
    double angleDeg = 45.0;
    double spacing = 2.0;  // 1…10 мм (ГОСТ 2.306)
};

// Залитый выпуклый многоугольник: стрелки размерных линий (ГОСТ 2.307), треугольник
// обозначения базы (ГОСТ 2.308), точка на конце линии-выноски (ГОСТ 2.316). 3…64 вершины,
// выпуклый, ненулевой площади. DXF — SOLID (веером треугольников), SVG — polygon с заливкой,
// PDF — оператор f.
struct SolidFill {
    std::vector<Point2> points;
};

using Geometry = std::variant<LineSeg, Arc, Circle, Polyline, Text, Hatch, SolidFill>;

struct Entity {
    Geometry geometry;
    std::string layer = "0";
    LineType lineType = LineType::SolidThick;
};

struct Layer {
    std::string name;  // ASCII: буквы, цифры, '_', '-'
    int aciColor = 7;  // цвет AutoCAD Color Index 1…255 (для DXF)
};

class Drawing {
public:
    // s — толщина сплошной толстой основной линии, 0,5…1,4 мм (ГОСТ 2.303).
    static Result<Drawing> create(Sheet sheet, double s = 0.5);

    const Sheet& sheet() const { return sheet_; }
    double mainThickness() const { return s_; }

    // Слой "0" существует всегда. Повторное имя — ошибка.
    Status addLayer(Layer layer);
    const std::vector<Layer>& layers() const { return layers_; }
    const Layer* findLayer(const std::string& name) const;

    // Замена параметров типа линии; проверяется на диапазоны ГОСТ 2.303.
    Status setLineTypeSpec(const LineTypeSpec& spec);
    const LineTypeSpec& lineTypeSpec(LineType type) const;
    // Толщина линии данного типа в мм.
    double lineWidth(LineType type) const;

    // Проверяет и добавляет примитив. Проверки: конечные координаты, слой существует,
    // радиусы > 0, полилиния ≥ 2 точек (замкнутая ≥ 3), текст — корректный UTF-8 без
    // управляющих символов, высотой из ряда ГОСТ 2.304 и коэффициентом ширины 0,5…1,0, штриховка — тонкой линией,
    // шаг 1…10 мм, контуры ≥ 3 точек; разомкнутая линия — только отрезок длиной 8…20 мм.
    Status add(Entity entity);
    const std::vector<Entity>& entities() const { return entities_; }

private:
    Drawing(Sheet sheet, double s);

    Sheet sheet_;
    double s_;
    std::vector<Layer> layers_;
    std::map<LineType, LineTypeSpec> specs_;
    std::vector<Entity> entities_;
};

// Сплошная волнистая линия от a до b: синусоида с амплитудой amplitude и длиной волны
// wavelength, аппроксимированная ломаной (32 точки на волну; число волн округляется до
// целого, чтобы концы лежали точно в a и b).
Result<Polyline> makeWavyLine(Point2 a, Point2 b, double amplitude = 1.0,
                              double wavelength = 8.0);

// Сплошная тонкая линия с изломами от a до b: count изломов, равномерно расположенных
// по длине; каждый излом — зигзаг шириной zigWidth вдоль линии и высотой ±zigHeight/2.
Result<Polyline> makeZigzagLine(Point2 a, Point2 b, int count = 1, double zigHeight = 5.0,
                                double zigWidth = 4.0);

// Отрезки штриховки после отсечения контурами (правило чётности). Линии проходят через
// точки (k + 1/2)·spacing по нормали к направлению штриховки от начала координат листа,
// поэтому соседние области одной детали штрихуются согласованно.
Result<std::vector<LineSeg>> hatchSegments(const Hatch& hatch);

}  // namespace bendcore::drawing_v2
