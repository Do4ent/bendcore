// PDF 1.4, одна страница. Координаты содержимого — пункты (1 мм = 72/25,4 pt), начало —
// левый нижний угол листа, как в модели. Текст — шрифт TrueType, встроенный целиком
// (FontFile2), CIDFontType2 + Type0 с кодировкой Identity-H (CID = индекс глифа),
// массив ширин W и карта ToUnicode для извлечения текста.

#include <bendcore/drawing_v2/export.hpp>

#include "render_util.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <numbers>
#include <set>
#include <sstream>

namespace bendcore::drawing_v2 {

using detail::num;

namespace {

constexpr double kPt = 72.0 / 25.4;

std::string pt(double mm) { return num(mm * kPt, 4); }

std::string hex4(unsigned v) {
    char buf[8];
    std::snprintf(buf, sizeof buf, "%04X", v & 0xFFFF);
    return buf;
}

// Строка PDF в UTF-16BE с BOM (для /Title).
std::string pdfTextString(const std::u32string& text) {
    std::string out = "<FEFF";
    for (char32_t cp : text) {
        if (cp >= 0x10000) {
            const unsigned v = unsigned(cp) - 0x10000;
            out += hex4(0xD800 + (v >> 10));
            out += hex4(0xDC00 + (v & 0x3FF));
        } else {
            out += hex4(unsigned(cp));
        }
    }
    return out + ">";
}

std::string utf16Hex(char32_t cp) {
    if (cp >= 0x10000) {
        const unsigned v = unsigned(cp) - 0x10000;
        return hex4(0xD800 + (v >> 10)) + hex4(0xDC00 + (v & 0x3FF));
    }
    return hex4(unsigned(cp));
}

class PdfBuilder {
public:
    // Резервирует номер объекта.
    int reserve() { return int(++count_); }
    void set(int id, std::string body) { objects_[id] = std::move(body); }
    int add(std::string body) {
        const int id = reserve();
        set(id, std::move(body));
        return id;
    }
    static std::string stream(const std::string& dict, const std::string& data) {
        return "<< " + dict + " /Length " + std::to_string(data.size()) + " >>\nstream\n" + data +
               "\nendstream";
    }
    std::string finish(int root, int info) const {
        std::string out = "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
        std::vector<std::size_t> offsets(count_ + 1, 0);
        for (unsigned id = 1; id <= count_; ++id) {
            offsets[id] = out.size();
            out += std::to_string(id) + " 0 obj\n" + objects_.at(int(id)) + "\nendobj\n";
        }
        const std::size_t xref = out.size();
        out += "xref\n0 " + std::to_string(count_ + 1) + "\n";
        out += "0000000000 65535 f \n";
        for (unsigned id = 1; id <= count_; ++id) {
            char buf[32];
            std::snprintf(buf, sizeof buf, "%010zu 00000 n \n", offsets[id]);
            out += buf;
        }
        out += "trailer\n<< /Size " + std::to_string(count_ + 1) + " /Root " + std::to_string(root) +
               " 0 R /Info " + std::to_string(info) + " 0 R >>\nstartxref\n" + std::to_string(xref) +
               "\n%%EOF\n";
        return out;
    }

private:
    unsigned count_ = 0;
    std::map<int, std::string> objects_;
};

}  // namespace

Result<std::string> writePdf(const Drawing& drawing, const PdfOptions& options) {
    const double W = drawing.sheet().widthMm, H = drawing.sheet().heightMm;
    const TrueTypeFont* font = options.font;

    // Сбор используемых глифов и проверка, что шрифт содержит все символы.
    std::map<std::uint16_t, char32_t> usedGlyphs;
    for (const Entity& e : drawing.entities()) {
        const auto* t = std::get_if<Text>(&e.geometry);
        if (!t) continue;
        if (!font) {
            return Error{ErrorCode::InvalidInput,
                         "PDF: drawing contains text, a TrueType font is required (PdfOptions::font)"};
        }
        auto cps = decodeUtf8(t->utf8);
        if (!cps) return cps.error();
        for (char32_t cp : cps.value()) {
            const std::uint16_t g = font->glyphIndex(cp);
            if (g == 0) {
                char buf[16];
                std::snprintf(buf, sizeof buf, "U+%04X", unsigned(cp));
                return Error{ErrorCode::Unsupported, std::string("PDF: font '") + font->source() +
                                                         "' has no glyph for " + buf};
            }
            usedGlyphs.emplace(g, cp);
        }
    }

    std::ostringstream cs;
    cs << "0 G 0 g 0 J 1 j\n";
    std::string lastStyle;
    auto style = [&](LineType lt) {
        const auto& spec = drawing.lineTypeSpec(lt);
        const double w = drawing.lineWidth(lt);
        std::string s = pt(w) + " w [";
        const auto dash = renderDashArray(spec.pattern, w);
        for (std::size_t i = 0; i < dash.size(); ++i) s += (i ? " " : "") + pt(dash[i]);
        s += "] 0 d\n";
        if (s != lastStyle) {
            cs << s;
            lastStyle = s;
        }
    };
    auto P = [](Point2 p) { return pt(p.x) + " " + pt(p.y); };
    auto line = [&](Point2 a, Point2 b) { cs << P(a) << " m " << P(b) << " l S\n"; };
    auto bezierPath = [&](const std::vector<Point2>& pts, bool move) {
        if (move) cs << P(pts[0]) << " m\n";
        for (std::size_t i = 1; i + 2 < pts.size(); i += 3) {
            cs << P(pts[i]) << ' ' << P(pts[i + 1]) << ' ' << P(pts[i + 2]) << " c\n";
        }
    };

    for (const Entity& e : drawing.entities()) {
        if (const auto* l = std::get_if<LineSeg>(&e.geometry)) {
            style(e.lineType);
            line(l->a, l->b);
        } else if (const auto* c = std::get_if<Circle>(&e.geometry)) {
            style(e.lineType);
            bezierPath(detail::arcToBezier(c->center, c->radius, 0.0, 2.0 * std::numbers::pi), true);
            cs << "h S\n";
        } else if (const auto* a = std::get_if<Arc>(&e.geometry)) {
            style(e.lineType);
            const double a0 = a->startDeg * std::numbers::pi / 180.0;
            const double a1 = a0 + detail::arcSweepDeg(a->startDeg, a->endDeg) * std::numbers::pi / 180.0;
            bezierPath(detail::arcToBezier(a->center, a->radius, a0, a1), true);
            cs << "S\n";
        } else if (const auto* p = std::get_if<Polyline>(&e.geometry)) {
            style(e.lineType);
            cs << P(p->points[0]) << " m\n";
            for (std::size_t i = 1; i < p->points.size(); ++i) cs << P(p->points[i]) << " l\n";
            cs << (p->closed ? "h S\n" : "S\n");
        } else if (const auto* ht = std::get_if<Hatch>(&e.geometry)) {
            auto segs = hatchSegments(*ht);
            if (!segs) return segs.error();
            style(e.lineType);
            for (const auto& s : segs.value()) line(s.a, s.b);
        } else if (const auto* t = std::get_if<Text>(&e.geometry)) {
            auto cps = decodeUtf8(t->utf8).value();
            const double width = font->textWidthMm(cps, t->height) * t->widthFactor;
            const Point2 o = detail::textBaselineOrigin(*t, width);
            const double fontSizePt = t->height / font->capHeightRatio() * kPt;
            const double r = t->rotationDeg * std::numbers::pi / 180.0;
            const double k = t->slanted ? std::tan((90.0 - kGostTextSlantDeg) * std::numbers::pi / 180.0) : 0.0;
            const double ca = std::cos(r), sa = std::sin(r);
            const double wf = t->widthFactor;
            // Tm = Scale(wf, 1) · Skew · Rotate: [wf·cos wf·sin (k·cos − sin) (k·sin + cos) x y].
            cs << "BT /F1 " << num(fontSizePt, 4) << " Tf " << num(wf * ca, 6) << ' ' << num(wf * sa, 6) << ' '
               << num(k * ca - sa, 6) << ' ' << num(k * sa + ca, 6) << ' ' << P(o) << " Tm <";
            for (char32_t cp : cps) cs << hex4(font->glyphIndex(cp));
            cs << "> Tj ET\n";
        }
    }

    PdfBuilder pdf;
    const int catalog = pdf.reserve();
    const int pages = pdf.reserve();
    const int page = pdf.reserve();
    const int contents = pdf.add(PdfBuilder::stream("", cs.str()));

    std::string resources = "<< >>";
    if (!usedGlyphs.empty()) {
        const double scale = 1000.0 / font->unitsPerEm();
        auto u = [&](int v) { return num(v * scale, 0); };
        const int fontFile = pdf.add(PdfBuilder::stream(
            "/Length1 " + std::to_string(font->bytes().size()),
            std::string(font->bytes().begin(), font->bytes().end())));
        int flags = 32;  // Nonsymbolic
        if (font->italicAngle() != 0.0) flags |= 64;
        const int descriptor = pdf.add(
            "<< /Type /FontDescriptor /FontName /" + font->postScriptName() + " /Flags " +
            std::to_string(flags) + " /FontBBox [" + u(font->xMin()) + " " + u(font->yMin()) + " " +
            u(font->xMax()) + " " + u(font->yMax()) + "] /ItalicAngle " + num(font->italicAngle(), 2) +
            " /Ascent " + u(font->ascender()) + " /Descent " + u(font->descender()) + " /CapHeight " +
            u(font->capHeight()) + " /StemV 80 /FontFile2 " + std::to_string(fontFile) + " 0 R >>");
        std::string widths = "[";
        for (const auto& [g, cp] : usedGlyphs) {
            widths += " " + std::to_string(g) + " [" + u(font->advanceWidth(g)) + "]";
        }
        widths += " ]";
        const int cidFont = pdf.add(
            "<< /Type /Font /Subtype /CIDFontType2 /BaseFont /" + font->postScriptName() +
            " /CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >> /FontDescriptor " +
            std::to_string(descriptor) + " 0 R /DW " + u(font->advanceWidth(0)) + " /W " + widths +
            " /CIDToGIDMap /Identity >>");
        std::string cmap =
            "/CIDInit /ProcSet findresource begin\n12 dict begin\nbegincmap\n"
            "/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n"
            "/CMapName /Adobe-Identity-UCS def\n/CMapType 2 def\n"
            "1 begincodespacerange\n<0000> <FFFF>\nendcodespacerange\n";
        std::vector<std::pair<std::uint16_t, char32_t>> entries(usedGlyphs.begin(), usedGlyphs.end());
        for (std::size_t i = 0; i < entries.size(); i += 100) {
            const std::size_t n = std::min<std::size_t>(100, entries.size() - i);
            cmap += std::to_string(n) + " beginbfchar\n";
            for (std::size_t j = i; j < i + n; ++j) {
                cmap += "<" + hex4(entries[j].first) + "> <" + utf16Hex(entries[j].second) + ">\n";
            }
            cmap += "endbfchar\n";
        }
        cmap += "endcmap\nCMapName currentdict /CMap defineresource pop\nend\nend";
        const int toUnicode = pdf.add(PdfBuilder::stream("", cmap));
        const int type0 = pdf.add("<< /Type /Font /Subtype /Type0 /BaseFont /" + font->postScriptName() +
                                  " /Encoding /Identity-H /DescendantFonts [" + std::to_string(cidFont) +
                                  " 0 R] /ToUnicode " + std::to_string(toUnicode) + " 0 R >>");
        resources = "<< /Font << /F1 " + std::to_string(type0) + " 0 R >> >>";
    }

    auto title = decodeUtf8(options.title.empty() ? drawing.sheet().name : options.title);
    if (!title) return title.error();
    const int info = pdf.add("<< /Title " + pdfTextString(title.value()) +
                             " /Producer (bendcore drawing_v2) >>");
    pdf.set(catalog, "<< /Type /Catalog /Pages " + std::to_string(pages) + " 0 R >>");
    pdf.set(pages, "<< /Type /Pages /Kids [" + std::to_string(page) + " 0 R] /Count 1 >>");
    pdf.set(page, "<< /Type /Page /Parent " + std::to_string(pages) + " 0 R /MediaBox [0 0 " + pt(W) +
                      " " + pt(H) + "] /Resources " + resources + " /Contents " +
                      std::to_string(contents) + " 0 R >>");
    return pdf.finish(catalog, info);
}

Result<std::size_t> saveToFile(const std::string& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return Error{ErrorCode::IoError, "cannot open for writing: " + path};
    out.write(bytes.data(), std::streamsize(bytes.size()));
    out.close();
    if (!out) return Error{ErrorCode::IoError, "write failed: " + path};
    return bytes.size();
}

}  // namespace bendcore::drawing_v2
