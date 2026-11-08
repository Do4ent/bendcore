// Знаки и аннотации: шероховатость ГОСТ 2.309, допуски формы и расположения ГОСТ 2.308 /
// ISO 1101, базы, выноски и технические требования ГОСТ 2.316, позиции ГОСТ 2.109 / ISO 6433.

#include <bendcore/drawing_v2/annotations.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <sstream>

namespace bendcore::drawing_v2::annotations {

namespace {

constexpr double kPi = std::numbers::pi;

Error invalid(std::string m) { return Error{ErrorCode::InvalidInput, std::move(m)}; }

Point2 add(Point2 a, Point2 b) { return {a.x + b.x, a.y + b.y}; }
Point2 sub(Point2 a, Point2 b) { return {a.x - b.x, a.y - b.y}; }
Point2 mul(double k, Point2 a) { return {k * a.x, k * a.y}; }
double norm(Point2 a) { return std::hypot(a.x, a.y); }

// Поворот на угол deg вокруг начала координат и перенос в origin.
struct Placement {
    Point2 origin;
    double deg = 0.0;
    Point2 operator()(Point2 p) const {
        const double r = deg * kPi / 180.0, c = std::cos(r), s = std::sin(r);
        return {origin.x + c * p.x - s * p.y, origin.y + s * p.x + c * p.y};
    }
};

double textWidth(const std::string& s, double h, const AnnotationStyle& st) {
    return estimateTextWidth(s, h, st.font);
}

DimensionLabel label(Point2 pos, const std::string& text, double h, double rot = 0.0, HAlign al = HAlign::Left) {
    return DimensionLabel{pos, rot, text, h, al};
}

SolidFill disk(Point2 c, double r) {
    SolidFill f;
    for (int i = 0; i < 16; ++i) {
        const double a = 2.0 * kPi * i / 16.0;
        f.points.push_back({c.x + r * std::cos(a), c.y + r * std::sin(a)});
    }
    return f;
}

}  // namespace

void SymbolGeometry::append(const SymbolGeometry& o) {
    lines.insert(lines.end(), o.lines.begin(), o.lines.end());
    arcs.insert(arcs.end(), o.arcs.begin(), o.arcs.end());
    circles.insert(circles.end(), o.circles.begin(), o.circles.end());
    polylines.insert(polylines.end(), o.polylines.begin(), o.polylines.end());
    fills.insert(fills.end(), o.fills.begin(), o.fills.end());
    labels.insert(labels.end(), o.labels.begin(), o.labels.end());
}

Status renderSymbol(Drawing& d, const SymbolGeometry& g, const AnnotationStyle& st) {
    if (auto s = ensureLayer(d, st.layer, st.layerColor); !s) return s;
    auto put = [&](Geometry geom) { return d.add(Entity{std::move(geom), st.layer, LineType::SolidThin}); };
    for (const auto& x : g.lines) {
        if (auto s = put(x); !s) return s;
    }
    for (const auto& x : g.arcs) {
        if (auto s = put(x); !s) return s;
    }
    for (const auto& x : g.circles) {
        if (auto s = put(x); !s) return s;
    }
    for (const auto& x : g.polylines) {
        if (auto s = put(x); !s) return s;
    }
    for (const auto& x : g.fills) {
        if (auto s = put(x); !s) return s;
    }
    for (const auto& l : g.labels) {
        Text t;
        t.position = l.position;
        t.utf8 = l.text;
        t.height = l.height;
        t.rotationDeg = l.rotationDeg;
        t.slanted = st.slantedText;
        t.hAlign = l.hAlign;
        t.vAlign = VAlign::Baseline;
        if (auto s = put(t); !s) return s;
    }
    return okStatus();
}

// --- Шероховатость ---

Result<SymbolGeometry> roughnessSign(Point2 tip, double surfaceAngleDeg, const RoughnessSpec& spec,
                                     const AnnotationStyle& st) {
    if (spec.heightFactor < 1.5 - 1e-9 || spec.heightFactor > 5.0 + 1e-9) {
        return invalid("GOST 2.309: sign height H must be 1.5..5 h");
    }
    if (!spec.value.empty() && spec.parameter.empty()) {
        return invalid("GOST 2.309: roughness value needs a parameter symbol (Ra, Rz, Rmax)");
    }
    const double h = st.textHeight, H = spec.heightFactor * h;
    const double t60 = std::tan(kPi / 3.0);
    const Placement P{tip, surfaceAngleDeg};
    SymbolGeometry g;
    const Point2 shortTop{-h / t60, h}, longTop{H / t60, H};
    g.lines.push_back({P({0, 0}), P(shortTop)});
    g.lines.push_back({P({0, 0}), P(longTop)});
    if (spec.method == RoughnessMethod::MaterialRemoval) {
        g.lines.push_back({P(shortTop), P({h / t60, h})});
    } else if (spec.method == RoughnessMethod::NoMaterialRemoval) {
        // Окружность, вписанная в равносторонний треугольник высотой h.
        g.circles.push_back({P({0, 2.0 * h / 3.0}), h / 3.0});
    }
    const std::string param = spec.value.empty() ? std::string() : spec.parameter + " " + spec.value;
    const double wParam = param.empty() ? 0.0 : textWidth(param, h, st);
    const double wProc = spec.process.empty() ? 0.0 : textWidth(spec.process, h, st);
    const double shelf = std::max(wParam, wProc);
    if (shelf > 0.0) {
        const Point2 a = longTop, b{longTop.x + shelf + 1.0, H};
        g.lines.push_back({P(a), P(b)});
        if (!param.empty()) g.labels.push_back(label(P({longTop.x + 0.5, H - 1.0 - h}), param, h, surfaceAngleDeg));
        if (!spec.process.empty()) {
            g.labels.push_back(label(P({longTop.x + 0.5, H + 1.0}), spec.process, h, surfaceAngleDeg));
        }
    }
    return g;
}

Result<SymbolGeometry> roughnessRemainder(const Rect& frame, const RoughnessSpec& spec, const AnnotationStyle& st) {
    AnnotationStyle big = st;
    // В 1,5 раза больше: ближайшая высота ряда ГОСТ 2.304 к 1,5·h.
    double hb = st.textHeight * 1.5, best = 1e9;
    for (double v : kGostTextHeights) {
        if (std::abs(v - st.textHeight * 1.5) < best) {
            best = std::abs(v - st.textHeight * 1.5);
            hb = v;
        }
    }
    big.textHeight = hb;
    const double H = spec.heightFactor * hb, t60 = std::tan(kPi / 3.0);
    const std::string param = spec.value.empty() ? std::string() : spec.parameter + " " + spec.value;
    const double shelf = param.empty() ? 0.0 : textWidth(param, hb, st) + 1.0;
    // Скобка и знак √ без параметра.
    RoughnessSpec basic;
    basic.method = RoughnessMethod::Unspecified;
    basic.parameter.clear();
    basic.heightFactor = spec.heightFactor;
    const double wOpen = textWidth("(", hb, st), wClose = textWidth(")", hb, st);
    const double basicW = hb / t60 + H / t60;
    const double totalW = hb / t60 + H / t60 + shelf + 2.0 + wOpen + basicW + wClose;
    const double margin = 5.0;
    const double x0 = frame.x1 - margin - totalW, y0 = frame.y1 - margin - H - 1.0;
    if (x0 < frame.x0) return invalid("roughness remainder sign does not fit the frame");
    auto main = roughnessSign({x0 + hb / t60, y0}, 0.0, spec, big);
    if (!main) return main.error();
    SymbolGeometry g = main.value();
    double x = x0 + hb / t60 + H / t60 + shelf + 2.0;
    g.labels.push_back(label({x, y0}, "(", hb));
    x += wOpen;
    auto sub2 = roughnessSign({x + hb / t60, y0}, 0.0, basic, big);
    if (!sub2) return sub2.error();
    g.append(sub2.value());
    x += basicW;
    g.labels.push_back(label({x, y0}, ")", hb));
    return g;
}

// --- Допуски формы и расположения ---

bool requiresDatum(GeometricCharacteristic c) {
    switch (c) {
        case GeometricCharacteristic::Straightness:
        case GeometricCharacteristic::Flatness:
        case GeometricCharacteristic::Roundness:
        case GeometricCharacteristic::Cylindricity:
            return false;
        case GeometricCharacteristic::LineProfile:
        case GeometricCharacteristic::SurfaceProfile:
        case GeometricCharacteristic::Position:
            return false;  // база не обязательна (ISO 1101: профиль и позиция — с базами или без)
        default:
            return true;
    }
}

SymbolGeometry characteristicSymbol(GeometricCharacteristic c, Point2 center, double size) {
    SymbolGeometry g;
    const double k = 0.6 * size;
    auto P = [&](double x, double y) { return Point2{center.x + x * k, center.y + y * k}; };
    auto arrow = [&](Point2 a, Point2 b) {
        g.lines.push_back({a, b});
        g.fills.push_back(arrowHead(b, sub(b, a), 0.3 * k, 30.0));
    };
    switch (c) {
        case GeometricCharacteristic::Straightness:
            g.lines.push_back({P(-0.5, 0), P(0.5, 0)});
            break;
        case GeometricCharacteristic::Flatness:
            g.polylines.push_back(Polyline{{P(-0.5, -0.2), P(0.25, -0.2), P(0.5, 0.2), P(-0.25, 0.2)}, true});
            break;
        case GeometricCharacteristic::Roundness:
            g.circles.push_back({center, 0.4 * k});
            break;
        case GeometricCharacteristic::Cylindricity: {
            const double r = 0.3;
            g.circles.push_back({center, r * k});
            // Две касательные под 60°.
            const double dx = r / std::sin(kPi / 3.0);
            const double cx = 0.5 * std::cos(kPi / 3.0), cy = 0.5 * std::sin(kPi / 3.0);
            for (double s : {-1.0, 1.0}) g.lines.push_back({P(s * dx - cx, -cy), P(s * dx + cx, cy)});
            break;
        }
        case GeometricCharacteristic::LineProfile:
            g.arcs.push_back(Arc{P(0, -0.2), 0.45 * k, 0.0, 180.0});
            break;
        case GeometricCharacteristic::SurfaceProfile:
            g.arcs.push_back(Arc{P(0, -0.2), 0.45 * k, 0.0, 180.0});
            g.lines.push_back({P(-0.45, -0.2), P(0.45, -0.2)});
            break;
        case GeometricCharacteristic::Parallelism: {
            const double cx = 0.5 * std::cos(kPi / 3.0), cy = 0.5 * std::sin(kPi / 3.0);
            for (double s : {-0.15, 0.15}) g.lines.push_back({P(s - cx, -cy), P(s + cx, cy)});
            break;
        }
        case GeometricCharacteristic::Perpendicularity:
            g.lines.push_back({P(-0.5, -0.45), P(0.5, -0.45)});
            g.lines.push_back({P(0, -0.45), P(0, 0.5)});
            break;
        case GeometricCharacteristic::Angularity:
            g.lines.push_back({P(-0.5, -0.4), P(0.5, -0.4)});
            g.lines.push_back({P(-0.5, -0.4), P(-0.5 + std::cos(kPi / 6.0), -0.4 + std::sin(kPi / 6.0))});
            break;
        case GeometricCharacteristic::Position:
            g.circles.push_back({center, 0.3 * k});
            g.lines.push_back({P(-0.5, 0), P(0.5, 0)});
            g.lines.push_back({P(0, -0.5), P(0, 0.5)});
            break;
        case GeometricCharacteristic::Coaxiality:
            g.circles.push_back({center, 0.2 * k});
            g.circles.push_back({center, 0.4 * k});
            break;
        case GeometricCharacteristic::Symmetry:
            g.lines.push_back({P(-0.5, 0), P(0.5, 0)});
            g.lines.push_back({P(-0.3, 0.25), P(0.3, 0.25)});
            g.lines.push_back({P(-0.3, -0.25), P(0.3, -0.25)});
            break;
        case GeometricCharacteristic::CircularRunout:
            arrow(P(-0.35, -0.35), P(0.35, 0.35));
            break;
        case GeometricCharacteristic::TotalRunout:
            arrow(P(-0.5, -0.4), P(0.0, 0.3));
            arrow(P(-0.05, -0.4), P(0.45, 0.3));
            g.lines.push_back({P(-0.5, -0.4), P(-0.05, -0.4)});
            break;
    }
    return g;
}

Result<FrameLayout> featureControlFrame(Point2 origin, const FeatureControlFrame& f, const AnnotationStyle& st) {
    if (f.tolerance.empty()) return invalid("GOST 2.308: tolerance value is empty");
    const bool form = f.characteristic == GeometricCharacteristic::Straightness ||
                      f.characteristic == GeometricCharacteristic::Flatness ||
                      f.characteristic == GeometricCharacteristic::Roundness ||
                      f.characteristic == GeometricCharacteristic::Cylindricity;
    if (form && !f.datums.empty()) return invalid("ISO 1101: a form tolerance has no datum");
    if (requiresDatum(f.characteristic) && f.datums.empty()) {
        return invalid("ISO 1101: this orientation/location/run-out tolerance needs a datum");
    }
    const double h = st.textHeight, H = 2.0 * h;
    const std::string value = (f.diameterZone ? "⌀" : "") + f.tolerance;
    std::vector<std::pair<double, std::string>> cells;  // ширина, текст
    cells.push_back({H, ""});
    cells.push_back({std::max(H, textWidth(value, h, st) + h), value});
    for (const auto& d : f.datums) {
        if (d.empty()) return invalid("GOST 2.308: empty datum letter");
        cells.push_back({std::max(H, textWidth(d, h, st) + h), d});
    }
    FrameLayout out;
    double x = origin.x;
    const double y0 = origin.y, y1 = origin.y + H;
    double total = 0.0;
    for (const auto& c : cells) total += c.first;
    out.box = {origin.x, y0, origin.x + total, y1};
    out.geometry.lines.push_back({{out.box.x0, y0}, {out.box.x1, y0}});
    out.geometry.lines.push_back({{out.box.x0, y1}, {out.box.x1, y1}});
    out.geometry.lines.push_back({{out.box.x0, y0}, {out.box.x0, y1}});
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const double w = cells[i].first;
        out.geometry.lines.push_back({{x + w, y0}, {x + w, y1}});
        if (i == 0) {
            out.geometry.append(characteristicSymbol(f.characteristic, {x + w / 2.0, (y0 + y1) / 2.0}, H));
        } else {
            out.geometry.labels.push_back(
                label({x + w / 2.0, (y0 + y1) / 2.0 - h / 2.0}, cells[i].second, h, 0.0, HAlign::Center));
        }
        x += w;
    }
    return out;
}

