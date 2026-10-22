// Сцена для 3D-окна (scene.hpp). Ожидаемые значения посчитаны вручную: оси вдоль x, зазоры и коробки считаются в уме.
#include "bendcore/scene.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

using namespace bendcore;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static bool near(double a, double b, double tol = 1e-6) { return std::abs(a - b) <= tol; }

static bool near3(const Vec3& a, const Vec3& b, double tol = 1e-6) {
    return near(a[0], b[0], tol) && near(a[1], b[1], tol) && near(a[2], b[2], tol);
}

static bool contains(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

// Минимальная проверка синтаксиса JSON: рукописный вывод легко ломается на запятых и кавычках.
struct JsonCheck {
    const std::string& s;
    std::size_t i = 0;

    explicit JsonCheck(const std::string& text) : s(text) {}

    void ws() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\t' || s[i] == '\r')) ++i;
    }
    bool lit(const char* w) {
        const std::size_t n = std::strlen(w);
        if (s.compare(i, n, w) == 0) {
            i += n;
            return true;
        }
        return false;
    }
    bool str() {
        if (s[i] != '"') return false;
        ++i;
        while (i < s.size()) {
            const char c = s[i++];
            if (c == '"') return true;
            if (c == '\\') {
                if (i >= s.size()) return false;
                const char e = s[i++];
                if (e == 'u') {
                    i += 4;
                } else if (!std::strchr("\"\\/bfnrt", e)) {
                    return false;
                }
            } else if (static_cast<unsigned char>(c) < 0x20) {
                return false;
            }
        }
        return false;
    }
    bool number() {
        const std::size_t start = i;
        if (s[i] == '-') ++i;
        while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '.' || s[i] == 'e' ||
                                s[i] == 'E' || s[i] == '+' || s[i] == '-')) {
            ++i;
        }
        return i > start;
    }
    bool value() {
        ws();
        if (i >= s.size()) return false;
        const char c = s[i];
        if (c == '{') {
            ++i;
            ws();
            if (s[i] == '}') {
                ++i;
                return true;
            }
            while (true) {
                ws();
                if (!str()) return false;
                ws();
                if (s[i] != ':') return false;
                ++i;
                if (!value()) return false;
                ws();
                if (s[i] == ',') {
                    ++i;
                    continue;
                }
                if (s[i] == '}') {
                    ++i;
                    return true;
                }
                return false;
            }
        }
        if (c == '[') {
            ++i;
            ws();
            if (s[i] == ']') {
                ++i;
                return true;
            }
            while (true) {
                if (!value()) return false;
                ws();
                if (s[i] == ',') {
                    ++i;
                    continue;
                }
                if (s[i] == ']') {
                    ++i;
                    return true;
                }
                return false;
            }
        }
        if (c == '"') return str();
        if (lit("true") || lit("false") || lit("null")) return true;
        return number();
    }
    bool valid() {
        if (!value()) return false;
        ws();
        return i == s.size();
    }
};

static bool valid_json(const std::string& s) { return JsonCheck(s).valid(); }

static SceneTubeInput tube_x(const std::string& id, double y, double z = 0.0, double od = 20.0,
                             std::vector<Issue> issues = {}) {
    return SceneTubeInput{PlacedTube{id, {{0, y, z}, {100, y, z}}, od}, std::move(issues)};
}

static void test_states() {
    check(state_from_issues({}) == "norm", "без замечаний — norm");
    check(state_from_issues({{"warning", -1, ""}}) == "warn", "warning -> warn");
    check(state_from_issues({{"warning", -1, ""}, {"technological", -1, ""}}) == "err_tech", "technological тяжелее warning");
    check(state_from_issues({{"mark_gap", -1, ""}, {"warning", -1, ""}}) == "mark_gap", "mark_gap тяжелее warning");
    check(state_from_issues({{"mark_cross", -1, ""}, {"mark_gap", -1, ""}}) == "mark_cross", "mark_cross тяжелее mark_gap");
    check(state_from_issues({{"mark_cross", -1, ""}, {"technological", -1, ""}}) == "err_tech", "technological тяжелее mark_cross");
    check(state_from_issues({{"technological", -1, ""}, {"geometric", -1, ""}}) == "err_geom", "geometric тяжелее всех");
    check(state_from_issues({{"geometric", -1, ""}, {"technological", -1, ""}}) == "err_geom", "порядок замечаний не важен");
    check(state_from_issues({{"info", -1, ""}}) == "info" && state_from_issues({{"suggestion", -1, ""}}) == "info",
          "info и suggestion -> info");
    check(state_from_issues({{"info", -1, ""}, {"warning", -1, ""}}) == "warn", "warn тяжелее info");
    check(state_from_issues({{"refused", -1, ""}}) == "warn" && state_from_issues({{"что-то новое", -1, ""}}) == "warn",
          "refused и неизвестный класс -> warn");
    check(state_from_issues({{"ref_lost", -1, ""}}) == "norm", "ref_lost состояния не меняет");
    check(flags_from_issues({{"ref_lost", -1, ""}, {"ref_lost", -1, ""}}) == std::vector<std::string>{"ref_lost"},
          "признак ref_lost один раз");
    check(flags_from_issues({{"warning", -1, ""}}).empty(), "без ref_lost признаков нет");
}

