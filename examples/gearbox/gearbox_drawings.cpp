// Комплект чертежей редуктора РЦ-80 (docs/drawing/DRAWING_DOCUMENT.md): рабочие чертежи шести
// деталей (корпус, крышка корпуса, вал-шестерня, вал, колесо, крышка подшипника сквозная) и
// сборочный чертёж с позициями, спецификация ГОСТ 2.106. Размеры, знаки шероховатости, допуски
// формы и расположения, базы и размеры резьбы ссылаются на элементы модели (имя + сигнатура):
// они пересчитываются при изменении параметров детали. Элементы находятся по геометрии
// (положение, радиус) в снимке модели.

#include "gearbox_internal.hpp"

#include <bendcore/modeling/part_document.hpp>

#include <BRep_Tool.hxx>
#include <TopoDS.hxx>
#include <gp.hxx>

#include <algorithm>
#include <cmath>

namespace bendcore::examples::gearbox {

using namespace bendcore::drawing_v2;
using namespace bendcore::drawing_v2::document;
using annotations::AnchorKind;
using annotations::DimensionType;
using annotations::GeometricCharacteristic;
using modeling::ElementKind;
using detail::fail;

namespace {

constexpr double kPi = 3.14159265358979323846;

// Аннотации листа по снимку модели.
class Sheet {
public:
    Sheet(SheetDef& s, const ModelSnapshot& m) : s_(s), m_(m) {}

    std::uint32_t index(ElementKind kind, const TopoDS_Shape& e) const {
        auto i = m_.named->indexOf(kind, e);
        if (!i) fail("element is not in the snapshot map");
        return std::uint32_t(*i);
    }
    modeling::TopoReference ref(ElementKind kind, const TopoDS_Shape& e) const {
        auto r = elementReference(m_, kind, index(kind, e));
        if (!r.ok()) fail("element reference: " + r.error().message);
        return r.value();
    }
    annotations::Anchor anchor(ElementKind kind, const TopoDS_Shape& e, AnchorKind ak) const {
        annotations::Anchor a;
        a.kind = ak;
        a.ref.kind = kind == ElementKind::Face ? annotations::RefKind::Face
                                               : (kind == ElementKind::Edge ? annotations::RefKind::Edge : annotations::RefKind::Vertex);
        a.ref.index = index(kind, e);
        a.ref.persistent = ref(kind, e);
        return a;
    }
    const TopoDS_Shape& shape() const { return m_.named->shape(); }

    TopoDS_Shape vertex(double x, double y, double z) const {
        const std::size_t n = m_.named->count(ElementKind::Vertex);
        std::optional<std::size_t> found;
        for (std::size_t i = 0; i < n; ++i) {
            const gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(m_.named->element(ElementKind::Vertex, i)));
            if (p.Distance(gp_Pnt(x, y, z)) > 1e-5) continue;
            if (found) fail("several vertices at (" + detail::num(x) + ", " + detail::num(y) + ", " + detail::num(z) + ")");
            found = i;
        }
        if (!found) fail("no vertex at (" + detail::num(x) + ", " + detail::num(y) + ", " + detail::num(z) + ")");
        return m_.named->element(ElementKind::Vertex, *found);
    }
    annotations::Anchor v(double x, double y, double z) const { return anchor(ElementKind::Vertex, vertex(x, y, z), AnchorKind::Point); }
    annotations::Anchor circ(double x, double y, double z, double r) const {
        return anchor(ElementKind::Edge, detail::circleEdge(shape(), gp_Pnt(x, y, z), r), AnchorKind::Center);
    }
    annotations::Anchor cyl(double r, gp_Pnt axisPoint, gp_Dir dir, gp_Pnt inside) const {
        return anchor(ElementKind::Face, detail::cylinderFaceAt(shape(), r, axisPoint, dir, inside), AnchorKind::Center);
    }

