// Разнесение полок номеров позиций (ГОСТ 2.109-73): одна линия, без пересечения линий-выносок,
// номера по порядку. Документация — docs/drawing/ANNOTATIONS.md, «Позиции».

#include <bendcore/drawing_v2/balloon_layout.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <numeric>
#include <optional>
#include <utility>

namespace bendcore::drawing_v2::annotations {

namespace {

double cross(Point2 o, Point2 a, Point2 b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); }

bool same(Point2 a, Point2 b) { return std::hypot(a.x - b.x, a.y - b.y) <= 1e-9; }

// Собственное пересечение отрезков (касание концом и общий конец не считаются).
bool segmentsCross(Point2 a, Point2 b, Point2 c, Point2 d) {
    if (same(a, c) || same(a, d) || same(b, c) || same(b, d)) return false;
    const double d1 = cross(c, d, a), d2 = cross(c, d, b), d3 = cross(a, b, c), d4 = cross(a, b, d);
    const double eps = 1e-12;
    return ((d1 > eps && d2 < -eps) || (d1 < -eps && d2 > eps)) && ((d3 > eps && d4 < -eps) || (d3 < -eps && d4 > eps));
}

double dist(Point2 a, Point2 b) { return std::hypot(a.x - b.x, a.y - b.y); }

Error invalid(std::string m) { return Error{ErrorCode::InvalidInput, std::move(m)}; }

bool boxesOverlap(const Rect& a, const Rect& b) {
    return std::min(a.x1, b.x1) > std::max(a.x0, b.x0) && std::min(a.y1, b.y1) > std::max(a.y0, b.y0);
}

Rect inflate(Rect r, double d) { return {r.x0 - d, r.y0 - d, r.x1 + d, r.y1 + d}; }

// Отрезок задевает прямоугольник (отсечение Лианга — Барски).
bool segmentHitsRect(Point2 p, Point2 q, const Rect& b) {
    double t0 = 0.0, t1 = 1.0;
    const double dx = q.x - p.x, dy = q.y - p.y;
    const double pp[4] = {-dx, dx, -dy, dy};
    const double qq[4] = {p.x - b.x0, b.x1 - p.x, p.y - b.y0, b.y1 - p.y};
    for (int i = 0; i < 4; ++i) {
        if (std::abs(pp[i]) < 1e-15) {
            if (qq[i] < 0) return false;
            continue;
        }
        const double r = qq[i] / pp[i];
        if (pp[i] < 0) t0 = std::max(t0, r);
        else t1 = std::min(t1, r);
        if (t0 > t1) return false;
    }
    return true;
}

bool rectBlocked(const Rect& r, const BalloonObstacles& obs) {
    for (const Rect& b : obs.boxes)
        if (boxesOverlap(r, b)) return true;
    for (const LineSeg& l : obs.lines)
        if (segmentHitsRect(l.a, l.b, r)) return true;
    return false;
}

// Пересечения линии-выноски с надписями и с линиями размеров.
std::pair<std::size_t, std::size_t> leaderHits(Point2 a, Point2 b, const BalloonObstacles& obs) {
    std::size_t texts = 0, lines = 0;
    for (const Rect& r : obs.boxes) texts += segmentHitsRect(a, b, r) ? 1 : 0;
    for (const LineSeg& l : obs.lines) lines += segmentsCross(a, b, l.a, l.b) ? 1 : 0;
    return {texts, lines};
}

}  // namespace

const char* toString(BalloonLine line) noexcept {
    switch (line) {
        case BalloonLine::Auto: return "auto";
        case BalloonLine::Right: return "right";
        case BalloonLine::Left: return "left";
        case BalloonLine::Above: return "above";
        case BalloonLine::Below: return "below";
    }
    return "auto";
}

void addDimensionObstacles(BalloonObstacles& obs, const DimensionGeometry& g, const AnnotationStyle& st) {
    for (const LineSeg& l : g.extensionLines) obs.lines.push_back(l);
    for (const LineSeg& l : g.dimensionLines) obs.lines.push_back(l);
    for (const Arc& a : g.dimensionArcs) {
        double sweep = a.endDeg - a.startDeg;
        while (sweep <= 0) sweep += 360.0;
        const int k = std::max(2, int(std::ceil(sweep / 10.0)));
        Point2 prev{};
        for (int i = 0; i <= k; ++i) {
            const double t = (a.startDeg + sweep * i / k) * std::numbers::pi / 180.0;
            const Point2 p{a.center.x + a.radius * std::cos(t), a.center.y + a.radius * std::sin(t)};
            if (i > 0) obs.lines.push_back({prev, p});
            prev = p;
        }
    }
    for (const views::Box2& b : labelBoxes(g, st))
        if (!b.empty) obs.boxes.push_back({b.minX, b.minY, b.maxX, b.maxY});
}

