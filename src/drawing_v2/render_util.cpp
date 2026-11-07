#include "render_util.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <numbers>

namespace bendcore::drawing_v2::detail {

std::string num(double v, int decimals) {
    if (std::fabs(v) < 0.5 * std::pow(10.0, -decimals)) return "0";
    char buf[64];
    auto res = std::to_chars(buf, buf + sizeof buf, v, std::chars_format::fixed, decimals);
    std::string s(buf, res.ptr);
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    if (s == "-0") s = "0";
    return s;
}

double textBaselineShift(VAlign v, double h) {
    switch (v) {
        case VAlign::Baseline: return 0.0;
        case VAlign::Middle: return -h / 2.0;
        case VAlign::Top: return -h;
    }
    return 0.0;
}

Point2 textBaselineOrigin(const Text& t, double width) {
    double along = 0.0;
    if (t.hAlign == HAlign::Center) along = -width / 2.0;
    if (t.hAlign == HAlign::Right) along = -width;
    const double across = textBaselineShift(t.vAlign, t.height);
    const double a = t.rotationDeg * std::numbers::pi / 180.0;
    const double c = std::cos(a), s = std::sin(a);
    return {t.position.x + along * c - across * s, t.position.y + along * s + across * c};
}

double arcSweepDeg(double startDeg, double endDeg) {
    double sweep = std::fmod(endDeg - startDeg, 360.0);
    if (sweep <= 0.0) sweep += 360.0;
    return sweep;
}

std::vector<Point2> arcToBezier(Point2 c, double r, double a0, double a1) {
    double sweep = a1 - a0;
    const int n = std::max(1, int(std::ceil(sweep / (std::numbers::pi / 2.0) - 1e-9)));
    const double step = sweep / n;
    const double k = 4.0 / 3.0 * std::tan(step / 4.0);
    std::vector<Point2> pts;
    pts.push_back({c.x + r * std::cos(a0), c.y + r * std::sin(a0)});
    for (int i = 0; i < n; ++i) {
        const double t0 = a0 + step * i, t1 = t0 + step;
        const double c0 = std::cos(t0), s0 = std::sin(t0), c1 = std::cos(t1), s1 = std::sin(t1);
        pts.push_back({c.x + r * (c0 - k * s0), c.y + r * (s0 + k * c0)});
        pts.push_back({c.x + r * (c1 + k * s1), c.y + r * (s1 - k * c1)});
        pts.push_back({c.x + r * c1, c.y + r * s1});
    }
    return pts;
}

}  // namespace bendcore::drawing_v2::detail
