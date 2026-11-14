// ZIP writer/reader of bendcore_kernel_analysis (docs/kernel/ANALYSIS.md, "ZIP и zlib").
//
// CRC-32: published check values (the CRC catalogue "CRC-32/ISO-HDLC" check value of
// "123456789" is 0xCBF43926; RFC 3720 B.4 lists none for this polynomial, so the remaining
// values come from `python3 -c "import zlib; print(hex(zlib.crc32(b'...')))"` and are, where the
// build links zlib, re-checked against zlib's crc32 on the same and on random data.
// Archives: stored archives are unpacked by the reader and their headers decoded by hand
// (signature, method, sizes, CRC); deflated archives (native only) are inflated back.

#include <bendcore/kernel/mesh_export.hpp>

#include <gtest/gtest.h>

#include <cstring>
#include <random>
#include <string>

#if BENDCORE_ANALYSIS_HAS_ZLIB
#include <zlib.h>
#endif

namespace k = bendcore::kernel;

namespace {

std::uint32_t le32(const std::string& s, std::size_t at) {
    std::uint32_t v;
    std::memcpy(&v, s.data() + at, 4);
    return v;
}
std::uint16_t le16(const std::string& s, std::size_t at) {
    std::uint16_t v;
    std::memcpy(&v, s.data() + at, 2);
    return v;
}

std::string randomBytes(std::size_t n, unsigned seed) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> byte(0, 255);
    std::string s(n, '\0');
    for (char& c : s) c = static_cast<char>(byte(gen));
    return s;
}

}  // namespace

TEST(ZipArchive, Crc32KnownValues) {
    EXPECT_EQ(k::crc32(""), 0x00000000u);
    EXPECT_EQ(k::crc32("123456789"), 0xCBF43926u);  // CRC-32/ISO-HDLC check value
    EXPECT_EQ(k::crc32("a"), 0xE8B7BE43u);
    EXPECT_EQ(k::crc32("abc"), 0x352441C2u);
    EXPECT_EQ(k::crc32("The quick brown fox jumps over the lazy dog"), 0x414FA339u);
    EXPECT_EQ(k::crc32(std::string(32, '\0')), 0x190A55ADu);
    EXPECT_EQ(k::crc32(std::string(32, '\xFF')), 0xFF6CAB0Bu);
}

TEST(ZipArchive, Crc32ContinuesAcrossChunks) {
    const std::string text = "The quick brown fox jumps over the lazy dog";
    const std::uint32_t first = k::crc32(text.substr(0, 10));
    EXPECT_EQ(k::crc32(text.substr(10), first), 0x414FA339u);
}

#if BENDCORE_ANALYSIS_HAS_ZLIB
TEST(ZipArchive, Crc32MatchesZlib) {
    for (unsigned seed = 1; seed <= 8; ++seed) {
        const std::string data = randomBytes(1000 * seed + seed, seed);
        const uLong z = ::crc32(::crc32(0L, Z_NULL, 0), reinterpret_cast<const Bytef*>(data.data()),
                                static_cast<uInt>(data.size()));
        EXPECT_EQ(k::crc32(data), static_cast<std::uint32_t>(z)) << "seed " << seed;
    }
}
#endif

TEST(ZipArchive, DefaultMethodFollowsZlibAvailability) {
    EXPECT_EQ(k::zipDeflateAvailable(), BENDCORE_ANALYSIS_HAS_ZLIB != 0);
    EXPECT_EQ(k::zipDefaultMethod(), BENDCORE_ANALYSIS_HAS_ZLIB ? k::ZipMethod::Deflate : k::ZipMethod::Stored);
}

TEST(ZipArchive, StoredArchiveHeadersAndRoundTrip) {
    const std::vector<k::ZipEntry> entries = {{"a.txt", "123456789"}, {"dir/b.bin", randomBytes(5000, 7)}, {"empty", ""}};
    auto zip = k::writeZip(entries, k::ZipMethod::Stored);
    ASSERT_TRUE(zip.ok()) << zip.error().message;
    const std::string& z = zip.value();

    // First local header, decoded by hand (APPNOTE 4.3.7).
    EXPECT_EQ(le32(z, 0), 0x04034b50u);
    EXPECT_EQ(le16(z, 4), 10);  // version needed 1.0
    EXPECT_EQ(le16(z, 8), 0);   // stored
    EXPECT_EQ(le32(z, 14), 0xCBF43926u);
    EXPECT_EQ(le32(z, 18), 9u);
    EXPECT_EQ(le32(z, 22), 9u);
    EXPECT_EQ(le16(z, 26), 5);
    EXPECT_EQ(z.substr(30, 5), "a.txt");
    EXPECT_EQ(z.substr(35, 9), "123456789");  // stored bytes follow the name verbatim

    // End of central directory: three entries.
    const std::size_t eocd = z.size() - 22;
    EXPECT_EQ(le32(z, eocd), 0x06054b50u);
    EXPECT_EQ(le16(z, eocd + 10), 3);

    auto back = k::readZip(z);
    ASSERT_TRUE(back.ok()) << back.error().message;
    ASSERT_EQ(back.value().size(), 3u);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        EXPECT_EQ(back.value()[i].name, entries[i].name);
        EXPECT_EQ(back.value()[i].data, entries[i].data);
    }
    // Same input, same bytes (fixed DOS date).
    EXPECT_EQ(k::writeZip(entries, k::ZipMethod::Stored).value(), z);
}

TEST(ZipArchive, CorruptedStoredByteIsDetected) {
    auto zip = k::writeZip({{"a.txt", "123456789"}}, k::ZipMethod::Stored);
    ASSERT_TRUE(zip.ok());
    std::string broken = zip.value();
    broken[35] ^= 0x01;  // first data byte
    auto back = k::readZip(broken);
    ASSERT_FALSE(back.ok());
    EXPECT_EQ(back.error().code, k::ErrorCode::ParseError);
    EXPECT_NE(back.error().message.find("CRC-32"), std::string::npos);
}

#if BENDCORE_ANALYSIS_HAS_ZLIB
TEST(ZipArchive, DeflatedArchiveRoundTrip) {
    const std::string text(20000, 'x');
    auto zip = k::writeZip({{"x.txt", text}}, k::ZipMethod::Deflate);
    ASSERT_TRUE(zip.ok());
    EXPECT_EQ(le16(zip.value(), 8), 8);  // deflate
    EXPECT_LT(zip.value().size(), text.size() / 10);
    auto back = k::readZip(zip.value());
    ASSERT_TRUE(back.ok()) << back.error().message;
    EXPECT_EQ(back.value()[0].data, text);
}
#else
TEST(ZipArchive, DeflateWithoutZlibIsUnsupported) {
    auto zip = k::writeZip({{"x.txt", "abc"}}, k::ZipMethod::Deflate);
    ASSERT_FALSE(zip.ok());
    EXPECT_EQ(zip.error().code, k::ErrorCode::Unsupported);
}
#endif

TEST(ZipArchive, MalformedArchivesAreRejected) {
    EXPECT_EQ(k::readZip("short").error().code, k::ErrorCode::ParseError);
    EXPECT_EQ(k::readZip(std::string(100, 'z')).error().code, k::ErrorCode::ParseError);
}
