// Сборка редуктора РЦ-80 (docs/assembly/ASSEMBLY_DOCUMENT.md): определения деталей с источниками
// <key>.bcpart.json, вхождения в положениях, уже удовлетворяющих сопряжениям, сопряжения и
// крепёж bendcore_fasteners (insertFastenerStacks). Степени свободы: валы вращаются в
// подшипниках; крышки, колесо, шпонки, втулка, прокладка, крышка корпуса жёстко связаны;
// осесимметричные детали (подшипники-кольца, крепёж, штифты, пробки и их прокладки) сохраняют
// поворот вокруг своей оси — он не меняет геометрию сборки и учитывается в ожидаемом числе
// степеней свободы.

#include "gearbox_internal.hpp"

#include <bendcore/assembly_v2/fasteners.hpp>

#include <gp.hxx>

#include <cmath>

namespace bendcore::examples::gearbox {

using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::fasteners;
using namespace detail;

namespace {

constexpr double kPi = 3.14159265358979323846;

template <class T>
T take(kernel::Result<T> r, const std::string& what) {
    if (!r.ok()) fail(what + ": " + r.error().message);
    return std::move(r).value();
}

class Builder {
public:
    explicit Builder(Gearbox& g) : g_(g) {}

    // Индекс грани определения по геометрическому зонду в локальной СК детали.
    GeometryRef face(const std::string& occ, const TopoDS_Shape& f) const {
        const OccurrenceId id = g_.occ.at(occ);
        const Definition& d = g_.doc->definition(g_.doc->findOccurrence(id)->definition);
        auto idx = d.topology->faceIndex(f);
        if (!idx) fail("face of '" + occ + "' is not in its definition topology");
        return GeometryRef{{id}, ElementRef::face(*idx), EntityUse::Auto};
    }
    GeometryRef named(const std::string& occ, const std::string& role) const {
        return GeometryRef{{g_.occ.at(occ)}, ElementRef::named(ElementKind::Face, faceName(role)), EntityUse::Auto};
    }
    const TopoDS_Shape& shapeOf(const std::string& occ) const {
        return g_.doc->definition(g_.doc->findOccurrence(g_.occ.at(occ))->definition).shape;
    }
    OccurrenceId place(const std::string& occ, const std::string& def, const Transform& t) {
        const OccurrenceId id = take(g_.doc->addOccurrence(g_.defs.at(def), t, occ), "occurrence " + occ);
        g_.occ[occ] = id;
        return id;
    }
    void mate(MateSpec m, const std::string& name) {
        m.name = name;
        take(g_.doc->addMate(std::move(m)), "mate " + name);
    }
    void rigid(const std::string& a, const std::string& b) { mate(MateSpec::rigid(g_.occ.at(a), g_.occ.at(b)), b + " rigid to " + a); }

private:
    Gearbox& g_;
};

Transform at(double x, double y, double z) { return Transform::fromTranslation(Vec3(x, y, z)); }

}  // namespace

Result<Gearbox> buildAssembly(const std::vector<PartEntry>& parts, const std::vector<StandardPart>& standard,
                              const Layout& L) {
    return guard([&]() -> Result<Gearbox> {
        Gearbox g;
        g.doc = std::make_shared<AssemblyDocument>("Редуктор цилиндрический");
        take(g.doc->setProperties(g.doc->root(), {"РЦ.080.00.000", "Редуктор цилиндрический", "", std::nullopt, std::nullopt}), "root properties");

        std::map<std::string, const modeling::PartDocument*> sources;
        for (const auto& p : parts) sources[p.file] = p.doc.get();
        for (const auto& s : standard) sources[s.key + ".bcpart.json"] = s.doc.get();
        const PartResolver resolver = [&sources](const std::string& src) -> kernel::Result<PartData> {
            auto it = sources.find(src);
            if (it == sources.end()) return Error{ErrorCode::InvalidInput, "unknown part source '" + src + "'"};
            return partDataFromDocument(*it->second);
        };
        for (const auto& p : parts)
            g.defs[p.key] = take(g.doc->addPartFromSource(p.key, p.file, resolver, {p.designation, p.name, p.material, p.density, std::nullopt}),
                                 "part " + p.key);
        for (const auto& s : standard)
        {
            // Подшипники в разрезах сборки рассекаются (упрощённое изображение ГОСТ 2.420 — сечение
            // колец); остальные стандартные изделия — по правилу по умолчанию (не рассекаются).
            const std::optional<bool> cut = s.key.rfind("bearing", 0) == 0 ? std::optional<bool>(true) : std::nullopt;
            g.defs[s.key] = take(g.doc->addPartFromSource(s.key, s.key + ".bcpart.json", resolver, {"", s.name, s.material, s.density, cut}),
                                 "part " + s.key);
        }
        auto fastener = [&](const std::string& key, FastenerSpec spec) {
            g.defs[key] = take(addFastenerPart(*g.doc, take(makeFastener(spec), "fastener " + key)), "fastener " + key);
        };
        fastener("bolt6", {FastenerStandard::Gost7798, 6, 20});
        fastener("spring6", {FastenerStandard::Gost6402, 6, 0});
        fastener("bolt10", {FastenerStandard::Gost7798, 10, 45});
        fastener("washer10", {FastenerStandard::Gost11371, 10, 0});
        fastener("nut10", {FastenerStandard::Gost5915, 10, 0});
        fastener("pin8", {FastenerStandard::Gost3128, 8, 25});

        Builder b(g);
        const double Za = L.axisHeight, aw = L.aw(), H = L.joint, Xb = L.boss, s = L.spigot;

        // ---- корпус, прокладка, крышка корпуса
        b.place("housing", "housing", Transform::identity());
        take(g.doc->setFixed(g.occ["housing"], true), "fix housing");
        b.place("gasket", "gasket", at(0, 0, H));
        b.rigid("housing", "gasket");
        b.place("cover", "cover", at(0, 0, H + L.gasket));
        b.rigid("housing", "cover");

        // ---- крышки подшипников: справа (x = +Xb) без поворота, слева — поворот 180° вокруг Z
        const Transform flip = Transform::fromAxisAngle(Vec3::UnitZ(), kPi);
        auto capAt = [&](double side, double y) {
            return side > 0 ? at(Xb, y, Za) : Transform::fromTranslation(Vec3(-Xb, y, Za)) * flip;
        };
        b.place("cap_in_l", "cap_in_t", capAt(-1, 0));
        b.place("cap_in_r", "cap_in_b", capAt(1, 0));
        b.place("cap_out_l", "cap_out_b", capAt(-1, aw));
        b.place("cap_out_r", "cap_out_t", capAt(1, aw));
        for (const char* c : {"cap_in_l", "cap_in_r", "cap_out_l", "cap_out_r"}) b.rigid("housing", c);

        // ---- подшипники: кольцо x ∈ [0, B]; наружный торец касается пояска крышки
        struct Brg {
            const char* occ;
            const char* def;
            const char* cap;
            double y, B, D, side;
        };
        const std::vector<Brg> brgs{{"bearing_in_l", "bearing205", "cap_in_l", 0, 15, 52, -1},
                                    {"bearing_in_r", "bearing205", "cap_in_r", 0, 15, 52, 1},
                                    {"bearing_out_l", "bearing206", "cap_out_l", aw, 16, 62, -1},
                                    {"bearing_out_r", "bearing206", "cap_out_r", aw, 16, 62, 1}};
        const double xo = Xb - s;  // наружный торец подшипника
        for (const Brg& r : brgs) {
            b.place(r.occ, r.def, at(r.side > 0 ? xo - r.B : -xo, r.y, Za));
            const TopoDS_Shape& hs = b.shapeOf("housing");
            const TopoDS_Shape bore = cylinderFaceAt(hs, r.D / 2, gp_Pnt(0, r.y, Za), gp::DX(), gp_Pnt(r.side * 55, r.y, Za + r.D / 2));
            const TopoDS_Shape outer = cylinderFace(b.shapeOf(r.occ), r.D / 2, gp::Origin(), gp::DX());
            b.mate(MateSpec::concentric(b.face(r.occ, outer), b.face("housing", bore)), std::string(r.occ) + " in bore");
            const TopoDS_Shape end = r.side > 0 ? planarFace(b.shapeOf(r.occ), gp::DX(), r.B) : planarFace(b.shapeOf(r.occ), -gp::DX(), 0);
            const TopoDS_Shape spig = planarFace(b.shapeOf(r.cap), -gp::DX(), s);
            b.mate(MateSpec::coincident(b.face(r.occ, end), b.face(r.cap, spig), Alignment::AntiAligned), std::string(r.occ) + " to cap");
        }

        // ---- валы: посадка в правый подшипник, заплечик к его внутреннему торцу
        const PartEntry* sin = findPart(parts, "shaft_in");
        const PartEntry* sout = findPart(parts, "shaft_out");
        const double x6 = sin->doc->variableValue("x6").value(), x7 = sin->doc->variableValue("x7").value();
        b.place("shaft_in", "shaft_in", at(xo - x7, 0, Za));
        {
            const TopoDS_Shape seat = cylinderFaceAt(b.shapeOf("shaft_in"), 12.5, gp::Origin(), gp::DX(), gp_Pnt(x7 - 5, 0, 12.5));
            const TopoDS_Shape bore = cylinderFace(b.shapeOf("bearing_in_r"), 12.5, gp::Origin(), gp::DX());
            b.mate(MateSpec::concentric(b.face("shaft_in", seat), b.face("bearing_in_r", bore)), "shaft_in in bearing");
            const TopoDS_Shape shoulder = planarFace(b.shapeOf("shaft_in"), gp::DX(), x6);
            b.mate(MateSpec::coincident(b.face("shaft_in", shoulder), b.face("bearing_in_r", planarFace(b.shapeOf("bearing_in_r"), -gp::DX(), 0)),
                                        Alignment::AntiAligned),
                   "shaft_in shoulder");
        }
        const double o4 = sout->doc->variableValue("x4").value(), o5 = sout->doc->variableValue("x5").value();
        const double outX0 = xo - o5;  // глобальная x левого конца выходного вала
        b.place("shaft_out", "shaft_out", at(outX0, aw, Za));
        {
            const TopoDS_Shape seat = cylinderFaceAt(b.shapeOf("shaft_out"), 15, gp::Origin(), gp::DX(), gp_Pnt(o5 - 5, 0, 15));
            const TopoDS_Shape bore = cylinderFace(b.shapeOf("bearing_out_r"), 15, gp::Origin(), gp::DX());
            b.mate(MateSpec::concentric(b.face("shaft_out", seat), b.face("bearing_out_r", bore)), "shaft_out in bearing");
            const TopoDS_Shape shoulder = planarFace(b.shapeOf("shaft_out"), gp::DX(), o4);
            b.mate(MateSpec::coincident(b.face("shaft_out", shoulder),
                                        b.face("bearing_out_r", planarFace(b.shapeOf("bearing_out_r"), -gp::DX(), 0)), Alignment::AntiAligned),
                   "shaft_out shoulder");
        }

        // ---- колесо, втулка, шпонки — жёстко на валах
        const double o2 = sout->doc->variableValue("x2").value(), o1 = sout->doc->variableValue("x1").value();
        const double hub = 20.0;  // половина длины ступицы (переменная lh колеса)
        b.place("wheel", "wheel", at(outX0 + o2 - hub, aw, Za));
        b.rigid("shaft_out", "wheel");
        b.place("spacer", "spacer", at(outX0 + o1, aw, Za));
        b.rigid("shaft_out", "spacer");
        const double xkw = sout->doc->variableValue("xkw").value(), xko = sout->doc->variableValue("xko").value();
        b.place("key_wheel", "key10", at(outX0 + xkw, aw, Za + 35.0 / 2 - 5));
        b.rigid("shaft_out", "key_wheel");
        b.place("key_out", "key8", at(outX0 + xko, aw, Za + 25.0 / 2 - 4));
        b.rigid("shaft_out", "key_out");
        const double inX = xo - x7;
        b.place("key_in", "key6", at(inX + sin->doc->variableValue("L0").value() / 2, 0, Za + 20.0 / 2 - 3.5));
        b.rigid("shaft_in", "key_in");

        // ---- штифты: в отверстиях фланцев, торец End1 в плоскости низа фланца корпуса
        const double tf = 12.0, by0 = -53 - 30 + 15, by1 = -53 + 213 + 30 - 15;
        for (const auto& [occ, x, y] : std::vector<std::tuple<std::string, double, double>>{{"pin_1", -15, by0}, {"pin_2", 15, by1}}) {
            b.place(occ, "pin8", at(x, y, H - tf));
            const TopoDS_Shape hole = cylinderFaceAt(b.shapeOf("housing"), 4, gp_Pnt(x, y, 0), gp::DZ(), gp_Pnt(x + 4, y, H - tf / 2));
            b.mate(MateSpec::concentric(b.named(occ, roles::kShank), b.face("housing", hole)), occ + " in hole");
            const TopoDS_Shape under = planarFaceAt(b.shapeOf("housing"), -gp::DZ(), gp_Pnt(x + 6, y, H - tf));
            b.mate(MateSpec::coincident(b.named(occ, std::string(roles::kEnd) + "1"), b.face("housing", under), Alignment::Aligned), occ + " end");
        }

        // ---- пробки с прокладками: сливная (торец бобышки y = 166) и заливная (верх крышки)
        const double yd = -53 + 213 + 6, zd = 18, Hc = 40, yv = 40;
        const Transform toY = Transform::fromAxisAngle(Vec3::UnitX(), -kPi / 2);  // локальная Z → глобальная +Y
        struct Plug {
            std::string tag;
            std::string host;
            Transform gasket, plug;
            TopoDS_Shape tap, seat;
        };
        std::vector<Plug> plugs{
            {"drain", "housing", Transform::fromTranslation(Vec3(0, yd, zd)) * toY, Transform::fromTranslation(Vec3(0, yd + 2, zd)) * toY,
             cylinderFaceAt(b.shapeOf("housing"), 7.25, gp_Pnt(0, 0, zd), gp::DY(), gp_Pnt(7.25, yd - 4, zd)), planarFace(b.shapeOf("housing"), gp::DY(), yd)},
            {"filler", "cover", at(0, yv, H + L.gasket + Hc), at(0, yv, H + L.gasket + Hc + 2),
             cylinderFaceAt(b.shapeOf("cover"), 7.25, gp_Pnt(0, yv, 0), gp::DZ(), gp_Pnt(7.25, yv, Hc - 4)), planarFace(b.shapeOf("cover"), gp::DZ(), Hc)}};
        for (const Plug& p : plugs) {
            const std::string go = "plug_gasket_" + p.tag, po = "plug_" + p.tag;
            b.place(go, "plug_gasket", p.gasket);
            b.mate(MateSpec::concentric(b.face(go, cylinderFace(b.shapeOf(go), 8.25, gp::Origin(), gp::DZ())), b.face(p.host, p.tap)), go + " axis");
            b.mate(MateSpec::coincident(b.face(go, planarFace(b.shapeOf(go), -gp::DZ(), 0)), b.face(p.host, p.seat), Alignment::AntiAligned),
                   go + " seat");
            b.place(po, "plug", p.plug);
            b.mate(MateSpec::concentric(b.face(po, cylinderFace(b.shapeOf(po), 8, gp::Origin(), gp::DZ())), b.face(p.host, p.tap)), po + " axis");
            b.mate(MateSpec::coincident(b.face(po, planarFace(b.shapeOf(po), -gp::DZ(), 0)), b.face(go, planarFace(b.shapeOf(go), gp::DZ(), 2)),
                                        Alignment::AntiAligned),
                   po + " seat");
            g.threadedScrews.push_back(g.occ[po]);
        }
        // Резьба пробки М16×1,5 в отверстии под резьбу (сверло D − P = 14,5): кольцо d² − 14,5² на
        // длине захода не больше длины стержня 14.
        g.threadAllowance[g.defs["plug"]] = kPi / 4 * (16.0 * 16.0 - 14.5 * 14.5) * 14.0;

        // ---- болты крышек подшипников М6 с пружинными шайбами (в резьбовые отверстия корпуса)
        double protrusion = 0;
        for (const auto& [cap, side] : std::vector<std::pair<std::string, double>>{
                 {"cap_in_l", -1}, {"cap_in_r", 1}, {"cap_out_l", -1}, {"cap_out_r", 1}}) {
            auto sites = take(findHoleSites(*g.doc, {g.occ[cap]}, Vec3(-side, 0, 0), 3.5), "hole sites of " + cap);
            if (sites.size() != 4) fail(cap + ": expected 4 screw holes, found " + std::to_string(sites.size()));
            auto placed = take(insertFastenerStacks(*g.doc, {g.defs["bolt6"], {g.defs["spring6"]}, {}, std::nullopt}, sites), "screws of " + cap);
            for (const StackPlacement& sp : placed) {
                g.threadedScrews.push_back(sp.screw);
                protrusion = std::max(protrusion, sp.protrusion);
            }
        }
        // Резьба М6 (сверло 5): заход болта в отверстие не длиннее выступа стержня за крышку.
        g.threadAllowance[g.defs["bolt6"]] = kPi / 4 * (6.0 * 6.0 - 5.0 * 5.0) * protrusion;

        // ---- болты фланца разъёма М10 с шайбами и гайками: вход — верх фланца крышки, выход —
        // низ фланца корпуса (пакет крышка + прокладка + корпус).
        auto sites = take(findHoleSites(*g.doc, {g.occ["cover"]}, Vec3(0, 0, -1), 5.5), "flange bolt holes");
        if (sites.size() != 4) fail("expected 4 flange bolt holes, found " + std::to_string(sites.size()));
        for (HoleSite& site : sites) {
            auto e = take(g.doc->worldEntity(site.hole), "flange hole axis");
            const Vec3 p = e.point;
            site.exitFace = b.face("housing", planarFaceAt(b.shapeOf("housing"), -gp::DZ(), gp_Pnt(p.x() + 7, p.y(), H - tf)));
            site.depth = 2 * tf + L.gasket;
        }
        take(insertFastenerStacks(*g.doc, {g.defs["bolt10"], {g.defs["washer10"]}, {g.defs["washer10"]}, g.defs["nut10"]}, sites),
             "flange bolts");

        auto rep = take(g.doc->solve(), "solve");
        if (!rep.success) fail("assembly does not solve: residual " + num(rep.residualNorm));
        // 2 вала + 4 подшипника + 2 штифта + 2 пробки + 2 прокладки пробок + 16 болтов + 16 шайб
        // + 4 болта + 8 шайб + 4 гайки.
        g.expectedDof = 2 + 4 + 2 + 2 + 2 + 16 + 16 + 4 + 8 + 4;
        return g;
    });
}

}  // namespace bendcore::examples::gearbox
