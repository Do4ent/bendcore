// Фильтр импорта DWFx (9.8): пресеты, правила вида, маски, слоя и размера, окрестность труб,
// ручные решения, единица отбора «компонент», отчёт о неприменённых правилах.
#include "bendcore/dwfx/import_filter.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace bendcore::dwfx;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

static FilterComponent box(const std::string& id, const std::string& name, std::array<double, 6> b) {
    FilterComponent c;
    c.id = id;
    c.name = name;
    c.bbox = b;
    return c;
}

static FilterSupportAxis axis_x(SupportSource source = SupportSource::File) {
    FilterSupportAxis a;
    a.tube_id = "T";
    a.source = source;
    a.polyline = {{0, 0, 0}, {100, 0, 0}};
    a.od_mm = 19.05;  // R = 9.525; D = max(1×19.05, 5 + 0) = 19.05; коридор 28.575 мм
    return a;
}

static const FilterDecision& decision(const FilterResult& r, const std::string& id) {
    for (const auto& d : r.decisions) {
        if (d.id == id) return d;
    }
    static const FilterDecision none;
    std::printf("FAIL: нет решения для %s\n", id.c_str());
    ++failures;
    return none;
}

static FilterComponent tube() {
    FilterComponent t = box("T1", "Труба", {0, -10, -10, 100, 10, 10});
    t.is_tube = true;
    return t;
}

static void check_geometry() {
    check(near(distance_point_polyline({5, 3, 0}, {{0, 0, 0}, {10, 0, 0}}), 3.0), "расстояние до отрезка: 3");
    check(near(distance_point_polyline({-4, 3, 0}, {{0, 0, 0}, {10, 0, 0}}), 5.0), "расстояние до конца: 5");
    check(near(distance_point_polyline({0, 0, 4}, {{1, 0, 0}}), std::sqrt(17.0)), "расстояние до точки");
    check(glob_match("Gib*", "gib-1"), "маска: без учёта регистра (ASCII)");
    check(glob_match("a?c", "abc") && !glob_match("a?c", "ac"), "маска: ? — один символ");
    check(glob_match("*", "") && glob_match("a*c", "abbbc") && !glob_match("a*c", "abd"), "маска: *");
}

static void check_default_preset() {
    const ImportSettings s = import_defaults();
    std::vector<FilterComponent> comps;
    comps.push_back(tube());
    comps.push_back(box("B1", "Опора", {40, 15, -5, 60, 25, 5}));    // ~15,8 мм от оси — в окрестности
    comps.push_back(box("B2", "Далёкий", {40, 100, -5, 60, 110, 5}));  // ~100 мм — вне окрестности
    FilterComponent port = box("P1", "Порт", {500, 500, 500, 510, 510, 510});
    port.is_port = true;
    comps.push_back(port);

    const FilterResult r = apply_import_filter(s, comps, {axis_x()}, {});
    check(decision(r, "T1").include && decision(r, "T1").reason == "tube", "пресет: труба включена");
    check(decision(r, "B1").include && decision(r, "B1").reason == "neighborhood", "окрестность: тело рядом включено");
    check(!decision(r, "B2").include && decision(r, "B2").reason == "outside_neighborhood", "окрестность: далёкое исключено");
    check(decision(r, "P1").include && decision(r, "P1").reason == "port", "порт включается всегда");
    check(r.included == 3, "включено 3 компонента");
    check(r.not_applied.empty(), "для значений по умолчанию нет неприменённых правил");
}

static void check_presets_and_rules() {
    std::vector<FilterComponent> comps = {tube(), box("B1", "Опора", {40, 15, -5, 60, 25, 5})};

    ImportSettings tubes_only = import_defaults();
    tubes_only.filter_preset = FilterPreset::TubesOnly;
    const FilterResult t = apply_import_filter(tubes_only, comps, {axis_x()}, {});
    check(!decision(t, "B1").include && decision(t, "B1").reason == "not_tube", "«Только трубы»: тело исключено");

    ImportSettings all = import_defaults();
    all.filter_preset = FilterPreset::AllSimplified;
    all.filter_simplify_outer_shell = true;
    all.contact_levels = 2;
    const FilterResult a = apply_import_filter(all, comps, {axis_x()}, {});
    check(a.decisions[1].include && a.decisions[1].reason == "rules", "«Всё с упрощением»: тело включено");
    check(a.not_applied.size() == 2, "неприменённые правила: упрощение и контактные тела");

    ImportSettings hidden = import_defaults();
    hidden.filter_only_visible = true;
    std::vector<FilterComponent> h = {tube(), box("H1", "Скрытое", {40, 15, -5, 60, 25, 5})};
    h[1].visible = false;
    const FilterResult hr = apply_import_filter(hidden, h, {axis_x()}, {});
    check(!decision(hr, "H1").include && decision(hr, "H1").reason == "hidden", "только видимые: скрытое исключено");
}