Rect balloonShelfBox(const Balloon& b, const BalloonOptions& o, const AnnotationStyle& st) {
    const double h = o.textHeight;
    const double w = std::max(estimateTextWidth(b.text, h, st.font) + 2.0, h);
    const bool right = b.at.x >= b.target.x;
    const double x0 = right ? b.at.x : b.at.x - w;
    return {x0, b.at.y, x0 + w, b.at.y + st.textGap + h};
}

bool leadersCross(const Balloon& a, const Balloon& b) { return segmentsCross(a.target, a.at, b.target, b.at); }

std::size_t leaderCrossings(const std::vector<Balloon>& balloons) {
    std::size_t n = 0;
    for (std::size_t i = 0; i < balloons.size(); ++i)
        for (std::size_t j = i + 1; j < balloons.size(); ++j) n += leadersCross(balloons[i], balloons[j]) ? 1 : 0;
    return n;
}


namespace {

using Leader = std::pair<Point2, Point2>;  // точка выноски → начало полки

bool isColumn(BalloonLine l) { return l == BalloonLine::Right || l == BalloonLine::Left; }

// Оценка раскладки — лексикографически: пересечения выносок, полки на препятствиях, выноски через
// размерные и выносные линии, через надписи и знаки, стоимость (длины выносок, сдвиги, линии).
struct Score {
    std::size_t crossings = 0;
    std::size_t shelfHits = 0;
    std::size_t lineHits = 0;
    std::size_t textHits = 0;
    double cost = 0;
    bool perfect() const { return crossings == 0 && shelfHits == 0 && lineHits == 0 && textHits == 0; }
    bool operator<(const Score& b) const {
        if (crossings != b.crossings) return crossings < b.crossings;
        if (shelfHits != b.shelfHits) return shelfHits < b.shelfHits;
        if (lineHits != b.lineHits) return lineHits < b.lineHits;
        if (textHits != b.textHits) return textHits < b.textHits;
        return cost < b.cost - 1e-9;
    }
};

// Уже поставленные линии полок: их выноски (пересечения) и полки (препятствия).
struct Env {
    std::vector<Leader> leaders;
    std::vector<Rect> shelves;
};

// Одна линия полок.
struct Line {
    BalloonLine side = BalloonLine::Right;
    std::vector<std::size_t> members;  // индексы кандидатов по возрастанию номеров
    std::vector<Point2> at;            // начало полки каждого члена
    std::vector<std::size_t> choice;   // точка выноски каждого члена
    std::vector<std::size_t> rank;     // место вдоль линии (0 — верхнее / левое)
    double shift = 0;                  // стоимость сдвигов линии и слотов
    Score score;
};

class Layout {
public:
    Layout(const std::vector<BalloonCandidate>& c, const BalloonArrangeOptions& o) : c_(c), o_(o) {
        h_ = o.balloon.textHeight;
        textTop_ = o.style.textGap + h_;
        gap_ = std::max(0.0, o.obstacleGap);
        width_.resize(c.size());
        for (std::size_t i = 0; i < c.size(); ++i)
            width_[i] = std::max(estimateTextWidth(std::to_string(c[i].number), h_, o.style.font) + 2.0, h_);
    }

    // Помещается ли линия полок со стороны side (место до рамки).
    bool sideFits(BalloonLine side, double maxW) const {
        const Rect& v = o_.view;
        const Rect& a = o_.area;
        const double room = o_.clearance + maxW + 5.0;
        switch (side) {
            case BalloonLine::Right: return v.x1 + room <= a.x1;
            case BalloonLine::Left: return v.x0 - room >= a.x0;
            case BalloonLine::Above: return v.y1 + o_.clearance + h_ + 3.0 <= a.y1;
            case BalloonLine::Below: return v.y0 - o_.clearance - 2.0 >= a.y0;
            default: return false;
        }
    }
    double maxWidth(const std::vector<std::size_t>& m) const {
        double w = h_;
        for (std::size_t i : m) w = std::max(w, width_[i]);
        return w;
    }

