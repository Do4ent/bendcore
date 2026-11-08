// DXF R2000 (AC1015), ASCII. Структура: HEADER, CLASSES, TABLES (VPORT, LTYPE, LAYER, STYLE,
// VIEW, UCS, APPID, DIMSTYLE, BLOCK_RECORD), BLOCKS (*Model_Space, *Paper_Space), ENTITIES,
// OBJECTS (корневой словарь). Каждый объект имеет дескриптор (5) и владельца (330).

#include <bendcore/drawing_v2/export.hpp>

#include "render_util.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <set>
#include <sstream>

namespace bendcore::drawing_v2 {

using detail::num;

int dxfLineweight(double widthMm) {
    static constexpr std::array<int, 24> kWeights = {0,  5,  9,  13, 15, 18,  20,  25,
                                                      30, 35, 40, 50, 53, 60,  70,  80,
                                                      90, 100, 106, 120, 140, 158, 200, 211};
    const double hundredths = widthMm * 100.0;
    int best = kWeights[0];
    for (int w : kWeights) {
        if (std::fabs(w - hundredths) < std::fabs(best - hundredths) - 1e-9) best = w;
    }
    return best;
}

namespace {

class DxfOut {
public:
    void pair(int code, const std::string& value) { os_ << code << '\n' << value << '\n'; }
    void pair(int code, int value) { pair(code, std::to_string(value)); }
    void real(int code, double value) { pair(code, num(value, 6)); }
    void point(int base, Point2 p) {
        real(base, p.x);
        real(base + 10, p.y);
        real(base + 20, 0.0);
    }
    std::string str() const { return os_.str(); }

private:
    std::ostringstream os_;
};

std::string hex(unsigned h) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%X", h);
    return buf;
}

// Строка DXF R2000: ASCII, символы вне ASCII — \U+XXXX; '\' сам по себе допустим.
Result<std::string> dxfString(const std::string& utf8) {
    auto decoded = decodeUtf8(utf8);
    if (!decoded) return decoded.error();
    std::string out;
    for (char32_t cp : decoded.value()) {
        if (cp < 0x20 || cp == 0x7F) {
            return Error{ErrorCode::InvalidInput, "DXF string contains a control character"};
        }
        if (cp < 0x80) {
            out.push_back(char(cp));
        } else if (cp <= 0xFFFF) {
            char buf[16];
            std::snprintf(buf, sizeof buf, "\\U+%04X", unsigned(cp));
            out += buf;
        } else {
            return Error{ErrorCode::Unsupported,
                         "DXF R2000 \\U+XXXX escapes cover only the BMP; code point U+" +
                             hex(unsigned(cp)) + " cannot be written"};
        }
    }
    return out;
}

struct Handles {
    unsigned next = 1;
    unsigned take() { return next++; }
};

}  // namespace

