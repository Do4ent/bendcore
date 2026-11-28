// Допуски и посадки ISO 286-1/-2 (ГОСТ 25346, ГОСТ 25347). Значения таблиц — мкм.
// Происхождение значений и что требует сверки — docs/drawing/ANNOTATIONS.md,
// docs/STANDARDS_VERIFICATION.md.

#include <bendcore/drawing_v2/tolerances.hpp>

#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>

namespace bendcore::drawing_v2::annotations {

namespace {

// Интервалы номинальных размеров ISO 286-1, табл. 1 (основные): свыше предыдущей границы
// до указанной включительно. 13 интервалов до 500 мм и 8 интервалов свыше 500 до 3150 мм.
constexpr std::array<double, 21> kMainSteps = {3,   6,   10,  18,  30,   50,   80,   120,  180, 250, 315,
                                               400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150};
constexpr std::size_t kMainSmall = 13;  // интервалов до 500 мм

// Промежуточные интервалы ISO 286-1, табл. 2 и 3 (основные отклонения): 25 до 500 мм и 16 свыше
// 500 до 3150 мм.
constexpr std::array<double, 41> kFineSteps = {
    3,    6,    10,   14,   18,   24,   30,   40,   50,   65,   80,   100,  120,
    140,  160,  180,  200,  225,  250,  280,  315,  355,  400,  450,  500,  560,
    630,  710,  800,  900,  1000, 1120, 1250, 1400, 1600, 1800, 2000, 2240, 2500,
    2800, 3150};
constexpr std::size_t kFineSmall = 25;  // интервалов до 500 мм

// Стандартные допуски IT01, IT0, IT1…IT18 (ISO 286-1, табл. 1), мкм, до 500 мм.
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

// IT1…IT18 свыше 500 до 3150 мм (ISO 286-1, табл. 1), мкм; интервалы 500–630, 630–800, 800–1000,
// 1000–1250, 1250–1600, 1600–2000, 2000–2500, 2500–3150. IT01 и IT0 для этих размеров не
// определены.
constexpr std::array<std::array<double, 8>, 18> kItLarge = {{
    {9, 10, 11, 13, 15, 18, 22, 26},                          // IT1
    {11, 13, 15, 18, 21, 25, 30, 36},                         // IT2
    {16, 18, 21, 24, 29, 35, 41, 50},                         // IT3
    {22, 25, 28, 33, 39, 46, 55, 68},                         // IT4
    {32, 36, 40, 47, 55, 65, 78, 96},                         // IT5
    {44, 50, 56, 66, 78, 92, 110, 135},                       // IT6
    {70, 80, 90, 105, 125, 150, 175, 210},                    // IT7
    {110, 125, 140, 165, 195, 230, 280, 330},                 // IT8
    {175, 200, 230, 260, 310, 370, 440, 540},                 // IT9
    {280, 320, 360, 420, 500, 600, 700, 860},                 // IT10
    {440, 500, 560, 660, 780, 920, 1100, 1350},               // IT11
    {700, 800, 900, 1050, 1250, 1500, 1750, 2100},            // IT12
    {1100, 1250, 1400, 1650, 1950, 2300, 2800, 3300},         // IT13
    {1750, 2000, 2300, 2600, 3100, 3700, 4400, 5400},         // IT14
    {2800, 3200, 3600, 4200, 5000, 6000, 7000, 8600},         // IT15
    {4400, 5000, 5600, 6600, 7800, 9200, 11000, 13500},       // IT16
    {7000, 8000, 9000, 10500, 12500, 15000, 17500, 21000},    // IT17
    {11000, 12500, 14000, 16500, 19500, 23000, 28000, 33000}, // IT18
}};

constexpr double kNA = std::numeric_limits<double>::quiet_NaN();  // для данной буквы не определено

using Fine = std::array<double, 41>;

// Таблица основных отклонений буквы: 25 значений до 500 мм и 16 — свыше 500 мм (kNA — не
// определено).
Fine makeFine(const std::array<double, 25>& small, const std::array<double, 16>& large) {
    Fine f{};
    for (std::size_t i = 0; i < 25; ++i) f[i] = small[i];
    for (std::size_t i = 0; i < 16; ++i) f[25 + i] = large[i];
    return f;
}

constexpr std::array<double, 16> kLargeNA = {kNA, kNA, kNA, kNA, kNA, kNA, kNA, kNA,
                                             kNA, kNA, kNA, kNA, kNA, kNA, kNA, kNA};

std::size_t mainOfFine(std::size_t fi) {
    for (std::size_t m = 0; m < kMainSteps.size(); ++m) {
        if (kFineSteps[fi] <= kMainSteps[m] + 1e-9) return m;
    }
    return kMainSteps.size() - 1;
}

// Допуск IT по индексу основного интервала и квалитету (без проверок диапазона квалитета).
double itAt(std::size_t mainIdx, int grade) {
    if (mainIdx < kMainSmall) return kIt[std::size_t(grade + 1)][mainIdx];
    return kItLarge[std::size_t(grade - 1)][mainIdx - kMainSmall];
}

// Верхние отклонения es валов a…h (ISO 286-1, табл. 2), мкм. Свыше 500 мм — только d…h.
const std::map<std::string, Fine>& upperShaft() {
    static const std::map<std::string, Fine> m = {
        {"a", makeFine({-270, -270, -280, -290, -290, -300, -300, -310, -320, -340, -360, -380, -410,
                        -460, -520, -580, -660, -740, -820, -920, -1050, -1200, -1350, -1500, -1650},
                       kLargeNA)},
        {"b", makeFine({-140, -140, -150, -150, -150, -160, -160, -170, -180, -190, -200, -220, -240,
                        -260, -280, -310, -340, -380, -420, -480, -540, -600, -680, -760, -840},
                       kLargeNA)},
        {"c", makeFine({-60, -70, -80, -95, -95, -110, -110, -120, -130, -140, -150, -170, -180,
                        -200, -210, -230, -240, -260, -280, -300, -330, -360, -400, -440, -480},
                       kLargeNA)},
        {"d", makeFine({-20, -30, -40, -50, -50, -65, -65, -80, -80, -100, -100, -120, -120,
                        -145, -145, -145, -170, -170, -170, -190, -190, -210, -210, -230, -230},
                       {-260, -260, -290, -290, -320, -320, -350, -350, -390, -390, -430, -430, -480, -480, -520, -520})},
        {"e", makeFine({-14, -20, -25, -32, -32, -40, -40, -50, -50, -60, -60, -72, -72,
                        -85, -85, -85, -100, -100, -100, -110, -110, -125, -125, -135, -135},
                       {-145, -145, -160, -160, -170, -170, -195, -195, -220, -220, -240, -240, -260, -260, -290, -290})},
        {"f", makeFine({-6, -10, -13, -16, -16, -20, -20, -25, -25, -30, -30, -36, -36,
                        -43, -43, -43, -50, -50, -50, -56, -56, -62, -62, -68, -68},
                       {-76, -76, -80, -80, -86, -86, -98, -98, -110, -110, -120, -120, -130, -130, -145, -145})},
        {"g", makeFine({-2, -4, -5, -6, -6, -7, -7, -9, -9, -10, -10, -12, -12,
                        -14, -14, -14, -15, -15, -15, -17, -17, -18, -18, -20, -20},
                       {-22, -22, -24, -24, -26, -26, -28, -28, -30, -30, -32, -32, -34, -34, -38, -38})},
        {"h", makeFine({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0})},
    };
    return m;
}

// Нижние отклонения ei валов k…zc (ISO 286-1, табл. 3), мкм. Для k — значения для IT4…IT7
// (для квалитетов ≤ 3 и > 7 ei = 0). Свыше 500 мм определены k, m, n, p, r, s, t, u; значения
// r, s, t, u свыше 500 мм считаются по формулам таблицы 4 (см. ниже).
const std::map<std::string, Fine>& lowerShaft() {
    static const std::map<std::string, Fine> m = [] {
        std::map<std::string, Fine> t = {
            {"k", makeFine({0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 5, 5},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0})},
            {"m", makeFine({2, 4, 6, 7, 7, 8, 8, 9, 9, 11, 11, 13, 13, 15, 15, 15, 17, 17, 17, 20, 20, 21, 21, 23, 23},
                           {26, 26, 30, 30, 34, 34, 40, 40, 48, 48, 58, 58, 68, 68, 76, 76})},
            {"n", makeFine({4, 8, 10, 12, 12, 15, 15, 17, 17, 20, 20, 23, 23, 27, 27, 27, 31, 31, 31, 34, 34, 37, 37, 40, 40},
                           {44, 44, 50, 50, 56, 56, 66, 66, 78, 78, 92, 92, 110, 110, 135, 135})},
            {"p", makeFine({6, 12, 15, 18, 18, 22, 22, 26, 26, 32, 32, 37, 37, 43, 43, 43, 50, 50, 50, 56, 56, 62, 62, 68, 68},
                           {78, 78, 88, 88, 100, 100, 120, 120, 140, 140, 170, 170, 195, 195, 240, 240})},
            {"r", makeFine({10, 15, 19, 23, 23, 28, 28, 34, 34, 41, 43, 51, 54, 63, 65, 68, 77, 80, 84, 94, 98, 108, 114, 126, 132},
                           kLargeNA)},
            {"s", makeFine({14, 19, 23, 28, 28, 35, 35, 43, 43, 53, 59, 71, 79, 92, 100, 108, 122, 130, 140, 158, 170, 190, 208, 232, 252},
                           kLargeNA)},
            {"t", makeFine({kNA, kNA, kNA, kNA, kNA, kNA, 41, 48, 54, 66, 75, 91, 104, 122, 134, 146, 166, 180, 196, 218, 240, 268, 294, 330, 360},
                           kLargeNA)},
            {"u", makeFine({18, 23, 28, 33, 33, 41, 48, 60, 70, 87, 102, 124, 144, 170, 190, 210, 236, 258, 284, 315, 350, 390, 435, 490, 540},
                           kLargeNA)},
            {"v", makeFine({kNA, kNA, kNA, kNA, 39, 47, 55, 68, 81, 102, 120, 146, 172, 202, 228, 252, 284, 310, 340, 385, 425, 475, 530, 595, 660},
                           kLargeNA)},
            {"x", makeFine({20, 28, 34, 40, 45, 54, 64, 80, 97, 122, 146, 178, 210, 248, 280, 310, 350, 385, 425, 475, 525, 590, 660, 740, 820},
                           kLargeNA)},
            {"y", makeFine({kNA, kNA, kNA, kNA, kNA, 63, 75, 94, 114, 144, 174, 214, 254, 300, 340, 380, 425, 470, 520, 580, 650, 730, 820, 920, 1000},
                           kLargeNA)},
            {"z", makeFine({26, 35, 42, 50, 60, 73, 88, 112, 136, 172, 210, 258, 310, 365, 415, 465, 520, 575, 640, 710, 790, 900, 1000, 1100, 1250},
                           kLargeNA)},
            {"za", makeFine({32, 42, 52, 64, 77, 98, 118, 148, 180, 226, 274, 335, 400, 470, 535, 600, 670, 740, 820, 920, 1000, 1150, 1300, 1450, 1600},
                            kLargeNA)},
            {"zb", makeFine({40, 50, 67, 90, 108, 136, 160, 200, 242, 300, 360, 445, 525, 620, 700, 780, 880, 960, 1050, 1200, 1350, 1500, 1650, 1850, 2100},
                            kLargeNA)},
            {"zc", makeFine({60, 80, 97, 130, 150, 188, 218, 274, 325, 405, 480, 585, 690, 800, 900, 1000, 1150, 1250, 1350, 1550, 1700, 1900, 2100, 2400, 2600},
                            kLargeNA)},
        };
        // r, s, t, u свыше 500 мм по формулам ISO 286-1, табл. 4: s = IT7 + 0,4·D, t = IT7 + 0,63·D,
        // u = IT7 + D, r — среднее геометрическое p и s; D — среднее геометрическое границ
        // промежуточного интервала. Округление (до 10 мкм, от 1000 мкм — до 50 мкм; r до 250 мкм —
        // до 5 мкм) выбрано здесь и может отличаться от таблиц стандарта на шаг округления.
        auto roundTo = [](double v, double step) { return std::round(v / step) * step; };
        auto big = [&](double v) { return v < 1000.0 ? roundTo(v, 10.0) : roundTo(v, 50.0); };
        for (std::size_t fi = kFineSmall; fi < kFineSteps.size(); ++fi) {
            const double D = std::sqrt(kFineSteps[fi - 1] * kFineSteps[fi]);
            const double it7 = itAt(mainOfFine(fi), 7);
            const double s = big(it7 + 0.4 * D);
            t.at("s")[fi] = s;
            t.at("t")[fi] = big(it7 + 0.63 * D);
            t.at("u")[fi] = big(it7 + D);
            const double r = std::sqrt(t.at("p")[fi] * s);
            t.at("r")[fi] = r < 250.0 ? roundTo(r, 5.0) : big(r);
        }
        return t;
    }();
    return m;
}

// Основные отклонения j (нижнее ei вала) и J (верхнее ES отверстия) по основным интервалам
// до 500 мм (ISO 286-1, табл. 3), мкм. Свыше 500 мм j и J стандартом не определены.
constexpr std::array<double, 13> kJ56 = {-2, -2, -2, -3, -4, -5, -7, -9, -11, -13, -16, -18, -20};
constexpr std::array<double, 13> kJ7 = {-4, -4, -5, -6, -8, -10, -12, -15, -18, -21, -26, -28, -32};
constexpr std::array<double, 13> kHoleJ6 = {2, 5, 5, 6, 8, 10, 13, 16, 18, 22, 25, 29, 33};
constexpr std::array<double, 13> kHoleJ7 = {4, 6, 8, 10, 12, 14, 18, 22, 26, 30, 36, 39, 43};
constexpr std::array<double, 13> kHoleJ8 = {6, 10, 12, 15, 20, 24, 28, 34, 41, 47, 55, 60, 66};

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

double lowerLetterValue(const std::string& letter, std::size_t fi) { return lowerShaft().at(letter)[fi]; }

}  // namespace

