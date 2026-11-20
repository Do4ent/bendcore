// Условное изображение зубчатых колёс (ГОСТ 2.402-68) и таблица параметров зубчатого венца
// (ГОСТ 2.403-75). Документация — docs/drawing/ANNOTATIONS.md, раздел «Зубчатые колёса».

#include <bendcore/drawing_v2/gears.hpp>
#include <bendcore/drawing_v2/stroke_font.hpp>
#include <bendcore/kernel/boolean.hpp>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Geom_Circle.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace bendcore::drawing_v2::annotations {

namespace {

constexpr double kPi = 3.14159265358979323846;

Point2 add(Point2 a, Point2 b) { return {a.x + b.x, a.y + b.y}; }
Point2 mul(double k, Point2 a) { return {k * a.x, k * a.y}; }

views::Vec3 vec(const gp_XYZ& v) { return {v.X(), v.Y(), v.Z()}; }

double invo(double a) { return std::tan(a) - a; }

// Число с десятичной запятой (ЕСКД), без лишних нулей: 40,058; 2; 0,25.
std::string decimal(double v, int digits = 3) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", digits, v);
    std::string s = buf;
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    if (s == "-0") s = "0";
    std::replace(s.begin(), s.end(), '.', ',');
    return s;
}

// Отсечение замкнутого контура полуплоскостью a·p ≤ c (Сазерленд — Ходжман).
std::vector<Point2> clipHalf(const std::vector<Point2>& poly, Point2 a, double c) {
    std::vector<Point2> out;
    const std::size_t n = poly.size();
    if (n == 0) return out;
    auto f = [&](Point2 p) { return a.x * p.x + a.y * p.y - c; };
    for (std::size_t i = 0; i < n; ++i) {
        const Point2 p = poly[i], q = poly[(i + 1) % n];
        const double fp = f(p), fq = f(q);
        if (fp <= 0) out.push_back(p);
        if ((fp < 0 && fq > 0) || (fp > 0 && fq < 0)) {
            const double t = fp / (fp - fq);
            out.push_back({p.x + t * (q.x - p.x), p.y + t * (q.y - p.y)});
        }
    }
    return out;
}

double polyArea(const std::vector<Point2>& p) {
    double a = 0.0;
    for (std::size_t i = 0; i < p.size(); ++i) {
        const Point2 u = p[i], v = p[(i + 1) % p.size()];
        a += u.x * v.y - v.x * u.y;
    }
    return 0.5 * a;
}

}  // namespace

namespace {

// Кольцо rIn…rOut на ширине венца.
Result<TopoDS_Shape> gearRing(const modeling::GearSpec& gear, double rIn, double rOut) {
    try {
        const gp_Ax2 frame(gear.axis.Location(), gear.axis.Direction(), gear.axis.XDirection());
        if (!(rIn > 0.0 && rOut > rIn)) return Error{ErrorCode::InvalidInput, "gear filler: root diameter is too small"};
        Handle(Geom_Circle) co = new Geom_Circle(gp_Circ(frame, rOut));
        Handle(Geom_Circle) ci = new Geom_Circle(gp_Circ(frame, rIn));
        BRepBuilderAPI_MakeFace mf(gp_Pln(gp_Ax3(frame)), BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(co)).Wire(), true);
        mf.Add(TopoDS::Wire(BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(ci)).Wire().Reversed()));
        if (!mf.IsDone()) return Error{ErrorCode::OperationFailed, "gear filler: ring face failed"};
        BRepPrimAPI_MakePrism prism(mf.Face(), gp_Vec(frame.Direction()) * gear.width, false);
        prism.Build();
        if (!prism.IsDone()) return Error{ErrorCode::OperationFailed, "gear filler: prism failed"};
        return prism.Shape();
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, std::string("gear filler: ") + e.what()};
    }
}

void mergeHistory(opencascade::handle<BRepTools_History>& h, const opencascade::handle<BRepTools_History>& next) {
    if (next.IsNull()) return;
    if (h.IsNull()) {
        h = new BRepTools_History(*next);
    } else {
        h->Merge(next);
    }
}

}  // namespace