Result<SymbolGeometry> frameLeader(const std::vector<Point2>& points, const AnnotationStyle& st) {
    if (points.size() < 2) return invalid("frame leader needs at least 2 points");
    SymbolGeometry g;
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        if (norm(sub(points[i + 1], points[i])) < 1e-9) return invalid("frame leader: zero-length segment");
        g.lines.push_back({points[i], points[i + 1]});
    }
    const Point2 a = points[points.size() - 2], b = points.back();
    g.fills.push_back(arrowHead(b, sub(b, a), st.arrowLength, st.arrowAngleDeg));
    return g;
}

Result<SymbolGeometry> datumSymbol(Point2 foot, double surfaceAngleDeg, const std::string& letter,
                                   double leaderLength, const AnnotationStyle& st) {
    if (letter.empty()) return invalid("GOST 2.308: datum letter is empty");
    if (!(leaderLength >= 0)) return invalid("datum: leader length must be non-negative");
    const double h = st.textHeight, H = 2.0 * h;
    const double side = 2.0 * h / std::sqrt(3.0);
    const Placement P{foot, surfaceAngleDeg};
    SymbolGeometry g;
    g.fills.push_back(SolidFill{{P({-side / 2.0, 0}), P({side / 2.0, 0}), P({0, h})}});
    const double y0 = h + leaderLength;
    if (leaderLength > 0) g.lines.push_back({P({0, h}), P({0, y0})});
    g.polylines.push_back(Polyline{{P({-H / 2, y0}), P({H / 2, y0}), P({H / 2, y0 + H}), P({-H / 2, y0 + H})}, true});
    // Буква — без поворота (читается при любом положении базы).
    const Point2 c = P({0, y0 + H / 2.0});
    g.labels.push_back(label({c.x, c.y - h / 2.0}, letter, h, 0.0, HAlign::Center));
    return g;
}

