#include <bendcore/drawing_v2/line_types.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace bendcore::drawing_v2 {

namespace {
constexpr double kEps = 1e-9;
}

LineTypeSpec defaultLineTypeSpec(LineType type) {
    LineTypeSpec s;
    s.type = type;
    switch (type) {
        case LineType::SolidThick:
            s.dxfName = "Continuous";
            s.gostName = "Сплошная толстая основная";
            s.widthFactor = s.minFactor = s.maxFactor = 1.0;
            break;
        case LineType::SolidThin:
            s.dxfName = "Continuous";
            s.gostName = "Сплошная тонкая";
            s.widthFactor = 0.5;
            s.minFactor = 1.0 / 3.0;
            s.maxFactor = 0.5;
            break;
        case LineType::SolidWavy:
            s.dxfName = "Continuous";
            s.gostName = "Сплошная волнистая";
            s.widthFactor = 0.5;
            s.minFactor = 1.0 / 3.0;
            s.maxFactor = 0.5;
            break;
        case LineType::Dashed:
            s.dxfName = "GOST_DASHED";
            s.gostName = "Штриховая";
            s.widthFactor = 0.5;
            s.minFactor = 1.0 / 3.0;
            s.maxFactor = 0.5;
            s.pattern = {4.0, -1.5};
            s.dashMin = 2.0;
            s.dashMax = 8.0;
            s.gapMin = 1.0;
            s.gapMax = 2.0;
            break;
        case LineType::DashDotThin:
            s.dxfName = "GOST_DASHDOT_THIN";
            s.gostName = "Штрихпунктирная тонкая";
            s.widthFactor = 0.5;
            s.minFactor = 1.0 / 3.0;
            s.maxFactor = 0.5;
            s.pattern = {15.0, -1.5, 0.0, -1.5};
            s.dashMin = 5.0;
            s.dashMax = 30.0;
            s.gapMin = 3.0;
            s.gapMax = 5.0;
            break;
        case LineType::DashDotThick:
            s.dxfName = "GOST_DASHDOT_THICK";
            s.gostName = "Штрихпунктирная утолщённая";
            s.widthFactor = 2.0 / 3.0;
            s.minFactor = 0.5;
            s.maxFactor = 2.0 / 3.0;
            s.pattern = {6.0, -1.5, 0.0, -1.5};
            s.dashMin = 3.0;
            s.dashMax = 8.0;
            s.gapMin = 3.0;
            s.gapMax = 4.0;
            break;
        case LineType::Open:
            s.dxfName = "Continuous";
            s.gostName = "Разомкнутая";
            s.widthFactor = 1.5;
            s.minFactor = 1.0;
            s.maxFactor = 1.5;
            s.strokeMin = 8.0;
            s.strokeMax = 20.0;
            break;
        case LineType::SolidThinZigzag:
            s.dxfName = "Continuous";
            s.gostName = "Сплошная тонкая с изломами";
            s.widthFactor = 0.5;
            s.minFactor = 1.0 / 3.0;
            s.maxFactor = 0.5;
            break;
        case LineType::DashDotDot:
            s.dxfName = "GOST_DASHDOTDOT";
            s.gostName = "Штрихпунктирная с двумя точками тонкая";
            s.widthFactor = 0.5;
            s.minFactor = 1.0 / 3.0;
            s.maxFactor = 0.5;
            s.pattern = {15.0, -5.0 / 3.0, 0.0, -5.0 / 3.0, 0.0, -5.0 / 3.0};
            s.dashMin = 5.0;
            s.dashMax = 30.0;
            s.gapMin = 4.0;
            s.gapMax = 6.0;
            break;
    }
    return s;
}

Status validateMainThickness(double s) {
    if (!std::isfinite(s) || s < 0.5 - kEps || s > 1.4 + kEps) {
        return Error{ErrorCode::InvalidInput,
                     "GOST 2.303: main line thickness s must be 0.5..1.4 mm, got " +
                         std::to_string(s)};
    }
    return okStatus();
}

PatternMetrics patternMetrics(const std::vector<double>& pattern) {
    PatternMetrics m;
    for (double v : pattern) {
        m.period += std::fabs(v);
        if (v > 0.0) {
            if (m.dash == 0.0) m.dash = v;
        } else if (v < 0.0) {
            m.gap += -v;
        } else {
            ++m.dots;
        }
    }
    return m;
}

