#pragma once

// Внутренние функции писателей: форматирование чисел, раскладка текста, дуги в Безье.

#include <bendcore/drawing_v2/model.hpp>

#include <string>
#include <vector>

namespace bendcore::drawing_v2::detail {

// Число в фиксированной записи без экспоненты (не зависит от локали), не более
// decimals знаков после точки, без хвостовых нулей; "-0" выводится как "0".
std::string num(double v, int decimals = 6);

struct Vec2 {
    double x, y;
};

// Начало базовой линии строки с учётом выравнивания. width — ширина строки в мм
// (нужна для Center/Right), h — высота прописных.
Point2 textBaselineOrigin(const Text& t, double width);

// Смещение базовой линии по вертикали (в системе строки, ось вверх) для выравнивания.
double textBaselineShift(VAlign v, double h);

// Кубические сегменты Безье дуги (против часовой стрелки от a0 до a1, радианы); каждый
// сегмент не больше 90°. Возвращает 1 + 3n точек: начало и по три точки на сегмент.
std::vector<Point2> arcToBezier(Point2 c, double r, double a0, double a1);

// Угол дуги (градусы, 0 < sweep ≤ 360) от start до end против часовой стрелки.
double arcSweepDeg(double startDeg, double endDeg);

}  // namespace bendcore::drawing_v2::detail
