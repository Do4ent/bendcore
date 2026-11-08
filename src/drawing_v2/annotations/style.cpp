// Общие функции оформления: ширина текста, стрелки, запись чисел и углов по ЕСКД.

#include <bendcore/drawing_v2/annotation_style.hpp>

#include <cmath>
#include <cstdio>
#include <numbers>

namespace bendcore::drawing_v2::annotations {

double estimateTextWidth(std::string_view utf8, double h, const TrueTypeFont* font, double widthFactor) {
    auto cps = decodeUtf8(utf8);
    if (!cps) return 0.0;
    if (font) return font->textWidthMm(cps.value(), h) * widthFactor;
    // Шрифт типа Б (ГОСТ 2.304): прописные и цифры 6/10 h, строчные 5/10 h, расстояние между
    // буквами 2/10 h.
    double w = 0.0;
    const auto& s = cps.value();
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char32_t c = s[i];
        const bool lowerCase = (c >= U'a' && c <= U'z') || (c >= U'а' && c <= U'я') || c == U'ё';
        w += (lowerCase ? 0.5 : 0.6) * h;
        if (i + 1 < s.size()) w += 0.2 * h;
    }
    return w * widthFactor;
}

Status ensureLayer(Drawing& drawing, const std::string& name, int aci) {
    if (drawing.findLayer(name)) return okStatus();
    return drawing.addLayer(Layer{name, aci});
}

SolidFill arrowHead(Point2 tip, Point2 direction, double length, double fullAngleDeg) {
    const double n = std::hypot(direction.x, direction.y);
    const double ux = direction.x / n, uy = direction.y / n;
    const double half = length * std::tan(fullAngleDeg / 2.0 * std::numbers::pi / 180.0);
    const Point2 base{tip.x - ux * length, tip.y - uy * length};
    return SolidFill{{tip, {base.x - uy * half, base.y + ux * half}, {base.x + uy * half, base.y - ux * half}}};
}

std::string formatNumber(double value, int decimals) {
    if (decimals < 0) decimals = 0;
    if (decimals > 6) decimals = 6;
    const double p = std::pow(10.0, decimals);
    double r = std::round(value * p) / p;
    if (r == 0.0) r = 0.0;  // «-0» → «0»
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, r);
    std::string s = buf;
    if (s.find('.') != std::string::npos) {
        while (s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
    }
    for (char& c : s) {
        if (c == '.') c = ',';
    }
    if (s == "-0") s = "0";
    return s;
}

std::string formatAngle(double degrees, bool seconds) {
    const bool neg = degrees < 0;
    const double unit = seconds ? 3600.0 : 60.0;
    long long total = std::llround(std::abs(degrees) * unit);
    const long long d = total / (long long)unit;
    total -= d * (long long)unit;
    long long m = seconds ? total / 60 : total;
    long long s = seconds ? total % 60 : 0;
    std::string out = (neg ? "−" : "") + std::to_string(d) + "°";
    if (m != 0 || s != 0) out += std::to_string(m) + "′";
    if (s != 0) out += std::to_string(s) + "″";
    return out;
}

}  // namespace bendcore::drawing_v2::annotations
