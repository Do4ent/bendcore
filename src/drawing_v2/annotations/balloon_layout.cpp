// Разнесение полок номеров позиций (ГОСТ 2.109-73): одна линия, без пересечения линий-выносок,
// номера по порядку. Документация — docs/drawing/ANNOTATIONS.md, «Позиции».

#include <bendcore/drawing_v2/balloon_layout.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
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

Result<BalloonArrangement> arrangeBalloons(const std::vector<BalloonCandidate>& cands, const BalloonArrangeOptions& o) {
    BalloonArrangement out;
    const std::size_t n = cands.size();
    if (n == 0) return out;
    for (const BalloonCandidate& c : cands)
        if (c.targets.empty()) return invalid("positions: part " + std::to_string(c.definition) + " has no leader point");
    const double h = o.balloon.textHeight;
    std::vector<double> width(n);
    for (std::size_t i = 0; i < n; ++i)
        width[i] = std::max(estimateTextWidth(std::to_string(cands[i].number), h, o.style.font) + 2.0, h);
    const double maxW = *std::max_element(width.begin(), width.end());

    // Линия полок.
    BalloonLine line = o.line;
    if (line == BalloonLine::Auto) {
        const double room = o.clearance + maxW + 5.0;
        if (o.view.x1 + room <= o.area.x1) line = BalloonLine::Right;
        else if (o.view.x0 - room >= o.area.x0) line = BalloonLine::Left;
        else if (o.view.y1 + o.clearance + h + 3.0 <= o.area.y1) line = BalloonLine::Above;
        else if (o.view.y0 - o.clearance - 2.0 >= o.area.y0) line = BalloonLine::Below;
        else line = BalloonLine::Right;
    }
    out.line = line;
    const bool column = line == BalloonLine::Right || line == BalloonLine::Left;

    // Порядок слотов — по номерам.
    std::vector<std::size_t> byNumber(n);
    std::iota(byNumber.begin(), byNumber.end(), 0);
    std::stable_sort(byNumber.begin(), byNumber.end(), [&](std::size_t a, std::size_t b) { return cands[a].number < cands[b].number; });

    // Слоты (точки начала полок) вдоль линии: k = 0 — первый номер.
    std::vector<Point2> slots(n);
    if (column) {
        const double x = line == BalloonLine::Right ? o.view.x1 + o.clearance : o.view.x0 - o.clearance;
        double step = std::max(o.step, h + 2.0);
        const double top = o.area.y1 - h - 1.0, bottom = o.area.y0 + 1.0;
        if (top - bottom < 0) return invalid("positions: no room for shelves");
        if (n > 1 && (n - 1) * step > top - bottom) {
            step = (top - bottom) / double(n - 1);
            if (step < h + 2.0) return invalid("positions: " + std::to_string(n) + " shelves do not fit the column");
        }
        double yc = 0;
        for (const BalloonCandidate& c : cands) yc += c.targets.front().y;
        yc /= double(n);
        double y0 = yc + 0.5 * step * double(n - 1);
        y0 = std::min(y0, top);
        y0 = std::max(y0, bottom + step * double(n - 1));
        for (std::size_t k = 0; k < n; ++k) slots[k] = {x, y0 - step * double(k)};
    } else {
        const double y = line == BalloonLine::Above ? o.view.y1 + o.clearance : o.view.y0 - o.clearance;
        // Ячейка слота — две ширины полки: начало полки посередине, полка влево или вправо.
        std::vector<double> cell(n);
        double total = 0;
        for (std::size_t k = 0; k < n; ++k) {
            cell[k] = 2.0 * width[byNumber[k]] + o.rowGap;
            total += cell[k];
        }
        if (total > o.area.width()) return invalid("positions: " + std::to_string(n) + " shelves do not fit the row");
        double xc = 0;
        for (const BalloonCandidate& c : cands) xc += c.targets.front().x;
        xc /= double(n);
        double x0 = std::clamp(xc - total / 2.0, o.area.x0, o.area.x1 - total);
        for (std::size_t k = 0; k < n; ++k) {
            slots[k] = {x0 + cell[k] / 2.0, y};
            x0 += cell[k];
        }
    }

    // Препятствия: слоты сдвигаются вдоль линии, при необходимости линия — наружу.
    const BalloonObstacles& obs = o.obstacles;
    const double gap = std::max(0.0, o.obstacleGap);
    const double textTop = o.style.textGap + h;
    // Координата вдоль линии s растёт с номером слота: колонна — s = −y, строка — s = x;
    // поперёк — L (колонна — x, строка — y).
    auto pointAt = [&](double L, double sv) { return column ? Point2{L, -sv} : Point2{sv, L}; };
    auto footprint = [&](Point2 at) -> Rect {
        if (line == BalloonLine::Right) return {at.x, at.y, at.x + maxW, at.y + textTop};
        if (line == BalloonLine::Left) return {at.x - maxW, at.y, at.x, at.y + textTop};
        return {at.x - maxW, at.y, at.x + maxW, at.y + textTop};
    };
    auto blocked = [&](Point2 at) { return rectBlocked(inflate(footprint(at), gap), obs); };
    bool anyBlocked = false;
    for (const Point2& p : slots) anyBlocked = anyBlocked || blocked(p);
    if (!obs.empty() && anyBlocked) {
        std::vector<double> ideal(n), sep(n, 0.0);
        for (std::size_t k = 0; k < n; ++k) ideal[k] = column ? -slots[k].y : slots[k].x;
        for (std::size_t k = 1; k < n; ++k) sep[k] = ideal[k] - ideal[k - 1];
        const double L0 = column ? slots[0].x : slots[0].y;
        const double sMin = column ? -(o.area.y1 - h - 1.0) : o.area.x0 + maxW;
        const double sMax = column ? -(o.area.y0 + 1.0) : o.area.x1 - maxW;
        const double grid = 0.5;
        const double inf = 1e300;
        // Слоты на линии L с наименьшей суммой |сдвигов| (порядок и шаг не меньше исходного);
        // нет размещения — +∞.
        auto pack = [&](double L, std::vector<double>& best) -> double {
            const long jLo = long(std::ceil((sMin - ideal[0]) / grid - 1e-9));
            const long jHi = long(std::floor((sMax - ideal[0]) / grid + 1e-9));
            if (jHi < jLo) return inf;
            const std::size_t G = std::size_t(jHi - jLo + 1);
            auto sAt = [&](std::size_t g) { return ideal[0] + double(jLo + long(g)) * grid; };
            std::vector<char> free(G);
            for (std::size_t g = 0; g < G; ++g) free[g] = !blocked(pointAt(L, sAt(g)));
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
            double cost = inf;
            for (std::size_t g = 0; g < G; ++g)
                if (dp[n - 1][g] < cost) {
                    cost = dp[n - 1][g];
                    arg = g;
                }
            if (cost >= inf) return inf;
            best.assign(n, 0.0);
            for (std::size_t k = n; k-- > 0;) {
                best[k] = sAt(arg);
                if (k > 0) arg = from[k][arg];
            }
            return cost;
        };
        // Линии-кандидаты: исходная и сразу за каждым препятствием (наружу от изображения).
        std::vector<double> lines{L0};
        auto consider = [&](double v) {
            const bool ok = (line == BalloonLine::Right && v > L0 && v + maxW + 1.0 <= o.area.x1) ||
                            (line == BalloonLine::Left && v < L0 && v - maxW - 1.0 >= o.area.x0) ||
                            (line == BalloonLine::Above && v > L0 && v + textTop + 1.0 <= o.area.y1) ||
                            (line == BalloonLine::Below && v < L0 && v >= o.area.y0 + 1.0);
            if (ok) lines.push_back(v);
        };
        auto edgeOut = [&](double minX, double minY, double maxX, double maxY) {
            switch (line) {
                case BalloonLine::Right: consider(maxX + gap + 1e-6); break;
                case BalloonLine::Left: consider(minX - gap - 1e-6); break;
                case BalloonLine::Above: consider(maxY + gap + 1e-6); break;
                default: consider(minY - gap - textTop - 1e-6); break;
            }
        };
        for (const Rect& b : obs.boxes) edgeOut(b.x0, b.y0, b.x1, b.y1);
        for (const LineSeg& l : obs.lines)
            edgeOut(std::min(l.a.x, l.b.x), std::min(l.a.y, l.b.y), std::max(l.a.x, l.b.x), std::max(l.a.y, l.b.y));
        std::sort(lines.begin(), lines.end(), [&](double a, double b) { return std::abs(a - L0) < std::abs(b - L0); });
        double bestCost = inf;
        double bestL = L0;
        std::vector<double> bestS;
        for (double L : lines) {
            const double shift = double(n) * std::abs(L - L0);  // сдвиг линии — n · расстояние
            if (shift >= bestCost) break;
            std::vector<double> sv;
            const double c = pack(L, sv);
            if (c + shift < bestCost) {
                bestCost = c + shift;
                bestL = L;
                bestS = std::move(sv);
            }
        }
        if (!bestS.empty())
            for (std::size_t k = 0; k < n; ++k) slots[k] = pointAt(bestL, bestS[k]);
    }

    // slotOf[i] — слот кандидата i; choice[i] — выбранная точка выноски.
    std::vector<std::size_t> slotOf(n), choice(n, 0);
    for (std::size_t k = 0; k < n; ++k) slotOf[byNumber[k]] = k;
    auto leader = [&](std::size_t i, std::size_t t) { return std::make_pair(cands[i].targets[t], slots[slotOf[i]]); };
    auto crossingsOf = [&](std::size_t i, std::size_t t) {
        const auto [a, b] = leader(i, t);
        std::size_t c = 0;
        for (std::size_t j = 0; j < n; ++j) {
            if (j == i) continue;
            const auto [p, q] = leader(j, choice[j]);
            c += segmentsCross(a, b, p, q) ? 1 : 0;
        }
        return c;
    };
    auto total = [&]() {
        std::size_t c = 0;
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = i + 1; j < n; ++j) {
                const auto [a, b] = leader(i, choice[i]);
                const auto [p, q] = leader(j, choice[j]);
                c += segmentsCross(a, b, p, q) ? 1 : 0;
            }
        return c;
    };
    // Пересечения выноски (i, t) с надписями и с линиями размеров.
    auto obstacleHits = [&](std::size_t i, std::size_t t) {
        if (obs.empty()) return std::pair<std::size_t, std::size_t>{0, 0};
        return leaderHits(cands[i].targets[t], slots[slotOf[i]], obs);
    };
    // Локальный поиск точек выносок: (пересечения выносок, с надписями размеров, с линиями
    // размеров, длина) — лексикографически.
    auto improve = [&]() {
        for (std::size_t i = 0; i < n; ++i) {
            std::size_t best = 0;
            std::pair<std::size_t, std::size_t> bestHits{~std::size_t(0), 0};
            double bestLen = 1e300;
            for (std::size_t t = 0; t < cands[i].targets.size(); ++t) {
                const auto hits = obstacleHits(i, t);
                const double len = dist(cands[i].targets[t], slots[slotOf[i]]);
                if (hits < bestHits || (hits == bestHits && len < bestLen)) {
                    bestHits = hits;
                    bestLen = len;
                    best = t;
                }
            }
            choice[i] = best;
        }
        for (int pass = 0; pass < 50; ++pass) {
            bool changed = false;
            for (std::size_t i = 0; i < n; ++i) {
                std::size_t bc = crossingsOf(i, choice[i]);
                auto bh = obstacleHits(i, choice[i]);
                double bl = dist(cands[i].targets[choice[i]], slots[slotOf[i]]);
                for (std::size_t t = 0; t < cands[i].targets.size(); ++t) {
                    if (t == choice[i]) continue;
                    const std::size_t c = crossingsOf(i, t);
                    const auto hh = obstacleHits(i, t);
                    const double l = dist(cands[i].targets[t], slots[slotOf[i]]);
                    if (c < bc || (c == bc && (hh < bh || (hh == bh && l < bl - 1e-9)))) {
                        bc = c;
                        bh = hh;
                        bl = l;
                        choice[i] = t;
                        changed = true;
                    }
                }
            }
            if (!changed) break;
        }
    };
    improve();
    // Пары пересекающихся выносок: одновременная смена точек обеих (одиночная смена может не
    // помочь — каждая из двух точек по отдельности пересекает выноску другой).
    for (std::size_t iter = 0; iter < 4 * n + 8; ++iter) {
        const std::size_t before = total();
        if (before == 0) break;
        bool improved = false;
        for (std::size_t i = 0; i < n && !improved; ++i)
            for (std::size_t j = i + 1; j < n && !improved; ++j) {
                const auto [a, b] = leader(i, choice[i]);
                const auto [p, q] = leader(j, choice[j]);
                if (!segmentsCross(a, b, p, q)) continue;
                const std::size_t ci = choice[i], cj = choice[j];
                std::size_t bestI = ci, bestJ = cj, best = before;
                double bestLen = 1e300;
                for (std::size_t ti = 0; ti < cands[i].targets.size(); ++ti)
                    for (std::size_t tj = 0; tj < cands[j].targets.size(); ++tj) {
                        choice[i] = ti;
                        choice[j] = tj;
                        const std::size_t c = total();
                        const double len = dist(cands[i].targets[ti], slots[slotOf[i]]) + dist(cands[j].targets[tj], slots[slotOf[j]]);
                        if (c < best || (c == best && c < before && len < bestLen)) {
                            best = c;
                            bestLen = len;
                            bestI = ti;
                            bestJ = tj;
                        }
                    }
                choice[i] = bestI;
                choice[j] = bestJ;
                improved = best < before;
            }
        if (!improved) break;
    }
    std::size_t crossings = total();
    if (crossings > 0) {
        // Порядок номеров уступает непересечению: слоты — по положению точек вдоль линии.
        std::vector<std::size_t> byPos(n);
        std::iota(byPos.begin(), byPos.end(), 0);
        std::stable_sort(byPos.begin(), byPos.end(), [&](std::size_t a, std::size_t b) {
            const Point2 pa = cands[a].targets[choice[a]], pb = cands[b].targets[choice[b]];
            return column ? pa.y > pb.y : pa.x < pb.x;
        });
        for (std::size_t k = 0; k < n; ++k) slotOf[byPos[k]] = k;
        // Обмен слотами пересекающихся выносок (сумма длин строго убывает — конечно).
        for (std::size_t iter = 0; iter < 4 * n * n + 16; ++iter) {
            bool swapped = false;
            for (std::size_t i = 0; i < n && !swapped; ++i)
                for (std::size_t j = i + 1; j < n && !swapped; ++j) {
                    const auto [a, b] = leader(i, choice[i]);
                    const auto [p, q] = leader(j, choice[j]);
                    if (!segmentsCross(a, b, p, q)) continue;
                    std::swap(slotOf[i], slotOf[j]);
                    swapped = true;
                }
            if (!swapped) break;
        }
        crossings = total();
        out.ordered = false;
    }
    // Порядок мог совпасть с порядком номеров.
    {
        bool asc = true;
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j)
                if (cands[i].number < cands[j].number && slotOf[i] > slotOf[j]) asc = false;
        out.ordered = asc;
    }
    out.crossings = crossings;
    out.balloons.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        Balloon& b = out.balloons[i];
        b.definition = cands[i].definition;
        b.target = cands[i].targets[choice[i]];
        b.at = slots[slotOf[i]];
        b.text = std::to_string(cands[i].number);
        if (!obs.empty()) {
            if (rectBlocked(balloonShelfBox(b, o.balloon, o.style), obs)) ++out.shelfHits;
            const auto [texts, lines] = leaderHits(b.target, b.at, obs);
            out.leaderTextHits += texts;
            out.leaderLineHits += lines;
        }
    }
    return out;
}

}  // namespace bendcore::drawing_v2::annotations
