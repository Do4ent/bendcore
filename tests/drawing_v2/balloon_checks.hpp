#pragma once

// Геометрические проверки раскладки позиций (ГОСТ 2.109-73): номера в каждой колонне и строке
// полок по возрастанию, линии-выноски попарно не пересекаются, не пересекают размерные и выносные
// линии, полки не налезают на буквы обозначений и знаки. Проверки независимы от реализации
// arrangeBalloons: разбирают только итоговые отрезки выносок и точки полок.

#include <bendcore/drawing_v2/balloon_layout.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace bendcore::drawing_v2::test_balloons {

inline double crossZ(Point2 o, Point2 a, Point2 b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); }

// Собственное пересечение отрезков (касание концом не считается).
inline bool segmentsCross(Point2 a, Point2 b, Point2 c, Point2 d) {
    const double e = 1e-9;
    const double d1 = crossZ(c, d, a), d2 = crossZ(c, d, b), d3 = crossZ(a, b, c), d4 = crossZ(a, b, d);
    return ((d1 > e && d2 < -e) || (d1 < -e && d2 > e)) && ((d3 > e && d4 < -e) || (d3 < -e && d4 > e));
}

// Отрезок проходит через внутренность прямоугольника.
inline bool segmentHitsRect(Point2 p, Point2 q, const Rect& r) {
    double t0 = 0, t1 = 1;
    const double dx = q.x - p.x, dy = q.y - p.y;
    const double pp[4] = {-dx, dx, -dy, dy};
    const double qq[4] = {p.x - r.x0, r.x1 - p.x, p.y - r.y0, r.y1 - p.y};
    for (int i = 0; i < 4; ++i) {
        if (std::abs(pp[i]) < 1e-15) {
            if (qq[i] <= 0) return false;
            continue;
        }
        const double t = qq[i] / pp[i];
        if (pp[i] < 0) t0 = std::max(t0, t);
        else t1 = std::min(t1, t);
        if (t0 >= t1) return false;
    }
    return t1 - t0 > 1e-9;
}

inline bool rectsOverlap(const Rect& a, const Rect& b) {
    return std::min(a.x1, b.x1) > std::max(a.x0, b.x0) && std::min(a.y1, b.y1) > std::max(a.y0, b.y0);
}

// Линия полок: колонна (общая абсцисса начала полки) или строка (общая ордината).
struct ShelfLine {
    std::string view;
    bool column = true;
    std::vector<const annotations::Balloon*> balloons;  // вдоль линии: колонна — сверху вниз, строка — слева направо
};

// Линии полок по видам: позиции вида с одинаковой абсциссой начала полки (≥ 2) — колонна,
// оставшиеся с одинаковой ординатой — строка, остальные — одиночные.
inline std::vector<ShelfLine> shelfLines(const std::vector<annotations::Balloon>& balloons,
                                         const std::vector<std::string>& viewOf) {
    std::map<std::string, std::vector<const annotations::Balloon*>> byView;
    for (std::size_t i = 0; i < balloons.size(); ++i) byView[i < viewOf.size() ? viewOf[i] : std::string()].push_back(&balloons[i]);
    std::vector<ShelfLine> out;
    auto key = [](double v) { return long(std::lround(v * 100.0)); };
    for (auto& [view, list] : byView) {
        std::map<long, std::vector<const annotations::Balloon*>> byX, byY;
        for (const auto* b : list) byX[key(b->at.x)].push_back(b);
        std::vector<const annotations::Balloon*> rest;
        for (auto& [x, col] : byX) {
            if (col.size() < 2) {
                rest.push_back(col.front());
                continue;
            }
            std::sort(col.begin(), col.end(), [](auto* a, auto* b) { return a->at.y > b->at.y; });
            out.push_back({view, true, col});
        }
        for (const auto* b : rest) byY[key(b->at.y)].push_back(b);
        for (auto& [y, row] : byY) {
            std::sort(row.begin(), row.end(), [](auto* a, auto* b) { return a->at.x < b->at.x; });
            out.push_back({view, false, row});
        }
    }
    return out;
}

// Номера, стоящие не по порядку на своей линии полок («вид: 5 после 8»).
inline std::vector<std::string> unorderedNumbers(const std::vector<ShelfLine>& lines) {
    std::vector<std::string> out;
    for (const ShelfLine& l : lines)
        for (std::size_t k = 1; k < l.balloons.size(); ++k)
            if (std::stoi(l.balloons[k]->text) < std::stoi(l.balloons[k - 1]->text))
                out.push_back(l.view + ": " + l.balloons[k]->text + " after " + l.balloons[k - 1]->text);
    return out;
}

// Пары пересекающихся линий-выносок («3 x 7»).
inline std::vector<std::string> crossingLeaders(const std::vector<annotations::Balloon>& balloons) {
    std::vector<std::string> out;
    for (std::size_t i = 0; i < balloons.size(); ++i)
        for (std::size_t j = i + 1; j < balloons.size(); ++j)
            if (segmentsCross(balloons[i].target, balloons[i].at, balloons[j].target, balloons[j].at))
                out.push_back(balloons[i].text + " x " + balloons[j].text);
    return out;
}

// Выноски, пересекающие отрезки (размерные и выносные линии): «номер».
inline std::vector<std::string> leadersCrossingLines(const std::vector<annotations::Balloon>& balloons,
                                                     const std::vector<LineSeg>& lines) {
    std::vector<std::string> out;
    for (const auto& b : balloons)
        for (const LineSeg& l : lines)
            if (segmentsCross(b.target, b.at, l.a, l.b)) {
                out.push_back(b.text);
                break;
            }
    return out;
}

// Полки (с номером), налезающие на прямоугольники (буквы, знаки) или пересекаемые отрезками.
inline std::vector<std::string> shelvesHitting(const std::vector<annotations::Balloon>& balloons,
                                               const std::vector<Rect>& boxes, const std::vector<LineSeg>& lines,
                                               const annotations::AnnotationStyle& style) {
    std::vector<std::string> out;
    for (const auto& b : balloons) {
        const Rect box = annotations::balloonShelfBox(b, {}, style);
        bool hit = false;
        for (const Rect& r : boxes) hit = hit || rectsOverlap(box, r);
        for (const LineSeg& l : lines) hit = hit || segmentHitsRect(l.a, l.b, box);
        if (hit) out.push_back(b.text);
    }
    return out;
}

inline std::string join(const std::vector<std::string>& v) {
    std::string s;
    for (const auto& x : v) s += (s.empty() ? "" : "; ") + x;
    return s;
}

}  // namespace bendcore::drawing_v2::test_balloons
