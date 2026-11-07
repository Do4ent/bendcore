#pragma once

// Типы линий ГОСТ 2.303-68, таблица 1: толщина относительно толщины s сплошной толстой
// основной линии и параметры штрихов.

#include <bendcore/drawing_v2/common.hpp>

#include <array>
#include <string>
#include <vector>

namespace bendcore::drawing_v2 {

enum class LineType {
    SolidThick,       // 1. Сплошная толстая основная, s = 0,5…1,4 мм
    SolidThin,        // 2. Сплошная тонкая, s/3…s/2
    SolidWavy,        // 3. Сплошная волнистая, s/3…s/2 (форма — геометрия, см. makeWavyLine)
    Dashed,           // 4. Штриховая, s/3…s/2; штрих 2…8, промежуток 1…2
    DashDotThin,      // 5. Штрихпунктирная тонкая, s/3…s/2; штрих 5…30, промежуток 3…5
    DashDotThick,     // 6. Штрихпунктирная утолщённая, s/2…2s/3; штрих 3…8, промежуток 3…4
    Open,             // 7. Разомкнутая, s…1,5s; длина штриха 8…20
    SolidThinZigzag,  // 8. Сплошная тонкая с изломами, s/3…s/2 (форма — makeZigzagLine)
    DashDotDot,       // 9. Штрихпунктирная с двумя точками тонкая, s/3…s/2;
                      //    штрих 5…30, промежуток 4…6
};

inline constexpr std::array<LineType, 9> kAllLineTypes = {
    LineType::SolidThick,  LineType::SolidThin,    LineType::SolidWavy,
    LineType::Dashed,      LineType::DashDotThin,  LineType::DashDotThick,
    LineType::Open,        LineType::SolidThinZigzag, LineType::DashDotDot};

struct LineTypeSpec {
    LineType type = LineType::SolidThick;
    std::string dxfName;   // имя в таблице LTYPE (ASCII)
    std::string gostName;  // наименование по ГОСТ 2.303 (UTF-8)

    // Толщина = widthFactor · s; допустимый диапазон по ГОСТ — [minFactor, maxFactor] · s.
    double widthFactor = 1.0;
    double minFactor = 1.0;
    double maxFactor = 1.0;

    // Шаблон штрихов, мм, в соглашении DXF: > 0 — штрих, < 0 — промежуток, 0 — точка.
    // Пустой шаблон — непрерывная линия.
    std::vector<double> pattern;

    // Диапазоны ГОСТ для проверки шаблона (0 — не нормируется).
    double dashMin = 0.0, dashMax = 0.0;  // длина штриха
    double gapMin = 0.0, gapMax = 0.0;    // расстояние между штрихами (вместе с точками)
    double strokeMin = 0.0, strokeMax = 0.0;  // длина штриха разомкнутой линии
};

// Значения по умолчанию (внутри диапазонов ГОСТ 2.303).
LineTypeSpec defaultLineTypeSpec(LineType type);

// Проверка шаблона и коэффициента толщины на соответствие диапазонам ГОСТ 2.303.
Status validateLineTypeSpec(const LineTypeSpec& spec);

// Проверка толщины s: 0,5…1,4 мм (ГОСТ 2.303).
Status validateMainThickness(double s);

// Длина штриха и расстояние между штрихами шаблона (для штрихпунктирных — промежуток
// между соседними штрихами, включая точки). Для непрерывных линий — {0, 0}.
struct PatternMetrics {
    double dash = 0.0;
    double gap = 0.0;
    int dots = 0;  // точек между соседними штрихами
    double period = 0.0;
};
PatternMetrics patternMetrics(const std::vector<double>& pattern);

// Шаблон для векторного вывода (SVG stroke-dasharray, PDF d): чередование штрих/промежуток,
// мм. Точка выводится штрихом длиной в толщину линии, соседние промежутки уменьшаются на
// половину этой длины, так что период шаблона сохраняется. Пустой — непрерывная линия.
std::vector<double> renderDashArray(const std::vector<double>& pattern, double lineWidth);

}  // namespace bendcore::drawing_v2