static void test_fit() {
    const auto f = fit_points({{0, 0, 0}, {10, 20, 30}}, 1.0);
    check(f && near3(f->lo, {-1, -1, -1}) && near3(f->hi, {11, 21, 31}) && near3(f->center, {5, 10, 15}) &&
              near(f->radius, 0.5 * std::sqrt(12.0 * 12.0 + 22.0 * 22.0 + 32.0 * 32.0)),
          "коробка с запасом 1: (-1..11, -1..21, -1..31), центр (5,10,15)");
    check(!fit_points({}).has_value(), "пустой набор — nullopt");
    const auto one = fit_points({{3, 4, 5}});
    check(one && near3(one->center, {3, 4, 5}) && near(one->radius, 0.0), "одна точка: нулевой радиус");
}

static void test_scene_tubes() {
    // А и Б параллельны на 24 мм: зазор между поверхностями 4 < 5 — обе трубы получают mark_gap; В далеко.
    const Scene s = build_scene({tube_x("А", 0), tube_x("Б", 24), tube_x("В", 1000)}, {});
    check(s.marks.size() == 1 && s.marks[0].kind == "mark_gap", "одна пометка зазора");
    check(s.tubes[0].state == "mark_gap" && s.tubes[1].state == "mark_gap" && s.tubes[2].state == "norm",
          "состояния: А и Б — mark_gap, В — norm");
    check(s.tubes[0].issues.size() == 1 && s.tubes[0].issues[0].kind == "mark_gap" && s.tubes[2].issues.empty(),
          "пометка попала в замечания обеих труб, но не третьей");

    // Своя ошибка тяжелее зазора; пометка при этом остаётся в списке замечаний.
    const Scene s2 = build_scene({tube_x("А", 0, 0, 20.0, {{"technological", 1, "вылет мал"}}), tube_x("Б", 24)}, {});
    check(s2.tubes[0].state == "err_tech" && s2.tubes[0].issues.size() == 2 && s2.tubes[1].state == "mark_gap",
          "технологическая ошибка тяжелее зазора, зазор сохранён");

    const Scene s3 = build_scene({tube_x("А", 0, 0, 20.0, {{"ref_lost", -1, ""}})}, {});
    check(s3.tubes[0].state == "norm" && s3.tubes[0].flags == std::vector<std::string>{"ref_lost"},
          "потерянная ссылка — признак, не состояние");

    // Нормы и пропуск пар передаются в проверку зазора.
    SceneOptions loose;
    loose.gap = 3.0;
    check(build_scene({tube_x("А", 0), tube_x("Б", 24)}, {}, loose).marks.empty(), "норма 3 мм: зазор 4 допустим");
    SceneOptions skip;
    skip.skip_pairs = {{"Б", "А"}};
    check(build_scene({tube_x("А", 0), tube_x("Б", 24)}, {}, skip).tubes[0].state == "norm", "пара в пропуске — без пометок");

    auto throws = [](auto fn) {
        try {
            fn();
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    check(throws([] { build_scene({tube_x("А", 0), tube_x("А", 50)}, {}); }), "повтор id трубы — исключение");
    check(throws([] {
              build_scene({}, {manual_port({0, 0, 0}, {1, 0, 0}, 20, "П1"), manual_port({1, 0, 0}, {1, 0, 0}, 20, "П1")});
          }),
          "повтор имени порта — исключение");
}

static void test_fit_scene() {
    const Scene s = build_scene({tube_x("А", 0), tube_x("Б", 24)}, {});
    check(s.fit_all && near3(s.fit_all->lo, {-10, -10, -10}) && near3(s.fit_all->hi, {110, 34, 10}) &&
              near3(s.fit_all->center, {50, 12, 0}) && near(s.fit_all->radius, 0.5 * std::sqrt(120.0 * 120.0 + 44.0 * 44.0 + 20.0 * 20.0)),
          "показать всё: коробка с учётом ∅, центр (50,12,0)");
    check(!s.fit_selected.has_value(), "ничего не выбрано — fit_selected пуст");

    SceneOptions sel;
    sel.selected_tubes = {"Б"};
    const Scene s2 = build_scene({tube_x("А", 0), tube_x("Б", 24)}, {}, sel);
    check(s2.tubes[1].selected && !s2.tubes[0].selected, "выбрана труба Б");
    check(s2.fit_selected && near3(s2.fit_selected->lo, {-10, 14, -10}) && near3(s2.fit_selected->hi, {110, 34, 10}),
          "показать выбранное: коробка Б");

    check(!build_scene({}, {}).fit_all.has_value(), "пустая сцена — fit_all пуст");

    // Порты входят в «показать всё»: порт далеко по x расширяет коробку.
    const Scene s3 = build_scene({tube_x("А", 0)}, {manual_port({500, 0, 0}, {1, 0, 0}, 20, "П1")});
    check(s3.fit_all && near(s3.fit_all->hi[0], 510.0), "порт входит в показать всё: x до 510");
}

static void test_ports() {
    Port p = manual_port({0, 0, 0}, {0, 0, 5}, 20.0, "П1");   // направление нормируется
    const Scene s = build_scene({}, {p});
    const PortGlyph& g = s.ports[0];
    check(near3(g.dir, {0, 0, 1}), "направление порта единичное");
    check(g.ring.size() == 25 && near3(g.ring.front(), g.ring.back(), 1e-12), "окружность: 24 отрезка, замкнута");
    bool on_ring = true;
    for (const auto& q : g.ring) on_ring = on_ring && near(norm(sub(q, p.pos)), 10.0, 1e-9) && near(q[2], 0.0, 1e-9);
    check(on_ring, "точки окружности на расстоянии 10 от порта, в плоскости ⟂ направлению");
    check(near3(g.arrow_tip, {0, 0, 30}), "стрелка по умолчанию 1,5·∅ = 30 мм");
    check(!g.show_zone && !g.zone && !g.link, "порт не выбран — зоны и связи нет");

    SceneOptions o;
    o.ring_segments = 12;
    o.arrow_len = 5.0;
    const Scene s2 = build_scene({}, {p}, o);
    check(s2.ports[0].ring.size() == 13 && near3(s2.ports[0].arrow_tip, {0, 0, 5}), "12 отрезков и стрелка 5 мм");
    o.ring_segments = 3;
    check(build_scene({}, {p}, o).ports[0].ring.size() == 9, "меньше 8 отрезков поднимается до 8");

    // Зона вылета и линия связи — только у выбранного порта или при трассировке.
    Port q = manual_port({110, 0, 0}, {1, 0, 0}, 20.0, "П2");
    q.min_straight = 40.0;
    q.occupied_by = "А";
    Port other = manual_port({0, 500, 0}, {0, 1, 0}, 20.0, "П3");
    other.min_straight = 25.0;

    SceneOptions pick;
    pick.selected_ports = {"П2"};
    const Scene s3 = build_scene({tube_x("А", 0)}, {q, other}, pick);
    check(s3.ports[0].show_zone && s3.ports[0].zone && near3(s3.ports[0].zone->first, {110, 0, 0}) &&
              near3(s3.ports[0].zone->second, {150, 0, 0}),
          "выбранный порт: зона вылета 40 мм вдоль направления");
    check(s3.ports[0].link && near3(s3.ports[0].link->first, {110, 0, 0}) && near3(s3.ports[0].link->second, {100, 0, 0}),
          "линия связи — к ближайшему концу трубы А");
    check(!s3.ports[1].show_zone && !s3.ports[1].zone && !s3.ports[1].link, "невыбранный порт — без зоны и связи");

    SceneOptions tr;
    tr.trace = true;
    const Scene s4 = build_scene({tube_x("А", 0)}, {q, other}, tr);
    check(s4.ports[0].zone && s4.ports[1].zone && near3(s4.ports[1].zone->second, {0, 525, 0}) && !s4.ports[1].link,
          "трассировка: зоны у всех портов, связь только у занятого");

    // Занят трубой, которой нет в сцене: линии связи нет, остальное работает.
    Port ghost = q;
    ghost.occupied_by = "Нет такой";
    check(!build_scene({tube_x("А", 0)}, {ghost}, tr).ports[0].link.has_value(), "труба-владелец вне сцены — связи нет");

    // Конец Б ближе: связь идёт к нему.
    Port near_b = manual_port({-10, 0, 0}, {-1, 0, 0}, 20.0, "П4");
    near_b.occupied_by = "А";
    const Scene s5 = build_scene({tube_x("А", 0)}, {near_b}, tr);
    check(s5.ports[0].link && near3(s5.ports[0].link->second, {0, 0, 0}), "связь к ближайшему концу (начало трубы)");

    Port zero;
    zero.name = "Нулевой";
    zero.direction = {0, 0, 0};
    bool threw = false;
    try {
        build_scene({}, {zero});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "нулевое направление порта — исключение");
}

static void test_json() {
    Port q = manual_port({110, 0, 0}, {1, 0, 0}, 20.0, "П2");
    q.min_straight = 40.0;
    q.occupied_by = "А";
    Port free_port = manual_port({0, 500, 0}, {0, 1, 0}, 20.0, "П3");

    SceneOptions o;
    o.selected_ports = {"П2"};
    o.selected_tubes = {"Б"};
    const Scene s = build_scene({tube_x("А", 0), tube_x("Б", 24, 0, 20.0, {{"warning", -1, "текст с \"кавычками\" и \\"}})},
                                {q, free_port}, o);
    const std::string j = scene_to_json(s);

    check(valid_json(j), "JSON синтаксически верен");
    check(j.rfind("{\"format\":\"bendcore-scene\",\"version\":1,\"tubes\":[", 0) == 0, "заголовок формата и версия");
    check(contains(j, "\"id\":\"А\"") && contains(j, "\"id\":\"Б\""), "русские id как UTF-8");
    check(contains(j, "\"state\":\"mark_gap\""), "состояние mark_gap в JSON");
    check(contains(j, "\"selected\":true") && contains(j, "\"selected\":false"), "признак выбора");
    check(contains(j, "\"occupiedBy\":\"А\"") && contains(j, "\"occupiedBy\":null"), "occupiedBy: строка и null");
    check(contains(j, "\"showZone\":true") && contains(j, "\"showZone\":false"), "showZone обоих видов");
    check(contains(j, "\"zone\":{\"from\":[110,0,0],\"to\":[150,0,0],\"radius\":10}"), "зона вылета: от (110,0,0) до (150,0,0), радиус 10");
    check(contains(j, "\"link\":{\"from\":[110,0,0],\"to\":[100,0,0]}"), "линия связи (110,0,0) -> (100,0,0)");
    check(contains(j, "\\\"кавычками\\\" и \\\\"), "кавычки и обратная косая экранируются");
    check(contains(j, "\"marks\":[{\"kind\":\"mark_gap\",\"tubeA\":\"А\",\"tubeB\":\"Б\""), "метка зазора в JSON");
    check(contains(j, "\"surfaceGap\":4") && contains(j, "\"norm\":5"), "зазор 4, норма 5");
    check(contains(j, "\"fitAll\":{\"lo\":[-10,-10,-10]"), "fitAll с нижним углом (-10,-10,-10)");
    // Труба Б: x 0..100, y 14..34, z -10..10; порт П2 в (110,0,0) с ∅20: x 100..120, y -10..10 — объединение:
    check(contains(j, "\"fitSelected\":{\"lo\":[-10,-10,-10],\"hi\":[120,34,10]"), "fitSelected: объединение трубы Б и порта П2");

    const std::string empty = scene_to_json(build_scene({}, {}));
    check(valid_json(empty) && contains(empty, "\"fitAll\":null") && contains(empty, "\"fitSelected\":null") &&
              contains(empty, "\"tubes\":[]") && contains(empty, "\"marks\":[]"),
          "пустая сцена: валидный JSON с пустыми списками и null");
}

int main() {
    test_states();
    test_fit();
    test_scene_tubes();
    test_fit_scene();
    test_ports();
    test_json();
    if (failures == 0) std::printf("scene: ok\n");
    return failures == 0 ? 0 : 1;
}