Result<double> standardToleranceUm(double nominalMm, int grade) {
    if (grade < -1 || grade > 18) return invalid("IT grade must be 01, 0 or 1..18");
    const auto i = stepIndex(kMainSteps, nominalMm);
    if (!i) return invalid("ISO 286: nominal size must be > 0 and <= 3150 mm");
    if (*i >= kMainSmall && grade < 1) return invalid("ISO 286-1: IT01 and IT0 are defined only for sizes up to 500 mm");
    if (grade >= 14 && nominalMm <= 1.0) return invalid("ISO 286-1: IT14..IT18 are not used for sizes up to 1 mm");
    return itAt(*i, grade);
}

Result<double> standardToleranceFormulaUm(double nominalMm, int grade) {
    if (grade < -1 || grade > 18) return invalid("IT grade must be 01, 0 or 1..18");
    const auto mi = stepIndex(kMainSteps, nominalMm);
    if (!mi) return invalid("ISO 286: nominal size must be > 0 and <= 3150 mm");
    const double lo = *mi == 0 ? 1.0 : kMainSteps[*mi - 1];
    const double D = std::sqrt(lo * kMainSteps[*mi]);
    if (*mi < kMainSmall) {
        const double i = 0.45 * std::cbrt(D) + 0.001 * D;
        static constexpr double k[] = {7, 10, 16, 25, 40, 64, 100, 160, 250, 400, 640, 1000, 1600, 2500};  // IT5…IT18
        if (grade >= 5) return k[grade - 5] * i;
        if (grade == -1) return 0.3 + 0.008 * D;
        if (grade == 0) return 0.5 + 0.012 * D;
        const double it1 = 0.8 + 0.020 * D;
        const double it5 = 7.0 * i;
        return it1 * std::pow(it5 / it1, double(grade - 1) / 4.0);  // IT2…IT4 — геометрический ряд IT1…IT5
    }
    if (grade < 1) return invalid("ISO 286-1: IT01 and IT0 are defined only for sizes up to 500 mm");
    const double I = 0.004 * D + 2.1;
    static constexpr double k[] = {2, 2.7, 3.7, 5, 7, 10, 16, 25, 40, 64, 100, 160, 250, 400, 640, 1000, 1600, 2500};
    return k[grade - 1] * I;
}

