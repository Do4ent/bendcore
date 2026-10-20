// Импорт пути и портов из CSV и JSON, сборка модели (без OCCT).
#include "bendcore/tube_model.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <variant>

using namespace bendcore;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

template <class F>
static bool throws_invalid(F&& f) {
    try {
        f();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static const char* kCsv =
    "kind,length,radius,angle,rotation,pitch,turns,direction,x,y,z,dx,dy,dz,od,name\n"
    "# комментарий\n"
    "straight,50,,,,,,,,,,,,,,\n"
    "bend,40,50,90,0,,,,,,,,,,\n"
    "straight,30,,,,,,,,,,,,,,\n"
    "bend,40,50,-60,90,,,,,,,,,,\n"
    "spiral,,30,,,10,2,-1,,,,,,,,\n"
    "straight,20,,,,,,,,,,,,,,\n"
    "port,,,,,,,,0,0,0,-1,0,0,20,A\n"
    "port,,,,,,,,10,20,30,0,0,5,16,\n";

static const char* kJson = R"({
  "path": [
    {"kind": "straight", "length": 50},
    {"kind": "bend", "radius": 50, "angle": 90, "rotation": 0},
    {"kind": "straight", "length": 30},
    {"kind": "bend", "radius": 50, "angle": -60, "rotation": 90},
    {"kind": "spiral", "radius": 30, "pitch": 10, "turns": 2, "direction": -1},
    {"kind": "straight", "length": 20}
  ],
  "ports": [
    {"name": "A", "pos": [0, 0, 0], "direction": [-1, 0, 0], "od": 20},
    {"pos": [10, 20, 30], "direction": [0, 0, 5], "od": 16}
  ]
})";

static void check_same_path(const ImportData& a, const ImportData& b) {
    check(a.path.size() == b.path.size(), "csv и json: одинаковое число участков");
    if (a.path.size() != b.path.size()) return;
    for (std::size_t i = 0; i < a.path.size(); ++i) {
        check(a.path[i].index() == b.path[i].index(), "csv и json: одинаковый вид участка");
    }
    check(a.ports.size() == b.ports.size(), "csv и json: одинаковое число портов");
}

static void test_csv() {
    const ImportData d = import_csv(kCsv);
    check(d.path.size() == 6, "csv: шесть участков (прямые, гибы, спираль)");
    check(std::holds_alternative<Straight>(d.path[0]) && std::get<Straight>(d.path[0]).S == 50.0, "csv: прямая 50");
    check(std::holds_alternative<Bend>(d.path[1]) && std::get<Bend>(d.path[1]).B == 90.0, "csv: гиб 90");
    check(std::holds_alternative<Bend>(d.path[3]) && std::get<Bend>(d.path[3]).A == 90.0, "csv: поворот A");
    check(std::holds_alternative<Spiral>(d.path[4]) && std::get<Spiral>(d.path[4]).direction == -1, "csv: спираль, левая");
    check(d.ports.size() == 2, "csv: два порта");
    if (d.ports.size() == 2) {
        check(d.ports[0].name == "A" && d.ports[0].od == 20.0, "csv: порт A");
        check(std::abs(d.ports[1].direction[2] - 1.0) < 1e-12, "csv: направление порта нормировано");
        check(d.ports[1].name.empty(), "csv: пустое имя остаётся пустым до make_model");
    }
    check_same_path(d, import_json(kJson));
}

static void test_json_matches_csv() {
    const ImportData c = import_csv(kCsv);
    const ImportData j = import_json(kJson);
    check_same_path(c, j);
    for (std::size_t i = 0; i < c.path.size() && i < j.path.size(); ++i) {
        if (std::holds_alternative<Bend>(c.path[i])) {
            const Bend& x = std::get<Bend>(c.path[i]);
            const Bend& y = std::get<Bend>(j.path[i]);
            check(x.R == y.R && x.B == y.B && x.A == y.A, "csv и json: гибы совпадают");
        }
    }
}

