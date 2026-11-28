// Графические обозначения материалов в сечениях по ГОСТ 2.306-68, табл. 1: металлы, неметаллы,
// древесина, стекло, бетон, жидкости. Узоры строятся в мм листа и обрезаются по области сечения
// (правило чётности). Документация — docs/drawing/VIEWS.md, «Штриховка по материалу».

#include <bendcore/drawing_v2/view_sheet.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string>

namespace bendcore::drawing_v2::views {

namespace {

constexpr std::array<std::pair<HatchMaterial, const char*>, 6> kNames = {{
    {HatchMaterial::Metal, "metal"},
    {HatchMaterial::NonMetal, "nonmetal"},
    {HatchMaterial::Wood, "wood"},
    {HatchMaterial::Glass, "glass"},
    {HatchMaterial::Concrete, "concrete"},
    {HatchMaterial::Liquid, "liquid"},
}};

// Нижний регистр ASCII и русских букв UTF-8 (А–Я, Ё → а–я, ё).
std::string lowerUtf8(std::string_view s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            out += char(std::tolower(c));
            continue;
        }
        if (i + 1 < s.size()) {
            const auto d = static_cast<unsigned char>(s[i + 1]);
            if (c == 0xD0 && d >= 0x90 && d <= 0x9F) {  // А–П → а–п
                out += char(0xD0);
                out += char(d + 0x20);
                ++i;
                continue;
            }
            if (c == 0xD0 && d >= 0xA0 && d <= 0xAF) {  // Р–Я → р–я
                out += char(0xD1);
                out += char(d - 0x20);
                ++i;
                continue;
            }
            if (c == 0xD0 && d == 0x81) {  // Ё → ё
                out += char(0xD1);
                out += char(0x91);
                ++i;
                continue;
            }
        }
        out += char(c);
    }
    return out;
}

bool hasAny(const std::string& text, std::initializer_list<const char*> words) {
    for (const char* w : words)
        if (text.find(w) != std::string::npos) return true;
    return false;
}

// Целое слово латиницей (обозначения пластмасс «PA6», «ABS»): слева и справа — не буква.
bool hasToken(const std::string& text, std::initializer_list<const char*> tokens) {
    for (const char* t : tokens) {
        const std::string tok(t);
        for (std::size_t p = text.find(tok); p != std::string::npos; p = text.find(tok, p + 1)) {
            const bool leftOk = p == 0 || !std::isalpha(static_cast<unsigned char>(text[p - 1]));
            const std::size_t e = p + tok.size();
            const bool rightOk = e >= text.size() || !std::isalpha(static_cast<unsigned char>(text[e]));
            if (leftOk && rightOk) return true;
        }
    }
    return false;
}

double polygonArea(const std::vector<Point2>& p) {
    double s = 0;
    for (std::size_t i = 0; i < p.size(); ++i) {
        const Point2& a = p[i];
        const Point2& b = p[(i + 1) % p.size()];
        s += a.x * b.y - b.x * a.y;
    }
    return 0.5 * s;
}

using Loops = std::vector<std::vector<Point2>>;

