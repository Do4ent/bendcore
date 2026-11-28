#include <bendcore/drawing_v2/truetype_font.hpp>

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <utility>

namespace bendcore::drawing_v2 {

namespace {

struct Reader {
    const std::vector<std::uint8_t>& d;
    bool in(std::uint32_t off, std::uint32_t len) const {
        return std::uint64_t(off) + len <= d.size();
    }
    std::uint16_t u16(std::uint32_t off) const {
        return std::uint16_t((d[off] << 8) | d[off + 1]);
    }
    std::int16_t s16(std::uint32_t off) const { return std::int16_t(u16(off)); }
    std::uint32_t u32(std::uint32_t off) const {
        return (std::uint32_t(d[off]) << 24) | (std::uint32_t(d[off + 1]) << 16) |
               (std::uint32_t(d[off + 2]) << 8) | std::uint32_t(d[off + 3]);
    }
};

struct TableRef {
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
};

Error fontError(const std::string& what) { return Error{ErrorCode::ParseError, "TrueType: " + what}; }

}  // namespace

Result<TrueTypeFont> TrueTypeFont::loadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return Error{ErrorCode::IoError, "cannot open font file: " + path};
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
    if (in.bad()) return Error{ErrorCode::IoError, "cannot read font file: " + path};
    return fromBytes(std::move(bytes), path);
}

Result<TrueTypeFont> TrueTypeFont::fromBytes(std::vector<std::uint8_t> bytes, std::string source) {
    TrueTypeFont f;
    f.data_ = std::move(bytes);
    f.source_ = std::move(source);
    if (auto st = f.parse(); !st) return st.error();
    return f;
}