// --- Выноски ---

Result<SymbolGeometry> leaderNote(Point2 target, Point2 knee, const std::string& text, LeaderEnd end,
                                  const AnnotationStyle& st) {
    if (text.empty()) return invalid("leader: empty text");
    if (norm(sub(knee, target)) < 1e-9) return invalid("leader: knee coincides with target");
    SymbolGeometry g;
    g.lines.push_back({target, knee});
    const double h = st.textHeight, w = textWidth(text, h, st) + 2.0;
    const bool right = knee.x >= target.x;
    const Point2 shelfEnd{knee.x + (right ? w : -w), knee.y};
    g.lines.push_back({knee, shelfEnd});
    g.labels.push_back(label({(knee.x + shelfEnd.x) / 2.0, knee.y + st.textGap}, text, h, 0.0, HAlign::Center));
    if (end == LeaderEnd::Arrow) {
        g.fills.push_back(arrowHead(target, sub(target, knee), st.arrowLength, st.arrowAngleDeg));
    } else if (end == LeaderEnd::Dot) {
        g.fills.push_back(disk(target, 0.75));
    }
    return g;
}

// --- Технические требования ---

Result<std::vector<std::string>> wrapText(const std::string& text, double width, double h,
                                          const AnnotationStyle& st) {
    std::istringstream is(text);
    std::vector<std::string> words;
    for (std::string w; is >> w;) words.push_back(w);
    if (words.empty()) return invalid("wrap: empty text");
    std::vector<std::string> lines;
    std::string cur;
    for (const auto& w : words) {
        if (textWidth(w, h, st) > width) return invalid("wrap: word wider than the column: " + w);
        const std::string cand = cur.empty() ? w : cur + " " + w;
        if (textWidth(cand, h, st) <= width) {
            cur = cand;
        } else {
            lines.push_back(cur);
            cur = w;
        }
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

Result<TechnicalRequirementsLayout> addTechnicalRequirements(Drawing& d, const TitleBlockLayout& layout,
                                                             const std::vector<std::string>& items,
                                                             const TechnicalRequirementsOptions& o,
                                                             const AnnotationStyle& st) {
    if (items.empty()) return invalid("technical requirements: no items");
    if (o.width <= 0 || o.width > 185.0 + 1e-9) return invalid("GOST 2.316: column width must be 0..185 mm");
    TechnicalRequirementsLayout out;
    const bool single = items.size() == 1;  // один пункт не нумеруется
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i].empty()) return invalid("technical requirements: empty item " + std::to_string(i + 1));
        const std::string text = single ? items[i] : std::to_string(i + 1) + ". " + items[i];
        auto lines = wrapText(text, o.width, o.textHeight, st);
        if (!lines) return lines.error();
        out.lines.insert(out.lines.end(), lines.value().begin(), lines.value().end());
    }
    const double pitch = o.lineSpacingFactor * o.textHeight;
    const double x0 = layout.titleBlock.x0, y0 = layout.titleBlock.y1 + o.gapAboveTitle;
    const double top = y0 + double(out.lines.size() - 1) * pitch + o.textHeight;
    if (top > layout.frame.y1) return invalid("technical requirements do not fit above the title block");
    out.box = {x0, y0, x0 + o.width, top};
    if (auto s = ensureLayer(d, st.layer, st.layerColor); !s) return s.error();
    for (std::size_t i = 0; i < out.lines.size(); ++i) {
        Text t;
        t.position = {x0, y0 + double(out.lines.size() - 1 - i) * pitch};
        t.utf8 = out.lines[i];
        t.height = o.textHeight;
        t.slanted = st.slantedText;
        if (auto s = d.add(Entity{t, st.layer, LineType::SolidThin}); !s) return s.error();
    }
    return out;
}