Result<std::string> writeDxf(const Drawing& drawing) {
    Handles h;
    DxfOut body;  // всё, кроме HEADER (HANDSEED известен только в конце)

    // Используемые типы линий с шаблоном (Continuous пишется всегда).
    std::vector<LineType> patterned;
    {
        std::set<std::string> seen;
        for (LineType t : kAllLineTypes) {
            const auto& spec = drawing.lineTypeSpec(t);
            if (!spec.pattern.empty() && seen.insert(spec.dxfName).second) patterned.push_back(t);
        }
    }

    body.pair(0, "SECTION");
    body.pair(2, "CLASSES");
    body.pair(0, "ENDSEC");

    body.pair(0, "SECTION");
    body.pair(2, "TABLES");

    auto beginTable = [&](const std::string& name, int count) {
        const unsigned th = h.take();
        body.pair(0, "TABLE");
        body.pair(2, name);
        body.pair(5, hex(th));
        body.pair(330, "0");
        body.pair(100, "AcDbSymbolTable");
        body.pair(70, count);
        return th;
    };
    auto record = [&](const std::string& type, unsigned owner, const char* subclass) {
        body.pair(0, type);
        body.pair(5, hex(h.take()));
        body.pair(330, hex(owner));
        body.pair(100, "AcDbSymbolTableRecord");
        body.pair(100, subclass);
    };

    const double W = drawing.sheet().widthMm, H = drawing.sheet().heightMm;

    // VPORT *Active: вид на весь лист.
    {
        const unsigned t = beginTable("VPORT", 1);
        record("VPORT", t, "AcDbViewportTableRecord");
        body.pair(2, "*Active");
        body.pair(70, 0);
        body.real(10, 0.0);
        body.real(20, 0.0);
        body.real(11, 1.0);
        body.real(21, 1.0);
        body.real(12, W / 2.0);
        body.real(22, H / 2.0);
        body.real(13, 0.0);
        body.real(23, 0.0);
        body.real(14, 10.0);
        body.real(24, 10.0);
        body.real(15, 10.0);
        body.real(25, 10.0);
        body.real(16, 0.0);
        body.real(26, 0.0);
        body.real(36, 1.0);
        body.real(17, 0.0);
        body.real(27, 0.0);
        body.real(37, 0.0);
        body.real(40, H * 1.05);
        body.real(41, W / H);
        body.real(42, 50.0);
        body.real(43, 0.0);
        body.real(44, 0.0);
        body.real(50, 0.0);
        body.real(51, 0.0);
        body.pair(71, 0);
        body.pair(72, 100);
        body.pair(73, 1);
        body.pair(74, 3);
        body.pair(75, 0);
        body.pair(76, 0);
        body.pair(77, 0);
        body.pair(78, 0);
        body.pair(0, "ENDTAB");
    }

    // LTYPE.
    {
        const unsigned t = beginTable("LTYPE", 3 + int(patterned.size()));
        for (const char* name : {"ByBlock", "ByLayer", "Continuous"}) {
            record("LTYPE", t, "AcDbLinetypeTableRecord");
            body.pair(2, name);
            body.pair(70, 0);
            body.pair(3, std::string(name) == "Continuous" ? "Solid line" : "");
            body.pair(72, 65);
            body.pair(73, 0);
            body.real(40, 0.0);
        }
        for (LineType lt : patterned) {
            const auto& spec = drawing.lineTypeSpec(lt);
            auto desc = dxfString("ГОСТ 2.303: " + spec.gostName);
            if (!desc) return desc.error();
            record("LTYPE", t, "AcDbLinetypeTableRecord");
            body.pair(2, spec.dxfName);
            body.pair(70, 0);
            body.pair(3, desc.value());
            body.pair(72, 65);
            body.pair(73, int(spec.pattern.size()));
            body.real(40, patternMetrics(spec.pattern).period);
            for (double v : spec.pattern) {
                body.real(49, v);
                body.pair(74, 0);
            }
        }
        body.pair(0, "ENDTAB");
    }

    // LAYER.
    {
        const unsigned t = beginTable("LAYER", int(drawing.layers().size()));
        for (const auto& layer : drawing.layers()) {
            record("LAYER", t, "AcDbLayerTableRecord");
            body.pair(2, layer.name);
            body.pair(70, 0);
            body.pair(62, layer.aciColor);
            body.pair(6, "Continuous");
            body.pair(370, -3);
        }
        body.pair(0, "ENDTAB");
    }

    // STYLE: Standard и два стиля ГОСТ (прямой и с наклоном 75° → oblique 15°).
    {
        const unsigned t = beginTable("STYLE", 3);
        struct S {
            const char* name;
            double oblique;
        };
        for (S s : {S{"Standard", 0.0}, S{"GOST_UPRIGHT", 0.0}, S{"GOST_SLANT", 90.0 - kGostTextSlantDeg}}) {
            record("STYLE", t, "AcDbTextStyleTableRecord");
            body.pair(2, s.name);
            body.pair(70, 0);
            body.real(40, 0.0);
            body.real(41, 1.0);
            body.real(50, s.oblique);
            body.pair(71, 0);
            body.real(42, 2.5);
            body.pair(3, std::string(s.name) == "Standard" ? "txt" : "DejaVuSans.ttf");
            body.pair(4, "");
        }
        body.pair(0, "ENDTAB");
    }

    for (const char* empty : {"VIEW", "UCS"}) {
        beginTable(empty, 0);
        body.pair(0, "ENDTAB");
    }
    {
        const unsigned t = beginTable("APPID", 1);
        record("APPID", t, "AcDbRegAppTableRecord");
        body.pair(2, "ACAD");
        body.pair(70, 0);
        body.pair(0, "ENDTAB");
    }
    {
        const unsigned t = h.take();
        body.pair(0, "TABLE");
        body.pair(2, "DIMSTYLE");
        body.pair(5, hex(t));
        body.pair(330, "0");
        body.pair(100, "AcDbSymbolTable");
        body.pair(70, 1);
        body.pair(100, "AcDbDimStyleTable");
        body.pair(0, "DIMSTYLE");
        body.pair(105, hex(h.take()));
        body.pair(330, hex(t));
        body.pair(100, "AcDbSymbolTableRecord");
        body.pair(100, "AcDbDimStyleTableRecord");
        body.pair(2, "Standard");
        body.pair(70, 0);
        body.pair(0, "ENDTAB");
    }
    unsigned modelRecord = 0, paperRecord = 0;
    {
        const unsigned t = beginTable("BLOCK_RECORD", 2);
        modelRecord = h.next;
        record("BLOCK_RECORD", t, "AcDbBlockTableRecord");
        body.pair(2, "*Model_Space");
        paperRecord = h.next;
        record("BLOCK_RECORD", t, "AcDbBlockTableRecord");
        body.pair(2, "*Paper_Space");
        body.pair(0, "ENDTAB");
    }
    body.pair(0, "ENDSEC");

    // BLOCKS.
    body.pair(0, "SECTION");
    body.pair(2, "BLOCKS");
    for (auto [name, owner] : {std::pair<const char*, unsigned>{"*Model_Space", modelRecord},
                               std::pair<const char*, unsigned>{"*Paper_Space", paperRecord}}) {
        const bool paper = owner == paperRecord;
        body.pair(0, "BLOCK");
        body.pair(5, hex(h.take()));
        body.pair(330, hex(owner));
        body.pair(100, "AcDbEntity");
        if (paper) body.pair(67, 1);
        body.pair(8, "0");
        body.pair(100, "AcDbBlockBegin");
        body.pair(2, name);
        body.pair(70, 0);
        body.point(10, {0, 0});
        body.pair(3, name);
        body.pair(1, "");
        body.pair(0, "ENDBLK");
        body.pair(5, hex(h.take()));
        body.pair(330, hex(owner));
        body.pair(100, "AcDbEntity");
        if (paper) body.pair(67, 1);
        body.pair(8, "0");
        body.pair(100, "AcDbBlockEnd");
    }
    body.pair(0, "ENDSEC");

    // ENTITIES (пространство модели, 1 единица = 1 мм листа).
    body.pair(0, "SECTION");
    body.pair(2, "ENTITIES");
    auto common = [&](const char* type, const Entity& e, LineType lt) {
        const auto& spec = drawing.lineTypeSpec(lt);
        body.pair(0, type);
        body.pair(5, hex(h.take()));
        body.pair(330, hex(modelRecord));
        body.pair(100, "AcDbEntity");
        body.pair(8, e.layer);
        body.pair(6, spec.pattern.empty() ? "Continuous" : spec.dxfName);
        body.pair(370, dxfLineweight(drawing.lineWidth(lt)));
    };
    auto writeLine = [&](const Entity& e, Point2 a, Point2 b) {
        common("LINE", e, e.lineType);
        body.pair(100, "AcDbLine");
        body.point(10, a);
        body.point(11, b);
    };
    for (const Entity& e : drawing.entities()) {
        if (const auto* l = std::get_if<LineSeg>(&e.geometry)) {
            writeLine(e, l->a, l->b);
        } else if (const auto* c = std::get_if<Circle>(&e.geometry)) {
            common("CIRCLE", e, e.lineType);
            body.pair(100, "AcDbCircle");
            body.point(10, c->center);
            body.real(40, c->radius);
        } else if (const auto* a = std::get_if<Arc>(&e.geometry)) {
            common("ARC", e, e.lineType);
            body.pair(100, "AcDbCircle");
            body.point(10, a->center);
            body.real(40, a->radius);
            body.pair(100, "AcDbArc");
            double s0 = std::fmod(a->startDeg, 360.0);
            if (s0 < 0) s0 += 360.0;
            body.real(50, s0);
            body.real(51, std::fmod(s0 + detail::arcSweepDeg(a->startDeg, a->endDeg), 360.0));
        } else if (const auto* p = std::get_if<Polyline>(&e.geometry)) {
            common("LWPOLYLINE", e, e.lineType);
            body.pair(100, "AcDbPolyline");
            body.pair(90, int(p->points.size()));
            // 128 — генерация шаблона типа линии непрерывно по всей полилинии.
            body.pair(70, (p->closed ? 1 : 0) | 128);
            for (const auto& pt : p->points) {
                body.real(10, pt.x);
                body.real(20, pt.y);
            }
        } else if (const auto* t = std::get_if<Text>(&e.geometry)) {
            auto value = dxfString(t->utf8);
            if (!value) return value.error();
            common("TEXT", e, LineType::SolidThin);
            body.pair(100, "AcDbText");
            body.point(10, t->position);
            body.real(40, t->height);
            body.pair(1, value.value());
            body.real(50, t->rotationDeg);
            body.real(41, t->widthFactor);
            body.real(51, t->slanted ? 90.0 - kGostTextSlantDeg : 0.0);
            body.pair(7, t->slanted ? "GOST_SLANT" : "GOST_UPRIGHT");
            const int hj = t->hAlign == HAlign::Left ? 0 : t->hAlign == HAlign::Center ? 1 : 2;
            const int vj = t->vAlign == VAlign::Baseline ? 0 : t->vAlign == VAlign::Middle ? 2 : 3;
            if (hj != 0) body.pair(72, hj);
            if (hj != 0 || vj != 0) body.point(11, t->position);
            body.pair(100, "AcDbText");
            if (vj != 0) body.pair(73, vj);
        } else if (const auto* ht = std::get_if<Hatch>(&e.geometry)) {
            auto segs = hatchSegments(*ht);
            if (!segs) return segs.error();
            for (const auto& s : segs.value()) writeLine(e, s.a, s.b);
        } else if (const auto* f = std::get_if<SolidFill>(&e.geometry)) {
            // SOLID — треугольник (четвёртая точка совпадает с третьей); выпуклый
            // многоугольник выводится веером треугольников от первой вершины.
            for (std::size_t i = 1; i + 1 < f->points.size(); ++i) {
                common("SOLID", e, e.lineType);
                body.pair(100, "AcDbTrace");
                body.point(10, f->points[0]);
                body.point(11, f->points[i]);
                body.point(12, f->points[i + 1]);
                body.point(13, f->points[i + 1]);
            }
        }
    }
    body.pair(0, "ENDSEC");

    // OBJECTS: корневой словарь и словарь групп.
    const unsigned rootDict = h.take();
    const unsigned groupDict = h.take();
    body.pair(0, "SECTION");
    body.pair(2, "OBJECTS");
    body.pair(0, "DICTIONARY");
    body.pair(5, hex(rootDict));
    body.pair(330, "0");
    body.pair(100, "AcDbDictionary");
    body.pair(281, 1);
    body.pair(3, "ACAD_GROUP");
    body.pair(350, hex(groupDict));
    body.pair(0, "DICTIONARY");
    body.pair(5, hex(groupDict));
    body.pair(330, hex(rootDict));
    body.pair(100, "AcDbDictionary");
    body.pair(281, 1);
    body.pair(0, "ENDSEC");
    body.pair(0, "EOF");

    DxfOut head;
    head.pair(0, "SECTION");
    head.pair(2, "HEADER");
    head.pair(9, "$ACADVER");
    head.pair(1, "AC1015");
    head.pair(9, "$DWGCODEPAGE");
    head.pair(3, "ANSI_1251");
    head.pair(9, "$INSBASE");
    head.point(10, {0, 0});
    head.pair(9, "$EXTMIN");
    head.point(10, {0, 0});
    head.pair(9, "$EXTMAX");
    head.point(10, {W, H});
    head.pair(9, "$LIMMIN");
    head.real(10, 0.0);
    head.real(20, 0.0);
    head.pair(9, "$LIMMAX");
    head.real(10, W);
    head.real(20, H);
    head.pair(9, "$LTSCALE");
    head.real(40, 1.0);
    head.pair(9, "$CELTSCALE");
    head.real(40, 1.0);
    head.pair(9, "$PSLTSCALE");
    head.pair(70, 0);
    head.pair(9, "$INSUNITS");
    head.pair(70, 4);
    head.pair(9, "$MEASUREMENT");
    head.pair(70, 1);
    head.pair(9, "$LWDISPLAY");
    head.pair(290, 1);
    head.pair(9, "$TEXTSTYLE");
    head.pair(7, "GOST_SLANT");
    head.pair(9, "$HANDSEED");
    head.pair(5, hex(h.next));
    head.pair(0, "ENDSEC");

    return head.str() + body.str();
}

}  // namespace bendcore::drawing_v2
