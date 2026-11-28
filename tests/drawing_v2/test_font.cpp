// Чтение TrueType против эталонных значений DejaVu Sans, полученных fontTools 4.x
// (независимый читатель): unitsPerEm 2048, 6253 глифа, 'H' → глиф 43, ширина 1540, yMax 1493;
// 'А' (U+0410) → 933, 1401; 'Я' → 964, 1423; 'ж' → 971, 1845; пробел → 3, 651.

#include "test_support.hpp"

#include <gtest/gtest.h>

using namespace dv2test;

TEST(DrawingV2Font, DejaVuSansMetricsMatchFontTools) {
    auto path = findSystemFont();
    ASSERT_TRUE(path) << path.error().message;
    if (path.value().find("DejaVuSans.ttf") == std::string::npos) {
        GTEST_SKIP() << "reference numbers are for DejaVu Sans, found " << path.value();
    }
    const TrueTypeFont* f = systemFont();
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->unitsPerEm(), 2048);
    EXPECT_EQ(f->numGlyphs(), 6253);
    EXPECT_EQ(f->postScriptName(), "DejaVuSans");
    EXPECT_EQ(f->fsType(), 0);
    EXPECT_EQ(f->ascender(), 1901);
    EXPECT_EQ(f->descender(), -483);
    EXPECT_EQ(f->glyphIndex(U'H'), 43);
    EXPECT_EQ(f->advanceWidth(43), 1540);
    EXPECT_EQ(f->capHeight(), 1493);
    EXPECT_EQ(f->glyphIndex(U'А'), 933);
    EXPECT_EQ(f->advanceWidth(933), 1401);
    EXPECT_EQ(f->glyphIndex(U'Я'), 964);
    EXPECT_EQ(f->advanceWidth(964), 1423);
    EXPECT_EQ(f->glyphIndex(U'ж'), 971);
    EXPECT_EQ(f->advanceWidth(971), 1845);
    EXPECT_EQ(f->glyphIndex(U' '), 3);
    EXPECT_EQ(f->advanceWidth(3), 651);
    EXPECT_EQ(f->glyphIndex(char32_t(0x10FFFD)), 0);
    // Ширина "HH" при высоте прописных 5 мм: 2·1540/2048 · (5 / (1493/2048)) = 2·1540·5/1493.
    EXPECT_NEAR(f->textWidthMm(U"HH", 5.0), 2.0 * 1540 * 5 / 1493, 1e-9);
}

TEST(DrawingV2Font, RejectsGarbageAndRestrictedFonts) {
    EXPECT_FALSE(TrueTypeFont::fromBytes({1, 2, 3}, "tiny"));
    EXPECT_FALSE(TrueTypeFont::fromBytes(std::vector<std::uint8_t>(64, 0), "zeros"));
    EXPECT_FALSE(TrueTypeFont::loadFile("/nonexistent/font.ttf"));
    const TrueTypeFont* f = systemFont();
    ASSERT_NE(f, nullptr);
    // Копия шрифта с fsType = 2 (Restricted License) должна быть отвергнута.
    std::vector<std::uint8_t> bytes = f->bytes();
    const auto u16 = [&](std::size_t o) { return std::uint16_t((bytes[o] << 8) | bytes[o + 1]); };
    const auto u32 = [&](std::size_t o) {
        return (std::uint32_t(bytes[o]) << 24) | (std::uint32_t(bytes[o + 1]) << 16) |
               (std::uint32_t(bytes[o + 2]) << 8) | bytes[o + 3];
    };
    bool patched = false;
    for (std::size_t i = 0; i < u16(4); ++i) {
        const std::size_t rec = 12 + i * 16;
        if (std::string(reinterpret_cast<const char*>(&bytes[rec]), 4) == "OS/2") {
            const std::size_t off = u32(rec + 8);
            bytes[off + 8] = 0;
            bytes[off + 9] = 2;
            patched = true;
        }
    }
    ASSERT_TRUE(patched);
    auto r = TrueTypeFont::fromBytes(bytes, "restricted");
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code, ErrorCode::Unsupported);
}