    // Линия полок side для членов members (по возрастанию номеров); allowReorder — порядок
    // уступает непересечению. Ошибка — полки не помещаются в area.
    Result<Line> line(BalloonLine side, std::vector<std::size_t> members, const Env& env, bool allowReorder) const {
        Line out;
        out.side = side;
        if (members.empty()) return out;
        std::stable_sort(members.begin(), members.end(), [&](std::size_t a, std::size_t b) { return c_[a].number < c_[b].number; });
        const std::size_t n = members.size();
        const bool column = isColumn(side);
        const double maxW = maxWidth(members);
        const Rect& v = o_.view;
        const Rect& area = o_.area;
        // Исходные слоты вдоль линии: s растёт с номером (колонна — s = −y, строка — s = x).
        std::vector<double> ideal(n);
        if (column) {
            double step = std::max(o_.step, h_ + 2.0);
            const double top = area.y1 - h_ - 1.0, bottom = area.y0 + 1.0;
            if (top - bottom < 0) return invalid("positions: no room for shelves");
            if (n > 1 && double(n - 1) * step > top - bottom) {
                step = (top - bottom) / double(n - 1);
                if (step < h_ + 2.0) return invalid("positions: " + std::to_string(n) + " shelves do not fit the column");
            }
            double yc = 0;
            for (std::size_t i : members) yc += mean(i).y;
            yc /= double(n);
            double y0 = yc + 0.5 * step * double(n - 1);
            y0 = std::min(y0, top);
            y0 = std::max(y0, bottom + step * double(n - 1));
            for (std::size_t k = 0; k < n; ++k) ideal[k] = -(y0 - step * double(k));
        } else {
            std::vector<double> cell(n);
            double total = 0;
            for (std::size_t k = 0; k < n; ++k) {
                cell[k] = 2.0 * width_[members[k]] + o_.rowGap;
                total += cell[k];
            }
            if (total > area.width()) return invalid("positions: " + std::to_string(n) + " shelves do not fit the row");
            double xc = 0;
            for (std::size_t i : members) xc += mean(i).x;
            xc /= double(n);
            double x0 = std::clamp(xc - total / 2.0, area.x0, area.x1 - total);
            for (std::size_t k = 0; k < n; ++k) {
                ideal[k] = x0 + cell[k] / 2.0;
                x0 += cell[k];
            }
        }
        const double L0 = side == BalloonLine::Right ? v.x1 + o_.clearance
                          : side == BalloonLine::Left ? v.x0 - o_.clearance
                          : side == BalloonLine::Above ? v.y1 + o_.clearance
                                                       : v.y0 - o_.clearance;
        const double Lin = side == BalloonLine::Right ? v.x1 + o_.minClearance
                           : side == BalloonLine::Left ? v.x0 - o_.minClearance
                           : side == BalloonLine::Above ? v.y1 + o_.minClearance
                                                        : v.y0 - o_.minClearance - textTop_;
        auto lineFits = [&](double L) {
            switch (side) {
                case BalloonLine::Right: return L + maxW + 1.0 <= area.x1 && L >= Lin - 1e-9;
                case BalloonLine::Left: return L - maxW - 1.0 >= area.x0 && L <= Lin + 1e-9;
                case BalloonLine::Above: return L + textTop_ + 1.0 <= area.y1 && L >= Lin - 1e-9;
                default: return L >= area.y0 + 1.0 && L <= Lin + 1e-9;
            }
        };
        // Линии-кандидаты: исходная, сразу за каждым препятствием наружу и перед ним (к изображению);
        // препятствия — вблизи слотов по проекции на линию.
        std::vector<double> lines{L0};
        auto consider = [&](double L) {
            if (lineFits(L)) lines.push_back(L);
        };
        const double sLo = ideal.front() - 30.0, sHi = ideal.back() + 30.0 + (column ? 0.0 : maxW);
        auto edges = [&](double minX, double minY, double maxX, double maxY) {
            const double lo = column ? -maxY : minX, hi = column ? -minY : maxX;
            if (hi < sLo || lo > sHi) return;
            switch (side) {
                case BalloonLine::Right: consider(maxX + gap_ + 1e-6), consider(minX - gap_ - maxW - 1e-6); break;
                case BalloonLine::Left: consider(minX - gap_ - 1e-6), consider(maxX + gap_ + maxW + 1e-6); break;
                case BalloonLine::Above: consider(maxY + gap_ + 1e-6), consider(minY - gap_ - textTop_ - 1e-6); break;
                default: consider(minY - gap_ - textTop_ - 1e-6), consider(maxY + gap_ + 1e-6); break;
            }
        };
        for (const Rect& b : o_.obstacles.boxes) edges(b.x0, b.y0, b.x1, b.y1);
        for (const Rect& b : env.shelves) edges(b.x0, b.y0, b.x1, b.y1);
        for (const LineSeg& l : o_.obstacles.lines)
            edges(std::min(l.a.x, l.b.x), std::min(l.a.y, l.b.y), std::max(l.a.x, l.b.x), std::max(l.a.y, l.b.y));
        std::stable_sort(lines.begin(), lines.end(), [&](double a, double b) { return std::abs(a - L0) < std::abs(b - L0); });
        {
            std::vector<double> uniq;
            for (double L : lines)
                if (std::none_of(uniq.begin(), uniq.end(), [&](double u) { return std::abs(u - L) < 0.25; })) uniq.push_back(L);
            lines = std::move(uniq);
            if (lines.size() > 24) lines.resize(24);
        }
        // Сдвиги всей линии слотов вдоль линии (выноска обходит надписи и размеры выбором места
        // полок): 0, ±15, ±30 мм в пределах area.
        const double sMin = column ? -(area.y1 - h_ - 1.0) : area.x0 + maxW;
        const double sMax = column ? -(area.y0 + 1.0) : area.x1 - maxW;
        bool have = false;
        for (double L : lines)
            for (double d : {0.0, -15.0, 15.0, -30.0, 30.0}) {
                if (d != 0.0 && (ideal.front() + d < sMin || ideal.back() + d > sMax)) continue;
                // Сдвиг линии — n · расстояние, сдвиг слотов вдоль линии — n · |d| / 2.
                const double lineShift = double(n) * (std::abs(L - L0) + 0.5 * std::abs(d));
                if (have && out.score.perfect() && lineShift >= out.score.cost) continue;
                std::vector<double> moved = ideal;
                for (double& x : moved) x += d;
                Line cand = place(side, members, moved, L, maxW, env, allowReorder);
                cand.shift += lineShift;
                cand.score.cost += lineShift;
                if (!have || cand.score < out.score) {
                    out = std::move(cand);
                    have = true;
                }
            }
        return out;
    }