Result<TopoDS_Shape> gearFiller(const modeling::GearSpec& gear) {
    auto g = modeling::gearGeometry(gear);
    if (!g) return g.error();
    return gearRing(gear, g.value().df / 2.0 - gear.module / 4.0, g.value().da / 2.0);
}

Result<ConventionalShape> conventionalGearShape(const TopoDS_Shape& real, const std::vector<modeling::GearInfo>& gears) {
    if (real.IsNull()) return Error{ErrorCode::InvalidInput, "conventional gear shape: empty shape"};
    if (gears.empty()) return ConventionalShape{real, {}};
    // Зубья срезаются кольцом df/2 − m/4 … da/2 + m, затем ставится сплошное кольцо до da/2:
    // цилиндр вершин — одна грань кольца (слияние частей вершин зубьев с заполнением впадин
    // UnifySameDomain выполняет не всегда — замкнутую цилиндрическую грань из частей не строит).
    std::vector<TopoDS_Shape> cutters, fillers;
    for (const auto& g : gears) {
        auto geo = modeling::gearGeometry(g.gear);
        if (!geo) return geo.error();
        const double rIn = geo.value().df / 2.0 - g.gear.module / 4.0;
        auto c = gearRing(g.gear, rIn, geo.value().da / 2.0 + g.gear.module);
        if (!c) return c.error();
        auto f = gearRing(g.gear, rIn, geo.value().da / 2.0);
        if (!f) return f.error();
        cutters.push_back(c.value());
        fillers.push_back(f.value());
    }
    kernel::BooleanOptions opt;
    opt.checkInputs = false;  // тело модели и кольца валидны по построению
    auto cut = kernel::booleanOp(kernel::BooleanType::Cut, {real}, cutters, opt);
    if (!cut) return Error{cut.error().code, "conventional gear shape: " + cut.error().message};
    auto fused = kernel::booleanOp(kernel::BooleanType::Fuse, {cut.value().shape}, fillers, opt);
    if (!fused) return Error{fused.error().code, "conventional gear shape: " + fused.error().message};
    try {
        ShapeUpgrade_UnifySameDomain unify(fused.value().shape, true, true, true);
        unify.Build();
        opencascade::handle<BRepTools_History> h;
        mergeHistory(h, cut.value().history);
        mergeHistory(h, fused.value().history);
        mergeHistory(h, unify.History());
        return ConventionalShape{unify.Shape(), h};
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, std::string("conventional gear shape: unify: ") + e.what()};
    }
}

Result<modeling::NamedShape> conventionalGearModel(const modeling::NamedShape& real,
                                                   const std::vector<modeling::GearInfo>& gears) {
    if (gears.empty()) return real;
    auto c = conventionalGearShape(real.shape(), gears);
    if (!c) return c.error();
    modeling::NamingRequest req;
    req.featureId = "GearConvention";
    req.result = c.value().shape;
    req.history = c.value().history;
    req.inputs = {&real};
    return modeling::buildNamedShape(req);
}

Result<std::vector<GearImage>> gearImages(const std::vector<modeling::GearInfo>& gears, const views::ViewFrame& frame,
                                          const std::optional<views::CuttingPlane>& sectionPlane,
                                          const GearImageOptions& options) {
    std::vector<GearImage> out;
    for (std::size_t i = 0; i < gears.size(); ++i) {
        const modeling::GearSpec& s = gears[i].gear;
        auto geo = modeling::gearGeometry(s);
        if (!geo) return geo.error();
        const views::Vec3 d = vec(s.axis.Direction().XYZ()), o = vec(s.axis.Location().XYZ());
        const double along = views::dot(d, frame.toward);
        GearImage img;
        img.gear = i;
        img.spec = s;
        img.geometry = geo.value();
        img.origin = frame.project(o);
        if (std::abs(along) <= options.angularTolerance) {
            img.aspect = GearAspect::Side;
            const Point2 a = frame.project(d);
            const double len = std::hypot(a.x, a.y);
            img.axis = {a.x / len, a.y / len};
        } else if (std::abs(std::abs(along) - 1.0) <= options.angularTolerance) {
            img.aspect = GearAspect::End;
        } else {
            continue;
        }
        if (sectionPlane && img.aspect == GearAspect::Side) {
            const double n = views::length(sectionPlane->sight);
            const double dist = views::dot(o - sectionPlane->point, sectionPlane->sight) / n;
            img.section = std::abs(dist) <= 1e-6 && std::abs(views::dot(d, sectionPlane->sight)) <= options.angularTolerance * n;
        }
        out.push_back(std::move(img));
    }
    return out;
}

