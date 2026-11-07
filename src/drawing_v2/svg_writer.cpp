// SVG 1.1. Пользовательская единица = 1 мм листа (viewBox = размер листа, width/height в мм).
// Ось Y листа направлена вверх, в SVG — вниз: y_svg = H − y.

#include <bendcore/drawing_v2/export.hpp>

#include "render_util.hpp"

#include <cmath>
#include <numbers>
#include <sstream>

namespace bendcore::drawing_v2 {

using detail::num;

namespace {

std::string xmlEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out.push_back(c);
        }
    }
    return out;
}

}  // namespace

Result<std::string> writeSvg(const Drawing& drawing, const SvgOptions& options) {
    const double W = drawing.sheet().widthMm, H = drawing.sheet().heightMm;
    double capRatio = options.capHeightRatio;
    if (options.font) capRatio = options.font->capHeightRatio();
    if (!(capRatio > 0.1 && capRatio < 1.5)) {
        return Error{ErrorCode::InvalidInput, "SVG: cap height ratio out of range"};
    }
    auto Y = [&](double y) { return H - y; };
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n";
    os << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"" << num(W) << "mm\" height=\""
       << num(H) << "mm\" viewBox=\"0 0 " << num(W) << ' ' << num(H) << "\">\n";
    if (!options.title.empty()) {
        if (auto d = decodeUtf8(options.title); !d) return d.error();
        os << "<title>" << xmlEscape(options.title) << "</title>\n";
    }
    os << "<rect x=\"0\" y=\"0\" width=\"" << num(W) << "\" height=\"" << num(H)
       << "\" fill=\"#ffffff\" stroke=\"none\"/>\n";

    auto strokeAttrs = [&](LineType lt) {
        const auto& spec = drawing.lineTypeSpec(lt);
        const double w = drawing.lineWidth(lt);
        std::string a = " fill=\"none\" stroke=\"#000000\" stroke-width=\"" + num(w) +
                        "\" stroke-linecap=\"butt\" stroke-linejoin=\"round\"";
        const auto dash = renderDashArray(spec.pattern, w);
        if (!dash.empty()) {
            a += " stroke-dasharray=\"";
            for (std::size_t i = 0; i < dash.size(); ++i) a += (i ? "," : "") + num(dash[i]);
            a += "\"";
        }
        a += " data-linetype=\"" + spec.dxfName + "\"";
        return a;
    };

    for (const Layer& layer : drawing.layers()) {
        bool any = false;
        for (const Entity& e : drawing.entities()) any = any || e.layer == layer.name;
        if (!any) continue;
        os << "<g id=\"layer-" << layer.name << "\">\n";
        for (const Entity& e : drawing.entities()) {
            if (e.layer != layer.name) continue;
            if (const auto* l = std::get_if<LineSeg>(&e.geometry)) {
                os << "<line x1=\"" << num(l->a.x) << "\" y1=\"" << num(Y(l->a.y)) << "\" x2=\""
                   << num(l->b.x) << "\" y2=\"" << num(Y(l->b.y)) << '"' << strokeAttrs(e.lineType)
                   << "/>\n";
            } else if (const auto* c = std::get_if<Circle>(&e.geometry)) {
                os << "<circle cx=\"" << num(c->center.x) << "\" cy=\"" << num(Y(c->center.y))
                   << "\" r=\"" << num(c->radius) << '"' << strokeAttrs(e.lineType) << "/>\n";
            } else if (const auto* a = std::get_if<Arc>(&e.geometry)) {
                const double sweep = detail::arcSweepDeg(a->startDeg, a->endDeg);
                const double a0 = a->startDeg * std::numbers::pi / 180.0;
                const double a1 = a0 + sweep * std::numbers::pi / 180.0;
                const Point2 p0{a->center.x + a->radius * std::cos(a0), a->center.y + a->radius * std::sin(a0)};
                const Point2 p1{a->center.x + a->radius * std::cos(a1), a->center.y + a->radius * std::sin(a1)};
                // Против часовой стрелки на листе = sweep-flag 0 в системе SVG (ось Y вниз).
                // Дугу 360° SVG не выражает одной командой A, поэтому делим её пополам.
                os << "<path d=\"M " << num(p0.x) << ' ' << num(Y(p0.y));
                if (sweep >= 359.999999) {
                    const Point2 pm{a->center.x - (p0.x - a->center.x), a->center.y - (p0.y - a->center.y)};
                    os << " A " << num(a->radius) << ' ' << num(a->radius) << " 0 0 0 " << num(pm.x)
                       << ' ' << num(Y(pm.y));
                    os << " A " << num(a->radius) << ' ' << num(a->radius) << " 0 0 0 " << num(p1.x)
                       << ' ' << num(Y(p1.y));
                } else {
                    os << " A " << num(a->radius) << ' ' << num(a->radius) << " 0 " << (sweep > 180.0 ? 1 : 0)
                       << " 0 " << num(p1.x) << ' ' << num(Y(p1.y));
                }
                os << '"' << strokeAttrs(e.lineType) << "/>\n";
            } else if (const auto* p = std::get_if<Polyline>(&e.geometry)) {
                os << '<' << (p->closed ? "polygon" : "polyline") << " points=\"";
                for (std::size_t i = 0; i < p->points.size(); ++i) {
                    os << (i ? " " : "") << num(p->points[i].x) << ',' << num(Y(p->points[i].y));
                }
                os << '"' << strokeAttrs(e.lineType) << "/>\n";
            } else if (const auto* t = std::get_if<Text>(&e.geometry)) {
                const double fontSize = t->height / capRatio;
                const char* anchor = t->hAlign == HAlign::Left     ? "start"
                                     : t->hAlign == HAlign::Center ? "middle"
                                                                   : "end";
                // Перенос в точку привязки, поворот, сдвиг базовой линии, наклон 75° (skewX −15°
                // в системе с осью Y вниз наклоняет верх букв вправо).
                const double shift = detail::textBaselineShift(t->vAlign, t->height);
                os << "<text x=\"0\" y=\"0\" transform=\"translate(" << num(t->position.x) << ' '
                   << num(Y(t->position.y)) << ')';
                if (t->rotationDeg != 0.0) os << " rotate(" << num(-t->rotationDeg) << ')';
                if (shift != 0.0) os << " translate(0 " << num(-shift) << ')';
                if (t->slanted) os << " skewX(" << num(-(90.0 - kGostTextSlantDeg)) << ')';
                if (t->widthFactor != 1.0) os << " scale(" << num(t->widthFactor) << " 1)";
                os << "\" font-family=\"" << xmlEscape(options.fontFamily) << "\" font-size=\""
                   << num(fontSize) << "\" text-anchor=\"" << anchor
                   << "\" fill=\"#000000\" stroke=\"none\" data-cap-height=\"" << num(t->height)
                   << "\">" << xmlEscape(t->utf8) << "</text>\n";
            } else if (const auto* ht = std::get_if<Hatch>(&e.geometry)) {
                auto segs = hatchSegments(*ht);
                if (!segs) return segs.error();
                os << "<g class=\"hatch\">\n";
                for (const auto& s : segs.value()) {
                    os << "<line x1=\"" << num(s.a.x) << "\" y1=\"" << num(Y(s.a.y)) << "\" x2=\""
                       << num(s.b.x) << "\" y2=\"" << num(Y(s.b.y)) << '"' << strokeAttrs(e.lineType)
                       << "/>\n";
                }
                os << "</g>\n";
            }
        }
        os << "</g>\n";
    }
    os << "</svg>\n";
    return os.str();
}

}  // namespace bendcore::drawing_v2