    // Оценка всех линий вместе (пересечения между линиями, полки линий друг на друге).
    Score evaluate(const std::vector<Line>& ls) const {
        Score s;
        std::vector<Leader> all;
        std::vector<Rect> shelves;
        std::vector<std::size_t> lineOf;
        std::size_t used = 0;
        for (std::size_t li = 0; li < ls.size(); ++li) {
            const Line& l = ls[li];
            if (!l.members.empty()) ++used;
            s.cost += l.shift;
            for (std::size_t k = 0; k < l.members.size(); ++k) {
                const Point2 t = c_[l.members[k]].targets[l.choice[k]];
                all.push_back({t, l.at[k]});
                shelves.push_back(shelfBox(l.members[k], t, l.at[k]));
                lineOf.push_back(li);
                s.cost += dist(t, l.at[k]);
            }
        }
        if (used > 1) s.cost += double(used - 1) * o_.splitPenalty;
        for (std::size_t i = 0; i < all.size(); ++i) {
            for (std::size_t j = i + 1; j < all.size(); ++j)
                s.crossings += segmentsCross(all[i].first, all[i].second, all[j].first, all[j].second) ? 1 : 0;
            const auto [texts, lines] = leaderHits(all[i].first, all[i].second, o_.obstacles);
            s.textHits += texts;
            s.lineHits += lines;
            bool hit = rectBlocked(shelves[i], o_.obstacles);
            for (std::size_t j = 0; j < all.size(); ++j) {
                if (lineOf[j] == lineOf[i]) continue;
                hit = hit || boxesOverlap(shelves[i], shelves[j]);
                s.textHits += segmentHitsRect(all[i].first, all[i].second, inflate(shelves[j], -1e-6)) ? 1 : 0;
            }
            s.shelfHits += hit ? 1 : 0;
        }
        return s;
    }

    // Выноски и полки всех линий, кроме except.
    Env envOf(const std::vector<Line>& ls, std::size_t except) const {
        Env e;
        for (std::size_t li = 0; li < ls.size(); ++li) {
            if (li == except) continue;
            const Line& l = ls[li];
            for (std::size_t k = 0; k < l.members.size(); ++k) {
                const Point2 t = c_[l.members[k]].targets[l.choice[k]];
                e.leaders.push_back({t, l.at[k]});
                e.shelves.push_back(shelfBox(l.members[k], t, l.at[k]));
            }
        }
        return e;
    }

    // Расстояние от точки до стороны side габарита изображения.
    double toSide(Point2 p, BalloonLine side) const {
        const Rect& v = o_.view;
        switch (side) {
            case BalloonLine::Right: return std::max(0.0, v.x1 - p.x);
            case BalloonLine::Left: return std::max(0.0, p.x - v.x0);
            case BalloonLine::Above: return std::max(0.0, v.y1 - p.y);
            default: return std::max(0.0, p.y - v.y0);
        }
    }

    Rect shelfBox(std::size_t i, Point2 target, Point2 at) const {
        Balloon b;
        b.target = target;
        b.at = at;
        b.text = std::to_string(c_[i].number);
        return balloonShelfBox(b, o_.balloon, o_.style);
    }

private:
    Point2 mean(std::size_t i) const {
        Point2 m{0, 0};
        for (const Point2& p : c_[i].targets) m = {m.x + p.x, m.y + p.y};
        const double k = double(c_[i].targets.size());
        return {m.x / k, m.y / k};
    }