    DimensionItem& dim(const std::string& id, const std::string& view, DimensionType type, std::vector<annotations::Anchor> a,
                       double offset, const std::string& tol = {}) {
        DimensionItem d;
        d.id = id;
        d.view = view;
        d.def.type = type;
        d.def.anchors = std::move(a);
        d.def.offset = offset;
        if (!tol.empty()) {
            auto t = annotations::parseToleranceClass(tol);
            if (!t.ok()) fail("tolerance " + tol + ": " + t.error().message);
            d.def.tolerance = t.value();
        }
        s_.dimensions.push_back(std::move(d));
        return s_.dimensions.back();
    }
    void roughness(const std::string& id, const std::string& view, const TopoDS_Shape& edge, const std::string& value, double at = 0.5) {
        RoughnessItem r;
        r.id = id;
        r.view = view;
        r.edge = ref(ElementKind::Edge, edge);
        r.at = at;
        r.spec.value = value;
        s_.roughness.push_back(r);
    }
    void datum(const std::string& id, const std::string& view, const TopoDS_Shape& edge, const std::string& letter, double at = 0.5) {
        DatumItem d;
        d.id = id;
        d.view = view;
        d.edge = ref(ElementKind::Edge, edge);
        d.at = at;
        d.letter = letter;
        s_.datums.push_back(d);
    }
    void tolerance(const std::string& id, const std::string& view, const TopoDS_Shape& edge, GeometricCharacteristic c,
                   const std::string& value, std::vector<std::string> datums = {}, double at = 0.5) {
        ToleranceItem t;
        t.id = id;
        t.view = view;
        t.edge = ref(ElementKind::Edge, edge);
        t.at = at;
        t.frame.characteristic = c;
        t.frame.tolerance = value;
        t.frame.datums = std::move(datums);
        s_.tolerances.push_back(t);
    }
    // Размер резьбы: грань резьбы (атрибуты thread.*), выбранная предикатом по ThreadInfo.
    template <class Pred>
    void threadNote(const std::string& id, const std::string& view, Pred pred, const std::string& field, double offset = 10) {
        for (const modeling::ThreadInfo& t : modeling::threadsOf(*m_.named)) {
            if (!pred(t)) continue;
            ThreadNoteItem n;
            n.id = id;
            n.view = view;
            auto r = elementReference(m_, ElementKind::Face, std::uint32_t(t.face));
            if (!r.ok()) fail("thread face reference: " + r.error().message);
            n.face = r.value();
            n.def.toleranceField = field;
            n.def.offset = offset;
            s_.threadNotes.push_back(n);
            return;
        }
        fail("thread for note " + id + " not found");
    }

private:
    SheetDef& s_;
    const ModelSnapshot& m_;
};


ViewDef view(const std::string& id, ViewKind kind, const std::string& parent = {}, std::optional<views::SheetSide> side = {}) {
    ViewDef v;
    v.id = id;
    v.kind = kind;
    v.parent = parent;
    v.side = side;
    return v;
}

ViewDef section(const std::string& id, views::Vec3 toward, views::Vec3 right, views::Vec3 point, const std::string& mark,
                views::SectionKind kind) {
    ViewDef v = view(id, ViewKind::Section);
    v.toward = toward;
    v.right = right;
    v.planePoint = point;
    v.markView = mark;
    v.sectionKind = kind;
    v.hidden = false;
    return v;
}

TitleFields title() {
    TitleFields t;
    t.organization = "bendcore";
    t.litera = "";
    t.developer = {"bendcore", "11.26"};  // сгенерировано программой; подписи — за человеком
    return t;
}

DrawingDocument partDrawing(const PartEntry& p, SheetFormat format) {
    DrawingDocument d;
    d.model().kind = ModelKind::Part;
    d.model().path = p.file;
    d.model().properties = {p.designation, p.name, p.material, p.density};
    SheetDef s;
    s.id = "S1";
    s.format = format;
    s.title = title();
    d.sheets().push_back(std::move(s));
    return d;
}

ModelSnapshot snap(const PartEntry& p) {
    auto r = snapshotPart(*p.doc, {p.designation, p.name, p.material, p.density});
    if (!r.ok()) fail(p.key + ": snapshot: " + r.error().message);
    return std::move(r).value();
}

double var(const PartEntry& p, const char* name) {
    auto v = p.doc->variableValue(name);
    if (!v.ok()) fail(p.key + ": variable " + name + ": " + v.error().message);
    return v.value();
}

const std::vector<std::string> kShaftRequirements = {
    "235...262 HB.",
    "Неуказанные предельные отклонения размеров: валов h14, остальных ±IT14/2.",
    "Неуказанные радиусы скруглений 0,5 мм; фаски 1×45°.",
};

// ---------------------------------------------------------------- вал

void shaftOutSheet(DrawingDocument& d, const PartEntry& p) {
    const ModelSnapshot m = snap(p);
    SheetDef& sd = d.sheets()[0];
    sd.views = {view("F", ViewKind::Base), view("T", ViewKind::Projected, "F", views::SheetSide::Below)};
    const double x1 = var(p, "x1"), x2 = var(p, "x2"), x4 = var(p, "x4"), x5 = var(p, "x5"), x6 = var(p, "x6"), x7 = var(p, "x7");
    const double xkw = var(p, "xkw"), xko = var(p, "xko");
    sd.views.push_back(section("A", {-1, 0, 0}, {0, -1, 0}, {xkw, 0, 0}, "F", views::SectionKind::Slice));
    sd.views.push_back(section("B", {-1, 0, 0}, {0, -1, 0}, {xko, 0, 0}, "F", views::SectionKind::Slice));
    Sheet s(sd, m);
    const gp_Dir X = gp::DX();
    s.dim("L", "F", DimensionType::Horizontal, {s.v(0, 0, 14), s.v(x7, 0, 11.5)}, -32);
    s.dim("Lend", "F", DimensionType::Horizontal, {s.v(x6, 0, 12.5), s.v(x7, 0, 11.5)}, 20);
    s.dim("Lseat", "F", DimensionType::Horizontal, {s.v(0, 0, 14), s.v(x1, 0, 17.5)}, 16);
    s.dim("Lcol", "F", DimensionType::Horizontal, {s.v(x2, 0, 21), s.v(x2 + 10, 0, 21)}, 10);
    s.dim("D1", "F", DimensionType::Diameter, {s.cyl(15, gp::Origin(), X, gp_Pnt(8, 0, 15))}, 8, "k6");
    s.dim("D2", "F", DimensionType::Diameter, {s.cyl(17.5, gp::Origin(), X, gp_Pnt(30, 0, 17.5))}, -8, "p6");
    s.dim("D3", "F", DimensionType::Diameter, {s.cyl(21, gp::Origin(), X, gp_Pnt(x2 + 5, 0, 21))}, 8);
    s.dim("D4", "F", DimensionType::Diameter, {s.cyl(15, gp::Origin(), X, gp_Pnt(x5 - 5, 0, 15))}, -8, "k6");
    s.dim("D5", "F", DimensionType::Diameter, {s.cyl(12.5, gp::Origin(), X, gp_Pnt(x7 - 2, 0, 12.5))}, 8, "h6");
    s.dim("bw", "T", DimensionType::Vertical, {s.v(xkw - 11, -5, 12.5), s.v(xkw - 11, 5, 12.5)}, 12, "N9");
    s.dim("bo", "T", DimensionType::Vertical, {s.v(xko - 12, -4, 8.5), s.v(xko - 12, 4, 8.5)}, 12, "N9");
    const TopoDS_Shape seatL = detail::lineEdge(s.shape(), gp_Pnt(1, 0, 15), gp_Pnt(x1 - 2, 0, 15));
    const TopoDS_Shape seatR = detail::lineEdge(s.shape(), gp_Pnt(x4 + 2, 0, 15), gp_Pnt(x5, 0, 15));
    const TopoDS_Shape wheelSeat = detail::lineEdge(s.shape(), gp_Pnt(x1, 0, 17.5), gp_Pnt(xkw - 16, 0, 17.5));
    s.roughness("Ra1", "F", seatL, "0,8", 0.5);
    s.roughness("Ra2", "F", seatR, "0,8", 0.5);
    s.roughness("Ra3", "F", wheelSeat, "1,6", 0.4);
    s.datum("DA", "F", seatL, "А", 0.5);
    s.tolerance("T1", "F", seatR, GeometricCharacteristic::CircularRunout, "0,016", {"А"}, 0.5);
    s.tolerance("T2", "F", wheelSeat, GeometricCharacteristic::CircularRunout, "0,025", {"А"}, 0.7);
    s.tolerance("T3", "F", seatL, GeometricCharacteristic::Cylindricity, "0,005", {}, 0.2);
    sd.roughnessRemainder = annotations::RoughnessSpec{annotations::RoughnessMethod::MaterialRemoval, "Ra", "6,3", "", 2.5};
    sd.technicalRequirements = kShaftRequirements;
    sd.technicalRequirements.push_back("Шпоночные пазы: глубина t1 = 5 (⌀35) и 4 (⌀25), длина 32 — по ГОСТ 23360-78.");
}

// ---------------------------------------------------------------- вал-шестерня

void shaftInSheet(DrawingDocument& d, const PartEntry& p) {
    const ModelSnapshot m = snap(p);
    SheetDef& sd = d.sheets()[0];
    sd.views = {view("F", ViewKind::Base), view("T", ViewKind::Projected, "F", views::SheetSide::Below)};
    const double x1 = var(p, "x1"), x2 = var(p, "x2"), x3 = var(p, "x3"), x4 = var(p, "x4"), x5 = var(p, "x5"), x6 = var(p, "x6"),
                 x7 = var(p, "x7"), L0 = var(p, "L0");
    sd.views.push_back(section("A", {-1, 0, 0}, {0, -1, 0}, {L0 / 2, 0, 0}, "F", views::SectionKind::Slice));
    Sheet s(sd, m);
    const gp_Dir X = gp::DX();
    s.dim("L", "F", DimensionType::Horizontal, {s.v(0, 0, 9), s.v(x7, 0, 11.5)}, -32);
    s.dim("L0", "F", DimensionType::Horizontal, {s.v(0, 0, 9), s.v(x1, 0, 11)}, 18);
    s.dim("b1", "F", DimensionType::Horizontal, {s.v(x4, 0, 20), s.v(x5, 0, 20)}, 8);
    s.dim("D0", "F", DimensionType::Diameter, {s.cyl(10, gp::Origin(), X, gp_Pnt(3, 0, 10))}, -8, "k6");
    s.dim("Ds1", "F", DimensionType::Diameter, {s.cyl(12.5, gp::Origin(), X, gp_Pnt(x2 + 5, 0, 12.5))}, 8, "k6");
    s.dim("Ds2", "F", DimensionType::Diameter, {s.cyl(12.5, gp::Origin(), X, gp_Pnt(x6 + 8, 0, 12.5))}, 8, "k6");
    s.dim("Dc", "F", DimensionType::Diameter, {s.cyl(15, gp::Origin(), X, gp_Pnt(x3 + 10, 0, 15))}, -8);
    s.dim("d", "F", DimensionType::Diameter, {s.cyl(20, gp::Origin(), X, gp_Pnt((x4 + x5) / 2, 0, 20))}, 8);
    s.dim("bk", "T", DimensionType::Vertical, {s.v(L0 / 2 - 11, -3, 6.5), s.v(L0 / 2 - 11, 3, 6.5)}, 12, "N9");
    const TopoDS_Shape seatL = detail::lineEdge(s.shape(), gp_Pnt(x2 + 1, 0, 12.5), gp_Pnt(x3 - 2, 0, 12.5));
    const TopoDS_Shape seatR = detail::lineEdge(s.shape(), gp_Pnt(x6 + 2, 0, 12.5), gp_Pnt(x7 - 1, 0, 12.5));
    const TopoDS_Shape rim = detail::lineEdge(s.shape(), gp_Pnt(x4, 0, 20), gp_Pnt(x5, 0, 20));
    s.roughness("Ra1", "F", seatL, "0,8");
    s.roughness("Ra2", "F", seatR, "0,8");
    s.roughness("Ra3", "F", rim, "1,6", 0.3);
    s.datum("DA", "F", seatL, "А");
    s.tolerance("T1", "F", seatR, GeometricCharacteristic::CircularRunout, "0,016", {"А"});
    s.tolerance("T2", "F", rim, GeometricCharacteristic::CircularRunout, "0,03", {"А"}, 0.7);
    sd.roughnessRemainder = annotations::RoughnessSpec{annotations::RoughnessMethod::MaterialRemoval, "Ra", "6,3", "", 2.5};
    sd.technicalRequirements = kShaftRequirements;
    sd.technicalRequirements.push_back(
        "Венец показан делительным цилиндром d = 40 (зубья не моделируются): m = 2, z = 20, угол профиля 20°, x = 0, da = 44h11, "
        "df = 35, степень точности 8-B ГОСТ 1643-81.");
    sd.technicalRequirements.push_back("Шпоночный паз: t1 = 3,5, длина 28 — по ГОСТ 23360-78.");
}

// ---------------------------------------------------------------- колесо

void wheelSheet(DrawingDocument& d, const PartEntry& p) {
    const ModelSnapshot m = snap(p);
    SheetDef& sd = d.sheets()[0];
    ViewDef f = view("F", ViewKind::Base);
    ViewDef l = view("L", ViewKind::Projected, "F", views::SheetSide::RightOf);
    sd.views = {f, l, section("A", {0, -1, 0}, {1, 0, 0}, {0, 0, 0}, "L", views::SectionKind::Cut)};
    Sheet s(sd, m);
    const double r = var(p, "d") / 2, bw = var(p, "bw"), lh = var(p, "lh"), rh = var(p, "dh") / 2, rb = var(p, "db") / 2,
                 bk = var(p, "bk"), t2 = var(p, "t2");
    const gp_Dir X = gp::DX();
    s.dim("b", "F", DimensionType::Horizontal, {s.v(-bw / 2, 0, r - 1.5), s.v(bw / 2, 0, r - 1.5)}, 12);
    s.dim("lh", "F", DimensionType::Horizontal, {s.v(-lh / 2, 0, rh - 1), s.v(lh / 2, 0, rh - 1)}, 50);
    s.dim("d", "F", DimensionType::Diameter, {s.cyl(r, gp::Origin(), X, gp_Pnt(0, 0, r))}, 10);
    s.dim("dh", "F", DimensionType::Diameter, {s.cyl(rh, gp::Origin(), X, gp_Pnt(12, 0, rh))}, -10);
    s.dim("db", "L", DimensionType::Diameter, {s.circ(-lh / 2, 0, 0, rb)}, 0, "H7").def.leaderAngleDeg = 225;
    s.dim("bk", "L", DimensionType::Aligned, {s.v(lh / 2, -bk / 2, rb + t2), s.v(lh / 2, bk / 2, rb + t2)}, 10, "JS9");
    const TopoDS_Shape rimLine = detail::lineEdge(s.shape(), gp_Pnt(-bw / 2 + 1.5, 0, r), gp_Pnt(bw / 2 - 1.5, 0, r));
    const TopoDS_Shape hubFace = detail::circleEdge(s.shape(), gp_Pnt(lh / 2, 0, 0), rh - 1);
    const TopoDS_Shape rimFace = detail::circleEdge(s.shape(), gp_Pnt(bw / 2, 0, 0), r - 1.5);
    s.roughness("Ra1", "F", rimLine, "1,6", 0.5);
    s.roughness("Ra2", "F", hubFace, "3,2", 0.5);
    s.datum("DA", "F", hubFace, "А", 0.3);
    s.tolerance("T1", "F", rimFace, GeometricCharacteristic::CircularRunout, "0,03", {"А"}, 0.7);
    sd.roughnessRemainder = annotations::RoughnessSpec{annotations::RoughnessMethod::MaterialRemoval, "Ra", "6,3", "", 2.5};
    sd.technicalRequirements = {
        "235...262 HB.",
        "Венец показан делительным цилиндром d = 120 (зубья не моделируются): m = 2, z = 60, угол профиля 20°, x = 0, da = 124h11, "
        "df = 115, степень точности 8-B ГОСТ 1643-81.",
        "Паз ступицы: t2 = 3,3 (размер 38,3+0,2) по ГОСТ 23360-78.",
        "Неуказанные предельные отклонения размеров: отверстий H14, валов h14, остальных ±IT14/2.",
        "Облегчающие отверстия: 6 отв. ⌀16 на ⌀78.",
    };
}

// ---------------------------------------------------------------- крышка подшипника сквозная

void capSheet(DrawingDocument& d, const PartEntry& p) {
    const ModelSnapshot m = snap(p);
    SheetDef& sd = d.sheets()[0];
    sd.views = {view("F", ViewKind::Base), view("L", ViewKind::Projected, "F", views::SheetSide::RightOf)};
    sd.views.push_back(section("A", {0, -1, 0}, {1, 0, 0}, {0, 0, 0}, "L", views::SectionKind::Cut));
    Sheet s(sd, m);
    const double D = var(p, "D"), Dc = var(p, "Dc"), tc = var(p, "tc"), sp = var(p, "s"), rb = var(p, "rb"), dh = var(p, "dh");
    const gp_Dir X = gp::DX();
    s.dim("tc", "F", DimensionType::Horizontal, {s.v(0, 0, Dc / 2), s.v(tc, 0, Dc / 2 - 1)}, 10);
    s.dim("s", "F", DimensionType::Horizontal, {s.v(-sp, 0, D / 2), s.v(0, 0, D / 2)}, -10);
    s.dim("Dc", "F", DimensionType::Diameter, {s.cyl(Dc / 2, gp::Origin(), X, gp_Pnt(tc / 2 - 0.5, 0, Dc / 2))}, 10);
    s.dim("D", "F", DimensionType::Diameter, {s.cyl(D / 2, gp::Origin(), X, gp_Pnt(-sp / 2, 0, D / 2))}, -10, "h8");
    s.dim("dh", "L", DimensionType::Diameter, {s.circ(-sp, 0, 0, dh / 2)}, 0, "H11").def.leaderAngleDeg = 200;
    const double c = std::cos(kPi / 4);
    auto& h = s.dim("dk", "L", DimensionType::Diameter, {s.circ(0, rb * c, rb * c, 3.5)}, 0);
    h.def.prefix = "4 отв. ";
    h.def.leaderAngleDeg = 45;
    const TopoDS_Shape spigot = detail::lineEdge(s.shape(), gp_Pnt(-sp, 0, D / 2), gp_Pnt(0, 0, D / 2));
    const TopoDS_Shape face = detail::circleEdge(s.shape(), gp_Pnt(0, 0, 0), Dc / 2);
    s.roughness("Ra1", "F", spigot, "1,6");
    s.roughness("Ra2", "F", face, "3,2", 0.75);
    s.datum("DA", "F", spigot, "А", 0.5);
    s.tolerance("T1", "F", face, GeometricCharacteristic::Perpendicularity, "0,03", {"А"}, 0.3);
    sd.roughnessRemainder = annotations::RoughnessSpec{annotations::RoughnessMethod::NoMaterialRemoval, "", "", "", 2.5};
    sd.technicalRequirements = {
        "Отливка по ГОСТ 26645-85, класс точности 9-0-0-9.",
        "Неуказанные литейные радиусы 2...3 мм.",
        "Отверстия ⌀7 — на окружности ⌀74, позиционный допуск ⌀0,2.",
        "Неуказанные предельные отклонения размеров: отверстий H14, валов h14, остальных ±IT14/2.",
    };
}

// ---------------------------------------------------------------- корпус

void housingSheet(DrawingDocument& d, const PartEntry& p, const Layout& L) {
    const ModelSnapshot m = snap(p);
    SheetDef& sd = d.sheets()[0];
    ViewDef f = view("F", ViewKind::Base);
    ViewDef t = view("T", ViewKind::Projected, "F", views::SheetSide::Below);
    ViewDef l = view("L", ViewKind::Projected, "F", views::SheetSide::RightOf);
    f.hidden = t.hidden = l.hidden = false;  // невидимый контур литого корпуса не выводится (читаемость)
    sd.views = {f, t, l, section("A", {0, 0, 1}, {1, 0, 0}, {0, 0, L.axisHeight + 2}, "F", views::SectionKind::Cut)};
    Sheet s(sd, m);
    const double Za = L.axisHeight, H = L.joint, aw = L.aw(), Xb = L.boss, hb = var(p, "hb"), Xf = var(p, "Xf"), y0 = var(p, "y0"),
                 y1 = var(p, "y1"), rf = var(p, "rf"), xp = var(p, "Wo") / 2 + var(p, "ep"), yp0 = var(p, "yA") - var(p, "ep");
    s.dim("Htot", "F", DimensionType::Vertical, {s.v(-xp, yp0, -hb), s.v(-Xf + rf, y0, H)}, 18);
    s.dim("hb", "F", DimensionType::Vertical, {s.v(xp, yp0, -hb), s.v(xp, yp0, 0)}, -10);
    s.dim("Wf", "T", DimensionType::Horizontal, {s.v(-Xf, y0 + rf, H), s.v(Xf, y0 + rf, H)}, -14);
    s.dim("Lf", "T", DimensionType::Vertical, {s.v(-Xf, y0 + rf, H), s.v(-Xf, y1 - rf, H)}, 14);
    s.dim("aw", "L", DimensionType::Horizontal, {s.circ(-Xb, 0, Za, 26), s.circ(-Xb, aw, Za, 31)}, 52).def.textOverride = "<>±0,03";
    s.dim("Za", "L", DimensionType::Vertical, {s.v(-xp, yp0, -hb), s.circ(-Xb, 0, Za, 26)}, 14);
    s.dim("Dp", "L", DimensionType::Diameter, {s.circ(-Xb, 0, Za, 26)}, 0, "H7").def.leaderAngleDeg = 135;
    s.dim("Dw", "L", DimensionType::Diameter, {s.circ(-Xb, aw, Za, 31)}, 0, "H7").def.leaderAngleDeg = 45;
    auto& bolt = s.dim("db", "T", DimensionType::Diameter, {s.circ(var(p, "bx"), var(p, "by0"), H, 5.5)}, 0);
    bolt.def.prefix = "4 отв. ";
    bolt.def.leaderAngleDeg = 300;
    auto& pin = s.dim("dp", "T", DimensionType::Diameter, {s.circ(-var(p, "px"), var(p, "by0"), H, 4)}, 0, "H7");
    pin.def.prefix = "2 отв. ";
    pin.def.leaderAngleDeg = 240;
    s.threadNote("TH1", "L", [&](const modeling::ThreadInfo& ti) {
        return std::abs(ti.thread.nominal - 6) < 1e-9 && ti.thread.entry.X() < 0;
    }, "6H", 8);
    const TopoDS_Shape joint = detail::lineEdge(s.shape(), gp_Pnt(-Xf + rf, y0, H), gp_Pnt(Xf - rf, y0, H));
    const TopoDS_Shape base = detail::lineEdge(s.shape(), gp_Pnt(-xp, yp0, -hb), gp_Pnt(xp, yp0, -hb));
    s.roughness("Ra1", "F", joint, "3,2", 0.3);
    s.roughness("Ra2", "F", base, "6,3", 0.7);
    s.datum("DA", "F", base, "А", 0.3);
    s.tolerance("T1", "F", joint, GeometricCharacteristic::Parallelism, "0,05", {"А"}, 0.7);
    sd.roughnessRemainder = annotations::RoughnessSpec{annotations::RoughnessMethod::NoMaterialRemoval, "", "", "", 2.5};
    sd.technicalRequirements = {
        "Отливка по ГОСТ 26645-85, класс точности 9-0-0-9. Формовочные уклоны 1°.",
        "Неуказанные литейные радиусы 3...5 мм.",
        "Отверстия под подшипники обрабатывать совместно с крышкой корпуса не требуется (разъём выше осей).",
        "Резьбовые отверстия М6-6H глубиной 14 — 8 шт. (по 4 на торцах бобышек), сливное М16×1,5-6H.",
        "Неуказанные предельные отклонения размеров: отверстий H14, валов h14, остальных ±IT14/2.",
    };
}

// ---------------------------------------------------------------- крышка корпуса

void coverSheet(DrawingDocument& d, const PartEntry& p) {
    const ModelSnapshot m = snap(p);
    SheetDef& sd = d.sheets()[0];
    ViewDef f = view("F", ViewKind::Base), t = view("T", ViewKind::Projected, "F", views::SheetSide::Below),
            l = view("L", ViewKind::Projected, "F", views::SheetSide::RightOf);
    f.hidden = t.hidden = l.hidden = false;
    sd.views = {f, t, l};
    Sheet s(sd, m);
    const double Xf = var(p, "Xf"), y0 = var(p, "y0"), rf = var(p, "rf"), tf = var(p, "tf"), bx = var(p, "bx"), by0 = var(p, "by0"),
                 by1 = var(p, "by1");
    s.dim("tf", "F", DimensionType::Vertical, {s.v(-Xf + rf, y0, 0), s.v(-Xf + rf, y0, tf)}, 10);
    s.dim("bx", "T", DimensionType::Horizontal, {s.circ(-bx, by0, tf, 5.5), s.circ(bx, by0, tf, 5.5)}, -22);
    s.dim("by", "T", DimensionType::Vertical, {s.circ(bx, by0, tf, 5.5), s.circ(bx, by1, tf, 5.5)}, -30);
    auto& bolt = s.dim("db", "T", DimensionType::Diameter, {s.circ(bx, by1, tf, 5.5)}, 0);
    bolt.def.prefix = "4 отв. ";
    bolt.def.leaderAngleDeg = 60;
    s.threadNote("TH1", "T", [](const modeling::ThreadInfo& ti) { return std::abs(ti.thread.nominal - 16) < 1e-9; }, "6H", 10);
    const TopoDS_Shape joint = detail::lineEdge(s.shape(), gp_Pnt(-Xf + rf, y0, 0), gp_Pnt(Xf - rf, y0, 0));
    s.roughness("Ra1", "F", joint, "3,2", 0.7);
    s.datum("DA", "F", joint, "А", 0.3);
    s.tolerance("T1", "F", joint, GeometricCharacteristic::Flatness, "0,05", {}, 0.5);
    sd.roughnessRemainder = annotations::RoughnessSpec{annotations::RoughnessMethod::NoMaterialRemoval, "", "", "", 2.5};
    sd.technicalRequirements = {
        "Отливка по ГОСТ 26645-85, класс точности 9-0-0-9. Формовочные уклоны 1°.",
        "Неуказанные литейные радиусы 3...5 мм.",
        "Отверстия под штифты 2 отв. ⌀8H7 сверлить совместно с корпусом.",
        "Неуказанные предельные отклонения размеров: отверстий H14, валов h14, остальных ±IT14/2.",
    };
}

// ---------------------------------------------------------------- сборочный чертёж

void assemblySheet(DrawingDocument& d, const Gearbox& g, const Layout& L) {
    auto snapR = snapshotAssembly(g.doc);
    if (!snapR.ok()) fail("assembly snapshot: " + snapR.error().message);
    const ModelSnapshot& m = snapR.value();
    d.model().kind = ModelKind::Assembly;
    d.model().path = "gearbox.bcasm.json";
    SheetDef sd;
    sd.id = "S1";
    sd.format = {GostFormat::A1, 1, Orientation::Landscape};
    sd.title = title();
    const double Za = L.axisHeight, aw = L.aw();
    // Главный вид и разрез А–А по осям валов на месте вида сверху (в проекционной связи) —
    // основные изображения; разрезы Б–Б (по оси колеса), В–В (по штифту), Г–Г (по оси
    // вала-шестерни) — 1:2,5 колонкой справа, их секущие плоскости обозначены на разрезе А–А.
    ViewDef f = view("F", ViewKind::Base);
    f.hidden = false;
    ViewDef a = section("A", {0, 0, 1}, {1, 0, 0}, {0, 0, Za}, "F", views::SectionKind::Cut);
    a.parent = "F";
    a.side = views::SheetSide::Below;
    a.autoBalloons = true;
    ViewDef b = section("B", {0, -1, 0}, {1, 0, 0}, {0, aw, 0}, "A", views::SectionKind::Cut);
    ViewDef c = section("C", {0, -1, 0}, {1, 0, 0}, {0, -68, 0}, "A", views::SectionKind::Cut);
    ViewDef dv = section("D", {0, -1, 0}, {1, 0, 0}, {0, 0, 0}, "A", views::SectionKind::Cut);
    b.scale = c.scale = dv.scale = views::ScaleRatio{2, 5};
    sd.views = {f, a, b, c, dv};
    Sheet s(sd, m);
    const double H = L.joint + L.gasket;
    // Концы валов (вершины шва фасок), верх заливной пробки, опорная плита.
    const auto inEnd = s.v(-110, 0, Za + 9), outEnd = s.v(112, aw, Za + 11.5);
    s.dim("Lx", "F", DimensionType::Horizontal, {inEnd, outEnd}, -40);
    s.dim("Hz", "F", DimensionType::Vertical, {s.v(-73, -78, -14), s.v(22 / std::sqrt(3.0), 40, H + 40 + 12)}, 30);
    s.dim("Ly", "A", DimensionType::Vertical, {s.v(-73, -78, -14), s.v(-73, 185, -14)}, 30);
    s.dim("aw", "A", DimensionType::Vertical, {inEnd, outEnd}, -20).def.textOverride = "<>±0,03";
    sd.technicalRequirements = {
        "* Размеры для справок.",
        "Плоскость разъёма покрыть герметиком; прокладку ставить на герметик.",
        "Валы должны проворачиваться от руки плавно, без заеданий.",
        "Залить масло И-Г-А-46 ГОСТ 17479.4-87 до уровня, при котором колесо погружено на высоту зуба.",
        "Зубчатые венцы показаны делительными цилиндрами, подшипники — упрощёнными кольцами d×D×B.",
    };
    d.sheets().push_back(std::move(sd));
}

}  // namespace

