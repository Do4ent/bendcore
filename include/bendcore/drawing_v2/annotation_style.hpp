#pragma once

// Общие параметры оформления размеров и аннотаций (ГОСТ 2.303, 2.304, 2.307) и оценка
// ширины текста. Документация — docs/drawing/ANNOTATIONS.md.

#include <bendcore/drawing_v2/model.hpp>
#include <bendcore/drawing_v2/truetype_font.hpp>

#include <array>
#include <string>
#include <string_view>

namespace bendcore::drawing_v2::annotations {

struct AnnotationStyle {
    // Размерные числа — шрифт ГОСТ 2.304 высотой 3,5 (ГОСТ 2.307 рекомендует не менее 3,5).
    double textHeight = 3.5;
    // Предельные отклонения — на ступень ряда ГОСТ 2.304 меньше размерного числа.
    double toleranceTextHeight = 2.5;
    bool slantedText = true;
    // Стрелка размерной линии: длина и полный угол при вершине. ГОСТ 2.307 показывает стрелку
    // на рисунке и требует выбирать её размер по толщине основной линии; принятые значения
    // требуют сверки (docs/drawing/ANNOTATIONS.md).
    double arrowLength = 4.0;
    double arrowAngleDeg = 20.0;
    // Выход выносной линии за конец стрелки — 1…5 мм (ГОСТ 2.307).
    double extensionOvershoot = 2.0;
    // Зазор между контуром и началом выносной линии: ЕСКД — 0, ISO 129-1 допускает зазор.
    double extensionGap = 0.0;
    // Зазор между размерной линией и основанием размерного числа.
    double textGap = 1.0;
    // Расстояние размерной линии от контура — не менее 10 мм, между параллельными размерными
    // линиями — не менее 7 мм (ГОСТ 2.307).
    double firstLineOffset = 10.0;
    double lineSpacing = 7.0;
    // Не писать нулевое предельное отклонение (ЕСКД); false — писать «0» (ISO 129-1).
    bool omitZeroDeviation = true;
    std::string layer = "DIM";
    int layerColor = 3;
    // Шрифт для ширины текста (полки, рамки допусков). Без шрифта ширина оценивается по
    // пропорциям шрифта типа Б ГОСТ 2.304 (см. estimateTextWidth).
    const TrueTypeFont* font = nullptr;
};

// Ширина строки высотой h, мм. Со шрифтом — по метрикам шрифта (без кернинга), умноженная на
// widthFactor. Без шрифта — шрифт типа Б ГОСТ 2.304: ширина прописных и цифр 6/10 h,
// строчных 5/10 h, расстояние между буквами 2/10 h, пробел 6/10 h (оценка: пропорции
// отдельных букв не учитываются).
double estimateTextWidth(std::string_view utf8, double h, const TrueTypeFont* font, double widthFactor = 1.0);

// Создаёт слой, если его нет.
Status ensureLayer(Drawing& drawing, const std::string& name, int aci);

// Залитая стрелка: вершина tip, направление direction (от основания к вершине, любой длины).
SolidFill arrowHead(Point2 tip, Point2 direction, double length, double fullAngleDeg);

// Число по ЕСКД: десятичная запятая, не более decimals знаков, без хвостовых нулей
// (ГОСТ 2.307: линейные размеры в мм без обозначения единицы).
std::string formatNumber(double value, int decimals = 2);

// Угол в градусах как «30°», «12°30′», «0°45′30″» (ГОСТ 2.307: градусы, минуты, секунды с
// обозначением единицы). Округление до seconds = false — до минут, иначе до секунд.
std::string formatAngle(double degrees, bool seconds = false);

}  // namespace bendcore::drawing_v2::annotations