static void check_mask_layer_size() {
    ImportSettings all = import_defaults();
    all.filter_preset = FilterPreset::AllSimplified;

    all.filter_name_mask = "Gib*";
    FilterComponent a = box("A", "gib-1", {0, 0, 0, 10, 10, 10});
    FilterComponent b = box("B", "Опора", {0, 0, 0, 10, 10, 10});
    FilterResult m = apply_import_filter(all, {a, b}, {}, {});
    check(m.decisions[0].include && !m.decisions[1].include && m.decisions[1].reason == "name_mask",
          "маска имени: совпавшее включено, остальное исключено");

    all.filter_name_mask.clear();
    all.filter_layers = " 1, 3 ";
    FilterComponent l1 = box("L1", "x", {0, 0, 0, 10, 10, 10});
    l1.layer = "3";
    FilterComponent l2 = box("L2", "x", {0, 0, 0, 10, 10, 10});
    l2.layer = "2";
    m = apply_import_filter(all, {l1, l2}, {}, {});
    check(m.decisions[0].include && m.decisions[1].reason == "layer", "слои: список через запятую");

    all.filter_layers.clear();
    all.filter_min_bbox_mm = 50.0;
    FilterComponent small = box("S", "x", {0, 0, 0, 10, 10, 10});
    FilterComponent big = box("G", "x", {0, 0, 0, 100, 100, 100});
    m = apply_import_filter(all, {small, big}, {}, {});
    check(!m.decisions[0].include && m.decisions[0].reason == "min_size" && m.decisions[1].include,
          "минимальный габарит: диагональ меньше порога исключается");

    all.filter_type_surfaces = false;
    FilterComponent surf = box("F", "x", {0, 0, 0, 100, 100, 100});
    surf.type = FilterObjectType::Surface;
    m = apply_import_filter(all, {surf}, {}, {});
    check(!m.decisions[0].include && m.decisions[0].reason == "type", "тип: поверхности выключены");
}

static void check_manual_and_group() {
    ImportSettings s = import_defaults();
    std::vector<FilterComponent> comps = {tube(), box("B1", "Опора", {40, 15, -5, 60, 25, 5}),
                                          box("B2", "Далёкий", {40, 100, -5, 60, 110, 5})};
    const FilterResult m = apply_import_filter(s, comps, {axis_x()}, {{"B1", false}, {"B2", true}});
    check(!decision(m, "B1").include && decision(m, "B1").reason == "manual_exclude", "ручное исключение сильнее правил");
    check(decision(m, "B2").include && decision(m, "B2").reason == "manual_include", "ручное включение сильнее окрестности");

    // Единица «компонент»: одна часть группы в окрестности — вся группа загружается.
    ImportSettings group = import_defaults();
    group.neighborhood_unit = NeighborhoodUnit::Component;
    FilterComponent g1 = box("G1", "Узел", {40, 15, -5, 60, 25, 5});
    g1.group_id = "node";
    FilterComponent g2 = box("G2", "Узел", {40, 100, -5, 60, 110, 5});
    g2.group_id = "node";
    FilterComponent o1 = box("O1", "Другой", {40, 100, -5, 60, 110, 5});
    o1.group_id = "other";
    const FilterResult g = apply_import_filter(group, {tube(), g1, g2, o1}, {axis_x()}, {});
    check(decision(g, "G2").include && decision(g, "G2").reason == "group", "группа: дальняя часть включена вместе с ближней");
    check(!decision(g, "O1").include, "группа: чужая далёкая группа исключена");

    ImportSettings body = import_defaults();
    const FilterResult b = apply_import_filter(body, {tube(), g1, g2}, {axis_x()}, {});
    check(!decision(b, "G2").include, "единица «тело»: дальняя часть не загружается");
}

static void check_support_sources() {
    ImportSettings s = import_defaults();
    s.neighborhood_support_target_tubes = false;
    std::vector<FilterComponent> comps = {tube(), box("B1", "Опора", {40, 15, -5, 60, 25, 5})};
    const FilterResult r = apply_import_filter(s, comps, {axis_x(SupportSource::Target)}, {});
    check(!decision(r, "B1").include, "опора из сборки выключена настройкой: тело вне окрестности");

    const FilterResult f = apply_import_filter(import_defaults(), comps, {axis_x(SupportSource::File)}, {});
    check(decision(f, "B1").include, "опора из файла включена по умолчанию");
}

int main() {
    check_geometry();
    check_default_preset();
    check_presets_and_rules();
    check_mask_layer_size();
    check_manual_and_group();
    check_support_sources();
    if (failures == 0) std::printf("OK dwfx_filter\n");
    return failures == 0 ? 0 : 1;
}