// Точка внутри области по правилу чётности.
bool inside(const Loops& loops, Point2 p) {
    bool in = false;
    for (const auto& l : loops) {
        for (std::size_t i = 0, j = l.size() - 1; i < l.size(); j = i++) {
            const Point2& a = l[i];
            const Point2& b = l[j];
            if ((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) in = !in;
        }
    }
    return in;
}

// Участки отрезка a→b внутри области (параметры [t0, t1] по возрастанию).
std::vector<std::pair<double, double>> clipParams(const Loops& loops, Point2 a, Point2 b) {
    std::vector<double> ts{0.0, 1.0};
    const double dx = b.x - a.x, dy = b.y - a.y;
    for (const auto& l : loops) {
        for (std::size_t i = 0; i < l.size(); ++i) {
            const Point2& p = l[i];
            const Point2& q = l[(i + 1) % l.size()];
            const double ex = q.x - p.x, ey = q.y - p.y;
            const double den = dx * ey - dy * ex;
            if (std::abs(den) < 1e-15) continue;
            const double t = ((p.x - a.x) * ey - (p.y - a.y) * ex) / den;
            const double u = ((p.x - a.x) * dy - (p.y - a.y) * dx) / den;
            if (t > 0 && t < 1 && u >= 0 && u <= 1) ts.push_back(t);
        }
    }
    std::sort(ts.begin(), ts.end());
    std::vector<std::pair<double, double>> out;
    for (std::size_t i = 0; i + 1 < ts.size(); ++i) {
        if (ts[i + 1] - ts[i] < 1e-12) continue;
        const double m = 0.5 * (ts[i] + ts[i + 1]);
        if (!inside(loops, {a.x + m * dx, a.y + m * dy})) continue;
        if (!out.empty() && std::abs(out.back().second - ts[i]) < 1e-12) out.back().second = ts[i + 1];
        else out.emplace_back(ts[i], ts[i + 1]);
    }
    return out;
}

Point2 lerp(Point2 a, Point2 b, double t) { return {a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)}; }

// Отрезок целиком внутри области.
bool wholeInside(const Loops& loops, Point2 a, Point2 b) {
    const auto parts = clipParams(loops, a, b);
    return parts.size() == 1 && parts[0].first < 1e-9 && parts[0].second > 1 - 1e-9;
}

// Ломаная, обрезанная по области: непрерывные участки — отдельные ломаные.
void clipPolyline(const Loops& loops, const std::vector<Point2>& pts, std::vector<Polyline>& out) {
    Polyline cur;
    auto flush = [&]() {
        if (cur.points.size() >= 2) out.push_back(std::move(cur));
        cur = Polyline{};
    };
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        const auto parts = clipParams(loops, pts[i], pts[i + 1]);
        for (const auto& [t0, t1] : parts) {
            const Point2 p = lerp(pts[i], pts[i + 1], t0), q = lerp(pts[i], pts[i + 1], t1);
            if (t0 > 1e-9 || cur.points.empty()) {
                flush();
                cur.points.push_back(p);
            }
            cur.points.push_back(q);
            if (t1 < 1 - 1e-9) flush();
        }
        if (parts.empty()) flush();
    }
    flush();
}

struct Bounds {
    double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
};

std::uint32_t hash2(int i, int j) {
    std::uint32_t h = std::uint32_t(i) * 73856093u ^ std::uint32_t(j) * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}
double unit(std::uint32_t h) { return double(h % 10007u) / 10007.0; }

}  // namespace

const char* toString(HatchMaterial m) noexcept {
    for (const auto& [v, n] : kNames)
        if (v == m) return n;
    return "metal";
}

std::optional<HatchMaterial> hatchMaterialFromString(std::string_view text) noexcept {
    for (const auto& [v, n] : kNames)
        if (text == n) return v;
    return std::nullopt;
}

HatchMaterial hatchMaterialByName(std::string_view material) {
    const std::string t = lowerUtf8(material);
    if (t.empty()) return HatchMaterial::Metal;
    // Стекло и другие светопрозрачные материалы (п. 7), в том числе органическое стекло.
    if (hasAny(t, {"стекл", "glass", "pmma", "плексиглас", "акрил"})) {
        // «Стеклотекстолит», «стеклопластик», «стекловолокно» — неметаллы (п. 2), не прозрачные.
        if (hasAny(t, {"стеклотекстолит", "стеклопластик", "стекловолок", "glass fibre", "glass fiber", "glass-filled", "gf"}))
            return HatchMaterial::NonMetal;
        return HatchMaterial::Glass;
    }
    if (hasAny(t, {"бетон", "железобетон", "concrete"})) return HatchMaterial::Concrete;
    if (hasAny(t, {"древесин", "дерев", "сосна", "дуб", "берёза", "береза", "бук", "фанер", "wood", "timber",
                   "pine", "oak", "plywood"}))
        return HatchMaterial::Wood;
    if (hasAny(t, {"жидкост", "вода", "масло", "liquid", "water", "oil"})) return HatchMaterial::Liquid;
    if (hasAny(t, {"пластмасс", "пластик", "полиамид", "капролон", "фторопласт", "полиэтилен", "полипропилен",
                   "поликарбонат", "полистирол", "полиацеталь", "полиформальдегид", "текстолит", "гетинакс", "резин",
                   "каучук", "паронит", "картон", "войлок", "кожа", "plastic", "rubber", "nylon", "polyamide",
                   "polyethylene", "polypropylene", "polycarbonate", "acetal", "ptfe", "teflon", "elastomer"}))
        return HatchMaterial::NonMetal;
    if (hasToken(t, {"abs", "pa", "pa6", "pa66", "pom", "pom-h", "pom-c", "pc", "pp", "pe", "hdpe", "ldpe", "pvc", "peek",
                     "pet", "pbt", "ps", "nbr", "epdm", "fkm"}))
        return HatchMaterial::NonMetal;
    return HatchMaterial::Metal;
}

