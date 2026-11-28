// Пост-обработка 2D-рёбер вида: удаление совпадающих участков (видимая линия закрывает
// невидимую, повторы видимых линий удаляются) и сборка ломаных из отрезков PolyAlgo.

#include "views_internal.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <tuple>

namespace bendcore::drawing_v2::views::detail {

namespace {

using Interval = std::pair<double, double>;

constexpr double kTwoPi = 2.0 * std::numbers::pi;

int classRank(EdgeClass c) {
    switch (c) {
        case EdgeClass::SectionCut: return 0;
        case EdgeClass::Sharp: return 1;
        case EdgeClass::Silhouette: return 2;
        case EdgeClass::Smooth: return 3;
    }
    return 4;
}

// Вычитание интервалов (отсортированные непересекающиеся куски остаются).
std::vector<Interval> subtract(std::vector<Interval> keep, Interval cut) {
    std::vector<Interval> out;
    for (const Interval& k : keep) {
        if (cut.second <= k.first || cut.first >= k.second) {
            out.push_back(k);
            continue;
        }
        if (cut.first > k.first) out.push_back({k.first, cut.first});
        if (cut.second < k.second) out.push_back({cut.second, k.second});
    }
    return out;
}

struct ArcParam {
    Point2 c;
    double r;
    double s;      // начало, рад
    double sweep;  // рад, (0, 2π]
};

std::optional<ArcParam> arcParam(const Curve2& c) {
    if (auto* a = std::get_if<Arc>(&c)) {
        double sweep = std::fmod(a->endDeg - a->startDeg, 360.0);
        if (sweep <= 0) sweep += 360.0;
        return ArcParam{a->center, a->radius, a->startDeg * std::numbers::pi / 180.0,
                        sweep * std::numbers::pi / 180.0};
    }
    if (auto* ci = std::get_if<Circle>(&c)) return ArcParam{ci->center, ci->radius, 0.0, kTwoPi};
    return std::nullopt;
}

double mod2pi(double a) {
    a = std::fmod(a, kTwoPi);
    if (a < 0) a += kTwoPi;
    return a;
}

}  // namespace

namespace {

// Корзины для поиска совпадающих линий: прямые — по углу направления и расстоянию до
// начала координат, окружности — по центру и радиусу. Совпадающие кривые попадают в одну
// или соседние корзины, поэтому сравниваются только кандидаты из 3×3 (3×3×3) соседних.
using BucketKey = std::tuple<long long, long long, long long>;

struct LineKey {
    double theta, rho;
};

LineKey lineKey(const LineSeg& l) {
    double dx = l.b.x - l.a.x, dy = l.b.y - l.a.y;
    double th = std::atan2(dy, dx);
    if (th < 0) th += std::numbers::pi;
    if (th >= std::numbers::pi) th -= std::numbers::pi;
    const double ux = std::cos(th), uy = std::sin(th);
    return {th, l.a.x * uy - l.a.y * ux};
}

}  // namespace

void removeOverlaps(std::vector<ViewEdge>& edges) {
    if (edges.empty()) return;
    const Box2 box = boundsOf(edges);
    const double diag = std::hypot(box.width(), box.height());
    const double tol = std::max(1e-7, 1e-6 * diag);
    const double minLen = 5.0 * tol;
    const double thStep = 1e-3;                       // рад
    const double rhoStep = std::max(100.0 * tol, 1e-4);  // мм
    const double cStep = std::max(100.0 * tol, 1e-4);

    std::vector<std::size_t> order(edges.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        const ViewEdge& ea = edges[a];
        const ViewEdge& eb = edges[b];
        const int va = ea.visibility == Visibility::Visible ? 0 : 1;
        const int vb = eb.visibility == Visibility::Visible ? 0 : 1;
        if (va != vb) return va < vb;
        if (classRank(ea.cls) != classRank(eb.cls)) return classRank(ea.cls) < classRank(eb.cls);
        return curveLength(ea.curve) > curveLength(eb.curve);
    });

