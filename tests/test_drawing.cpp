// Чертёжная среда (раздел 7): масштабы, форматы, виды, развёртка, таблицы, выпуск, компоновка.
#include "bendcore/drawing.hpp"

#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>

using namespace bendcore;
using namespace bendcore::drawing;

static int failures = 0;
static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
static bool near(double a, double b, double eps = 1e-6) { return std::abs(a - b) <= eps; }

static Path L_tube() { return Path{Straight{100}, Bend{50, 90}, Straight{100}}; }
static Path spatial() {
    return Path{Straight{120}, Bend{40, 90, 0}, Straight{80}, Bend{40, 45, 90}, Straight{60},
                Bend{40, 60, -30}, Straight{150}};
}

static void test_standards() {
    check(projection_method(Standard::GOST) == Projection::FirstAngle, "ГОСТ — первый угол");
    check(projection_method(Standard::ANSI) == Projection::ThirdAngle, "ANSI — третий угол");
    check(scale_label(0.4, Standard::GOST) == "1:2,5", "ГОСТ 1:2,5");
    check(scale_label(2.0, Standard::ISO) == "2:1" && scale_label(1.0, Standard::ISO) == "1:1", "ISO 2:1, 1:1");
    check(!is_standard_scale(0.4, Standard::ISO) && is_standard_scale(0.4, Standard::GOST), "1:2,5 есть только в ГОСТ");
    const auto s = pick_scale(400, 100, 190, 100, Standard::ISO);
    check(s && near(*s, 0.2), "ISO: 400 мм в 190 мм → 1:5");
    const auto g = pick_scale(400, 100, 190, 100, Standard::GOST);
    check(g && near(*g, 0.4), "ГОСТ: 400 мм в 190 мм → 1:2,5");
    check(!pick_scale(1e6, 10, 100, 100, Standard::ISO), "не помещается — nullopt");
    const Format a3 = format_by_name("A3");
    check(near(a3.w, 420) && near(a3.h, 297), "A3 альбомный");
    check(near(format_by_name("A4", false).w, 210), "A4 книжный");
    bool threw = false;
    try {
        (void)format_by_name("A9");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "неизвестный формат — ошибка");
    const SheetFrame sf = make_sheet(Standard::GOST, a3);
    check(near(sf.border.x0, 20) && near(sf.border.y0, 5), "рамка ГОСТ 2.104: слева 20, остальные 5");
    check(near(sf.title_block.w(), 185) && near(sf.title_block.h(), 55), "основная надпись 185×55");
    check(near(sf.title_block.x1, 415) && near(sf.title_block.y0, 5), "штамп в правом нижнем углу");
}

static int count_style(const Geometry& g, LineStyle st) {
    int n = 0;
    for (const auto& l : g.lines) n += l.style == st;
    return n;
}

static void test_views() {
    const double od = 20, r = 10;
    const Geometry top = tube_view(L_tube(), od, ViewKind::Top);
    const Box2 tb = top.bbox();
    check(near(tb.lo[0], 0, 1e-6) && near(tb.hi[0], 150 + r, 1e-6), "вид сверху: X от 0 до 150 + r");
    check(near(tb.lo[1], -r, 1e-6) && near(tb.hi[1], 150, 1e-6), "вид сверху: Y от −r до 150");
    check(top.circles.empty(), "в плоскости вида окружностей нет");
    check(top.dims.size() == 2 && top.dims[0].text == "100,0" && top.dims[1].text == "100,0", "размеры S на виде сверху");
    check(count_style(top, LineStyle::Axis) == 1, "осевая линия");
    check(top.texts.size() == 1 && top.texts[0].text == "1" && top.texts[0].frame, "метка гиба");

    const Geometry front = tube_view(L_tube(), od, ViewKind::Front);
    check(front.circles.size() == 1 && near(front.circles[0].c[0], 150) && near(front.circles[0].r, r),
          "главный вид: участок на наблюдателя — окружность");
    check(front.dims.size() == 1, "главный вид: размер только у параллельного участка");

    const Geometry iso = tube_view(spatial(), od, ViewKind::Iso);
    check(iso.dims.empty() && !iso.lines.empty(), "изометрия без размеров");

    ViewOptions lopt;
    lopt.dims = DimMode::L;
    const Geometry topL = tube_view(L_tube(), od, ViewKind::Top, lopt);
    check(topL.dims.size() == 2 && topL.dims[0].text == "150,0", "режим L: между вершинами");
    ViewOptions none;
    none.dims = DimMode::TableOnly;
    check(tube_view(L_tube(), od, ViewKind::Top, none).dims.empty(), "режим «только таблица»");

    ViewOptions e;
    e.E_A = 12;
    const Box2 eb = tube_view(L_tube(), od, ViewKind::Top, e).bbox();
    check(near(eb.lo[0], -12, 1e-6), "деталь после подрезки: участок E на виде");

    ViewOptions bp;
    bp.bend_no = 2;
    const Geometry plane = tube_view(spatial(), od, ViewKind::BendPlane, bp);
    check(!plane.lines.empty(), "вид по плоскости гиба 2");
    const ViewBasis vb = view_basis(ViewKind::BendPlane, spatial(), 2);
    check(near(dot(vb.dir, vb.right), 0) && near(norm(cross(vb.right, vb.up)), 1), "базис вида ортонормирован");
    bool threw = false;
    try {
        ViewOptions bad;
        bad.bend_no = 9;
        (void)tube_view(spatial(), od, ViewKind::BendPlane, bad);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "нет такого гиба — ошибка");
}

