#include <bendcore/drawing_v2/sheet.hpp>

#include <array>
#include <string>
#include <utility>

namespace bendcore::drawing_v2 {

namespace {

struct SizePair {
    double shortSide;
    double longSide;
};

// ГОСТ 2.301-68, таблица 1.
constexpr std::array<SizePair, 5> kGostBasic = {{
    {841, 1189},  // А0
    {594, 841},   // А1
    {420, 594},   // А2
    {297, 420},   // А3
    {210, 297},   // А4
}};

struct Multiple {
    GostFormat base;
    int n;
    SizePair size;
};

// ГОСТ 2.301-68, таблица 2 (дополнительные форматы).
constexpr std::array<Multiple, 19> kGostMultiples = {{
    {GostFormat::A0, 2, {1189, 1682}}, {GostFormat::A0, 3, {1189, 2523}},
    {GostFormat::A1, 3, {841, 1783}},  {GostFormat::A1, 4, {841, 2378}},
    {GostFormat::A2, 3, {594, 1261}},  {GostFormat::A2, 4, {594, 1682}},
    {GostFormat::A2, 5, {594, 2102}},  {GostFormat::A3, 3, {420, 891}},
    {GostFormat::A3, 4, {420, 1189}},  {GostFormat::A3, 5, {420, 1486}},
    {GostFormat::A3, 6, {420, 1783}},  {GostFormat::A3, 7, {420, 2080}},
    {GostFormat::A4, 3, {297, 630}},   {GostFormat::A4, 4, {297, 841}},
    {GostFormat::A4, 5, {297, 1051}},  {GostFormat::A4, 6, {297, 1261}},
    {GostFormat::A4, 7, {297, 1471}},  {GostFormat::A4, 8, {297, 1682}},
    {GostFormat::A4, 9, {297, 1892}},
}};

// ISO 216, ряд A.
constexpr std::array<SizePair, 11> kIsoA = {{
    {841, 1189}, {594, 841}, {420, 594}, {297, 420}, {210, 297}, {148, 210},
    {105, 148},  {74, 105},  {52, 74},   {37, 52},   {26, 37},
}};

void orient(Sheet& s, SizePair p, Orientation o) {
    s.orientation = o;
    if (o == Orientation::Portrait) {
        s.widthMm = p.shortSide;
        s.heightMm = p.longSide;
    } else {
        s.widthMm = p.longSide;
        s.heightMm = p.shortSide;
    }
}

}  // namespace

const char* gostFormatName(GostFormat base) {
    switch (base) {
        case GostFormat::A0: return "\xD0\x90" "0";
        case GostFormat::A1: return "\xD0\x90" "1";
        case GostFormat::A2: return "\xD0\x90" "2";
        case GostFormat::A3: return "\xD0\x90" "3";
        case GostFormat::A4: return "\xD0\x90" "4";
    }
    return "?";
}

Result<Sheet> gostSheet(GostFormat base, int multiplier, Orientation orientation) {
    Sheet s;
    s.standard = SheetStandard::Gost2301;
    s.gostBase = base;
    s.multiplier = multiplier;
    if (multiplier == 1) {
        orient(s, kGostBasic[std::size_t(base)], orientation);
        s.name = gostFormatName(base);
        return s;
    }
    for (const auto& m : kGostMultiples) {
        if (m.base == base && m.n == multiplier) {
            orient(s, m.size, orientation);
            s.name = std::string(gostFormatName(base)) + "x" + std::to_string(multiplier);
            return s;
        }
    }
    return Error{ErrorCode::InvalidInput,
                 std::string("GOST 2.301 has no format ") + gostFormatName(base) + "x" +
                     std::to_string(multiplier)};
}

Result<Sheet> iso216Sheet(int n, Orientation orientation) {
    if (n < 0 || n > 10) {
        return Error{ErrorCode::InvalidInput, "ISO 216 A-series index must be 0..10"};
    }
    Sheet s;
    s.standard = SheetStandard::Iso216;
    s.isoIndex = n;
    s.name = "ISO A" + std::to_string(n);
    orient(s, kIsoA[std::size_t(n)], orientation);
    return s;
}

}  // namespace bendcore::drawing_v2