Result<DrawingSet> defineDrawings(const std::vector<PartEntry>& parts, const Gearbox& g, const Layout& L) {
    return detail::guard([&]() -> Result<DrawingSet> {
        DrawingSet set;
        auto part = [&](const std::string& key, SheetFormat f, auto fill) {
            const PartEntry* p = findPart(parts, key);
            if (!p) fail("part " + key + " missing");
            DrawingSet::PartDrawing pd{key, key + ".bcdrw.json", partDrawing(*p, f)};
            fill(pd.doc, *p);
            set.parts.push_back(std::move(pd));
        };
        const SheetFormat a3{GostFormat::A3, 1, Orientation::Landscape};
        part("housing", SheetFormat{GostFormat::A1, 1, Orientation::Landscape},
             [&](DrawingDocument& d, const PartEntry& p) { housingSheet(d, p, L); });
        part("cover", SheetFormat{GostFormat::A2, 1, Orientation::Landscape}, coverSheet);
        part("shaft_in", a3, shaftInSheet);
        part("shaft_out", a3, shaftOutSheet);
        part("wheel", a3, wheelSheet);
        part("cap_out_t", a3, capSheet);
        assemblySheet(set.assembly, g, L);
        set.specification.model().kind = ModelKind::Assembly;
        set.specification.model().path = "gearbox.bcasm.json";
        set.specification.title() = title();
        set.specification.settings().assemblyDrawingFormat = "А1";
        for (const auto& p : parts) set.specification.settings().formats[p.designation] = "А3";
        set.specification.settings().formats["РЦ.080.00.001"] = "А1";
        set.specification.settings().formats["РЦ.080.00.002"] = "А2";
        return set;
    });
}