static void test_unfold() {
    const double od = 20;
    const double arc = 50 * std::numbers::pi / 2;
    UnfoldResult u = unfold_view(L_tube(), od);
    check(u.blank_length && near(*u.blank_length, 200 + arc, 1e-9), "заготовка = длина по оси");
    check(u.geom.hatches.empty() && near(u.delta_start, 0) && near(u.delta_end, 0), "без припусков зон нет");
    check(u.marks.size() == 1 && near(u.marks[0].position, 100) && near(u.marks[0].B, 90), "метка гиба 1 на 100");
    check(u.caption == "Подача с конца А", "подпись развёртки");

    UnfoldOptions o;
    o.N_first = 150;  // зажим требует 150, а прямой участок 100 → Δ = 50
    UnfoldResult a = unfold_view(L_tube(), od, o);
    check(near(a.delta_start, 50) && a.blank_length && near(*a.blank_length, 250 + arc, 1e-9), "припуск Δ = 50");
    check(a.geom.hatches.size() == 1, "зона припуска заштрихована");
    int cut = 0;
    for (const auto& l : a.geom.lines) cut += l.style == LineStyle::Cut;
    check(cut == 1, "линия реза");
    check(near(a.marks[0].position, 150), "метка от торца заготовки с припуском");
    check(near(a.trimmed_length, 200 + arc, 1e-9), "длина после подрезки");

    o.feed = 'B';
    UnfoldResult b = unfold_view(Path{Straight{100}, Bend{50, 90}, Straight{200}}, od, o);
    check(b.caption == "Подача с конца Б" && near(b.delta_start, 0) && near(b.marks[0].position, 200),
          "с конца Б: первый участок 200, припуска нет");

    UnfoldOptions e;
    e.end_A.E = 10;
    e.end_B.E = 15;
    UnfoldResult ue = unfold_view(L_tube(), od, e);
    check(ue.blank_length && near(*ue.blank_length, 225 + arc, 1e-9), "E входит в заготовку");
    check(near(ue.marks[0].position, 110), "метка с учётом E");

    UnfoldOptions vm;
    vm.mark_base = MarkBase::Vertex;
    check(near(unfold_view(L_tube(), od, vm).marks[0].position, 150), "база метки — вершина");

    UnfoldOptions k;
    k.springback_K = 0.98;
    UnfoldResult uk = unfold_view(L_tube(), od, k);
    check(uk.blank_length && !near(*uk.blank_length, 200 + arc, 1e-3), "компенсация меняет заготовку");
    UnfoldOptions ch;
    ch.chain = true;
    const auto uc = unfold_view(spatial(), od, ch);
    int refs = 0;
    for (const auto& d : uc.geom.dims) refs += d.reference;
    check(refs >= 1, "цепочка: последнее звено справочное");
}

static void test_tables() {
    const Table t = bend_table(L_tube(), 10, 5, Profile::Right, true, 1);
    check(t.note == "R = 50,0", "R в шапке");
    check(t.rows.size() == 2 && t.rows[0][1] == "110,0" && t.rows[1][1] == "105,0", "первый и последний от физических торцов");
    const Table v = vertex_table(L_tube(), 0, 0, {}, 2);
    check(v.rows.size() == 3 && v.rows[1][1] == "150,00" && v.rows[1][4] == "50,00", "координаты вершин");
    UnfoldOptions o;
    o.N_first = 150;
    const Table m = blank_map(L_tube(), o);
    check(m.rows.front()[0] == "Подрезать" && m.rows.front()[1] == "50,0", "карта: подрезка");
    const double arc = 50 * std::numbers::pi / 2;
    check(m.rows.back()[1] == fmt(round_to(250 + arc, 0.1), 1, ','), "карта: длина заготовки");
}

