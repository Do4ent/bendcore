// Допуски и посадки ISO 286-1/-2 (ГОСТ 25346, ГОСТ 25347). Значения таблиц — мкм.
// Происхождение значений и что требует сверки — docs/drawing/ANNOTATIONS.md.

#include <bendcore/drawing_v2/tolerances.hpp>

#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>

namespace bendcore::drawing_v2::annotations {

namespace {

// Интервалы номинальных размеров ISO 286-1, табл. 1 (основные): свыше предыдущей границы
// до указанной включительно.
constexpr std::array<double, 13> kMainSteps = {3, 6, 10, 18, 30, 50, 80, 120, 180, 250, 315, 400, 500};

// Промежуточные интервалы ISO 286-1, табл. 2 и 3 (основные отклонения).
constexpr std::array<double, 25> kFineSteps = {3,   6,   10,  14,  18,  24,  30,  40,  50,
                                               65,  80,  100, 120, 140, 160, 180, 200, 225,
                                               250, 280, 315, 355, 400, 450, 500};

// Стандартные допуски IT01, IT0, IT1…IT18 (ISO 286-1, табл. 1), мкм.
constexpr std::array<std::array<double, 13>, 20> kIt = {{
    {0.3, 0.4, 0.4, 0.5, 0.6, 0.6, 0.8, 1, 1.2, 2, 2.5, 3, 4},               // IT01
    {0.5, 0.6, 0.6, 0.8, 1, 1, 1.2, 1.5, 2, 3, 4, 5, 6},                     // IT0
    {0.8, 1, 1, 1.2, 1.5, 1.5, 2, 2.5, 3.5, 4.5, 6, 7, 8},                   // IT1
    {1.2, 1.5, 1.5, 2, 2.5, 2.5, 3, 4, 5, 7, 8, 9, 10},                      // IT2
    {2, 2.5, 2.5, 3, 4, 4, 5, 6, 8, 10, 12, 13, 15},                         // IT3
    {3, 4, 4, 5, 6, 7, 8, 10, 12, 14, 16, 18, 20},                           // IT4
    {4, 5, 6, 8, 9, 11, 13, 15, 18, 20, 23, 25, 27},                         // IT5
    {6, 8, 9, 11, 13, 16, 19, 22, 25, 29, 32, 36, 40},                       // IT6
    {10, 12, 15, 18, 21, 25, 30, 35, 40, 46, 52, 57, 63},                    // IT7
    {14, 18, 22, 27, 33, 39, 46, 54, 63, 72, 81, 89, 97},                    // IT8
    {25, 30, 36, 43, 52, 62, 74, 87, 100, 115, 130, 140, 155},               // IT9
    {40, 48, 58, 70, 84, 100, 120, 140, 160, 185, 210, 230, 250},            // IT10
    {60, 75, 90, 110, 130, 160, 190, 220, 250, 290, 320, 360, 400},          // IT11
    {100, 120, 150, 180, 210, 250, 300, 350, 400, 460, 520, 570, 630},       // IT12
    {140, 180, 220, 270, 330, 390, 460, 540, 630, 720, 810, 890, 970},       // IT13
    {250, 300, 360, 430, 520, 620, 740, 870, 1000, 1150, 1300, 1400, 1550},  // IT14
    {400, 480, 580, 700, 840, 1000, 1200, 1400, 1600, 1850, 2100, 2300, 2500},   // IT15
    {600, 750, 900, 1100, 1300, 1600, 1900, 2200, 2500, 2900, 3200, 3600, 4000},  // IT16
    {1000, 1200, 1500, 1800, 2100, 2500, 3000, 3500, 4000, 4600, 5200, 5700, 6300},  // IT17
    {1400, 1800, 2200, 2700, 3300, 3900, 4600, 5400, 6300, 7200, 8100, 8900, 9700},  // IT18
}};

using Fine = std::array<double, 25>;

// Верхние отклонения es валов a…h (ISO 286-1, табл. 2), мкм.
const std::map<std::string, Fine>& upperShaft() {
    static const std::map<std::string, Fine> m = {
        {"a", {-270, -270, -280, -290, -290, -300, -300, -310, -320, -340, -360, -380, -410,
               -460, -520, -580, -660, -740, -820, -920, -1050, -1200, -1350, -1500, -1650}},
        {"b", {-140, -140, -150, -150, -150, -160, -160, -170, -180, -190, -200, -220, -240,
               -260, -280, -310, -340, -380, -420, -480, -540, -600, -680, -760, -840}},
        {"c", {-60, -70, -80, -95, -95, -110, -110, -120, -130, -140, -150, -170, -180,
               -200, -210, -230, -240, -260, -280, -300, -330, -360, -400, -440, -480}},
        {"d", {-20, -30, -40, -50, -50, -65, -65, -80, -80, -100, -100, -120, -120,
               -145, -145, -145, -170, -170, -170, -190, -190, -210, -210, -230, -230}},
        {"e", {-14, -20, -25, -32, -32, -40, -40, -50, -50, -60, -60, -72, -72,
               -85, -85, -85, -100, -100, -100, -110, -110, -125, -125, -135, -135}},
        {"f", {-6, -10, -13, -16, -16, -20, -20, -25, -25, -30, -30, -36, -36,
               -43, -43, -43, -50, -50, -50, -56, -56, -62, -62, -68, -68}},
        {"g", {-2, -4, -5, -6, -6, -7, -7, -9, -9, -10, -10, -12, -12,
               -14, -14, -14, -15, -15, -15, -17, -17, -18, -18, -20, -20}},
        {"h", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    };
    return m;
}

// Нижние отклонения ei валов m…s (ISO 286-1, табл. 3), мкм. Для k — значения для IT4…IT7
// (для квалитетов ≤ 3 и > 7 ei = 0).
const std::map<std::string, Fine>& lowerShaft() {
    static const std::map<std::string, Fine> m = {
        {"k", {0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 5, 5}},
        {"m", {2, 4, 6, 7, 7, 8, 8, 9, 9, 11, 11, 13, 13, 15, 15, 15, 17, 17, 17, 20, 20, 21, 21, 23, 23}},
        {"n", {4, 8, 10, 12, 12, 15, 15, 17, 17, 20, 20, 23, 23, 27, 27, 27, 31, 31, 31, 34, 34, 37, 37, 40, 40}},
        {"p", {6, 12, 15, 18, 18, 22, 22, 26, 26, 32, 32, 37, 37, 43, 43, 43, 50, 50, 50, 56, 56, 62, 62, 68, 68}},
        {"r", {10, 15, 19, 23, 23, 28, 28, 34, 34, 41, 43, 51, 54, 63, 65, 68, 77, 80, 84, 94, 98, 108, 114, 126, 132}},
        {"s", {14, 19, 23, 28, 28, 35, 35, 43, 43, 53, 59, 71, 79, 92, 100, 108, 122, 130, 140, 158, 170, 190, 208, 232, 252}},
    };
    return m;
}

Error invalid(std::string m) { return Error{ErrorCode::InvalidInput, std::move(m)}; }

template <std::size_t N>
std::optional<std::size_t> stepIndex(const std::array<double, N>& steps, double d) {
    if (!(d > 0.0) || d > steps.back() + 1e-9) return std::nullopt;
    for (std::size_t i = 0; i < N; ++i) {
        if (d <= steps[i] + 1e-9) return i;
    }
    return std::nullopt;
}

std::string lower(std::string s) {
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trimNumber(double v, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, v);
    std::string s = buf;
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    for (char& c : s) {
        if (c == '.') c = ',';
    }
    return s;
}

}  // namespace

Result<double> standardToleranceUm(double nominalMm, int grade) {
    if (grade < -1 || grade > 18) return invalid("IT grade must be 01, 0 or 1..18");
    const auto i = stepIndex(kMainSteps, nominalMm);
    if (!i) return invalid("ISO 286: nominal size must be > 0 and <= 500 mm");
    if (grade >= 14 && nominalMm <= 1.0) return invalid("ISO 286-1: IT14..IT18 are not used for sizes up to 1 mm");
    return kIt[std::size_t(grade + 1)][*i];
}

bool ToleranceClass::hole() const {
    return !letter.empty() && std::isupper(static_cast<unsigned char>(letter[0]));
}

std::string ToleranceClass::label() const { return letter + std::to_string(grade); }

Result<ToleranceClass> parseToleranceClass(const std::string& text) {
    std::size_t i = 0;
    while (i < text.size() && std::isalpha(static_cast<unsigned char>(text[i]))) ++i;
    const std::string letter = text.substr(0, i);
    const std::string digits = text.substr(i);
    if (letter.empty() || digits.empty() || digits.size() > 2) return invalid("tolerance class '" + text + "': expected letter(s) and grade, e.g. H7");
    for (char c : digits) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return invalid("tolerance class '" + text + "': bad grade");
    }
    const int grade = std::stoi(digits);
    if (grade < 1 || grade > 18) return invalid("tolerance class '" + text + "': grade must be 1..18");
    const std::string l = lower(letter);
    const bool upperCase = std::isupper(static_cast<unsigned char>(letter[0]));
    // Регистр всех букв одинаков (js / JS).
    for (char c : letter) {
        if (bool(std::isupper(static_cast<unsigned char>(c))) != upperCase) {
            return invalid("tolerance class '" + text + "': mixed letter case");
        }
    }
    const bool known = upperShaft().count(l) || lowerShaft().count(l) || l == "js";
    if (!known) {
        return invalid("tolerance class '" + text +
                       "': fundamental deviation not supported (a..h, js, k, m, n, p, r, s and A..H, JS, K, M, N, P, R, S)");
    }
    return ToleranceClass{letter, grade};
}

Result<LimitDeviations> limitDeviations(double nominalMm, const std::string& cls) {
    auto c = parseToleranceClass(cls);
    if (!c) return c.error();
    return limitDeviations(nominalMm, c.value());
}

Result<LimitDeviations> limitDeviations(double nominalMm, const ToleranceClass& cls) {
    auto itr = standardToleranceUm(nominalMm, cls.grade);
    if (!itr) return itr.error();
    const double it = itr.value();
    const auto fi = stepIndex(kFineSteps, nominalMm);
    if (!fi) return invalid("ISO 286: nominal size must be > 0 and <= 500 mm");
    const std::string l = lower(cls.letter);
    if ((l == "a" || l == "b") && nominalMm <= 1.0) {
        return invalid("ISO 286-1: fundamental deviations a, b, A, B are not used for sizes up to 1 mm");
    }

    LimitDeviations d;
    d.itUm = it;
    if (l == "js") {
        d.upperUm = it / 2.0;
        d.lowerUm = -it / 2.0;
        d.fundamentalUm = 0.0;
        return d;
    }

    // Нижнее отклонение вала ei для букв k…s (k зависит от квалитета).
    auto shaftEi = [&](const std::string& letter, int grade) -> double {
        if (letter == "k" && (grade <= 3 || grade > 7)) return 0.0;
        return lowerShaft().at(letter)[*fi];
    };

    if (!cls.hole()) {
        if (auto it2 = upperShaft().find(l); it2 != upperShaft().end()) {
            d.upperUm = it2->second[*fi];
            d.lowerUm = d.upperUm - it;
            d.fundamentalUm = d.upperUm;
        } else {
            d.lowerUm = shaftEi(l, cls.grade);
            d.upperUm = d.lowerUm + it;
            d.fundamentalUm = d.lowerUm;
        }
        return d;
    }

    // Отверстия. Общее правило: основное отклонение отверстия симметрично основному
    // отклонению вала той же буквы относительно нулевой линии: EI = −es (A…H),
    // ES = −ei (K…S). Специальное правило (K, M, N до IT8; P…S до IT7): ES = −ei + Δ,
    // Δ = ITn − IT(n−1) — разность допусков данного и ближайшего более точного квалитета.
    if (auto it2 = upperShaft().find(l); it2 != upperShaft().end()) {
        d.lowerUm = -it2->second[*fi];
        d.upperUm = d.lowerUm + it;
        d.fundamentalUm = d.lowerUm;
        return d;
    }
    auto delta = [&]() -> double {
        if (nominalMm <= 3.0 + 1e-9) return 0.0;  // ISO 286-1: для размеров до 3 мм Δ = 0
        const double prev = standardToleranceUm(nominalMm, cls.grade - 1).value();
        return it - prev;
    };
    double es = 0.0;
    if (l == "k") {
        if (cls.grade <= 8) {
            es = -shaftEi("k", cls.grade) + delta();
        } else {
            es = 0.0;
        }
    } else if (l == "m") {
        if (cls.grade <= 8) {
            es = -shaftEi("m", cls.grade) + delta();
            // ISO 286-1, табл. 3, примечание: для M6 свыше 250 до 315 мм ES = −9 мкм.
            if (cls.grade == 6 && nominalMm > 250.0 && nominalMm <= 315.0 + 1e-9) es = -9.0;
        } else {
            es = -shaftEi("m", cls.grade);
        }
    } else if (l == "n") {
        if (nominalMm <= 3.0 + 1e-9) {
            if (cls.grade > 8 && nominalMm <= 1.0) {
                return invalid("ISO 286-1: N above IT8 is not used for sizes up to 1 mm");
            }
            es = -4.0;
        } else if (cls.grade <= 8) {
            es = -shaftEi("n", cls.grade) + delta();
        } else {
            es = 0.0;
        }
    } else {  // p, r, s
        es = -shaftEi(l, cls.grade) + (cls.grade <= 7 ? delta() : 0.0);
    }
    d.upperUm = es;
    d.lowerUm = es - it;
    d.fundamentalUm = es;
    return d;
}

std::string formatDeviationMm(double um) {
    if (std::abs(um) < 1e-9) return "0";
    const std::string mag = trimNumber(std::abs(um) / 1000.0, 4);
    return (um > 0 ? "+" : "−") + mag;
}

ToleranceText toleranceText(const std::string& nominal, const ToleranceClass& cls, const LimitDeviations& dev,
                            ToleranceDisplay display, bool omitZero) {
    ToleranceText t;
    const bool symmetric = std::abs(dev.upperUm + dev.lowerUm) < 1e-9 && std::abs(dev.upperUm) > 1e-9;
    if (display == ToleranceDisplay::ClassOnly) {
        t.main = nominal + cls.label();
        return t;
    }
    const std::string sym = "±" + trimNumber(std::abs(dev.upperUm) / 1000.0, 4);
    if (display == ToleranceDisplay::Deviations) {
        if (symmetric) {
            t.main = nominal + sym;
            return t;
        }
        t.main = nominal;
    } else {
        t.main = nominal + cls.label();
        if (symmetric) {
            t.main += "(" + sym + ")";
            return t;
        }
        t.suffixOpen = "(";
        t.suffixClose = ")";
    }
    const bool zeroUpper = std::abs(dev.upperUm) < 1e-9, zeroLower = std::abs(dev.lowerUm) < 1e-9;
    if (!(omitZero && zeroUpper)) t.upper = formatDeviationMm(dev.upperUm);
    if (!(omitZero && zeroLower)) t.lower = formatDeviationMm(dev.lowerUm);
    return t;
}

}  // namespace bendcore::drawing_v2::annotations