Status validateLineTypeSpec(const LineTypeSpec& spec) {
    const LineTypeSpec ref = defaultLineTypeSpec(spec.type);
    if (spec.dxfName.empty()) {
        return Error{ErrorCode::InvalidInput, "line type needs a DXF name"};
    }
    for (char c : spec.dxfName) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) {
            return Error{ErrorCode::InvalidInput, "DXF line type name must be ASCII [A-Za-z0-9_-]"};
        }
    }
    if (!std::isfinite(spec.widthFactor) || spec.widthFactor < ref.minFactor - kEps ||
        spec.widthFactor > ref.maxFactor + kEps) {
        return Error{ErrorCode::InvalidInput,
                     "GOST 2.303: width factor of '" + ref.gostName + "' must be in [" +
                         std::to_string(ref.minFactor) + ", " + std::to_string(ref.maxFactor) +
                         "]·s"};
    }
    const bool dashed = ref.dashMax > 0.0;
    if (!dashed) {
        if (!spec.pattern.empty()) {
            return Error{ErrorCode::InvalidInput,
                         "GOST 2.303: '" + ref.gostName + "' is a continuous line"};
        }
        return okStatus();
    }
    // Шаблон: штрих, затем чередование промежутков и точек; точек столько же, сколько в
    // эталоне (одна у штрихпунктирной, две у линии с двумя точками, ноль у штриховой).
    if (spec.pattern.size() != ref.pattern.size()) {
        return Error{ErrorCode::InvalidInput,
                     "pattern of '" + ref.gostName + "' must have " +
                         std::to_string(ref.pattern.size()) + " elements"};
    }
    for (std::size_t i = 0; i < spec.pattern.size(); ++i) {
        const double v = spec.pattern[i];
        const double r = ref.pattern[i];
        const bool sameKind = (r > 0 && v > 0) || (r < 0 && v < 0) || (r == 0 && v == 0);
        if (!std::isfinite(v) || !sameKind) {
            return Error{ErrorCode::InvalidInput,
                         "pattern element " + std::to_string(i) + " of '" + ref.gostName +
                             "' has wrong kind (dash/gap/dot)"};
        }
    }
    const PatternMetrics m = patternMetrics(spec.pattern);
    if (m.dash < ref.dashMin - kEps || m.dash > ref.dashMax + kEps) {
        return Error{ErrorCode::InvalidInput,
                     "GOST 2.303: dash length of '" + ref.gostName + "' must be " +
                         std::to_string(ref.dashMin) + ".." + std::to_string(ref.dashMax) +
                         " mm"};
    }
    if (m.gap < ref.gapMin - kEps || m.gap > ref.gapMax + kEps) {
        return Error{ErrorCode::InvalidInput,
                     "GOST 2.303: gap of '" + ref.gostName + "' must be " +
                         std::to_string(ref.gapMin) + ".." + std::to_string(ref.gapMax) + " mm"};
    }
    return okStatus();
}

std::vector<double> renderDashArray(const std::vector<double>& pattern, double lineWidth) {
    std::vector<double> out;
    if (pattern.empty()) return out;
    // Сначала переводим в последовательность «штрих / промежуток», точка = штрих lineWidth,
    // соседние с точкой промежутки уменьшаются на lineWidth/2 каждый.
    std::vector<double> elems(pattern.begin(), pattern.end());
    const std::size_t n = elems.size();
    std::vector<double> lengths(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double v = elems[i];
        lengths[i] = v == 0.0 ? lineWidth : std::fabs(v);
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (elems[i] != 0.0) continue;
        const std::size_t prev = (i + n - 1) % n;
        const std::size_t next = (i + 1) % n;
        if (elems[prev] < 0.0) lengths[prev] = std::max(0.0, lengths[prev] - lineWidth / 2.0);
        if (elems[next] < 0.0) lengths[next] = std::max(0.0, lengths[next] - lineWidth / 2.0);
    }
    // Слияние соседних элементов одного вида (штрих+штрих, промежуток+промежуток).
    for (std::size_t i = 0; i < n; ++i) {
        const bool on = elems[i] >= 0.0;
        const bool lastOn = !out.empty() && (out.size() % 2 == 1);
        if (!out.empty() && on == lastOn) {
            out.back() += lengths[i];
        } else if (out.empty() && !on) {
            out.push_back(0.0);
            out.push_back(lengths[i]);
        } else {
            out.push_back(lengths[i]);
        }
    }
    if (out.size() % 2 == 1) out.push_back(0.0);
    return out;
}

}  // namespace bendcore::drawing_v2