static void test_csv_errors() {
    check(throws_invalid([] { import_csv(""); }), "csv: пустой файл — ошибка");
    check(throws_invalid([] { import_csv("length\n50\n"); }), "csv: нет колонки kind — ошибка");
    check(throws_invalid([] { import_csv("kind,length\nwave,50\n"); }), "csv: неизвестный kind — ошибка");
    check(throws_invalid([] { import_csv("kind,length\nstraight,abc\n"); }), "csv: не число — ошибка");
    check(throws_invalid([] { import_csv("kind,length\nstraight,\n"); }), "csv: пустая длина — ошибка");
    check(throws_invalid([] { import_csv("kind,radius,angle\nbend,50,\n"); }), "csv: гиб без угла — ошибка");
    check(throws_invalid([] {
              import_csv("kind,radius,pitch,turns,direction\nspiral,30,10,2,0\n");
          }), "csv: направление спирали не ±1 — ошибка");
    check(throws_invalid([] {
              import_csv("kind,x,y,z,dx,dy,dz,od\nport,0,0,0,0,0,0,20\n");
          }), "csv: нулевое направление порта — ошибка");
    check(throws_invalid([] {
              import_csv("kind,x,y,z,dx,dy,dz\nport,0,0,0,1,0,0\n");
          }), "csv: порт без ∅ — ошибка");
    bool haveLine = false;
    try {
        import_csv("kind,length\nstraight,50\nstraight,xx\n");
    } catch (const std::invalid_argument& e) {
        haveLine = std::string(e.what()).find("строка 3") != std::string::npos;
    }
    check(haveLine, "csv: ошибка указывает номер строки");
}

static void test_json_errors() {
    check(throws_invalid([] { import_json("[]"); }), "json: корень не объект — ошибка");
    check(throws_invalid([] { import_json("{}"); }), "json: нет path и ports — ошибка");
    check(throws_invalid([] { import_json(R"({"path":[{"kind":"wave"}]})"); }), "json: неизвестный kind — ошибка");
    check(throws_invalid([] { import_json(R"({"path":[{"kind":"straight"}]})"); }), "json: нет length — ошибка");
    check(throws_invalid([] { import_json(R"({"path":[{"kind":"straight","length":5}],})"); }),
          "json: лишняя запятая — ошибка");
    check(throws_invalid([] { import_json(R"({"ports":[{"pos":[0,0],"direction":[1,0,0],"od":20}]})"); }),
          "json: pos не из трёх чисел — ошибка");
    check(throws_invalid([] { import_json(R"({"ports":[{"pos":[0,0,0],"direction":[0,0,0],"od":20}]})"); }),
          "json: нулевое направление — ошибка");
    check(throws_invalid([] { import_json(R"({"path":[{"kind":"straight","length":5}]} extra)"); }),
          "json: текст после документа — ошибка");
}

static void test_make_model() {
    const ImportData d = import_csv(kCsv);
    const TubeModel m = make_model("труба-1", "csv", d, 20.0, 2.0);
    check(m.id == "труба-1" && m.source == "csv", "модель: id и источник");
    check(m.od == 20.0 && m.wall == 2.0, "модель: ∅ и стенка");
    check(m.path.has_value() && m.path->size() == d.path.size(), "модель: путь сохранён");
    check(m.ports.size() == 2, "модель: два ручных порта");
    if (m.ports.size() == 2) {
        check(m.ports[0].name == "A", "модель: имя порта A сохранено");
        check(!m.ports[1].name.empty() && m.ports[1].name != "A", "модель: пустое имя получает авто-имя");
    }
    ImportData onlyPorts;
    onlyPorts.ports = d.ports;
    check(!make_model("p", "json", onlyPorts, 20.0).path.has_value(), "модель: без пути, только порты");
    check(throws_invalid([&] { make_model("x", "csv", ImportData{}, 20.0); }), "модель: пусто — ошибка");
    check(throws_invalid([&] { make_model("x", "csv", d, 20.0, 10.0); }), "модель: стенка ≥ радиуса — ошибка");
    check(throws_invalid([&] { make_model("x", "csv", d, 0.0); }), "модель: ∅ ≤ 0 — ошибка");
    ImportData dup = d;
    dup.ports[1].name = "A";
    check(throws_invalid([&] { make_model("x", "csv", dup, 20.0); }), "модель: повтор имени порта — ошибка");
}

int main() {
    test_csv();
    test_json_matches_csv();
    test_csv_errors();
    test_json_errors();
    test_make_model();
    if (failures == 0) std::printf("OK import\n");
    return failures == 0 ? 0 : 1;
}
