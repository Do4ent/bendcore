// Диспетчер команд ядра (JSON) — тот же, что в bendcore_cli и WebAssembly.
#include "bendcore/core_api.hpp"

#include <cmath>
#include <cstdio>
#include <numbers>

using namespace bendcore::api;

static int failures = 0;
static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static json L_path() {
    return json::array({{{"kind", "straight"}, {"length", 100}},
                        {{"kind", "bend"}, {"radius", 50}, {"angle", 90}, {"rotation", 0}},
                        {{"kind", "straight"}, {"length", 100}}});
}

int main() {
    Core core;
    check(core.call("health", {})["ok"] == true, "health");
    check(core.call("nope", {})["ok"] == false, "неизвестная команда — ошибка");

    auto lra = core.call("bend.lra", {{"path", L_path()}, {"precision", 1}});
    check(lra["ok"] == true && lra["data"]["rows"].size() == 2, "bend.lra");
    check(lra["data"]["csv"] == "Гиб;L;R;A;Радиус\n1;100.0;0.0;90.0;50.0\nКонец;100.0;;;\n", "bend.lra csv");

    auto xyz = core.call("bend.xyz", {{"path", L_path()}, {"feed", "B"}});
    check(xyz["ok"] == true && xyz["data"]["rows"][0]["label"] == "Б", "bend.xyz с конца Б");

    json spiral = json::array({{{"kind", "straight"}, {"length", 50}},
                               {{"kind", "spiral"}, {"radius", 30}, {"pitch", 10}, {"turns", 2}},
                               {{"kind", "straight"}, {"length", 50}}});
    auto bad = core.call("bend.lra", {{"path", spiral}});
    check(bad["ok"] == false && bad["error"].get<std::string>().find("спираль") != std::string::npos,
          "спираль: понятная ошибка");

    auto tab = core.call("bend.table", {{"path", L_path()}});
    check(tab["ok"] == true && std::abs(tab["data"]["finished_length"].get<double>() - (200 + 25 * std::numbers::pi)) < 1e-9,
          "bend.table: длина готовой трубы");

    json tubes = json::array({{{"id", "T1"}, {"path", L_path()}, {"od", 12}},
                              {{"id", "T2"}, {"path", L_path()}, {"od", 12},
                               {"placement", {{"origin", {0, 15, 0}}}}}});
    auto chk = core.call("check.run", {{"tubes", tubes}});
    check(chk["ok"] == true && chk["data"]["violations"].size() >= 2, "check.run: пометки зазора");
    check(chk["data"]["tubes"]["T1"]["state"] == "violations", "check.run: состояние трубы");

    json data = {{"path", L_path()}, {"od", 12}, {"material", "Cu"}};
    check(core.call("instances.add_part", {{"part", "P1"}, {"designation", "БК.001"}, {"data", data}})["ok"] == true,
          "instances.add_part");
    core.call("instances.insert", {{"part", "P1"}, {"instance", "i1"}});
    auto st = core.call("instances.insert", {{"part", "P1"}, {"instance", "i2"}});
    check(st["data"]["instances"][0]["primary"] == true && st["data"]["instances"][1]["editable"] == false,
          "главный и копия");
    auto ro = core.call("instances.edit_path", {{"instance", "i2"}, {"path", L_path()}});
    check(ro["ok"] == false, "правка копии — ошибка");
    auto pos = core.call("instances.state", {});
    check(pos["data"]["positions"].size() == 1 && pos["data"]["positions"][0]["quantity"] == 2, "одна позиция, 2 шт.");
    auto other = core.call("instances.state", {{"doc", "doc2"}});
    check(other["data"]["instances"].empty(), "у другого документа свой реестр");

    auto dr = core.call("drawing.part", {{"path", L_path()}, {"od", 20}, {"title", {{"Обозначение", "БК.001"}}}});
    check(dr["ok"] == true && dr["data"]["sheets"].size() == 1, "drawing.part: один лист");
    check(dr["data"]["sheets"][0]["views"].size() == 3 && dr["data"]["sheets"][0]["tables"].size() == 3, "drawing.part: виды и таблицы");
    check(dr["data"]["sheets"][0]["title"]["Масштаб"].is_string(), "drawing.part: масштаб в штампе");
    auto rel = core.call("drawing.release_check", {{"lost_dim_refs", 1}, {"strictness", "soft"}, {"role", "Выпускающий"}});
    check(rel["data"]["can_release"] == true && rel["data"]["can_unrelease"] == true, "release_check мягкий");

    const std::string line = core.handle_line(R"({"id":7,"command":"health"})");
    check(json::parse(line)["id"] == 7, "handle_line сохраняет id");
    check(json::parse(core.handle_line("{oops"))["ok"] == false, "неверный JSON — ошибка без падения");
    auto missing = core.call("bend.lra", json::object());
    check(missing["ok"] == false && missing["error"] == "нет поля path", "нет поля — понятная ошибка");

    if (failures) {
        std::printf("core_api: %d FAIL\n", failures);
        return 1;
    }
    std::printf("core_api: OK\n");
    return 0;
}