    Rect footprint(BalloonLine side, Point2 at, double maxW) const {
        if (side == BalloonLine::Right) return {at.x, at.y, at.x + maxW, at.y + textTop_};
        if (side == BalloonLine::Left) return {at.x - maxW, at.y, at.x, at.y + textTop_};
        return {at.x - maxW, at.y, at.x + maxW, at.y + textTop_};
    }

    // Слоты на линии L (сдвиги вдоль линии в обход препятствий) и точки выносок.
    Line place(BalloonLine side, const std::vector<std::size_t>& members, const std::vector<double>& ideal, double L,
               double maxW, const Env& env, bool allowReorder) const {
        const std::size_t n = members.size();
        const bool column = isColumn(side);
        auto pointAt = [&](double sv) { return column ? Point2{L, -sv} : Point2{sv, L}; };
        // Препятствия полосы линии.
        BalloonObstacles band;
        {
            const double big = 1e9;
            const Rect strip = column ? Rect{side == BalloonLine::Right ? L - gap_ : L - maxW - gap_, -big,
                                             side == BalloonLine::Right ? L + maxW + gap_ : L + gap_, big}
                                      : Rect{-big, L - gap_, big, L + textTop_ + gap_};
            const Rect wide = inflate(strip, 1e-6);
            for (const Rect& b : o_.obstacles.boxes)
                if (boxesOverlap(wide, b)) band.boxes.push_back(b);
            for (const Rect& b : env.shelves)
                if (boxesOverlap(wide, b)) band.boxes.push_back(b);
            for (const LineSeg& l : o_.obstacles.lines)
                if (segmentHitsRect(l.a, l.b, wide)) band.lines.push_back(l);
        }
        auto blocked = [&](Point2 at) { return rectBlocked(inflate(footprint(side, at, maxW), gap_), band); };
        std::vector<double> sv = ideal;
        double shift = 0;
        bool anyBlocked = false;
        for (double s : ideal) anyBlocked = anyBlocked || blocked(pointAt(s));
        if (anyBlocked) {
            // Слоты с наименьшей суммой |сдвигов| (порядок и промежутки не меньше исходных).
            std::vector<double> sep(n, 0.0);
            for (std::size_t k = 1; k < n; ++k) sep[k] = ideal[k] - ideal[k - 1];
            const double sMin = column ? -(o_.area.y1 - h_ - 1.0) : o_.area.x0 + maxW;
            const double sMax = column ? -(o_.area.y0 + 1.0) : o_.area.x1 - maxW;
            const double grid = 0.5, inf = 1e300;
            const long jLo = long(std::ceil((sMin - ideal[0]) / grid - 1e-9));
            const long jHi = long(std::floor((sMax - ideal[0]) / grid + 1e-9));
            double cost = inf;
            std::vector<double> best;
            if (jHi >= jLo) {
                const std::size_t G = std::size_t(jHi - jLo + 1);
                auto sAt = [&](std::size_t g) { return ideal[0] + double(jLo + long(g)) * grid; };
                std::vector<char> free(G);
                for (std::size_t g = 0; g < G; ++g) free[g] = !blocked(pointAt(sAt(g)));
                std::vector<std::vector<double>> dp(n, std::vector<double>(G, inf));
                std::vector<std::vector<std::size_t>> from(n, std::vector<std::size_t>(G, 0));
                for (std::size_t g = 0; g < G; ++g)
                    if (free[g]) dp[0][g] = std::abs(sAt(g) - ideal[0]);
                for (std::size_t k = 1; k < n; ++k) {
                    const std::size_t d = std::size_t(std::max(1.0, std::ceil(sep[k] / grid - 1e-9)));
                    double runMin = inf;  // min dp[k-1][g'] по g' ≤ g − d
                    std::size_t runArg = 0;
                    for (std::size_t g = d; g < G; ++g) {
                        if (dp[k - 1][g - d] < runMin) {
                            runMin = dp[k - 1][g - d];
                            runArg = g - d;
                        }
                        if (!free[g] || runMin >= inf) continue;
                        dp[k][g] = runMin + std::abs(sAt(g) - ideal[k]);
                        from[k][g] = runArg;
                    }
                }
                std::size_t arg = 0;
                for (std::size_t g = 0; g < G; ++g)
                    if (dp[n - 1][g] < cost) {
                        cost = dp[n - 1][g];
                        arg = g;
                    }
                if (cost < inf) {
                    best.assign(n, 0.0);
                    for (std::size_t k = n; k-- > 0;) {
                        best[k] = sAt(arg);
                        if (k > 0) arg = from[k][arg];
                    }
                }
            }
            if (!best.empty()) {
                sv = std::move(best);
                shift = cost;
            }
        }
        std::vector<Point2> slots(n);
        for (std::size_t k = 0; k < n; ++k) slots[k] = pointAt(sv[k]);

        // slotOf[k] — слот члена k; choice[k] — его точка выноски.
        std::vector<std::size_t> slotOf(n), choice(n, 0);
        std::iota(slotOf.begin(), slotOf.end(), 0);
        auto target = [&](std::size_t k, std::size_t t) { return c_[members[k]].targets[t]; };
        auto crosses = [&](Point2 a, Point2 b, std::size_t skip) {
            std::size_t c = 0;
            for (std::size_t j = 0; j < n; ++j)
                if (j != skip) c += segmentsCross(a, b, target(j, choice[j]), slots[slotOf[j]]) ? 1 : 0;
            for (const Leader& l : env.leaders) c += segmentsCross(a, b, l.first, l.second) ? 1 : 0;
            return c;
        };
        auto total = [&]() {
            std::size_t c = 0;
            for (std::size_t i = 0; i < n; ++i) {
                const Point2 a = target(i, choice[i]), b = slots[slotOf[i]];
                for (std::size_t j = i + 1; j < n; ++j) c += segmentsCross(a, b, target(j, choice[j]), slots[slotOf[j]]) ? 1 : 0;
                for (const Leader& l : env.leaders) c += segmentsCross(a, b, l.first, l.second) ? 1 : 0;
            }
            return c;
        };
        // Пересечения выноски с размерными и выносными линиями, с надписями, знаками и полками.
        auto hits = [&](Point2 a, Point2 b) {
            auto [texts, lines] = leaderHits(a, b, o_.obstacles);
            for (const Rect& r : env.shelves) texts += segmentHitsRect(a, b, r) ? 1 : 0;
            return std::make_pair(lines, texts);
        };
        using Hits = std::pair<std::size_t, std::size_t>;
        auto better = [](std::size_t c, Hits h, double l, std::size_t bc, Hits bh, double bl) {
            return c < bc || (c == bc && (h < bh || (h == bh && l < bl - 1e-9)));
        };
        auto improve = [&]() {
            for (std::size_t k = 0; k < n; ++k) {
                Hits bh{~std::size_t(0), 0};
                double bl = 1e300;
                for (std::size_t t = 0; t < c_[members[k]].targets.size(); ++t) {
                    const Hits hh = hits(target(k, t), slots[slotOf[k]]);
                    const double l = dist(target(k, t), slots[slotOf[k]]);
                    if (hh < bh || (hh == bh && l < bl)) bh = hh, bl = l, choice[k] = t;
                }
            }
            for (int pass = 0; pass < 50; ++pass) {
                bool changed = false;
                for (std::size_t k = 0; k < n; ++k) {
                    const Point2 at = slots[slotOf[k]];
                    std::size_t bc = crosses(target(k, choice[k]), at, k);
                    Hits bh = hits(target(k, choice[k]), at);
                    double bl = dist(target(k, choice[k]), at);
                    for (std::size_t t = 0; t < c_[members[k]].targets.size(); ++t) {
                        if (t == choice[k]) continue;
                        const Point2 p = target(k, t);
                        const std::size_t cc = crosses(p, at, k);
                        const Hits hh = hits(p, at);
                        const double l = dist(p, at);
                        if (better(cc, hh, l, bc, bh, bl)) {
                            bc = cc, bh = hh, bl = l;
                            choice[k] = t;
                            changed = true;
                        }
                    }
                }
                if (!changed) break;
            }
            // Пары пересекающихся выносок: одновременная смена точек обеих (по отдельности каждая
            // из двух точек может пересекать выноску другой).
            for (std::size_t iter = 0; iter < 4 * n + 8; ++iter) {
                const std::size_t before = total();
                if (before == 0) break;
                bool improved = false;
                for (std::size_t i = 0; i < n && !improved; ++i)
                    for (std::size_t j = i + 1; j < n && !improved; ++j) {
                        if (!segmentsCross(target(i, choice[i]), slots[slotOf[i]], target(j, choice[j]), slots[slotOf[j]])) continue;
                        std::size_t bi = choice[i], bj = choice[j], best = before;
                        double bestLen = 1e300;
                        for (std::size_t ti = 0; ti < c_[members[i]].targets.size(); ++ti)
                            for (std::size_t tj = 0; tj < c_[members[j]].targets.size(); ++tj) {
                                choice[i] = ti;
                                choice[j] = tj;
                                const std::size_t cc = total();
                                const double len = dist(target(i, ti), slots[slotOf[i]]) + dist(target(j, tj), slots[slotOf[j]]);
                                if (cc < best || (cc == best && cc < before && len < bestLen)) best = cc, bestLen = len, bi = ti, bj = tj;
                            }
                        choice[i] = bi;
                        choice[j] = bj;
                        improved = best < before;
                    }
                if (!improved) break;
            }
        };
        improve();
        if (allowReorder && total() > 0) {
            // Порядок номеров уступает непересечению: слоты — по положению точек вдоль линии, затем
            // обмен слотами пересекающихся выносок.
            std::vector<std::size_t> byPos(n);
            std::iota(byPos.begin(), byPos.end(), 0);
            std::stable_sort(byPos.begin(), byPos.end(), [&](std::size_t a, std::size_t b) {
                const Point2 pa = target(a, choice[a]), pb = target(b, choice[b]);
                return column ? pa.y > pb.y : pa.x < pb.x;
            });
            for (std::size_t k = 0; k < n; ++k) slotOf[byPos[k]] = k;
            for (std::size_t iter = 0; iter < 4 * n * n + 16; ++iter) {
                bool swapped = false;
                for (std::size_t i = 0; i < n && !swapped; ++i)
                    for (std::size_t j = i + 1; j < n && !swapped; ++j)
                        if (segmentsCross(target(i, choice[i]), slots[slotOf[i]], target(j, choice[j]), slots[slotOf[j]])) {
                            std::swap(slotOf[i], slotOf[j]);
                            swapped = true;
                        }
                if (!swapped) break;
            }
        }
        Line out;
        out.side = side;
        out.members = members;
        out.shift = shift;
        out.choice = choice;
        out.at.resize(n);
        out.rank.resize(n);
        for (std::size_t k = 0; k < n; ++k) {
            out.at[k] = slots[slotOf[k]];
            out.rank[k] = slotOf[k];
        }
        out.score.crossings = total();
        out.score.cost = shift;
        for (std::size_t k = 0; k < n; ++k) {
            const Point2 a = target(k, choice[k]), b = out.at[k];
            const auto [lines, texts] = hits(a, b);
            out.score.lineHits += lines;
            out.score.textHits += texts;
            out.score.cost += dist(a, b);
            const Rect box = shelfBox(members[k], a, b);
            bool hit = rectBlocked(box, o_.obstacles);
            for (const Rect& r : env.shelves) hit = hit || boxesOverlap(box, r);
            out.score.shelfHits += hit ? 1 : 0;
        }
        return out;
    }