Result<double> shaftFundamentalFormulaUm(double nominalMm, const std::string& letter) {
    const auto fi = stepIndex(kFineSteps, nominalMm);
    if (!fi) return invalid("ISO 286: nominal size must be > 0 and <= 3150 mm");
    const double lo = *fi == 0 ? 1.0 : kFineSteps[*fi - 1];
    const double D = std::sqrt(lo * kFineSteps[*fi]);
    const std::size_t mi = mainOfFine(*fi);
    const bool large = *fi >= kFineSmall;
    auto it = [&](int g) { return itAt(mi, g); };
    const std::string l = lower(letter);
    if (l == "a") return D <= 120.0 ? -(265.0 + 1.3 * D) : -3.5 * D;
    if (l == "b") return D <= 160.0 ? -(140.0 + 0.85 * D) : -1.8 * D;
    if (l == "c") return D <= 40.0 ? -52.0 * std::pow(D, 0.2) : -(95.0 + 0.8 * D);
    if (l == "d") return -16.0 * std::pow(D, 0.44);
    if (l == "e") return -11.0 * std::pow(D, 0.41);
    if (l == "f") return -5.5 * std::pow(D, 0.41);
    if (l == "g") return -2.5 * std::pow(D, 0.34);
    if (l == "k") return large ? 0.0 : 0.6 * std::cbrt(D);
    if (l == "m") return large ? 0.024 * D + 12.6 : it(7) - it(6);
    if (l == "n") return large ? 0.04 * D + 21.0 : 5.0 * std::pow(D, 0.34);
    if (l == "p") {
        if (!large) return invalid("ISO 286-1: no formula for p up to 500 mm (IT7 + 0…5 µm, table values)");
        return 0.072 * D + 37.8;
    }
    if (l == "s") {
        if (D <= 50.0) return invalid("ISO 286-1: no formula for s up to 50 mm (IT8 + 1…4 µm, table values)");
        return it(7) + 0.4 * D;
    }
    if (l == "r") {
        const auto p = shaftFundamentalFormulaUm(nominalMm, "p");
        const auto s = shaftFundamentalFormulaUm(nominalMm, "s");
        if (!large || !p || !s) return invalid("ISO 286-1: formula for r (geometric mean of p and s) only above 500 mm");
        return std::sqrt(p.value() * s.value());
    }
    if (l == "t") return it(7) + 0.63 * D;
    if (l == "u") return it(7) + D;
    if (l == "v") return it(7) + 1.25 * D;
    if (l == "x") return it(7) + 1.6 * D;
    if (l == "y") return it(7) + 2.0 * D;
    if (l == "z") return it(7) + 2.5 * D;
    if (l == "za") return it(8) + 3.15 * D;
    if (l == "zb") return it(9) + 4.0 * D;
    if (l == "zc") return it(10) + 5.0 * D;
    return invalid("ISO 286-1: no formula for fundamental deviation '" + letter + "'");
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
    const bool known = upperShaft().count(l) || lowerShaft().count(l) || l == "js" || l == "j";
    if (!known) {
        return invalid("tolerance class '" + text +
                       "': fundamental deviation not supported (a..h, js, j, k, m, n, p, r, s, t, u, v, x, y, z, za, zb, zc and A..H, JS, J, K, M, N, P, R, S, T, U, V, X, Y, Z, ZA, ZB, ZC)");
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
    const auto mi = stepIndex(kMainSteps, nominalMm);
    if (!fi || !mi) return invalid("ISO 286: nominal size must be > 0 and <= 3150 mm");
    const bool large = nominalMm > 500.0 + 1e-9;
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
    if (l == "j") {
        if (large) return invalid("ISO 286-1: fundamental deviations j, J are not defined for sizes over 500 mm");
        if (!cls.hole()) {
            if (cls.grade < 5 || cls.grade > 7) return invalid("ISO 286-1: j is tabulated for IT5, IT6 and IT7 only");
            d.lowerUm = (cls.grade == 7 ? kJ7 : kJ56)[*mi];
            d.upperUm = d.lowerUm + it;
            d.fundamentalUm = d.lowerUm;
        } else {
            if (cls.grade < 6 || cls.grade > 8) return invalid("ISO 286-1: J is tabulated for IT6, IT7 and IT8 only");
            d.upperUm = (cls.grade == 6 ? kHoleJ6 : cls.grade == 7 ? kHoleJ7 : kHoleJ8)[*mi];
            d.lowerUm = d.upperUm - it;
            d.fundamentalUm = d.upperUm;
        }
        return d;
    }

    const std::string undefinedMsg = "ISO 286-1: fundamental deviation '" + cls.letter + "' is not defined for this nominal size";
    if (large && !(upperShaft().count(l) && upperShaft().at(l)[*fi] == upperShaft().at(l)[*fi]) &&
        !(lowerShaft().count(l) && lowerShaft().at(l)[*fi] == lowerShaft().at(l)[*fi])) {
        return invalid(undefinedMsg + " (letters a, b, c, cd, ef, fg, j, v, x, y, z, za, zb, zc stop at 500 mm)");
    }

    // Нижнее отклонение вала ei для букв k…zc (k зависит от квалитета).
    auto shaftEi = [&](const std::string& letter, int grade) -> double {
        if (letter == "k" && (grade <= 3 || grade > 7)) return 0.0;
        return lowerLetterValue(letter, *fi);
    };

    if (!cls.hole()) {
        if (auto it2 = upperShaft().find(l); it2 != upperShaft().end()) {
            d.upperUm = it2->second[*fi];
            if (std::isnan(d.upperUm)) return invalid(undefinedMsg);
            d.lowerUm = d.upperUm - it;
            d.fundamentalUm = d.upperUm;
        } else {
            d.lowerUm = shaftEi(l, cls.grade);
            if (std::isnan(d.lowerUm)) return invalid(undefinedMsg);
            d.upperUm = d.lowerUm + it;
            d.fundamentalUm = d.lowerUm;
        }
        return d;
    }

    // Отверстия. Общее правило: основное отклонение отверстия симметрично основному
    // отклонению вала той же буквы относительно нулевой линии: EI = −es (A…H),
    // ES = −ei (K…ZC). Специальное правило (K, M, N до IT8; P…ZC до IT7): ES = −ei + Δ,
    // Δ = ITn − IT(n−1) — разность допусков данного и ближайшего более точного квалитета.
    if (auto it2 = upperShaft().find(l); it2 != upperShaft().end()) {
        d.lowerUm = -it2->second[*fi];
        if (std::isnan(d.lowerUm)) return invalid(undefinedMsg);
        d.upperUm = d.lowerUm + it;
        d.fundamentalUm = d.lowerUm;
        return d;
    }
    if (std::isnan(shaftEi(l, cls.grade))) return invalid(undefinedMsg);
    auto delta = [&]() -> Result<double> {
        if (nominalMm <= 3.0 + 1e-9) return 0.0;  // ISO 286-1: для размеров до 3 мм Δ = 0
        auto prev = standardToleranceUm(nominalMm, cls.grade - 1);
        if (!prev) return invalid("ISO 286-1: Δ is not defined for " + cls.label() + " at this size");
        return it - prev.value();
    };
    double es = 0.0;
    if (l == "k") {
        if (cls.grade <= 8) {
            auto dl = delta();
            if (!dl) return dl.error();
            es = -shaftEi("k", cls.grade) + dl.value();
        } else {
            es = 0.0;
        }
    } else if (l == "m") {
        if (cls.grade <= 8) {
            auto dl = delta();
            if (!dl) return dl.error();
            es = -shaftEi("m", cls.grade) + dl.value();
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
            auto dl = delta();
            if (!dl) return dl.error();
            es = -shaftEi("n", cls.grade) + dl.value();
        } else {
            es = 0.0;
        }
    } else {  // p, r, s, t, u, v, x, y, z, za, zb, zc
        double dl = 0.0;
        if (cls.grade <= 7) {
            auto dr = delta();
            if (!dr) return dr.error();
            dl = dr.value();
        }
        es = -shaftEi(l, cls.grade) + dl;
    }
    d.upperUm = es;
    d.lowerUm = es - it;
    d.fundamentalUm = es;
    return d;
}

Result<FitResult> fitLimits(double nominalMm, const std::string& holeClass, const std::string& shaftClass) {
    auto hc = parseToleranceClass(holeClass);
    if (!hc) return hc.error();
    auto sc = parseToleranceClass(shaftClass);
    if (!sc) return sc.error();
    if (!hc.value().hole()) return invalid("fit: first class must be a hole (upper-case letter), got '" + holeClass + "'");
    if (sc.value().hole()) return invalid("fit: second class must be a shaft (lower-case letter), got '" + shaftClass + "'");
    auto h = limitDeviations(nominalMm, hc.value());
    if (!h) return h.error();
    auto s = limitDeviations(nominalMm, sc.value());
    if (!s) return s.error();
    FitResult f;
    f.label = hc.value().label() + "/" + sc.value().label();
    f.hole = h.value();
    f.shaft = s.value();
    f.maxClearanceUm = f.hole.upperUm - f.shaft.lowerUm;
    f.minClearanceUm = f.hole.lowerUm - f.shaft.upperUm;
    f.fitToleranceUm = f.hole.itUm + f.shaft.itUm;
    f.type = f.minClearanceUm >= 0.0   ? FitType::Clearance
             : f.maxClearanceUm <= 0.0 ? FitType::Interference
                                       : FitType::Transition;
    return f;
}

Result<FitResult> fitLimits(double nominalMm, const std::string& fit) {
    const auto slash = fit.find('/');
    if (slash == std::string::npos) return invalid("fit '" + fit + "': expected hole/shaft, e.g. H7/s6");
    return fitLimits(nominalMm, fit.substr(0, slash), fit.substr(slash + 1));
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
