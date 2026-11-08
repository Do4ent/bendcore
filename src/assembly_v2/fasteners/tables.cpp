// Таблицы размеров стандартного крепежа (docs/assembly/FASTENERS.md, раздел 2).
//
// ВНИМАНИЕ: доступа к официальным изданиям стандартов и к открытым справочным сайтам при
// составлении таблиц не было (сеть закрыта). Значения записаны по памяти из стандартов и
// ТРЕБУЮТ СВЕРКИ С ОФИЦИАЛЬНЫМ ИЗДАНИЕМ. Ряды ограничены размерами, в значениях которых
// автор уверен; размеры вне ряда — ошибка Unsupported, а не экстраполяция.

#include <bendcore/assembly_v2/fasteners.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <sstream>

namespace bendcore::assembly_v2::fasteners {

using kernel::ErrorCode;

namespace {

constexpr const char* kUnverified =
    "требует сверки с официальным изданием: значения записаны по памяти, доступа к тексту "
    "стандарта при составлении не было";

// Шаг крупной резьбы, ISO 261 / ГОСТ 8724-2002 (требует сверки с официальным изданием).
struct PitchRow {
    int d;
    double p;
};
constexpr std::array<PitchRow, 11> kCoarsePitch{{
    {3, 0.5}, {4, 0.7}, {5, 0.8}, {6, 1.0}, {8, 1.25}, {10, 1.5},
    {12, 1.75}, {14, 2.0}, {16, 2.0}, {20, 2.5}, {24, 3.0},
}};

// Болт ISO 4014 / ISO 4017: s, k (номинальная), e min (класс точности A).
// Требует сверки с официальным изданием.
constexpr std::array<HexBoltRow, 11> kIso4014{{
    {3, 0.5, 5.5, 2.0, 6.01},   {4, 0.7, 7.0, 2.8, 7.66},    {5, 0.8, 8.0, 3.5, 8.79},
    {6, 1.0, 10.0, 4.0, 11.05}, {8, 1.25, 13.0, 5.3, 14.38}, {10, 1.5, 16.0, 6.4, 17.77},
    {12, 1.75, 18.0, 7.5, 20.03}, {14, 2.0, 21.0, 8.8, 23.36}, {16, 2.0, 24.0, 10.0, 26.75},
    {20, 2.5, 30.0, 12.5, 33.53}, {24, 3.0, 36.0, 15.0, 39.98},
}};

// Болт ГОСТ 7798-70 (стандарт начинается с М6): s (основное исполнение), k.
// e min в таблицу не внесён. Требует сверки с официальным изданием.
const std::array<HexBoltRow, 8> kGost7798{{
    {6, 1.0, 10.0, 4.0, std::nullopt},   {8, 1.25, 13.0, 5.5, std::nullopt},
    {10, 1.5, 17.0, 7.0, std::nullopt},  {12, 1.75, 19.0, 8.0, std::nullopt},
    {14, 2.0, 22.0, 9.0, std::nullopt},  {16, 2.0, 24.0, 10.0, std::nullopt},
    {20, 2.5, 30.0, 13.0, std::nullopt}, {24, 3.0, 36.0, 15.0, std::nullopt},
}};

// Винт ISO 4762 (та же таблица принята для ГОСТ 11738-84): dk max, k = d, s, t min.
// Требует сверки с официальным изданием (для ГОСТ 11738 — и совпадения с ISO 4762).
constexpr std::array<SocketScrewRow, 11> kIso4762{{
    {3, 0.5, 5.5, 3, 2.5, 1.3},     {4, 0.7, 7.0, 4, 3.0, 2.0},     {5, 0.8, 8.5, 5, 4.0, 2.5},
    {6, 1.0, 10.0, 6, 5.0, 3.0},    {8, 1.25, 13.0, 8, 6.0, 4.0},   {10, 1.5, 16.0, 10, 8.0, 5.0},
    {12, 1.75, 18.0, 12, 10.0, 6.0}, {14, 2.0, 21.0, 14, 12.0, 7.0}, {16, 2.0, 24.0, 16, 14.0, 8.0},
    {20, 2.5, 30.0, 20, 17.0, 10.0}, {24, 3.0, 36.0, 24, 19.0, 12.0},
}};

// Гайка ISO 4032: s, m max, e min (A до М16, B — М20, М24). Требует сверки.
constexpr std::array<HexNutRow, 11> kIso4032{{
    {3, 0.5, 5.5, 2.4, 6.01},    {4, 0.7, 7.0, 3.2, 7.66},     {5, 0.8, 8.0, 4.7, 8.79},
    {6, 1.0, 10.0, 5.2, 11.05},  {8, 1.25, 13.0, 6.8, 14.38},  {10, 1.5, 16.0, 8.4, 17.77},
    {12, 1.75, 18.0, 10.8, 20.03}, {14, 2.0, 21.0, 12.8, 23.36}, {16, 2.0, 24.0, 14.8, 26.75},
    {20, 2.5, 30.0, 18.0, 32.95}, {24, 3.0, 36.0, 21.5, 39.55},
}};

// Гайка ГОСТ 5915-70 (исполнение 1): s, m. e min не внесён. Требует сверки.
const std::array<HexNutRow, 11> kGost5915{{
    {3, 0.5, 5.5, 2.4, std::nullopt},   {4, 0.7, 7.0, 3.2, std::nullopt},   {5, 0.8, 8.0, 4.0, std::nullopt},
    {6, 1.0, 10.0, 5.0, std::nullopt},  {8, 1.25, 13.0, 6.5, std::nullopt}, {10, 1.5, 17.0, 8.0, std::nullopt},
    {12, 1.75, 19.0, 10.0, std::nullopt}, {14, 2.0, 22.0, 11.0, std::nullopt}, {16, 2.0, 24.0, 13.0, std::nullopt},
    {20, 2.5, 30.0, 16.0, std::nullopt}, {24, 3.0, 36.0, 19.0, std::nullopt},
}};

// Шайба ISO 7089: d1 min, d2 max, h. Требует сверки.
constexpr std::array<WasherRow, 11> kIso7089{{
    {3, 3.2, 7, 0.5},   {4, 4.3, 9, 0.8},   {5, 5.3, 10, 1.0},  {6, 6.4, 12, 1.6},
    {8, 8.4, 16, 1.6},  {10, 10.5, 20, 2.0}, {12, 13, 24, 2.5}, {14, 15, 28, 2.5},
    {16, 17, 30, 3.0},  {20, 21, 37, 3.0},  {24, 25, 44, 4.0},
}};

// Шайба ГОСТ 11371-78, исполнение 1: d, D, s. Требует сверки.
constexpr std::array<WasherRow, 11> kGost11371{{
    {3, 3.2, 7, 0.5},    {4, 4.3, 9, 0.8},    {5, 5.3, 10, 1.0},  {6, 6.4, 12.5, 1.6},
    {8, 8.4, 17, 1.6},   {10, 10.5, 21, 2.0}, {12, 13, 24, 2.5},  {14, 15, 28, 2.5},
    {16, 17, 30, 3.0},   {20, 21, 37, 3.0},   {24, 25, 44, 4.0},
}};

// Шайба пружинная ГОСТ 6402-70, нормальная (Н): d, s = b. Ряд ограничен М5–М12 —
// в значениях для больших и меньших размеров автор не уверен. Требует сверки.
constexpr std::array<SpringWasherRow, 5> kGost6402{{
    {5, 5.1, 1.2, 1.2}, {6, 6.1, 1.4, 1.4}, {8, 8.2, 2.0, 2.0}, {10, 10.2, 2.5, 2.5}, {12, 12.2, 3.0, 3.0},
}};

// Штифт ISO 2338 (та же таблица фасок принята для ГОСТ 3128-70): d, c ≈. Требует сверки.
constexpr std::array<PinRow, 9> kIso2338{{
    {3, 0.5}, {4, 0.63}, {5, 0.8}, {6, 1.2}, {8, 1.6}, {10, 2.0}, {12, 2.5}, {16, 3.0}, {20, 3.5},
}};

template <typename Row>
std::vector<int> sizesOf(std::span<const Row> rows) {
    std::vector<int> out;
    for (const Row& r : rows) out.push_back(r.d);
    return out;
}

template <typename Row>
kernel::Result<Row> lookup(std::span<const Row> rows, FastenerStandard standard, int d) {
    for (const Row& r : rows) {
        if (r.d == d) return r;
    }
    std::ostringstream os;
    os << toString(standard) << ": size " << d << " is not in the table; available:";
    for (const Row& r : rows) os << ' ' << r.d;
    return kernel::Result<Row>::failure(ErrorCode::Unsupported, os.str());
}

template <typename Row>
kernel::Result<Row> wrongKind(FastenerStandard standard, const char* expected) {
    return kernel::Result<Row>::failure(ErrorCode::InvalidInput, std::string(toString(standard)) + " is a " +
                                                                     toString(kindOf(standard)) + ", not a " +
                                                                     expected);
}

}  // namespace

const char* toString(FastenerKind kind) noexcept {
    switch (kind) {
        case FastenerKind::HexBolt: return "HexBolt";
        case FastenerKind::SocketHeadScrew: return "SocketHeadScrew";
        case FastenerKind::HexNut: return "HexNut";
        case FastenerKind::PlainWasher: return "PlainWasher";
        case FastenerKind::SpringWasher: return "SpringWasher";
        case FastenerKind::ParallelPin: return "ParallelPin";
    }
    return "Unknown";
}

const char* toString(FastenerStandard standard) noexcept {
    switch (standard) {
        case FastenerStandard::Gost7798: return "ГОСТ 7798-70";
        case FastenerStandard::Iso4014: return "ISO 4014";
        case FastenerStandard::Iso4017: return "ISO 4017";
        case FastenerStandard::Gost11738: return "ГОСТ 11738-84";
        case FastenerStandard::Iso4762: return "ISO 4762";
        case FastenerStandard::Gost5915: return "ГОСТ 5915-70";
        case FastenerStandard::Iso4032: return "ISO 4032";
        case FastenerStandard::Gost11371: return "ГОСТ 11371-78";
        case FastenerStandard::Iso7089: return "ISO 7089";
        case FastenerStandard::Gost6402: return "ГОСТ 6402-70";
        case FastenerStandard::Gost3128: return "ГОСТ 3128-70";
        case FastenerStandard::Iso2338: return "ISO 2338";
    }
    return "Unknown";
}

FastenerKind kindOf(FastenerStandard standard) noexcept {
    switch (standard) {
        case FastenerStandard::Gost7798:
        case FastenerStandard::Iso4014:
        case FastenerStandard::Iso4017: return FastenerKind::HexBolt;
        case FastenerStandard::Gost11738:
        case FastenerStandard::Iso4762: return FastenerKind::SocketHeadScrew;
        case FastenerStandard::Gost5915:
        case FastenerStandard::Iso4032: return FastenerKind::HexNut;
        case FastenerStandard::Gost11371:
        case FastenerStandard::Iso7089: return FastenerKind::PlainWasher;
        case FastenerStandard::Gost6402: return FastenerKind::SpringWasher;
        case FastenerStandard::Gost3128:
        case FastenerStandard::Iso2338: return FastenerKind::ParallelPin;
    }
    return FastenerKind::HexBolt;
}

bool isGost(FastenerStandard standard) noexcept {
    switch (standard) {
        case FastenerStandard::Gost7798:
        case FastenerStandard::Gost11738:
        case FastenerStandard::Gost5915:
        case FastenerStandard::Gost11371:
        case FastenerStandard::Gost6402:
        case FastenerStandard::Gost3128: return true;
        default: return false;
    }
}

std::optional<FastenerStandard> counterpart(FastenerStandard standard) noexcept {
    switch (standard) {
        case FastenerStandard::Gost7798: return FastenerStandard::Iso4014;
        case FastenerStandard::Iso4014:
        case FastenerStandard::Iso4017: return FastenerStandard::Gost7798;
        case FastenerStandard::Gost11738: return FastenerStandard::Iso4762;
        case FastenerStandard::Iso4762: return FastenerStandard::Gost11738;
        case FastenerStandard::Gost5915: return FastenerStandard::Iso4032;
        case FastenerStandard::Iso4032: return FastenerStandard::Gost5915;
        case FastenerStandard::Gost11371: return FastenerStandard::Iso7089;
        case FastenerStandard::Iso7089: return FastenerStandard::Gost11371;
        case FastenerStandard::Gost3128: return FastenerStandard::Iso2338;
        case FastenerStandard::Iso2338: return FastenerStandard::Gost3128;
        case FastenerStandard::Gost6402: return std::nullopt;
    }
    return std::nullopt;
}

std::vector<FastenerStandard> allStandards() {
    return {FastenerStandard::Gost7798,  FastenerStandard::Iso4014,   FastenerStandard::Iso4017,
            FastenerStandard::Gost11738, FastenerStandard::Iso4762,   FastenerStandard::Gost5915,
            FastenerStandard::Iso4032,   FastenerStandard::Gost11371, FastenerStandard::Iso7089,
            FastenerStandard::Gost6402,  FastenerStandard::Gost3128,  FastenerStandard::Iso2338};
}

std::optional<double> coarsePitch(int nominalDiameter) noexcept {
    for (const PitchRow& r : kCoarsePitch) {
        if (r.d == nominalDiameter) return r.p;
    }
    return std::nullopt;
}

double externalMinorDiameter(double d, double pitch) noexcept { return d - 1.226869 * pitch; }

TableInfo tableInfo(FastenerStandard standard) {
    TableInfo info;
    info.standard = standard;
    info.verification = kUnverified;
    info.verified = false;
    switch (standard) {
        case FastenerStandard::Gost7798:
            info.source = "ГОСТ 7798-70, таблица размеров болтов нормальной точности (s основного исполнения, k)";
            info.sizes = sizesOf<HexBoltRow>(kGost7798);
            break;
        case FastenerStandard::Iso4014:
        case FastenerStandard::Iso4017:
            info.source = std::string(toString(standard)) + ", таблица размеров (s, k номинальная, e min класса A); "
                          "длина резьбы b — ISO 4014 (2d+6 / 2d+12 / 2d+25)";
            info.sizes = sizesOf<HexBoltRow>(kIso4014);
            break;
        case FastenerStandard::Gost11738:
        case FastenerStandard::Iso4762:
            info.source = "ISO 4762, таблица размеров (dk max, k = d, s, t min); для ГОСТ 11738-84 принята та же "
                          "таблица (совпадение с ГОСТ требует сверки)";
            info.sizes = sizesOf<SocketScrewRow>(kIso4762);
            break;
        case FastenerStandard::Gost5915:
            info.source = "ГОСТ 5915-70, исполнение 1 (s, m)";
            info.sizes = sizesOf<HexNutRow>(kGost5915);
            break;
        case FastenerStandard::Iso4032:
            info.source = "ISO 4032 (s, m max, e min класса A до M16 и B для M20, M24)";
            info.sizes = sizesOf<HexNutRow>(kIso4032);
            break;
        case FastenerStandard::Gost11371:
            info.source = "ГОСТ 11371-78, исполнение 1 (d, D, s)";
            info.sizes = sizesOf<WasherRow>(kGost11371);
            break;
        case FastenerStandard::Iso7089:
            info.source = "ISO 7089 (d1 min, d2 max, h)";
            info.sizes = sizesOf<WasherRow>(kIso7089);
            break;
        case FastenerStandard::Gost6402:
            info.source = "ГОСТ 6402-70, шайбы нормальные Н (d, s = b); ряд ограничен М5–М12";
            info.sizes = sizesOf<SpringWasherRow>(kGost6402);
            break;
        case FastenerStandard::Gost3128:
        case FastenerStandard::Iso2338:
            info.source = "ISO 2338 (d, фаска c ≈); для ГОСТ 3128-70 принята та же таблица фасок";
            info.sizes = sizesOf<PinRow>(kIso2338);
            break;
    }
    return info;
}

kernel::Result<HexBoltRow> hexBoltRow(FastenerStandard standard, int d) {
    switch (standard) {
        case FastenerStandard::Gost7798: return lookup<HexBoltRow>(kGost7798, standard, d);
        case FastenerStandard::Iso4014:
        case FastenerStandard::Iso4017: return lookup<HexBoltRow>(kIso4014, standard, d);
        default: return wrongKind<HexBoltRow>(standard, "hex bolt");
    }
}

kernel::Result<SocketScrewRow> socketScrewRow(FastenerStandard standard, int d) {
    if (kindOf(standard) != FastenerKind::SocketHeadScrew) return wrongKind<SocketScrewRow>(standard, "socket head screw");
    return lookup<SocketScrewRow>(kIso4762, standard, d);
}

kernel::Result<HexNutRow> hexNutRow(FastenerStandard standard, int d) {
    switch (standard) {
        case FastenerStandard::Gost5915: return lookup<HexNutRow>(kGost5915, standard, d);
        case FastenerStandard::Iso4032: return lookup<HexNutRow>(kIso4032, standard, d);
        default: return wrongKind<HexNutRow>(standard, "hex nut");
    }
}

kernel::Result<WasherRow> washerRow(FastenerStandard standard, int d) {
    switch (standard) {
        case FastenerStandard::Gost11371: return lookup<WasherRow>(kGost11371, standard, d);
        case FastenerStandard::Iso7089: return lookup<WasherRow>(kIso7089, standard, d);
        default: return wrongKind<WasherRow>(standard, "plain washer");
    }
}

kernel::Result<SpringWasherRow> springWasherRow(FastenerStandard standard, int d) {
    if (standard != FastenerStandard::Gost6402) return wrongKind<SpringWasherRow>(standard, "spring washer");
    return lookup<SpringWasherRow>(kGost6402, standard, d);
}

kernel::Result<PinRow> pinRow(FastenerStandard standard, int d) {
    if (kindOf(standard) != FastenerKind::ParallelPin) return wrongKind<PinRow>(standard, "parallel pin");
    return lookup<PinRow>(kIso2338, standard, d);
}

}  // namespace bendcore::assembly_v2::fasteners