std::vector<GearLine> gearLines(const GearImage& img, const views::SheetView& view, const views::RenderOptions& o,
                                double overshoot, const GearImageOptions& io) {
    std::vector<GearLine> out;
    const double r = img.geometry.d / 2.0, rf = img.geometry.df / 2.0;
    const Point2 c = view.toSheet(img.origin);
    if (img.aspect == GearAspect::End) {
        out.push_back({GearLine::Role::PitchCircle, Circle{c, r * view.scale}, LineType::DashDotThin, o.centerLayer});
        if (io.rootCircleOnEnd)
            out.push_back({GearLine::Role::RootCircle, Circle{c, rf * view.scale}, LineType::SolidThin, o.thinLayer});
        return out;
    }
    // Ось на листе: масштаб положительный, направления вида и листа совпадают.
    const Point2 u = img.axis, w{-img.axis.y, img.axis.x};
    const double b = img.spec.width * view.scale;
    for (double side : {1.0, -1.0}) {
        const Point2 p0 = add(c, mul(side * r * view.scale, w));
        out.push_back({GearLine::Role::Pitch, LineSeg{add(p0, mul(-overshoot, u)), add(p0, mul(b + overshoot, u))},
                       LineType::DashDotThin, o.centerLayer});
        const Point2 q0 = add(c, mul(side * rf * view.scale, w));
        out.push_back({GearLine::Role::Root, LineSeg{q0, add(q0, mul(b, u))},
                       img.section ? LineType::SolidThick : LineType::SolidThin, img.section ? o.visibleLayer : o.thinLayer});
    }
    return out;
}

Status renderGears(Drawing& drawing, const views::SheetView& view, const std::vector<GearImage>& images,
                   const views::RenderOptions& options, double overshoot, const GearImageOptions& io) {
    if (auto st = views::ensureViewLayers(drawing, options); !st) return st;
    for (const GearImage& img : images)
        for (const GearLine& l : gearLines(img, view, options, overshoot, io)) {
            Geometry g = std::holds_alternative<LineSeg>(l.curve) ? Geometry(std::get<LineSeg>(l.curve))
                                                                  : Geometry(std::get<Circle>(l.curve));
            if (auto st = drawing.add(Entity{g, l.layer, l.lineType}); !st) return st;
        }
    return okStatus();
}

views::SectionView withUncutTeeth(const views::SectionView& section, const std::vector<GearImage>& images) {
    views::SectionView out = section;
    for (const GearImage& img : images) {
        if (!img.section || img.aspect != GearAspect::Side) continue;
        const Point2 u = img.axis, w{-img.axis.y, img.axis.x}, o = img.origin;
        const double oU = u.x * o.x + u.y * o.y, oW = w.x * o.x + w.y * o.y;
        const double b = img.spec.width, rf = img.geometry.df / 2.0;
        // Выпуклые области дополнения полосы: s ≤ 0; s ≥ b; 0 ≤ s ≤ b и |t| ≤ rf.
        struct Half {
            Point2 a;
            double c;
        };
        const std::vector<std::vector<Half>> keep = {
            {{u, oU}},
            {{mul(-1.0, u), -(oU + b)}},
            {{mul(-1.0, u), -oU}, {u, oU + b}, {w, oW + rf}, {mul(-1.0, w), -(oW - rf)}},
        };
        for (auto& region : out.regions) {
            std::vector<std::vector<Point2>> clipped;
            for (const auto& contour : region)
                for (const auto& k : keep) {
                    std::vector<Point2> p = contour;
                    for (const Half& h : k) p = clipHalf(p, h.a, h.c);
                    if (p.size() >= 3 && std::abs(polyArea(p)) > 1e-9) clipped.push_back(std::move(p));
                }
            region = std::move(clipped);
        }
    }
    // Пустые области удаляются вместе с их владельцами.
    views::SectionView compact = out;
    compact.regions.clear();
    compact.regionOwner.clear();
    for (std::size_t i = 0; i < out.regions.size(); ++i) {
        if (out.regions[i].empty()) continue;
        compact.regions.push_back(out.regions[i]);
        if (i < out.regionOwner.size()) compact.regionOwner.push_back(out.regionOwner[i]);
    }
    return compact;
}