    std::vector<ViewEdge> kept;
    kept.reserve(edges.size());
    std::map<BucketKey, std::vector<std::size_t>> lineBuckets, arcBuckets;
    const long long nTheta = (long long)std::ceil(std::numbers::pi / thStep);
    auto lineBucket = [&](const LineKey& k) {
        return BucketKey{(long long)std::floor(k.theta / thStep), (long long)std::floor(k.rho / rhoStep), 0};
    };
    auto arcBucket = [&](const ArcParam& p) {
        return BucketKey{(long long)std::floor(p.c.x / cStep), (long long)std::floor(p.c.y / cStep),
                         (long long)std::floor(p.r / cStep)};
    };
    auto keep = [&](ViewEdge e) {
        if (auto* l = std::get_if<LineSeg>(&e.curve)) lineBuckets[lineBucket(lineKey(*l))].push_back(kept.size());
        else if (auto ap = arcParam(e.curve)) arcBuckets[arcBucket(*ap)].push_back(kept.size());
        kept.push_back(std::move(e));
    };
    // Кандидаты-прямые: соседние корзины по углу (с переходом через π, где ρ меняет знак)
    // и по расстоянию.
    auto lineCandidates = [&](const LineSeg& l, std::vector<std::size_t>& out) {
        out.clear();
        const BucketKey k = lineBucket(lineKey(l));
        for (long long dt = -1; dt <= 1; ++dt) {
            long long t = std::get<0>(k) + dt;
            long long rb = std::get<1>(k);
            if (t < 0 || t >= nTheta) {
                t = (t + nTheta) % nTheta;
                rb = -rb - 1;
            }
            for (long long dr = -1; dr <= 1; ++dr) {
                auto it = lineBuckets.find({t, rb + dr, 0});
                if (it != lineBuckets.end()) out.insert(out.end(), it->second.begin(), it->second.end());
            }
        }
    };
    std::vector<std::size_t> cand;
    for (std::size_t oi : order) {
        const ViewEdge& e = edges[oi];
        if (auto* l = std::get_if<LineSeg>(&e.curve)) {
            const double dx = l->b.x - l->a.x, dy = l->b.y - l->a.y;
            const double len = std::hypot(dx, dy);
            if (len < minLen) continue;
            const double ux = dx / len, uy = dy / len;
            std::vector<Interval> rest{{0.0, len}};
            lineCandidates(*l, cand);
            for (std::size_t ki : cand) {
                const auto* kl = std::get_if<LineSeg>(&kept[ki].curve);
                const double d1 = (kl->a.x - l->a.x) * uy - (kl->a.y - l->a.y) * ux;
                const double d2 = (kl->b.x - l->a.x) * uy - (kl->b.y - l->a.y) * ux;
                if (std::abs(d1) > tol || std::abs(d2) > tol) continue;
                double t1 = (kl->a.x - l->a.x) * ux + (kl->a.y - l->a.y) * uy;
                double t2 = (kl->b.x - l->a.x) * ux + (kl->b.y - l->a.y) * uy;
                if (t1 > t2) std::swap(t1, t2);
                rest = subtract(rest, {t1 - tol, t2 + tol});
                if (rest.empty()) break;
            }
            for (const Interval& iv : rest) {
                if (iv.second - iv.first < minLen) continue;
                ViewEdge piece = e;
                piece.curve = LineSeg{{l->a.x + ux * iv.first, l->a.y + uy * iv.first},
                                      {l->a.x + ux * iv.second, l->a.y + uy * iv.second}};
                keep(std::move(piece));
            }
            continue;
        }
        if (auto ap = arcParam(e.curve)) {
            std::vector<Interval> rest{{0.0, ap->sweep}};
            const double angTol = tol / std::max(ap->r, tol);
            const BucketKey k = arcBucket(*ap);
            cand.clear();
            for (long long a = -1; a <= 1; ++a)
                for (long long b = -1; b <= 1; ++b)
                    for (long long c = -1; c <= 1; ++c) {
                        auto it = arcBuckets.find({std::get<0>(k) + a, std::get<1>(k) + b, std::get<2>(k) + c});
                        if (it != arcBuckets.end()) cand.insert(cand.end(), it->second.begin(), it->second.end());
                    }
            for (std::size_t ki : cand) {
                auto kp = arcParam(kept[ki].curve);
                if (std::hypot(kp->c.x - ap->c.x, kp->c.y - ap->c.y) > tol ||
                    std::abs(kp->r - ap->r) > tol)
                    continue;
                const double off = mod2pi(kp->s - ap->s);
                // Покрытие [off, off + sweep] и его сдвиг на −2π.
                rest = subtract(rest, {off - angTol, off + kp->sweep + angTol});
                rest = subtract(rest, {off - kTwoPi - angTol, off - kTwoPi + kp->sweep + angTol});
                if (rest.empty()) break;
            }
            const bool untouched = rest.size() == 1 && rest[0].first == 0.0 &&
                                   rest[0].second == ap->sweep;
            if (untouched) {
                keep(e);
                continue;
            }
            for (const Interval& iv : rest) {
                if ((iv.second - iv.first) * ap->r < minLen) continue;
                ViewEdge piece = e;
                const double a0 = (ap->s + iv.first) * 180.0 / std::numbers::pi;
                const double a1 = (ap->s + iv.second) * 180.0 / std::numbers::pi;
                piece.curve = Arc{ap->c, ap->r, a0, a1};
                keep(std::move(piece));
            }
            continue;
        }
        keep(e);
    }
    edges = std::move(kept);
}