static void test_release() {
    ReleaseInputs in;
    check(release_check(in).can_release, "без замечаний — можно выпускать");
    in.tech_or_geom_errors = true;
    auto r = release_check(in, Strictness::Soft);
    check(!r.can_release && r.not_for_production, "ошибки блокируют всегда, даже в мягком режиме");
    in.tech_or_geom_errors = false;
    in.lost_dim_refs = 2;
    in.unfold_sheet_changed = true;
    check(!release_check(in, Strictness::Strict).can_release, "строгий: потерянные ссылки блокируют");
    r = release_check(in, Strictness::Soft);
    check(r.can_release && r.warnings.size() == 2, "мягкий: только предупреждения");
    ReleaseInputs st;
    st.stale_views = 3;
    r = release_check(st);
    check(r.can_release && r.offer_update_and_release, "устаревшие виды — предупреждение и «Обновить и выпустить»");
    check(can_unrelease("Выпускающий") && can_unrelease("Администратор") && !can_unrelease("Конструктор"),
          "снять выпуск: Выпускающий или администратор");
}

static void test_layout() {
    DrawingInput in;
    in.path = spatial();
    in.od = 20;
    in.title["Обозначение"] = "БК.001";
    DrawingSheet m = make_part_drawing(in);
    check(m.views.size() == 3, "минимальный набор: главный, изометрия, развёртка");
    check(m.tables.size() == 2, "таблица гибов и карта заготовки");
    check(m.title.at("Масштаб").size() > 0 && m.title.at("Обозначение") == "БК.001", "поля штампа");
    for (const auto& v : m.views) {
        check(is_standard_scale(v.scale, Standard::GOST), "масштабы из ряда ГОСТ");
        const Box2 b = v.geom.bbox();
        const double x0 = v.origin[0] + v.scale * b.lo[0], x1 = v.origin[0] + v.scale * b.hi[0];
        const double y0 = v.origin[1] + v.scale * b.lo[1], y1 = v.origin[1] + v.scale * b.hi[1];
        check(x0 >= m.frame.border.x0 - 1e-6 && x1 <= m.frame.title_block.x0 + 1e-6 && y0 >= m.frame.border.y0 - 1e-6 &&
                  y1 <= m.frame.border.y1 + 1e-6,
              "вид внутри рамки и левее колонки таблиц");
    }

    in.preset = Preset::Full;
    DrawingSheet f = make_part_drawing(in);
    check(f.views.size() == 5 && f.tables.size() == 3, "полный набор: 4 вида + развёртка, 3 таблицы");
    const PlacedView *front = nullptr, *top = nullptr, *side = nullptr;
    for (const auto& v : f.views) {
        if (v.kind == ViewKind::Front) front = &v;
        if (v.kind == ViewKind::Top) top = &v;
        if (v.kind == ViewKind::Left || v.kind == ViewKind::Right) side = &v;
    }
    check(front && top && side, "главный, сверху и боковой");
    check(near(front->origin[0], top->origin[0]) && near(front->origin[1], side->origin[1]), "проекционная связь");
    check(top->origin[1] < front->origin[1] && side->kind == ViewKind::Left, "первый угол: сверху — под главным, слева — справа");

    in.standard = Standard::ANSI;
    in.format = "ANSI B";
    DrawingSheet a = make_part_drawing(in);
    for (const auto& v : a.views) {
        if (v.kind == ViewKind::Front) front = &v;
        if (v.kind == ViewKind::Top) top = &v;
        if (v.kind == ViewKind::Right) side = &v;
    }
    check(top->origin[1] > front->origin[1] && side->kind == ViewKind::Right, "ANSI третий угол: сверху — над главным");

    DrawingInput err = in;
    err.has_errors = true;
    check(make_part_drawing(err).not_for_production, "«Не для производства» при ошибках");

    DrawingInput longtube;
    longtube.path = Path{Straight{60000}, Bend{40, 90}, Straight{100}};
    longtube.od = 20;
    longtube.format = "A4";
    bool warned = false;
    for (const auto& w : make_part_drawing(longtube).warnings) warned = warned || w.find("разрыв") != std::string::npos;
    check(warned, "длинная труба: предупреждение о разрыве вида");
}

int main() {
    test_standards();
    test_views();
    test_unfold();
    test_tables();
    test_release();
    test_layout();
    if (failures) {
        std::printf("drawing: %d FAIL\n", failures);
        return 1;
    }
    std::printf("drawing: OK\n");
    return 0;
}