Result<MaterialHatchGeometry> materialHatchGeometry(const std::vector<std::vector<Point2>>& loops, HatchMaterial material,
                                                    const HatchPattern& pattern, double spacing) {
    using R = Result<MaterialHatchGeometry>;
    if (!(spacing >= 1.0 - 1e-9 && spacing <= 10.0 + 1e-9))
        return R::failure(ErrorCode::InvalidInput, "material hatch: spacing must be 1..10 mm (GOST 2.306)");
    MaterialHatchGeometry g;
    Loops L;
    double area = 0;
    Bounds b;
    for (const auto& l : loops) {
        if (l.size() < 3) continue;
        for (const Point2& p : l) {
            if (!std::isfinite(p.x) || !std::isfinite(p.y))
                return R::failure(ErrorCode::InvalidInput, "material hatch: point is not finite");
            b.x0 = std::min(b.x0, p.x);
            b.y0 = std::min(b.y0, p.y);
            b.x1 = std::max(b.x1, p.x);
            b.y1 = std::max(b.y1, p.y);
        }
        area += std::abs(polygonArea(l));
        L.push_back(l);
    }
    if (L.empty() || area < 1e-12) return g;
    const double s = spacing;
    switch (material) {
        case HatchMaterial::Metal:
            g.hatches.push_back(Hatch{L, pattern.angleDeg, s});
            return g;
        case HatchMaterial::NonMetal:
            // Сетка (п. 2): линии под 45° в обе стороны с одним шагом.
            g.hatches.push_back(Hatch{L, 45.0, s});
            g.hatches.push_back(Hatch{L, 135.0, s});
            return g;
        case HatchMaterial::Wood: {
            // Волнистые линии вдоль волокон: синусоида амплитудой s/4, длина волны 4s, ряды через s
            // (направление — по наклону узора: 45° → горизонтально, 135° → вертикально).
            const bool vertical = std::abs(pattern.angleDeg - 135.0) < 1e-6;
            const double A = 0.25 * s, lambda = 4.0 * s, step = lambda / 16.0;
            const double u0 = (vertical ? b.y0 : b.x0) - lambda, u1 = (vertical ? b.y1 : b.x1) + lambda;
            const double v0 = vertical ? b.x0 : b.y0, v1 = vertical ? b.x1 : b.y1;
            for (double v = std::floor(v0 / s) * s + 0.5 * s; v < v1 + A; v += s) {
                std::vector<Point2> pts;
                for (double u = u0; u <= u1 + 1e-9; u += step) {
                    const double w = v + A * std::sin(2.0 * std::numbers::pi * u / lambda);
                    pts.push_back(vertical ? Point2{w, u} : Point2{u, w});
                }
                clipPolyline(L, pts, g.polylines);
            }
            return g;
        }
        case HatchMaterial::Glass: {
            // Группы по три коротких штриха под 45° (длина 1,5s, не короче 3 мм; расстояние между
            // штрихами s/2), узлы группы — сетка с шагом 4s, ряды со сдвигом на 2s.
            const double P = 4.0 * s, len = std::max(3.0, 1.5 * s), gap = 0.5 * s;
            const double a = pattern.angleDeg * std::numbers::pi / 180.0;
            const Point2 u{std::cos(a), std::sin(a)}, n{-u.y, u.x};
            int row = 0;
            for (double y = std::floor(b.y0 / P) * P; y <= b.y1 + P; y += P, ++row) {
                for (double x = std::floor(b.x0 / P) * P + (row % 2 ? 0.5 * P : 0.0); x <= b.x1 + P; x += P) {
                    for (int k = -1; k <= 1; ++k) {
                        const Point2 c{x + k * gap * n.x, y + k * gap * n.y};
                        const double h = (k == 0 ? len : 0.7 * len) / 2.0;
                        const Point2 p{c.x - h * u.x, c.y - h * u.y}, q{c.x + h * u.x, c.y + h * u.y};
                        if (wholeInside(L, p, q)) g.lines.push_back({p, q});
                    }
                }
            }
            return g;
        }
        case HatchMaterial::Concrete: {
            // Щебень: в каждой клетке сетки с шагом 2s — точка (две из трёх клеток) или треугольник
            // со стороной 0,8s, сдвиг и поворот — детерминированные по номеру клетки.
            const double P = 2.0 * s;
            const int i0 = int(std::floor(b.x0 / P)) - 1, i1 = int(std::ceil(b.x1 / P)) + 1;
            const int j0 = int(std::floor(b.y0 / P)) - 1, j1 = int(std::ceil(b.y1 / P)) + 1;
            for (int i = i0; i <= i1; ++i) {
                for (int j = j0; j <= j1; ++j) {
                    const std::uint32_t h = hash2(i, j);
                    const Point2 c{(i + 0.5) * P + (unit(h) - 0.5) * 0.5 * P, (j + 0.5) * P + (unit(h >> 7) - 0.5) * 0.5 * P};
                    if (h % 3u != 0u) {
                        const double r = std::max(0.25, 0.08 * s);
                        if (inside(L, c) && wholeInside(L, {c.x - r, c.y}, {c.x + r, c.y}) &&
                            wholeInside(L, {c.x, c.y - r}, {c.x, c.y + r}))
                            g.dots.push_back({c, r});
                        continue;
                    }
                    const double side = 0.8 * s, R0 = side / std::sqrt(3.0), rot = unit(h >> 11) * 2.0 * std::numbers::pi;
                    Polyline t;
                    t.closed = true;
                    for (int k = 0; k < 3; ++k) {
                        const double ang = rot + k * 2.0 * std::numbers::pi / 3.0;
                        t.points.push_back({c.x + R0 * std::cos(ang), c.y + R0 * std::sin(ang)});
                    }
                    bool ok = true;
                    for (int k = 0; k < 3 && ok; ++k) ok = wholeInside(L, t.points[k], t.points[(k + 1) % 3]);
                    if (ok) g.polylines.push_back(std::move(t));
                }
            }
            return g;
        }
        case HatchMaterial::Liquid: {
            // Горизонтальные штрихи длиной 3s с промежутком s рядами через s; соседние ряды сдвинуты
            // на 2s.
            const double dash = 3.0 * s, period = 4.0 * s;
            int row = 0;
            for (double y = std::floor(b.y0 / s) * s + 0.5 * s; y < b.y1; y += s, ++row) {
                const double shift = row % 2 ? 0.5 * period : 0.0;
                for (double x = std::floor((b.x0 - shift) / period) * period + shift; x < b.x1; x += period) {
                    for (const auto& [t0, t1] : clipParams(L, {x, y}, {x + dash, y})) {
                        const Point2 p{x + t0 * dash, y}, q{x + t1 * dash, y};
                        if (q.x - p.x > 0.2) g.lines.push_back({p, q});
                    }
                }
            }
            return g;
        }
    }
    return g;
}

}  // namespace bendcore::drawing_v2::views