    const std::vector<BalloonCandidate>& c_;
    const BalloonArrangeOptions& o_;
    std::vector<double> width_;
    double h_ = 3.5, textTop_ = 4.5, gap_ = 1.0;
};

}  // namespace

Result<BalloonArrangement> arrangeBalloons(const std::vector<BalloonCandidate>& cands, const BalloonArrangeOptions& o) {
    BalloonArrangement out;
    const std::size_t n = cands.size();
    if (n == 0) return out;
    for (const BalloonCandidate& c : cands)
        if (c.targets.empty()) return invalid("positions: part " + std::to_string(c.definition) + " has no leader point");
    Layout lay(cands, o);
    std::vector<std::size_t> all(n);
    std::iota(all.begin(), all.end(), 0);
    const double maxW = lay.maxWidth(all);
    // Стороны вида, где помещается линия полок, в порядке предпочтения.
    const bool autoLine = o.line == BalloonLine::Auto;
    std::vector<BalloonLine> sides;
    if (autoLine) {
        for (BalloonLine s : {BalloonLine::Right, BalloonLine::Left, BalloonLine::Above, BalloonLine::Below})
            if (lay.sideFits(s, maxW)) sides.push_back(s);
        if (sides.empty()) sides.push_back(BalloonLine::Right);
    } else {
        sides.push_back(o.line);
    }
    const BalloonLine primary = sides.front();

    std::vector<Line> best;
    Score bestScore;
    std::optional<Error> firstError;
    auto consider = [&](std::vector<Line> ls) {
        const Score s = lay.evaluate(ls);
        if (best.empty() || s < bestScore) {
            best = std::move(ls);
            bestScore = s;
        }
    };
    // Одна линия: основная сторона, затем остальные, если основная не даёт чистой раскладки.
    for (BalloonLine s : sides) {
        if (!best.empty() && bestScore.perfect()) break;
        auto l = lay.line(s, all, {}, false);
        if (!l) {
            if (!firstError) firstError = l.error();
            continue;
        }
        consider({std::move(l).value()});
    }
    // Несколько линий по сторонам вида: каждая позиция — к стороне, ближайшей к её точкам; затем
    // перенос позиций между сторонами, пока раскладка улучшается.
    if (autoLine && o.split && sides.size() > 1 && (best.empty() || !bestScore.perfect())) {
        std::vector<std::size_t> assign(n, 0);
        for (std::size_t i = 0; i < n; ++i) {
            double bd = 1e300;
            for (std::size_t s = 0; s < sides.size(); ++s)
                for (const Point2& p : cands[i].targets) {
                    const double d = lay.toSide(p, sides[s]);
                    if (d < bd - 1e-9) bd = d, assign[i] = s;
                }
        }
        auto build = [&](const std::vector<std::size_t>& as, std::vector<Line>& ls) -> bool {
            ls.assign(sides.size(), Line{});
            for (std::size_t s = 0; s < sides.size(); ++s) ls[s].side = sides[s];
            for (int pass = 0; pass < 2; ++pass)
                for (std::size_t s = 0; s < sides.size(); ++s) {
                    std::vector<std::size_t> m;
                    for (std::size_t i = 0; i < n; ++i)
                        if (as[i] == s) m.push_back(i);
                    auto l = lay.line(sides[s], m, lay.envOf(ls, s), false);
                    if (!l) return false;
                    ls[s] = std::move(l).value();
                }
            return true;
        };
        // Локальный поиск от начального распределения: перенос одной позиции на другую сторону.
        auto search = [&](std::vector<std::size_t> as0) {
            std::vector<Line> cur;
            if (!build(as0, cur)) return;
            Score curScore = lay.evaluate(cur);
            for (int pass = 0; pass < 5 && !curScore.perfect(); ++pass) {
                bool moved = false;
                for (std::size_t i = 0; i < n; ++i)
                    for (std::size_t s = 0; s < sides.size(); ++s) {
                        if (s == as0[i]) continue;
                        std::vector<std::size_t> as = as0;
                        as[i] = s;
                        std::vector<Line> trial;
                        if (!build(as, trial)) continue;
                        const Score ts = lay.evaluate(trial);
                        if (ts < curScore) {
                            as0 = std::move(as);
                            cur = std::move(trial);
                            curScore = ts;
                            moved = true;
                        }
                    }
                if (!moved) break;
            }
            consider(std::move(cur));
        };
        // Начала: ближайшая сторона; ближайшая из двух первых сторон (колонна и противоположная
        // ей); все на основной стороне.
        search(assign);
        if (!bestScore.perfect() && sides.size() > 2) {
            std::vector<std::size_t> two(n, 0);
            for (std::size_t i = 0; i < n; ++i) {
                double bd = 1e300;
                for (std::size_t s = 0; s < 2; ++s)
                    for (const Point2& p : cands[i].targets) {
                        const double d = lay.toSide(p, sides[s]);
                        if (d < bd - 1e-9) bd = d, two[i] = s;
                    }
            }
            if (two != assign) search(two);
        }
        if (!bestScore.perfect()) search(std::vector<std::size_t>(n, 0));
    }
    if (best.empty()) return firstError ? *firstError : invalid("positions: no room for shelves");
    // Пересечения остались — порядок номеров уступает непересечению на этих линиях.
    if (bestScore.crossings > 0) {
        std::vector<Line> trial = best;
        for (std::size_t li = 0; li < trial.size(); ++li) {
            if (trial[li].members.empty()) continue;
            auto l = lay.line(trial[li].side, trial[li].members, lay.envOf(trial, li), true);
            if (l) trial[li] = std::move(l).value();
        }
        const Score ts = lay.evaluate(trial);
        if (ts.crossings < bestScore.crossings) {
            best = std::move(trial);
            bestScore = ts;
        }
    }

    out.balloons.resize(n);
    out.lines.assign(n, primary);
    std::map<BalloonLine, std::size_t> perSide;
    out.lineCount = 0;
    for (const Line& l : best) {
        if (l.members.empty()) continue;
        ++out.lineCount;
        perSide[l.side] += l.members.size();
        for (std::size_t k = 0; k < l.members.size(); ++k) {
            const std::size_t i = l.members[k];
            Balloon& b = out.balloons[i];
            b.definition = cands[i].definition;
            b.target = cands[i].targets[l.choice[k]];
            b.at = l.at[k];
            b.text = std::to_string(cands[i].number);
            out.lines[i] = l.side;
            // members — по возрастанию номеров: место по порядку — k, фактическое — rank[k].
            if (l.rank[k] != k) out.outOfOrder.push_back(cands[i].number);
        }
    }
    std::size_t most = 0;
    for (BalloonLine s : sides)
        if (perSide[s] > most) most = perSide[s], out.line = s;
    std::sort(out.outOfOrder.begin(), out.outOfOrder.end());
    out.ordered = out.outOfOrder.empty();
    out.crossings = bestScore.crossings;
    if (!o.obstacles.empty())
        for (const Balloon& b : out.balloons) {
            if (rectBlocked(balloonShelfBox(b, o.balloon, o.style), o.obstacles)) ++out.shelfHits;
            const auto [texts, lines] = leaderHits(b.target, b.at, o.obstacles);
            out.leaderTextHits += texts;
            out.leaderLineHits += lines;
        }
    return out;
}

}  // namespace bendcore::drawing_v2::annotations