double commonNormalLength(const modeling::GearSpec& gear, int& zn) {
    const auto g = modeling::gearGeometry(gear);
    const double a = gear.rack.alpha, m = gear.module, x = gear.shift;
    const double alphaT = g ? g.value().alphaT : a;
    const double zv = gear.teeth * invo(alphaT) / invo(a);  // приведённое число зубьев
    const double ax = std::acos(zv * std::cos(a) / (zv + 2.0 * x));
    zn = static_cast<int>(std::lround(zv * ax / kPi + 0.5));
    zn = std::max(zn, 1);
    return m * std::cos(a) * (kPi * (zn - 0.5) + zv * invo(a)) + 2.0 * x * m * std::sin(a);
}

Result<std::vector<GearTableRow>> gearTableRows(const modeling::GearSpec& gear, const GearTableDef& def) {
    auto g = modeling::gearGeometry(gear);
    if (!g) return g.error();
    std::vector<GearTableRow> rows;
    const bool helical = gear.helix > 0.0;
    rows.push_back({"Модуль", "m", decimal(gear.module), 1});
    rows.push_back({"Число зубьев", "z", std::to_string(gear.teeth), 1});
    rows.push_back({"Тип зуба", "—", helical ? "Косозубый" : "Прямозубый", 1});
    if (helical) {
        rows.push_back({"Направление линии зуба", "—", gear.hand == modeling::GearHand::Left ? "Левое" : "Правое", 1});
        const double deg = gear.helix * 180.0 / kPi;
        const int dd = static_cast<int>(std::floor(deg + 1e-9));
        const int mm = static_cast<int>(std::lround((deg - dd) * 60.0));
        rows.push_back({"Угол наклона", "β", std::to_string(dd) + "°" + (mm ? std::to_string(mm) + "′" : ""), 1});
    }
    rows.push_back({"Нормальный исходный контур", "—", gear.rack.standard, 1});
    rows.push_back({"Коэффициент смещения", "x", decimal(gear.shift), 1});
    if (!def.accuracy.empty()) rows.push_back({"Степень точности", "—", def.accuracy, 1});
    if (def.commonNormal) {
        int zn = 0;
        const double w = commonNormalLength(gear, zn);
        rows.push_back({"Длина общей нормали", "W", decimal(w), 2});
        rows.push_back({"Число зубьев в длине общей нормали", "zn", std::to_string(zn), 2});
    }
    rows.push_back({"Делительный диаметр", "d", decimal(g.value().d), 3});
    if (def.matingTeeth > 0) rows.push_back({"Число зубьев сопряжённого колеса", "z2", std::to_string(def.matingTeeth), 3});
    if (!def.matingDesignation.empty()) rows.push_back({"Обозначение чертежа сопряжённого колеса", "—", def.matingDesignation, 3});
    if (def.centerDistance > 0.0) rows.push_back({"Межосевое расстояние", "aw", decimal(def.centerDistance), 3});
    return rows;
}

