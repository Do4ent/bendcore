// Детали редуктора РЦ-80 — параметрические документы детали (docs/modeling/PART_DOCUMENT.md).
// Размеры — переменные документа; эскизы — выражения этих переменных; ссылки операций — на
// элементы, найденные по геометрии. Локальные СК: корпус, крышка, прокладка — XY — плоскость
// подошвы (корпус) или разъёма (крышка, прокладка), ось Y — вдоль межосевого расстояния; валы,
// колесо, крышки подшипников, втулка, подшипники — ось X; пробки — ось Z.

#include "gearbox_internal.hpp"

#include <gp.hxx>

#include <cmath>
#include <cstdio>

namespace bendcore::examples::gearbox {

using namespace bendcore::modeling;
using namespace detail;

namespace {

constexpr double kPi = 3.14159265358979323846;

FeatureSpec spec(const std::string& type, const std::string& id, std::map<std::string, std::string> params,
                 std::map<std::string, std::vector<TopoReference>> refs, std::map<std::string, std::string> options,
                 std::map<std::string, std::string> links = {}) {
    FeatureSpec s;
    s.type = type;
    s.id = id;
    s.parameters = std::move(params);
    s.references = std::move(refs);
    s.options = std::move(options);
    s.links = std::move(links);
    return s;
}

// Общие размеры корпуса, крышки и прокладки (одни и те же выражения во всех трёх документах).
void commonHousingVars(PartBuilder& b, const Layout& L) {
    b.var("Wo", 96);     // наружная ширина корпуса у подошвы (вдоль осей валов)
    b.var("yA", -53);    // наружная стенка у шестерни
    b.var("Lo", 213);    // наружная длина корпуса у подошвы
    b.var("H", L.joint);
    b.var("al", 1);      // литейный уклон, градусы
    b.var("t", 8);       // стенка
    b.var("ef", 30);     // вылет фланца разъёма по торцам (вдоль Y)
    b.var("efx", 14);    // вылет фланца вдоль осей валов: фланец кончается до торцов бобышек x = ±64
    b.var("Xf", "Wo / 2 + efx");
    b.var("y0", "yA - ef");
    b.var("y1", "yA + Lo + ef");
    // Внутренний контур фланца: полость на плоскости разъёма с запасом 1,5 мм.
    b.var("xi", "Wo / 2 - t + H * tan(al) + 1.5");
    b.var("yi0", "yA + t - H * tan(al) - 1.5");
    b.var("yi1", "yA + Lo - t + H * tan(al) + 1.5");
    b.var("bx", 40);     // болты фланца x = ±bx (на торцевых полках фланца)
    b.var("by0", "y0 + 15");
    b.var("by1", "y1 - 15");
    b.var("px", 15);     // штифты (−px, by0) и (px, by1)
    b.var("db", 11);     // отверстие под болт М10
    b.var("dp", 8);      // отверстие под штифт 8
    b.var("rf", 10);     // скругление углов фланца
}

std::vector<std::pair<std::string, std::string>> flangeBolts() {
    return {{"-bx", "by0"}, {"bx", "by0"}, {"bx", "by1"}, {"-bx", "by1"}};
}
std::vector<std::pair<std::string, std::string>> flangePins() { return {{"-px", "by0"}, {"px", "by1"}}; }

// Скругление четырёх вертикальных рёбер углов фланца z ∈ [z0, z1].
void filletFlangeCorners(PartBuilder& b, const std::string& id, double z0, double z1) {
    const double X = b.value("Xf"), Y0 = b.value("y0"), Y1 = b.value("y1");
    std::vector<TopoDS_Shape> edges;
    for (auto [x, y] : std::vector<std::pair<double, double>>{{-X, Y0}, {X, Y0}, {X, Y1}, {-X, Y1}})
        edges.push_back(lineEdge(b.shape().shape(), gp_Pnt(x, y, z0), gp_Pnt(x, y, z1)));
    b.fillet(id, edges, "rf");
}

// ---------------------------------------------------------------- корпус

std::unique_ptr<PartDocument> housing(const Layout& L) {
    PartBuilder b("housing");
    commonHousingVars(b, L);
    b.var("tf", 12);    // толщина фланца
    b.var("Za", L.axisHeight);
    b.var("aw", L.aw());
    b.var("Xb", L.boss);
    b.var("xe", 45);    // внутренний торец бобышки (в теле стенки)
    b.var("Rp", 40);    // бобышка подшипника шестерни
    b.var("Rw", 45);    // бобышка подшипника колеса
    b.var("Dp", 52);    // отверстие под подшипник 205
    b.var("Dw", 62);    // отверстие под подшипник 206
    b.var("rp", 32);    // окружность резьбовых отверстий крышки подшипника шестерни
    b.var("rw", 37);
    b.var("hb", 14);    // опорная плита
    b.var("ep", 25);
    b.var("dm", 14);    // отверстия под фундаментные болты
    b.var("zr", 10);    // основание рёбер
    b.var("tr", 8);     // толщина рёбер
    b.var("zd", 18);    // ось сливного отверстия
    b.var("Rd", 14);

    b.sketch("Outline", "XY", "0", rect("0", "yA + Lo / 2", "Wo", "Lo"));
    b.extrude("Block", "Outline", {}, {{"distance", "H"}});
    {
        const TopoDS_Shape& s = b.shape().shape();
        const double Wo = b.value("Wo"), yA = b.value("yA"), Lo = b.value("Lo");
        std::vector<TopoReference> sides{b.ref(planarFace(s, -gp::DX(), Wo / 2)), b.ref(planarFace(s, gp::DX(), Wo / 2)),
                                         b.ref(planarFace(s, -gp::DY(), -yA)), b.ref(planarFace(s, gp::DY(), yA + Lo))};
        // Уклон стенок наружу кверху: модель и стержень полости извлекаются вверх.
        b.add(spec("Draft", "Drafts", {{"angle", "al"}}, {{"faces", sides}, {"neutralFace", {b.ref(planarFace(s, -gp::DZ(), 0))}}},
                   {{"neutral", "face"}, {"reversed", "true"}}));
    }
    b.add(spec("Shell", "Walls", {{"thickness", "t"}}, {{"faces", {b.ref(planarFace(b.shape().shape(), gp::DZ(), L.joint))}}},
               {{"direction", "inward"}}));
    b.sketch("FlangeSketch", "XY", "H", frame("-Xf", "y0", "Xf", "y1", "-xi", "yi0", "xi", "yi1"));
    b.extrude("Flange", "FlangeSketch", {{"reversed", "true"}}, {{"distance", "tf"}});
    // Бобышки подшипников: с торца x = Xb внутрь до xe, зеркально относительно YZ. Окружности
    // бобышек пересекаются — один эскиз, область по умолчанию — их объединение.
    b.sketch("BossSketch", "YZ", "Xb", circles({{"0", "Za", "Rp"}, {"aw", "Za", "Rw"}}));
    b.extrude("Bosses", "BossSketch", {{"reversed", "true"}}, {{"distance", "Xb - xe"}});
    b.add(spec("Mirror", "BossesMirror", {}, {}, {{"what", "feature"}, {"plane", "YZ"}}, {{"feature", "Bosses"}}));
    // Рёбра жёсткости под бобышками (в плоскостях осей валов), зеркально.
    for (const auto& [id, y, R] : std::vector<std::tuple<std::string, std::string, std::string>>{
             {"RibP", "0", "Rp"}, {"RibW", "aw", "Rw"}}) {
        SketchBuilder line;
        line.line(line.point("Wo / 2 + zr * tan(al)", "zr", true), line.point("Xb - 3", "Za - " + R, true));
        b.sketch(id + "Line", "XZ", "-(" + y + ")", line.build(), true);
        b.add(spec("Rib", id, {{"thickness", "tr"}}, {}, {}, {{"sketch", id + "Line"}}));
        b.add(spec("Mirror", id + "Mirror", {}, {}, {{"what", "feature"}, {"plane", "YZ"}}, {{"feature", id}}));
    }
    // Отверстия под подшипники — насквозь через обе стенки.
    b.sketch("BoreSketch", "YZ", "0", circles({{"0", "Za", "Dp / 2"}, {"aw", "Za", "Dw / 2"}}));
    b.extrude("Bores", "BoreSketch", {{"mode", "cut"}, {"extent", "throughAllBoth"}}, {});
    // Опорная плита и фундаментные отверстия.
    b.sketch("PlateSketch", "XY", "0", rect("0", "yA + Lo / 2", "Wo + 2 * ep", "Lo + 2 * ep"));
    b.extrude("Plate", "PlateSketch", {{"reversed", "true"}}, {{"distance", "hb"}});
    // Бобышка сливного отверстия на торцевой стенке y = yA + Lo.
    b.sketch("DrainBossSketch", "XZ", "-(yA + Lo + 6)", circles({{"0", "zd", "Rd"}}));
    b.extrude("DrainBoss", "DrainBossSketch", {}, {{"distance", "10"}});
    {
        const double yd = b.value("yA") + b.value("Lo") + 6;
        b.add(spec("ThreadedHole", "DrainTap", {{"nominal", "16"}, {"pitch", "1.5"}, {"threadLength", "0"}, {"x", "0"},
                                                {"y", "yA + Lo + 6"}, {"z", "zd"}},
                   {{"face", {b.ref(planarFace(b.shape().shape(), gp::DY(), yd))}}}, {{"end", "through"}}));
    }
    // Резьбовые отверстия М6 под крышки подшипников (по 4 на каждую) на обоих торцах бобышек
    // x = ±Xb. Эскиз на грани: ось x эскиза — глобальная Y, ось y — нормаль × Y, т. е. +Z на
    // торце x = +Xb и −Z на торце x = −Xb (знак sz).
    {
        const double Xb = b.value("Xb"), Za = b.value("Za");
        for (const auto& [id, side] : std::vector<std::pair<std::string, int>>{{"CapTaps", 1}, {"CapTapsLeft", -1}}) {
            const std::string zs = side > 0 ? "" : "-";
            std::vector<std::pair<std::string, std::string>> pts;
            for (const auto& [yc, r] : std::vector<std::pair<std::string, std::string>>{{"0", "rp"}, {"aw", "rw"}})
                for (const auto& [sy, sz] : std::vector<std::pair<const char*, const char*>>{{"+", "+"}, {"-", "+"}, {"-", "-"}, {"+", "-"}})
                    pts.push_back({yc + " " + sy + " " + r + " * cos(45)", zs + "(Za " + std::string(sz) + " " + r + " * sin(45))"});
            const TopoDS_Shape face = planarFaceAt(b.shape().shape(), side * gp::DX(), gp_Pnt(side * Xb, 0, Za + 30));
            b.faceSketch(id + "Sketch", face, num(side * Xb), "0", "0", points(pts));
            b.add(spec("ThreadedHole", id, {{"nominal", "6"}, {"threadLength", "14"}}, {{"face", {b.ref(face)}}},
                       {{"end", "blind"}}, {{"sketch", id + "Sketch"}}));
        }
        const double rp = b.value("rp"), c = std::cos(kPi / 4);
        for (int side : {1, -1})
            (void)cylinderFaceAt(b.shape().shape(), 2.5, gp_Pnt(0, rp * c, Za + rp * c), gp::DX(),
                                 gp_Pnt(side * (Xb - 5), rp * c, Za + rp * c + 2.5));
    }
    // Отверстия фланца разъёма: болты и штифты.
    {
        const TopoDS_Shape top = planarFaceAt(b.shape().shape(), gp::DZ(), gp_Pnt(b.value("bx"), b.value("by0"), L.joint));
        b.faceSketch("BoltSketch", top, "0", "0", "H", points(flangeBolts()));
        b.add(spec("Hole", "BoltHoles", {{"diameter", "db"}}, {{"face", {b.ref(top)}}}, {{"end", "through"}}, {{"sketch", "BoltSketch"}}));
        const TopoDS_Shape top2 = planarFaceAt(b.shape().shape(), gp::DZ(), gp_Pnt(-b.value("px"), b.value("by0"), L.joint));
        b.faceSketch("PinSketch", top2, "0", "0", "H", points(flangePins()));
        b.add(spec("Hole", "PinHoles", {{"diameter", "dp"}}, {{"face", {b.ref(top2)}}}, {{"end", "through"}}, {{"sketch", "PinSketch"}}));
    }
    // Фундаментные отверстия: грань подошвы плиты z = −hb (ось v эскиза = −Y).
    {
        const TopoDS_Shape bottom = planarFace(b.shape().shape(), -gp::DZ(), b.value("hb"));
        const std::string ux = "Wo / 2 + ep - 12", y0 = "yA - ep + 12", y1 = "yA + Lo + ep - 12";
        b.faceSketch("MountSketch", bottom, "0", "0", "-hb",
                     points({{"-(" + ux + ")", "-(" + y0 + ")"}, {ux, "-(" + y0 + ")"}, {ux, "-(" + y1 + ")"}, {"-(" + ux + ")", "-(" + y1 + ")"}}));
        b.add(spec("Hole", "MountHoles", {{"diameter", "dm"}}, {{"face", {b.ref(bottom)}}}, {{"end", "through"}}, {{"sketch", "MountSketch"}}));
        const double mx = b.value("Wo") / 2 + b.value("ep") - 12, my = b.value("yA") - b.value("ep") + 12;
        (void)cylinderFaceAt(b.shape().shape(), 7, gp_Pnt(mx, my, 0), gp::DZ(), gp_Pnt(mx + 7, my, -7));
    }
    filletFlangeCorners(b, "FlangeRounds", L.joint - b.value("tf"), L.joint);
    return b.release();
}

// ---------------------------------------------------------------- крышка корпуса

std::unique_ptr<PartDocument> cover(const Layout& L) {
    PartBuilder b("cover");
    commonHousingVars(b, L);
    b.var("Hc", 40);
    b.var("tf", 12);
    b.var("xo", "Wo / 2 + H * tan(al)");  // наружный контур = контур корпуса на плоскости разъёма
    b.var("yo0", "yA - H * tan(al)");
    b.var("yo1", "yA + Lo + H * tan(al)");
    b.var("yv", 40);  // заливная пробка (0, yv)
    b.sketch("Outline", "XY", "0", polygon({{"-xo", "yo0"}, {"xo", "yo0"}, {"xo", "yo1"}, {"-xo", "yo1"}}));
    b.extrude("Block", "Outline", {}, {{"distance", "Hc"}});
    {
        const TopoDS_Shape& s = b.shape().shape();
        const double xo = b.value("xo"), yo0 = b.value("yo0"), yo1 = b.value("yo1");
        std::vector<TopoReference> sides{b.ref(planarFace(s, -gp::DX(), xo)), b.ref(planarFace(s, gp::DX(), xo)),
                                         b.ref(planarFace(s, -gp::DY(), -yo0)), b.ref(planarFace(s, gp::DY(), yo1))};
        b.add(spec("Draft", "Drafts", {{"angle", "al"}}, {{"faces", sides}, {"neutralFace", {b.ref(planarFace(s, -gp::DZ(), 0))}}},
                   {{"neutral", "face"}}));
    }
    b.add(spec("Shell", "Walls", {{"thickness", "t"}}, {{"faces", {b.ref(planarFace(b.shape().shape(), -gp::DZ(), 0))}}},
               {{"direction", "inward"}}));
    b.sketch("FlangeSketch", "XY", "0", frame("-Xf", "y0", "Xf", "y1", "-xi", "yi0", "xi", "yi1"));
    b.extrude("Flange", "FlangeSketch", {}, {{"distance", "tf"}});
    {
        const TopoDS_Shape top = planarFaceAt(b.shape().shape(), gp::DZ(), gp_Pnt(b.value("bx"), b.value("by0"), b.value("tf")));
        b.faceSketch("BoltSketch", top, "0", "0", "tf", points(flangeBolts()));
        b.add(spec("Hole", "BoltHoles", {{"diameter", "db"}}, {{"face", {b.ref(top)}}}, {{"end", "through"}}, {{"sketch", "BoltSketch"}}));
        const TopoDS_Shape top2 = planarFaceAt(b.shape().shape(), gp::DZ(), gp_Pnt(-b.value("px"), b.value("by0"), b.value("tf")));
        b.faceSketch("PinSketch", top2, "0", "0", "tf", points(flangePins()));
        b.add(spec("Hole", "PinHoles", {{"diameter", "dp"}}, {{"face", {b.ref(top2)}}}, {{"end", "through"}}, {{"sketch", "PinSketch"}}));
    }
    b.add(spec("ThreadedHole", "FillerTap", {{"nominal", "16"}, {"pitch", "1.5"}, {"threadLength", "0"}, {"x", "0"}, {"y", "yv"}, {"z", "Hc"}},
               {{"face", {b.ref(planarFace(b.shape().shape(), gp::DZ(), b.value("Hc")))}}}, {{"end", "through"}}));
    filletFlangeCorners(b, "FlangeRounds", 0, b.value("tf"));
    return b.release();
}

// ---------------------------------------------------------------- прокладка разъёма

std::unique_ptr<PartDocument> flangeGasket(const Layout& L) {
    PartBuilder b("gasket");
    commonHousingVars(b, L);
    b.var("tg", L.gasket);
    SketchBuilder s;
    s.fixedPolygon({{"-Xf", "y0"}, {"Xf", "y0"}, {"Xf", "y1"}, {"-Xf", "y1"}});
    s.fixedPolygon({{"-xi", "yi0"}, {"xi", "yi0"}, {"xi", "yi1"}, {"-xi", "yi1"}});
    for (const auto& [x, y] : flangeBolts()) s.fixedCircle(x, y, "db / 2");
    for (const auto& [x, y] : flangePins()) s.fixedCircle(x, y, "dp / 2");
    b.sketch("Profile", "XY", "0", s.build());
    b.extrude("Plate", "Profile", {}, {{"distance", "tg"}});
    filletFlangeCorners(b, "Rounds", 0, L.gasket);
    return b.release();
}

// ---------------------------------------------------------------- валы

// Профиль тела вращения вокруг X: точки (x, r) по контуру; первая и последняя — на оси.
SketchData revolveProfile(const std::vector<std::pair<std::string, std::string>>& pts) { return polygon(pts); }

// Канавка для выхода шлифовального круга (ГОСТ 8820, упрощённо: прямоугольная 2 × 0,5).
void groove(PartBuilder& b, const std::string& id, const std::string& x0, const std::string& r) {
    b.sketch(id + "Sketch", "XZ", "0", polygon({{x0, r + " - 0.5"}, {x0 + " + 2", r + " - 0.5"}, {x0 + " + 2", r + " + 1"}, {x0, r + " + 1"}}));
    b.revolve(id, id + "Sketch", "X", "cut");
}

// Шпоночный паз исполнения 1 (фрезой): плоскость z = r, паз l × b с центром xc, глубина t1.
void keyway(PartBuilder& b, const std::string& id, const std::string& r, const std::string& xc, const std::string& l,
            const std::string& w, const std::string& t1) {
    b.add(spec("DatumPlane", id + "Plane", {{"offset", r}}, {}, {{"base", "XY"}}));
    FeatureSpec ks;
    ks.type = "Sketch";
    ks.id = id + "Sketch";
    ks.options = {{"plane", "datum"}};
    ks.links = {{"plane", id + "Plane"}};
    ks.sketch = slot(xc, "0", l, w);
    b.add(ks);
    b.extrude(id, id + "Sketch", {{"mode", "cut"}, {"reversed", "true"}}, {{"distance", t1}});
}

std::unique_ptr<PartDocument> inputShaft(const Layout& L) {
    PartBuilder b("shaft_in");
    b.var("d0", 20);  b.var("L0", 38);   // выходной конец под муфту
    b.var("d1", 22);  b.var("l1", 12);   // под щелевое уплотнение крышки
    b.var("ds", 25);  b.var("ls", 15);   // под подшипник 205
    b.var("dc", 30);  b.var("lc", 27);   // заплечики
    b.var("m", L.module); b.var("z", L.z1);  // венец шестерни: m, z (исходный контур ГОСТ 13755)
    b.var("d", "m * z"); b.var("da", "d + 2 * m"); b.var("b1", 36);  // делительный диаметр, диаметр вершин, ширина
    b.var("x1", "L0"); b.var("x2", "x1 + l1"); b.var("x3", "x2 + ls"); b.var("x4", "x3 + lc");
    b.var("x5", "x4 + b1"); b.var("x6", "x5 + lc"); b.var("x7", "x6 + ls");
    b.var("bk", 6); b.var("tk", 3.5); b.var("lk", 28);  // шпонка 6×6×28: паз b, t1, l
    b.var("c", 1);
    b.sketch("Profile", "XZ", "0",
             revolveProfile({{"0", "0"}, {"0", "d0 / 2"}, {"x1", "d0 / 2"}, {"x1", "d1 / 2"}, {"x2", "d1 / 2"}, {"x2", "ds / 2"},
                             {"x3", "ds / 2"}, {"x3", "dc / 2"}, {"x4", "dc / 2"}, {"x4", "da / 2"}, {"x5", "da / 2"},
                             {"x5", "dc / 2"}, {"x6", "dc / 2"}, {"x6", "ds / 2"}, {"x7", "ds / 2"}, {"x7", "0"}}));
    b.revolve("Body", "Profile", "X");
    // Зубчатый венец на цилиндре вершин: нарезание зубьев (впадины) по всей ширине b1.
    {
        const double x4 = b.value("x4"), x5 = b.value("x5"), ra = b.value("da") / 2;
        const TopoDS_Shape tip = cylinderFaceAt(b.shape().shape(), ra, gp::Origin(), gp::DX(), gp_Pnt(0.5 * (x4 + x5), 0, ra));
        b.add(spec("GearTeeth", "Teeth", {{"module", "m"}, {"teeth", "z"}}, {{"face", {b.ref(tip)}}}, {{"axis", "face"}}));
    }
    groove(b, "GrooveL", "x3 - 2", "ds / 2");
    groove(b, "GrooveR", "x6", "ds / 2");
    keyway(b, "Keyway", "d0 / 2", "L0 / 2", "lk", "bk", "tk");
    const double x2 = b.value("x2"), x7 = b.value("x7");
    b.chamfer("EndChamfers", {circleEdge(b.shape().shape(), gp_Pnt(0, 0, 0), 10), circleEdge(b.shape().shape(), gp_Pnt(x7, 0, 0), 12.5),
                              circleEdge(b.shape().shape(), gp_Pnt(x2, 0, 0), 12.5)},
              "c");
    return b.release();
}

std::unique_ptr<PartDocument> outputShaft(const Layout&) {
    PartBuilder b("shaft_out");
    b.var("ds", 30);   b.var("ls", 16);    // под подшипник 206 (левый)
    b.var("dw", 35);   b.var("lw", 64);    // под колесо и распорную втулку
    b.var("dcl", 42);  b.var("lcl", 10);   // буртик
    b.var("dsh", 36);  b.var("lsh", 14);   // заплечик правого подшипника
    b.var("ls2", 16);                      // под подшипник 206 (правый)
    b.var("dg", 28);   b.var("lg", 12);    // под щелевое уплотнение
    b.var("d0", 25);   b.var("L0", 40);    // выходной конец
    b.var("x1", "ls"); b.var("x2", "x1 + lw"); b.var("x3", "x2 + lcl"); b.var("x4", "x3 + lsh");
    b.var("x5", "x4 + ls2"); b.var("x6", "x5 + lg"); b.var("x7", "x6 + L0");
    b.var("bw", 10); b.var("tw", 5); b.var("lkw", 32); b.var("xkw", "x1 + 44");  // шпонка 10×8×32 под колесом
    b.var("bo", 8);  b.var("to", 4); b.var("lko", 32); b.var("xko", "x6 + L0 / 2");  // шпонка 8×7×32
    b.var("c", 1);
    b.sketch("Profile", "XZ", "0",
             revolveProfile({{"0", "0"}, {"0", "ds / 2"}, {"x1", "ds / 2"}, {"x1", "dw / 2"}, {"x2", "dw / 2"}, {"x2", "dcl / 2"},
                             {"x3", "dcl / 2"}, {"x3", "dsh / 2"}, {"x4", "dsh / 2"}, {"x4", "ds / 2"}, {"x5", "ds / 2"},
                             {"x5", "dg / 2"}, {"x6", "dg / 2"}, {"x6", "d0 / 2"}, {"x7", "d0 / 2"}, {"x7", "0"}}));
    b.revolve("Body", "Profile", "X");
    groove(b, "GrooveL", "x1 - 2", "ds / 2");
    groove(b, "GrooveR", "x4", "ds / 2");
    keyway(b, "KeywayWheel", "dw / 2", "xkw", "lkw", "bw", "tw");
    keyway(b, "KeywayEnd", "d0 / 2", "xko", "lko", "bo", "to");
    const double x7 = b.value("x7");
    b.chamfer("EndChamfers", {circleEdge(b.shape().shape(), gp_Pnt(0, 0, 0), 15), circleEdge(b.shape().shape(), gp_Pnt(x7, 0, 0), 12.5)}, "c");
    return b.release();
}

// ---------------------------------------------------------------- колесо

std::unique_ptr<PartDocument> wheel(const Layout& L) {
    PartBuilder b("wheel");
    b.var("m", L.module); b.var("z", L.z2);  // венец колеса: m, z (исходный контур ГОСТ 13755)
    b.var("d", "m * z");  // делительный диаметр
    b.var("da", "d + 2 * m");  // диаметр вершин
    b.var("bw", 32);     // ширина венца
    b.var("dr", 96);     // внутренний диаметр обода
    b.var("tw", 10);     // диск
    b.var("dh", 56);     // ступица
    b.var("lh", 40);
    b.var("db", 35);     // отверстие
    b.var("bk", 10); b.var("t2", 3.3);  // паз ступицы под шпонку 10×8
    b.var("Dh", 78); b.var("dl", 16); b.var("nl", 6);  // облегчающие отверстия
    b.sketch("Profile", "XZ", "0",
             revolveProfile({{"-lh / 2", "db / 2"}, {"lh / 2", "db / 2"}, {"lh / 2", "dh / 2"}, {"tw / 2", "dh / 2"},
                             {"tw / 2", "dr / 2"}, {"bw / 2", "dr / 2"}, {"bw / 2", "da / 2"}, {"-bw / 2", "da / 2"},
                             {"-bw / 2", "dr / 2"}, {"-tw / 2", "dr / 2"}, {"-tw / 2", "dh / 2"}, {"-lh / 2", "dh / 2"}}));
    b.revolve("Body", "Profile", "X");
    // Зубчатый венец: нарезание зубьев на ободе ⌀da по всей ширине bw (фаски на торцах зубьев
    // операцией не строятся — их нет и на модели).
    {
        const double ra = b.value("da") / 2;
        const TopoDS_Shape tip = cylinderFaceAt(b.shape().shape(), ra, gp::Origin(), gp::DX(), gp_Pnt(0, 0, ra));
        b.add(spec("GearTeeth", "Teeth", {{"module", "m"}, {"teeth", "z"}}, {{"face", {b.ref(tip)}}}, {{"axis", "face"}}));
    }
    b.sketch("KeySketch", "YZ", "0", polygon({{"-bk / 2", "0"}, {"bk / 2", "0"}, {"bk / 2", "db / 2 + t2"}, {"-bk / 2", "db / 2 + t2"}}));
    b.extrude("Keyway", "KeySketch", {{"mode", "cut"}, {"extent", "throughAllBoth"}}, {});
    // Облегчающее отверстие — операция «Отверстие» насквозь с торца диска x = tw / 2.
    {
        const double tw = b.value("tw"), Dh = b.value("Dh");
        const TopoDS_Shape disc = planarFaceAt(b.shape().shape(), gp::DX(), gp_Pnt(tw / 2, 0, Dh / 2));
        b.add(spec("Hole", "Lightening", {{"diameter", "dl"}, {"x", "tw / 2"}, {"y", "0"}, {"z", "Dh / 2"}},
                   {{"face", {b.ref(disc)}}}, {{"end", "through"}}));
    }
    b.add(spec("CircularPattern", "LighteningPattern", {{"count", "nl"}}, {}, {{"axis", "X"}}, {{"feature", "Lightening"}}));
    const double lh = b.value("lh"), rh = b.value("dh") / 2;
    b.chamfer("HubChamfers", {circleEdge(b.shape().shape(), gp_Pnt(-lh / 2, 0, 0), rh), circleEdge(b.shape().shape(), gp_Pnt(lh / 2, 0, 0), rh)}, "1");
    return b.release();
}

// ---------------------------------------------------------------- крышки подшипников

// Фланцевая крышка: фланец Dc × tc (x ∈ [0, tc]), центрирующий поясок D длиной s (x ∈ [−s, 0]),
// 4 отверстия d7 на окружности rb под 45°; сквозная — отверстие dh под щелевое уплотнение,
// глухая — выточка в торце пояска (не касается кольца и торца вала).
std::unique_ptr<PartDocument> bearingCap(const std::string& key, double D, double Dc, double rb, std::optional<double> dh,
                                         const Layout& L) {
    PartBuilder b(key);
    b.var("D", D); b.var("Dc", Dc); b.var("tc", 8); b.var("s", L.spigot); b.var("rb", rb); b.var("dk", 7);
    b.var("c", 1);
    if (dh) {
        b.var("dh", *dh);
        b.sketch("Profile", "XZ", "0",
                 revolveProfile({{"-s", "dh / 2"}, {"-s", "D / 2"}, {"0", "D / 2"}, {"0", "Dc / 2"}, {"tc", "Dc / 2"}, {"tc", "dh / 2"}}));
    } else {
        b.var("rr", "D / 2 - 6");
        b.sketch("Profile", "XZ", "0",
                 revolveProfile({{"-s + 2", "0"}, {"-s + 2", "rr"}, {"-s", "rr"}, {"-s", "D / 2"}, {"0", "D / 2"}, {"0", "Dc / 2"},
                                 {"tc", "Dc / 2"}, {"tc", "0"}}));
    }
    b.revolve("Body", "Profile", "X");
    const double tc = b.value("tc");
    {
        const double rb0 = b.value("rb"), c45 = std::cos(kPi / 4);
        const TopoDS_Shape flange = planarFaceAt(b.shape().shape(), gp::DX(), gp_Pnt(tc, rb0 * c45, rb0 * c45));
        b.add(spec("Hole", "ScrewHole", {{"diameter", "dk"}, {"x", "tc"}, {"y", "rb * cos(45)"}, {"z", "rb * sin(45)"}},
                   {{"face", {b.ref(flange)}}}, {{"end", "through"}}));
    }
    b.add(spec("CircularPattern", "ScrewHoles", {{"count", "4"}}, {}, {{"axis", "X"}}, {{"feature", "ScrewHole"}}));
    b.chamfer("RimChamfer", {circleEdge(b.shape().shape(), gp_Pnt(tc, 0, 0), Dc / 2)}, "c");
    return b.release();
}

// ---------------------------------------------------------------- мелкие детали

std::unique_ptr<PartDocument> spacer() {
    PartBuilder b("spacer");
    b.var("D", 42); b.var("d", 35); b.var("l", 24); b.var("c", 0.5);
    b.sketch("Ring", "YZ", "0", circles({{"0", "0", "D / 2"}, {"0", "0", "d / 2"}}));
    b.extrude("Body", "Ring", {}, {{"distance", "l"}});
    b.chamfer("Chamfers", {circleEdge(b.shape().shape(), gp_Pnt(0, 0, 0), 21), circleEdge(b.shape().shape(), gp_Pnt(24, 0, 0), 21)}, "c");
    return b.release();
}

std::unique_ptr<PartDocument> plug() {
    PartBuilder b("plug");
    b.var("s", 22); b.var("k", 10); b.var("d", 16); b.var("l", 14); b.var("c", 1);
    b.var("R", "s / sqrt(3)");
    std::vector<std::pair<std::string, std::string>> hex;
    for (int i = 0; i < 6; ++i) hex.push_back({"R * cos(" + std::to_string(60 * i) + ")", "R * sin(" + std::to_string(60 * i) + ")"});
    b.sketch("Hex", "XY", "0", polygon(hex));
    b.extrude("Head", "Hex", {}, {{"distance", "k"}});
    b.sketch("ShankSketch", "XY", "0", circles({{"0", "0", "d / 2"}}));
    b.extrude("Shank", "ShankSketch", {{"reversed", "true"}}, {{"distance", "l"}});
    b.chamfer("TipChamfer", {circleEdge(b.shape().shape(), gp_Pnt(0, 0, -14), 8)}, "c");
    return b.release();
}

std::unique_ptr<PartDocument> plugGasket() {
    PartBuilder b("plug_gasket");
    b.var("D", 26); b.var("d", 16.5); b.var("h", 2);
    b.sketch("Ring", "XY", "0", circles({{"0", "0", "D / 2"}, {"0", "0", "d / 2"}}));
    b.extrude("Body", "Ring", {}, {{"distance", "h"}});
    return b.release();
}

std::unique_ptr<PartDocument> ring(const std::string& key, double d, double D, double B) {
    PartBuilder b(key);
    b.var("d", d); b.var("D", D); b.var("B", B);
    b.sketch("Ring", "YZ", "0", circles({{"0", "0", "D / 2"}, {"0", "0", "d / 2"}}));
    b.extrude("Body", "Ring", {}, {{"distance", "B"}});
    return b.release();
}

std::unique_ptr<PartDocument> key(const std::string& id, double bk, double h, double l) {
    PartBuilder b(id);
    b.var("b", bk); b.var("h", h); b.var("l", l);
    b.sketch("Outline", "XY", "0", slot("0", "0", "l", "b"));
    b.extrude("Body", "Outline", {}, {{"distance", "h"}});
    return b.release();
}

PartEntry entry(std::string key, std::string designation, std::string name, std::string material, double density,
                std::unique_ptr<PartDocument> doc) {
    PartEntry e;
    e.key = key;
    e.file = key + ".bcpart.json";
    e.designation = std::move(designation);
    e.name = std::move(name);
    e.material = std::move(material);
    e.density = density;
    e.doc = std::move(doc);
    return e;
}

// Фаза зацепления. Оси шестерни и колеса в сборке — вдоль X без поворота, колесо смещено на aw по
// +Y (линия центров c = +Y). Поворот шестерни вокруг её оси, при котором зуб шестерни входит во
// впадину колеса: в исходном положении зуб шестерни направлен по c, впадина колеса — по −c;
// поворот колеса на δ требует поворота шестерни на −s·u·δ (s = +1 при сонаправленных осях).
double meshingTurn(const modeling::PartDocument& pinion, const modeling::PartDocument& wheel) {
    const auto gp = modeling::gearsOf(pinion.finalShape()), gw = modeling::gearsOf(wheel.finalShape());
    if (gp.size() != 1 || gw.size() != 1) fail("meshing: expected one gear rim on the pinion and on the wheel");
    const gp_Dir c(0, 1, 0);
    const gp_Ax3 &ap = gp.front().gear.axis, &aw = gw.front().gear.axis;
    auto angleAround = [](const gp_Dir& v, const gp_Dir& axis, const gp_Dir& ref) {
        return std::atan2(axis.Crossed(ref).Dot(v), ref.Dot(v));
    };
    auto wrap = [](double a, double period) { return a - period * std::round(a / period); };
    const int z1 = gp.front().gear.teeth, z2 = gw.front().gear.teeth;
    const double tauW = 2 * M_PI / z2, tauP = 2 * M_PI / z1;
    const double s = ap.Direction().Dot(aw.Direction()) >= 0 ? 1.0 : -1.0;
    const double space = wrap(angleAround(aw.XDirection(), aw.Direction(), c.Reversed()) + tauW / 2, tauW);
    const double now = angleAround(ap.XDirection(), ap.Direction(), c);
    return wrap(-s * (double(z2) / z1) * space - now, tauP);
}

}  // namespace

Result<std::vector<PartEntry>> buildParts(const Layout& L) {
    return guard([&]() -> Result<std::vector<PartEntry>> {
        const std::string castIron = "СЧ 20 ГОСТ 1412-85", steel45 = "Сталь 45 ГОСТ 1050-2013";
        std::vector<PartEntry> v;
        v.push_back(entry("housing", "РЦ.080.00.001", "Корпус", castIron, 7200, housing(L)));
        v.push_back(entry("cover", "РЦ.080.00.002", "Крышка корпуса", castIron, 7200, cover(L)));
        v.push_back(entry("shaft_in", "РЦ.080.00.003", "Вал-шестерня", "Сталь 40Х ГОСТ 4543-2016", 7850, inputShaft(L)));
        v.push_back(entry("shaft_out", "РЦ.080.00.004", "Вал", steel45, 7850, outputShaft(L)));
        v.push_back(entry("wheel", "РЦ.080.00.005", "Колесо зубчатое", steel45, 7850, wheel(L)));
        v.push_back(entry("cap_in_t", "РЦ.080.00.006", "Крышка подшипника сквозная", "СЧ 15 ГОСТ 1412-85", 7200,
                          bearingCap("cap_in_t", 52, 74, 32, 24.0, L)));
        v.push_back(entry("cap_in_b", "РЦ.080.00.007", "Крышка подшипника глухая", "СЧ 15 ГОСТ 1412-85", 7200,
                          bearingCap("cap_in_b", 52, 74, 32, std::nullopt, L)));
        v.push_back(entry("cap_out_b", "РЦ.080.00.008", "Крышка подшипника глухая", "СЧ 15 ГОСТ 1412-85", 7200,
                          bearingCap("cap_out_b", 62, 84, 37, std::nullopt, L)));
        v.push_back(entry("cap_out_t", "РЦ.080.00.009", "Крышка подшипника сквозная", "СЧ 15 ГОСТ 1412-85", 7200,
                          bearingCap("cap_out_t", 62, 84, 37, 30.0, L)));
        v.push_back(entry("spacer", "РЦ.080.00.010", "Втулка распорная", "Сталь 20 ГОСТ 1050-2013", 7850, spacer()));
        v.push_back(entry("gasket", "РЦ.080.00.011", "Прокладка", "Паронит ПОН-Б ГОСТ 481-80", 1800, flangeGasket(L)));
        v.push_back(entry("plug", "РЦ.080.00.012", "Пробка М16×1,5", "Сталь 20 ГОСТ 1050-2013", 7850, plug()));
        v.push_back(entry("plug_gasket", "РЦ.080.00.013", "Прокладка", "Паронит ПОН-Б ГОСТ 481-80", 1800, plugGasket()));
        // Зубья шестерни — в фазе зацепления с колесом (параметр angle венца, градусы).
        {
            modeling::PartDocument& pinion = *v[2].doc;
            const double turn = meshingTurn(pinion, *v[4].doc);
            const double deg = turn * 180.0 / M_PI;
            if (std::abs(deg) > 1e-9) {
                char text[40];
                std::snprintf(text, sizeof text, "%.17g", deg);
                if (auto st = pinion.setParameter("Teeth", "angle", text); !st.ok()) fail("pinion phase: " + st.error().message);
                pinion.rebuild();
                if (pinion.feature("Teeth").value().state != modeling::FeatureState::Ok) fail("pinion phase: teeth not rebuilt");
                if (std::abs(meshingTurn(pinion, *v[4].doc)) > 1e-12) fail("pinion phase: the teeth are not in mesh");
            }
        }
        return v;
    });
}

const PartEntry* findPart(const std::vector<PartEntry>& parts, const std::string& key) {
    for (const auto& p : parts)
        if (p.key == key) return &p;
    return nullptr;
}

Result<std::vector<StandardPart>> buildStandardParts(const Layout&) {
    return guard([&]() -> Result<std::vector<StandardPart>> {
        std::vector<StandardPart> v;
        auto add = [&](std::string key, std::string name, std::string material, double density, std::unique_ptr<PartDocument> doc,
                       double vol) {
            StandardPart s;
            s.key = std::move(key);
            s.name = std::move(name);
            s.material = std::move(material);
            s.density = density;
            s.doc = std::move(doc);
            s.formulaVolume = vol;
            v.push_back(std::move(s));
        };
        auto ringVol = [](double d, double D, double B) { return kPi / 4 * (D * D - d * d) * B; };
        auto keyVol = [](double b, double h, double l) { return ((l - b) * b + kPi * b * b / 4) * h; };
        // Подшипники шариковые радиальные ГОСТ 8338-75 (d × D × B — требуют сверки): тело-кольцо.
        add("bearing205", "Подшипник 205 ГОСТ 8338-75", "Сталь ШХ15 ГОСТ 801-78", 7810, ring("bearing205", 25, 52, 15), ringVol(25, 52, 15));
        add("bearing206", "Подшипник 206 ГОСТ 8338-75", "Сталь ШХ15 ГОСТ 801-78", 7810, ring("bearing206", 30, 62, 16), ringVol(30, 62, 16));
        // Шпонки призматические ГОСТ 23360-78, исполнение 1 (b × h × l — требуют сверки).
        add("key6", "Шпонка 6×6×28 ГОСТ 23360-78", "Сталь 45 ГОСТ 1050-2013", 7850, key("key6", 6, 6, 28), keyVol(6, 6, 28));
        add("key10", "Шпонка 10×8×32 ГОСТ 23360-78", "Сталь 45 ГОСТ 1050-2013", 7850, key("key10", 10, 8, 32), keyVol(10, 8, 32));
        add("key8", "Шпонка 8×7×32 ГОСТ 23360-78", "Сталь 45 ГОСТ 1050-2013", 7850, key("key8", 8, 7, 32), keyVol(8, 7, 32));
        return v;
    });
}

}  // namespace bendcore::examples::gearbox
