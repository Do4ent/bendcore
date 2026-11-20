// Разнесение полок номеров позиций (ГОСТ 2.109-73): одна линия, без пересечения линий-выносок,
// номера по порядку. Документация — docs/drawing/ANNOTATIONS.md, «Позиции».

#include <bendcore/drawing_v2/balloon_layout.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>

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
    // Локальный поиск точек выносок: (число пересечений, длина) — лексикографически.
    auto improve = [&]() {
        for (std::size_t i = 0; i < n; ++i) {
            std::size_t best = 0;
            double bestLen = 1e300;
            for (std::size_t t = 0; t < cands[i].targets.size(); ++t) {
                const double len = dist(cands[i].targets[t], slots[slotOf[i]]);
                if (len < bestLen) {
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
                double bl = dist(cands[i].targets[choice[i]], slots[slotOf[i]]);
                for (std::size_t t = 0; t < cands[i].targets.size(); ++t) {
                    if (t == choice[i]) continue;
                    const std::size_t c = crossingsOf(i, t);
                    const double l = dist(cands[i].targets[t], slots[slotOf[i]]);
                    if (c < bc || (c == bc && l < bl - 1e-9)) {
                        bc = c;
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
    }
    return out;
}

}  // namespace bendcore::drawing_v2::annotations
