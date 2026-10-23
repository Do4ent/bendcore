// Экземпляры (главный + копии) и одинаковые трубы (5.8, 5.9).
#include "bendcore/instances.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using namespace bendcore;
using namespace bendcore::assembly;

static int failures = 0;
static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
template <class F>
static bool throws(F f) {
    try {
        f();
    } catch (const InstanceError&) {
        return true;
    }
    return false;
}

static TubeSignatureData data(Path p) {
    TubeSignatureData d;
    d.path = std::move(p);
    d.od = 12;
    d.wall = 1;
    d.material = "Cu-DHP";
    d.supply_state = "R220";
    d.bending_tool = "HB-12";
    return d;
}
static Path spatial(double rot2 = 90.0) {
    return Path{Straight{120}, Bend{40, 90, 0}, Straight{80}, Bend{40, 45, rot2}, Straight{60},
                Bend{40, 60, -30}, Straight{150}};
}

static void test_primary_copies() {
    Registry r;
    r.add_part("P1", "БК.001", data(spatial()));
    r.insert("P1", "i1");
    r.insert("P1", "i2");
    r.insert("P1", "i3");
    check(r.is_primary("i1") && !r.is_primary("i2"), "первый экземпляр — главный");
    check(!r.can_edit("i2"), "копия только для чтения");
    check(throws([&] { (void)r.edit_path("i2", spatial(10)); }), "правка копии запрещена");
    const auto upd = r.edit_path("i1", spatial(10));
    check(upd.size() == 3, "правка главного обновляет все экземпляры");
    check(throws([&] { r.set_adaptive("i3", true); }), "адаптивной может быть только главная");
    r.set_adaptive("i1", true);
    r.set_primary("i2");
    check(r.is_primary("i2") && r.instance("i2").adaptive && !r.instance("i1").adaptive,
          "«Сделать главным» переносит адаптивность");
    r.remove("i2");
    check(r.is_primary("i1") && r.instance("i1").adaptive, "удаление главного: роль следующему");
    check(r.positions().size() == 1 && r.positions()[0].quantity == 2, "одна позиция, 2 шт.");
}

static void test_detach() {
    Registry r;
    r.add_part("P1", "БК.001", data(spatial()));
    r.insert("P1", "i1");
    r.insert("P1", "i2");
    r.set_adaptive("i1", true);
    r.detach("i1", "P2");
    check(r.instance("i1").part_id == "P2" && r.is_primary("i1") && r.is_primary("i2"), "отвязанная — главная своей детали");
    check(r.instance("i2").adaptive && !r.instance("i1").adaptive, "адаптивность осталась у детали, отвязанная заморожена");
    auto pos = r.positions();
    check(pos.size() == 1 && pos[0].quantity == 2, "после отвязки та же позиция (сигнатура совпадает)");
    (void)r.edit_path("i1", spatial(20));
    pos = r.positions();
    check(pos.size() == 2, "после правки — разные позиции");
    check(throws([&] { r.detach("i1", "P3"); }), "единственный экземпляр отвязывать нечего");
}

static void test_signature() {
    const auto a = data(spatial());
    check(compare(a, a) == Match::Same, "совпадает сама с собой");
    // Та же труба, развёрнутая концами: путь из LRA с конца Б.
    auto rev = data(output::path_from_lra(output::lra(spatial(), output::Feed::B)));
    rev.feed_from = 'B';
    check(compare(a, rev) == Match::SameReversed, "развёрнутая концами — та же позиция");
    auto rev_wrong_feed = rev;
    rev_wrong_feed.feed_from = 'A';
    check(compare(a, rev_wrong_feed) == Match::Different, "подача с другого конца — другая позиция");
    // Зеркальная: все повороты с обратным знаком.
    auto mir = data(Path{Straight{120}, Bend{40, 90, 0}, Straight{80}, Bend{40, 45, -90}, Straight{60},
                         Bend{40, 60, 30}, Straight{150}});
    check(compare(a, mir) == Match::Mirror, "зеркальная — отдельная позиция");
    auto tol = data(Path{Straight{120.05}, Bend{40, 90.05, 0}, Straight{80}, Bend{40, 45, 90.05}, Straight{60},
                         Bend{40, 60, -30}, Straight{150}});
    check(compare(a, tol) == Match::Same, "в допуске 0,1 мм / 0,1°");
    auto out = data(spatial(90.3));
    check(compare(a, out) == Match::Different, "вне допуска — разные");
    auto e = a;
    e.E_A = 10;
    check(compare(a, e) == Match::Different, "глубина вставки входит в сигнатуру");
    auto mat = a;
    mat.supply_state = "R290";
    check(compare(a, mat) == Match::Different, "состояние поставки входит в сигнатуру");
    // Конец-зависимые параметры переставляются при развороте.
    auto ae = a;
    ae.E_A = 10;
    auto re = rev;
    re.E_B = 10;
    check(compare(ae, re) == Match::SameReversed, "E переставляется вместе с концами");
}

static void test_designations() {
    Registry r;
    r.add_part("P1", "БК.001", data(spatial()));
    r.add_part("P2", "БК.002", data(spatial()));  // независимая копия с другим обозначением
    r.insert("P1", "i1");
    r.insert("P2", "i2");
    auto pos = r.positions();
    check(pos.size() == 1 && pos[0].designations_differ && pos[0].designation == "БК.001",
          "одинаковые трубы с разными обозначениями — предупреждение, обозначение первой");
    r.unify_designation({"i1", "i2"}, "БК.001");
    check(!r.positions()[0].designations_differ, "«Привести к одному»");
}

static void test_scale() {
    Registry r;
    for (int i = 0; i < 2500; ++i) {
        const std::string pid = "P" + std::to_string(i);
        r.add_part(pid, pid, data(spatial(i % 50)));  // 50 разных форм
        r.insert(pid, "i" + std::to_string(i));
    }
    const auto pos = r.positions();
    check(pos.size() == 50, "2500 независимых копий → 50 позиций");
}

int main() {
    test_primary_copies();
    test_detach();
    test_signature();
    test_designations();
    test_scale();
    if (failures) {
        std::printf("instances: %d FAIL\n", failures);
        return 1;
    }
    std::printf("instances: OK\n");
    return 0;
}