// --- Позиции ---

Result<SymbolGeometry> balloonGeometry(const Balloon& b, const BalloonOptions& o, const AnnotationStyle& st) {
    if (b.text.empty()) return invalid("balloon: position number is empty (call synchronizeBalloons)");
    const double h = o.textHeight;
    SymbolGeometry g;
    g.fills.push_back(disk(b.target, o.dotRadius));
    if (o.style == BalloonStyle::Shelf) {
        const double w = std::max(textWidth(b.text, h, st) + 2.0, h);
        const bool right = b.at.x >= b.target.x;
        const Point2 end{b.at.x + (right ? w : -w), b.at.y};
        g.lines.push_back({b.target, b.at});
        g.lines.push_back({b.at, end});
        g.labels.push_back(label({(b.at.x + end.x) / 2.0, b.at.y + st.textGap}, b.text, h, 0.0, HAlign::Center));
    } else {
        const double r = std::max(h, textWidth(b.text, h, st) / 2.0 + 1.0);
        g.circles.push_back({b.at, r});
        const Point2 d = sub(b.target, b.at);
        const double n = norm(d);
        if (n <= r) return invalid("balloon: target lies inside the balloon circle");
        g.lines.push_back({add(b.at, mul(r / n, d)), b.target});
        g.labels.push_back(label({b.at.x, b.at.y - h / 2.0}, b.text, h, 0.0, HAlign::Center));
    }
    return g;
}

void alignBalloonsInRow(std::vector<Balloon>& balloons, double y) {
    for (auto& b : balloons) b.at.y = y;
}

void alignBalloonsInColumn(std::vector<Balloon>& balloons, double x) {
    for (auto& b : balloons) b.at.x = x;
}

}  // namespace bendcore::drawing_v2::annotations