Status TrueTypeFont::parse() {
    const Reader r{data_};
    if (!r.in(0, 12)) return fontError("file too short");
    const std::uint32_t version = r.u32(0);
    if (version != 0x00010000 && version != 0x74727565 /* 'true' */) {
        return fontError("not a TrueType-outline font (sfnt version " + std::to_string(version) +
                         "); CFF/OpenType-CFF fonts are not supported");
    }
    const std::uint16_t numTables = r.u16(4);
    if (!r.in(12, std::uint32_t(numTables) * 16)) return fontError("truncated table directory");
    std::map<std::string, TableRef> tables;
    for (std::uint16_t i = 0; i < numTables; ++i) {
        const std::uint32_t rec = 12 + std::uint32_t(i) * 16;
        std::string tag(reinterpret_cast<const char*>(&data_[rec]), 4);
        TableRef t{r.u32(rec + 8), r.u32(rec + 12)};
        if (!r.in(t.offset, t.length)) return fontError("table '" + tag + "' out of file bounds");
        tables[tag] = t;
    }
    for (const char* need : {"head", "hhea", "maxp", "hmtx", "cmap", "loca", "glyf"}) {
        if (!tables.count(need)) return fontError(std::string("missing table '") + need + "'");
    }

    const TableRef head = tables["head"];
    if (head.length < 54) return fontError("short 'head'");
    unitsPerEm_ = r.u16(head.offset + 18);
    if (unitsPerEm_ < 16) return fontError("bad unitsPerEm");
    xMin_ = r.s16(head.offset + 36);
    yMin_ = r.s16(head.offset + 38);
    xMax_ = r.s16(head.offset + 40);
    yMax_ = r.s16(head.offset + 42);
    indexToLocFormat_ = r.s16(head.offset + 50);

    const TableRef hhea = tables["hhea"];
    if (hhea.length < 36) return fontError("short 'hhea'");
    ascender_ = r.s16(hhea.offset + 4);
    descender_ = r.s16(hhea.offset + 6);
    numHMetrics_ = r.u16(hhea.offset + 34);

    const TableRef maxp = tables["maxp"];
    if (maxp.length < 6) return fontError("short 'maxp'");
    numGlyphs_ = r.u16(maxp.offset + 4);
    if (numHMetrics_ == 0 || numHMetrics_ > numGlyphs_) return fontError("bad numberOfHMetrics");

    const TableRef hmtx = tables["hmtx"];
    if (hmtx.length < std::uint32_t(numHMetrics_) * 4) return fontError("short 'hmtx'");
    hmtxOffset_ = hmtx.offset;

    const TableRef loca = tables["loca"];
    const std::uint32_t locaNeed = (std::uint32_t(numGlyphs_) + 1) * (indexToLocFormat_ ? 4 : 2);
    if (loca.length < locaNeed) return fontError("short 'loca'");
    locaOffset_ = loca.offset;
    glyfOffset_ = tables["glyf"].offset;
    glyfLength_ = tables["glyf"].length;

    // cmap: предпочитаем формат 12 (3,10), затем формат 4 (3,1) или (0,*).
    const TableRef cmap = tables["cmap"];
    if (cmap.length < 4) return fontError("short 'cmap'");
    const std::uint16_t nSub = r.u16(cmap.offset + 2);
    if (!r.in(cmap.offset + 4, std::uint32_t(nSub) * 8)) return fontError("truncated 'cmap'");
    std::uint32_t sub4 = 0, sub12 = 0;
    for (std::uint16_t i = 0; i < nSub; ++i) {
        const std::uint32_t rec = cmap.offset + 4 + std::uint32_t(i) * 8;
        const std::uint16_t platform = r.u16(rec), encoding = r.u16(rec + 2);
        const std::uint32_t off = cmap.offset + r.u32(rec + 4);
        if (!r.in(off, 4)) continue;
        const std::uint16_t format = r.u16(off);
        if (format == 12 && (platform == 3 && encoding == 10)) sub12 = off;
        if (format == 4 && ((platform == 3 && encoding == 1) || platform == 0) && !sub4) sub4 = off;
    }
    if (sub12) {
        if (!r.in(sub12, 16)) return fontError("truncated cmap format 12");
        const std::uint32_t nGroups = r.u32(sub12 + 12);
        if (!r.in(sub12 + 16, nGroups * 12)) return fontError("truncated cmap format 12 groups");
        for (std::uint32_t g = 0; g < nGroups; ++g) {
            const std::uint32_t rec = sub12 + 16 + g * 12;
            const std::uint32_t start = r.u32(rec), end = r.u32(rec + 4), gid = r.u32(rec + 8);
            if (end < start || end - start > 0x10FFFF) return fontError("bad cmap group");
            for (std::uint32_t cp = start; cp <= end; ++cp) {
                const std::uint32_t gi = gid + (cp - start);
                if (gi < numGlyphs_) cmap_[char32_t(cp)] = std::uint16_t(gi);
            }
        }
    } else if (sub4) {
        if (!r.in(sub4, 14)) return fontError("truncated cmap format 4");
        const std::uint16_t segX2 = r.u16(sub4 + 6);
        const std::uint32_t endOff = sub4 + 14;
        const std::uint32_t startOff = endOff + segX2 + 2;
        const std::uint32_t deltaOff = startOff + segX2;
        const std::uint32_t rangeOff = deltaOff + segX2;
        if (!r.in(endOff, std::uint32_t(segX2) * 4 + 2)) return fontError("truncated cmap format 4 segments");
        for (std::uint32_t s = 0; s < segX2 / 2u; ++s) {
            const std::uint16_t end = r.u16(endOff + 2 * s);
            const std::uint16_t start = r.u16(startOff + 2 * s);
            const std::uint16_t delta = r.u16(deltaOff + 2 * s);
            const std::uint16_t range = r.u16(rangeOff + 2 * s);
            if (start > end) continue;
            for (std::uint32_t cp = start; cp <= end; ++cp) {
                if (cp == 0xFFFF) break;
                std::uint16_t gid = 0;
                if (range == 0) {
                    gid = std::uint16_t(cp + delta);
                } else {
                    const std::uint32_t addr = rangeOff + 2 * s + range + 2 * (cp - start);
                    if (!r.in(addr, 2)) return fontError("cmap format 4 glyph index out of bounds");
                    gid = r.u16(addr);
                    if (gid != 0) gid = std::uint16_t(gid + delta);
                }
                if (gid != 0 && gid < numGlyphs_) cmap_[char32_t(cp)] = gid;
            }
        }
    } else {
        return fontError("no Unicode cmap subtable (format 4 or 12)");
    }

    // OS/2: права на встраивание и высота прописных.
    capHeight_ = 0;
    if (tables.count("OS/2")) {
        const TableRef os2 = tables["OS/2"];
        if (os2.length >= 10) {
            fsType_ = r.u16(os2.offset + 8);
            const std::uint16_t ver = r.u16(os2.offset);
            if (ver >= 2 && os2.length >= 90) capHeight_ = r.s16(os2.offset + 88);
        }
    }
    if ((fsType_ & 0x000F) == 0x0002) {
        return Error{ErrorCode::Unsupported,
                     "font '" + source_ + "' forbids embedding (OS/2 fsType = Restricted License)"};
    }
    if (capHeight_ <= 0) {
        const std::uint16_t h = glyphIndex(U'H');
        if (h == 0) return fontError("cannot determine cap height (no OS/2 sCapHeight and no 'H')");
        capHeight_ = glyphYMax(h);
        if (capHeight_ <= 0) return fontError("glyph 'H' has no outline");
    }

    if (tables.count("post") && tables["post"].length >= 8) {
        const std::int32_t fixed = std::int32_t(r.u32(tables["post"].offset + 4));
        italicAngle_ = double(fixed) / 65536.0;
    }

    // Имя PostScript (nameID 6): платформа 3 (UTF-16BE) или 1 (ASCII).
    psName_.clear();
    if (tables.count("name")) {
        const TableRef nm = tables["name"];
        if (nm.length >= 6) {
            const std::uint16_t count = r.u16(nm.offset + 2);
            const std::uint32_t strBase = nm.offset + r.u16(nm.offset + 4);
            for (std::uint16_t i = 0; i < count && psName_.empty(); ++i) {
                const std::uint32_t rec = nm.offset + 6 + std::uint32_t(i) * 12;
                if (!r.in(rec, 12)) break;
                const std::uint16_t platform = r.u16(rec), nameId = r.u16(rec + 6);
                const std::uint16_t len = r.u16(rec + 8), off = r.u16(rec + 10);
                if (nameId != 6 || !r.in(strBase + off, len)) continue;
                std::string out;
                if (platform == 3 || platform == 0) {
                    for (std::uint16_t k = 0; k + 1 < len; k += 2) {
                        const std::uint16_t ch = r.u16(strBase + off + k);
                        if (ch > 0x20 && ch < 0x7F) out.push_back(char(ch));
                    }
                } else if (platform == 1) {
                    for (std::uint16_t k = 0; k < len; ++k) {
                        const char ch = char(data_[strBase + off + k]);
                        if (ch > 0x20 && ch < 0x7F) out.push_back(ch);
                    }
                }
                psName_ = out;
            }
        }
    }
    // Символы, запрещённые в имени PDF, заменяем.
    for (char& ch : psName_) {
        if (ch == '/' || ch == '(' || ch == ')' || ch == '<' || ch == '>' || ch == '[' ||
            ch == ']' || ch == '{' || ch == '}' || ch == '%' || ch == '#') {
            ch = '-';
        }
    }
    if (psName_.empty()) psName_ = "EmbeddedTrueType";
    return okStatus();
}