void chainSegments(std::vector<ViewEdge>& edges) {
    if (edges.empty()) return;
    const Box2 box = boundsOf(edges);
    // Соседние участки PolyAlgo одного ребра расходятся до ~1e-6 от размера модели.
    const double tol = std::max(1e-7, 1e-5 * std::hypot(box.width(), box.height()));
    using Key = std::tuple<int, int, long long, long long>;
    std::map<Key, std::vector<std::size_t>> groups;
    std::vector<ViewEdge> other;
    for (std::size_t i = 0; i < edges.size(); ++i) {
        const ViewEdge& e = edges[i];
        if (!std::holds_alternative<LineSeg>(e.curve)) {
            other.push_back(e);
            continue;
        }
        groups[{int(e.visibility), int(e.cls), e.edgeIndex ? (long long)*e.edgeIndex : -1,
                e.faceIndex ? (long long)*e.faceIndex : -1}]
            .push_back(i);
    }
    std::vector<ViewEdge> out = std::move(other);
    auto close = [tol](Point2 a, Point2 b) { return std::hypot(a.x - b.x, a.y - b.y) <= tol; };
    for (auto& [key, idx] : groups) {
        std::vector<char> used(idx.size(), 0);
        for (std::size_t s = 0; s < idx.size(); ++s) {
            if (used[s]) continue;
            used[s] = 1;
            const LineSeg& first = std::get<LineSeg>(edges[idx[s]].curve);
            std::vector<Point2> chain{first.a, first.b};
            bool grown = true;
            while (grown) {
                grown = false;
                for (std::size_t j = 0; j < idx.size(); ++j) {
                    if (used[j]) continue;
                    const LineSeg& l = std::get<LineSeg>(edges[idx[j]].curve);
                    if (close(chain.back(), l.a)) chain.push_back(l.b);
                    else if (close(chain.back(), l.b)) chain.push_back(l.a);
                    else if (close(chain.front(), l.b)) chain.insert(chain.begin(), l.a);
                    else if (close(chain.front(), l.a)) chain.insert(chain.begin(), l.b);
                    else continue;
                    used[j] = 1;
                    grown = true;
                }
            }
            ViewEdge e = edges[idx[s]];
            // Прямая цепочка — один отрезок.
            const Point2 a = chain.front(), b = chain.back();
            const double len = std::hypot(b.x - a.x, b.y - a.y);
            bool straight = len > tol;
            for (const Point2& p : chain) {
                if (!straight) break;
                const double d = std::abs((p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x)) / len;
                if (d > tol * 10) straight = false;
            }
            if (chain.size() == 2 || straight) {
                e.curve = LineSeg{a, b};
            } else {
                const bool closed = close(chain.front(), chain.back()) && chain.size() > 3;
                if (closed) chain.pop_back();
                e.curve = Polyline{std::move(chain), closed};
            }
            out.push_back(std::move(e));
        }
    }
    edges = std::move(out);
}

}  // namespace bendcore::drawing_v2::views::detail