Result<UpdateSummary> updateDrawings(DrawingSet& set, const std::vector<PartEntry>& parts, const Gearbox& g,
                                     views::ViewCache& cache) {
    UpdateSummary out;
    auto collect = [&](const std::string& file, const UpdateReport& r) {
        for (const auto& x : r.dangling) out.dangling.push_back(file + ": " + x);
        for (const auto& x : r.stale) out.stale.push_back(file + ": " + x);
        for (const auto& x : r.warnings) out.warnings.push_back(file + ": " + x);
        out.dimensions += r.dimensions.size();
        for (const auto& b : r.balloons) out.balloons += b.placed.size();
    };
    for (auto& pd : set.parts) {
        const PartEntry* p = findPart(parts, pd.key);
        if (!p) return Error{ErrorCode::InvalidInput, "part " + pd.key + " missing"};
        auto m = snapshotPart(*p->doc, pd.doc.model().properties);
        if (!m.ok()) return m.error();
        auto r = pd.doc.update(m.value(), cache);
        if (!r.ok()) return Error{r.error().code, pd.file + ": " + r.error().message};
        collect(pd.file, r.value());
    }
    auto m = snapshotAssembly(g.doc);
    if (!m.ok()) return m.error();
    auto su = set.specification.update(m.value());
    if (!su.ok()) return Error{su.error().code, "specification: " + su.error().message};
    auto r = set.assembly.update(m.value(), cache, set.specification.specification());
    if (!r.ok()) return Error{r.error().code, "gearbox.bcdrw.json: " + r.error().message};
    // Составные части, не видимые в разрезе А–А (крышка корпуса, прокладка, штифты, болты
    // фланца, заливная пробка), получают ручные позиции на главном виде: полки — колонной слева
    // от вида, шаг 10 мм (ГОСТ 2.109). Один раз: ручные позиции сохраняются в файле.
    if (set.assembly.sheets()[0].balloons.empty()) {
        std::vector<assembly_v2::DefinitionId> missing;
        for (const BalloonReport& b : r.value().balloons) missing.insert(missing.end(), b.withoutBalloon.begin(), b.withoutBalloon.end());
        const ComputedSheet& cs = set.assembly.computedSheets()[0];
        // Порядок видов для ручных позиций: главный, разрезы Б–Б (по оси колеса), В–В (по штифту),
        // Г–Г (по оси вала-шестерни), вид сверху.
        std::map<std::string, std::pair<const ComputedView*, double>> views;  // вид → (вид, следующая полка y)
        for (const auto& v : cs.views) views[v.id] = {&v, v.sheetView.sheetBounds().maxY};
        // Кандидаты — видимые прямые линии детали по убыванию длины; ссылка на линию, совпадающую
        // с линией соседней детали, неоднозначна (позиция висячая) — берётся следующая. Полки
        // колонной слева от вида в порядке высоты точек выносок (меньше пересечений линий-выносок).
        struct Pending {
            std::string view;
            double targetY = 0;
            std::vector<std::uint32_t> edges;
        };
        std::vector<Pending> pending;
        for (assembly_v2::DefinitionId def : missing) {
            for (const char* vid : {"F", "B", "C", "D"}) {
                auto it = views.find(vid);
                if (it == views.end()) continue;
                const ComputedView* cv = it->second.first;
                std::vector<std::tuple<double, std::uint32_t, double>> edges;
                for (const views::ViewEdge& e : cv->sheetView.view.edges) {
                    if (e.visibility != views::Visibility::Visible || !e.edgeIndex || *e.edgeIndex >= m.value().edgeOwner.size()) continue;
                    if (m.value().edgeOwner[*e.edgeIndex] != def) continue;
                    const auto* l = std::get_if<LineSeg>(&e.curve);
                    if (!l) continue;
                    const Point2 mid = cv->sheetView.toSheet(Point2{(l->a.x + l->b.x) / 2, (l->a.y + l->b.y) / 2});
                    edges.push_back({std::hypot(l->b.x - l->a.x, l->b.y - l->a.y), *e.edgeIndex, mid.y});
                }
                if (edges.empty()) continue;
                std::sort(edges.begin(), edges.end(), [](const auto& x, const auto& z) { return std::get<0>(x) > std::get<0>(z); });
                Pending pd{vid, std::get<2>(edges.front()), {}};
                for (const auto& [len, idx, y] : edges)
                    if (std::find(pd.edges.begin(), pd.edges.end(), idx) == pd.edges.end()) pd.edges.push_back(idx);
                pending.push_back(std::move(pd));
                break;
            }
        }
        std::stable_sort(pending.begin(), pending.end(), [](const Pending& x, const Pending& z) {
            return x.view != z.view ? x.view < z.view : x.targetY > z.targetY;
        });
        std::vector<std::vector<std::uint32_t>> candidates;
        int k = 0;
        for (Pending& pd : pending) {
            auto& slot = views[pd.view];
            BalloonItem item;
            item.id = "P" + std::to_string(++k);
            item.view = pd.view;
            item.at = Point2{slot.first->sheetView.sheetBounds().minX - 25.0, slot.second};
            slot.second -= 10.0;
            set.assembly.sheets()[0].balloons.push_back(item);
            candidates.push_back(std::move(pd.edges));
        }
        {
            std::vector<std::size_t> pick(candidates.size(), 0);
            for (int pass = 0; pass < 8; ++pass) {
                for (std::size_t i = 0; i < candidates.size(); ++i) {
                    auto er = elementReference(m.value(), modeling::ElementKind::Edge, candidates[i][pick[i]]);
                    if (!er.ok()) return er.error();
                    set.assembly.sheets()[0].balloons[i].edge = er.value();
                }
                r = set.assembly.update(m.value(), cache, set.specification.specification());
                if (!r.ok()) return Error{r.error().code, "gearbox.bcdrw.json: " + r.error().message};
                bool again = false;
                const auto& mb = set.assembly.computedSheets()[0].manualBalloons;
                for (std::size_t i = 0; i < candidates.size(); ++i)
                    if (!mb[i] && pick[i] + 1 < candidates[i].size()) ++pick[i], again = true;
                if (!again) break;
            }
        }
    }
    collect("gearbox.bcdrw.json", r.value());
    for (const auto& b : set.assembly.computedSheets()[0].manualBalloons)
        if (b) ++out.balloons;
    return out;
}

}  // namespace bendcore::examples::gearbox