std::uint16_t TrueTypeFont::glyphIndex(char32_t cp) const {
    const auto it = cmap_.find(cp);
    return it == cmap_.end() ? 0 : it->second;
}

std::uint16_t TrueTypeFont::advanceWidth(std::uint16_t glyph) const {
    const Reader r{data_};
    const std::uint16_t idx = glyph < numHMetrics_ ? glyph : std::uint16_t(numHMetrics_ - 1);
    return r.u16(hmtxOffset_ + std::uint32_t(idx) * 4);
}

int TrueTypeFont::glyphYMax(std::uint16_t glyph) const {
    if (glyph >= numGlyphs_) return 0;
    const Reader r{data_};
    std::uint32_t start = 0, end = 0;
    if (indexToLocFormat_) {
        start = r.u32(locaOffset_ + 4u * glyph);
        end = r.u32(locaOffset_ + 4u * (glyph + 1u));
    } else {
        start = 2u * r.u16(locaOffset_ + 2u * glyph);
        end = 2u * r.u16(locaOffset_ + 2u * (glyph + 1u));
    }
    if (end <= start || end - start < 10 || end > glyfLength_) return 0;
    return r.s16(glyfOffset_ + start + 8);
}

double TrueTypeFont::textWidthMm(std::u32string_view text, double capHeightMm) const {
    const double fontSizeMm = capHeightMm / capHeightRatio();
    double units = 0.0;
    for (char32_t cp : text) units += advanceWidth(glyphIndex(cp));
    return units * fontSizeMm / unitsPerEm_;
}

std::vector<std::string> defaultFontCandidates() {
    std::vector<std::string> out;
    if (const char* env = std::getenv("BENDCORE_DRAWING_FONT"); env && *env) out.emplace_back(env);
    for (const char* p : {
             "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
             "/usr/share/fonts/TTF/DejaVuSans.ttf",
             "/usr/share/fonts/dejavu/DejaVuSans.ttf",
             "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
             "/usr/local/share/fonts/DejaVuSans.ttf",
             "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
             "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
             "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
             "/Library/Fonts/DejaVuSans.ttf",
             "C:/Windows/Fonts/DejaVuSans.ttf",
         }) {
        out.emplace_back(p);
    }
    return out;
}

Result<std::string> findSystemFont() {
    for (const auto& path : defaultFontCandidates()) {
        std::ifstream in(path, std::ios::binary);
        if (in.good()) return path;
    }
    return Error{ErrorCode::IoError,
                 "no permissively licensed TrueType font found (DejaVu Sans or Liberation Sans); "
                 "set BENDCORE_DRAWING_FONT to a .ttf file"};
}

}  // namespace bendcore::drawing_v2