Result<GearTableLayout> addGearTable(Drawing& drawing, const Rect& frame, const std::vector<GearTableRow>& rows,
                                     const TextMetrics* metrics) {
    if (rows.empty()) return Error{ErrorCode::InvalidInput, "gear table: no rows"};
    const TextMetrics& tm = metrics ? *metrics : static_cast<const TextMetrics&>(gostStrokeFont());
    if (auto st = views::ensureViewLayers(drawing); !st) return st.error();
    constexpr double kW1 = 65.0, kW2 = 10.0, kW3 = 35.0, kRow = 8.0;
    const double x1 = frame.x1, x0 = x1 - (kW1 + kW2 + kW3), y1 = frame.y1;
    const double y0 = y1 - kRow * double(rows.size());
    if (x0 < frame.x0 || y0 < frame.y0) return Error{ErrorCode::InvalidInput, "gear table does not fit the frame"};
    GearTableLayout lay;
    lay.box = {x0, y0, x1, y1};
    auto line = [&](Point2 a, Point2 b, bool thick) {
        return drawing.add(Entity{LineSeg{a, b}, thick ? "VISIBLE" : "THIN", thick ? LineType::SolidThick : LineType::SolidThin});
    };
    for (Point2 a : {Point2{x0, y0}, Point2{x0, y1}})
        if (auto st = line(a, {x1, a.y}, true); !st) return st.error();
    for (double x : {x0, x0 + kW1, x0 + kW1 + kW2, x1})
        if (auto st = line({x, y0}, {x, y1}, true); !st) return st.error();
    for (std::size_t i = 1; i < rows.size(); ++i) {
        const double y = y1 - kRow * double(i);
        const bool partBreak = rows[i].part != rows[i - 1].part;
        if (auto st = line({x0, y}, {x1, y}, partBreak); !st) return st.error();
    }
    // Текст строки: высота 3,5, если помещается, иначе 2,5; иначе — две строки по 2,5.
    auto fits = [&](const std::u32string& t, double h, double width) { return tm.textWidthMm(t, h) <= width; };
    auto cellText = [&](const std::string& utf8, double xa, double width, double yTop, HAlign align) -> Status {
        auto u = decodeUtf8(utf8);
        if (!u) return u.error();
        for (char32_t cp : u.value())
            if (cp != U' ' && !tm.hasGlyph(cp)) return okStatus();  // знака нет в шрифте
        const double pad = 1.5, avail = width - 2 * pad;
        const double xText = align == HAlign::Left ? xa + pad : xa + width / 2;
        for (double h : {3.5, 2.5}) {
            if (!fits(u.value(), h, avail)) continue;
            Text t;
            t.position = {xText, yTop - kRow / 2};
            t.utf8 = utf8;
            t.height = h;
            t.hAlign = align;
            t.vAlign = VAlign::Middle;
            return drawing.add(Entity{t, "SYMBOLS", LineType::SolidThin});
        }
        // Две строки: перенос по последнему пробелу, при котором первая строка помещается.
        const std::u32string& s = u.value();
        for (std::size_t k = s.size(); k-- > 0;) {
            if (s[k] != U' ') continue;
            const std::u32string a = s.substr(0, k), b = s.substr(k + 1);
            if (!fits(a, 2.5, avail) || !fits(b, 2.5, avail)) continue;
            for (int line = 0; line < 2; ++line) {
                Text t;
                t.position = {xText, yTop - (line == 0 ? 3.4 : 7.0)};
                t.utf8 = encodeUtf8(line == 0 ? a : b);
                t.height = 2.5;
                t.hAlign = align;
                t.vAlign = VAlign::Baseline;
                if (auto st = drawing.add(Entity{t, "SYMBOLS", LineType::SolidThin}); !st) return st;
            }
            return okStatus();
        }
        return Status::failure(ErrorCode::InvalidInput, "gear table: '" + utf8 + "' does not fit its cell");
    };
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const double top = y1 - kRow * double(i);
        lay.rowTops.push_back(top);
        if (auto st = cellText(rows[i].name, x0, kW1, top, HAlign::Left); !st) return st.error();
        if (auto st = cellText(rows[i].symbol, x0 + kW1, kW2, top, HAlign::Center); !st) return st.error();
        if (auto st = cellText(rows[i].value, x0 + kW1 + kW2, kW3, top, HAlign::Center); !st) return st.error();
    }
    return lay;
}

}  // namespace bendcore::drawing_v2::annotations
