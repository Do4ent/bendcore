#include "bendcore/core_api.hpp"

#include <functional>
#include <map>
#include <memory>
#include <stdexcept>

#include "bendcore/batch_check.hpp"
#include "bendcore/bend_output.hpp"
#include "bendcore/drawing.hpp"
#include "bendcore/instances.hpp"

namespace bendcore::api {
namespace {

struct UserError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

double num(const json& j, const char* key, double def) {
    if (!j.contains(key) || j.at(key).is_null()) return def;
    if (!j.at(key).is_number()) throw UserError(std::string("поле ") + key + " должно быть числом");
    return j.at(key).get<double>();
}
std::string str(const json& j, const char* key, const std::string& def = "") {
    if (!j.contains(key) || j.at(key).is_null()) return def;
    if (!j.at(key).is_string()) throw UserError(std::string("поле ") + key + " должно быть строкой");
    return j.at(key).get<std::string>();
}
bool flag(const json& j, const char* key, bool def) {
    if (!j.contains(key) || j.at(key).is_null()) return def;
    return j.at(key).get<bool>();
}
const json& need(const json& j, const char* key) {
    if (!j.contains(key)) throw UserError(std::string("нет поля ") + key);
    return j.at(key);
}

Vec3 vec(const json& j) {
    if (!j.is_array() || j.size() != 3) throw UserError("ожидается вектор [x, y, z]");
    return {j[0].get<double>(), j[1].get<double>(), j[2].get<double>()};
}
json vec_json(const Vec3& v) { return json::array({v[0], v[1], v[2]}); }
json vec2_json(const drawing::Vec2& v) { return json::array({v[0], v[1]}); }

output::Frame frame_from(const json& j) {
    output::Frame f;
    if (j.is_null()) return f;
    if (j.contains("origin")) f.origin = vec(j.at("origin"));
    if (j.contains("x")) f.x = vec(j.at("x"));
    if (j.contains("y")) f.y = vec(j.at("y"));
    if (j.contains("z")) f.z = vec(j.at("z"));
    return f;
}

output::Feed feed_from(const json& a) {
    const std::string f = str(a, "feed", "A");
    if (f == "A" || f == "А") return output::Feed::A;
    if (f == "B" || f == "Б") return output::Feed::B;
    throw UserError("подача с конца: A или B");
}

Profile profile_from(const std::string& s) {
    if (s == "both") return Profile::Both;
    if (s == "left") return Profile::Left;
    return Profile::Right;
}

const char* side_name(Side s) { return s == Side::Left ? "L" : s == Side::Right ? "R" : ""; }

drawing::Standard standard_from(const std::string& s) {
    if (s == "ISO") return drawing::Standard::ISO;
    if (s == "DIN") return drawing::Standard::DIN;
    if (s == "ANSI") return drawing::Standard::ANSI;
    return drawing::Standard::GOST;
}

const char* style_name(drawing::LineStyle s) {
    switch (s) {
        case drawing::LineStyle::Contour: return "contour";
        case drawing::LineStyle::Thin: return "thin";
        case drawing::LineStyle::Axis: return "axis";
        case drawing::LineStyle::Cut: return "cut";
        case drawing::LineStyle::Hidden: return "hidden";
    }
    return "thin";
}
const char* kind_name(drawing::ViewKind k) {
    switch (k) {
        case drawing::ViewKind::Front: return "front";
        case drawing::ViewKind::Top: return "top";
        case drawing::ViewKind::Left: return "left";
        case drawing::ViewKind::Right: return "right";
        case drawing::ViewKind::Iso: return "iso";
        case drawing::ViewKind::BendPlane: return "bend_plane";
        case drawing::ViewKind::Unfold: return "unfold";
    }
    return "";
}

json geometry_json(const drawing::Geometry& g) {
    json out;
    out["lines"] = json::array();
    for (const auto& l : g.lines) {
        json pts = json::array();
        for (const auto& p : l.pts) pts.push_back(vec2_json(p));
        out["lines"].push_back({{"pts", pts}, {"style", style_name(l.style)}, {"closed", l.closed}});
    }
    out["circles"] = json::array();
    for (const auto& c : g.circles)
        out["circles"].push_back({{"c", vec2_json(c.c)}, {"r", c.r}, {"style", style_name(c.style)}});
    out["texts"] = json::array();
    for (const auto& t : g.texts)
        out["texts"].push_back({{"p", vec2_json(t.p)}, {"text", t.text}, {"h", t.height}, {"anchor", t.anchor},
                                {"frame", t.frame}, {"shift", vec2_json(t.shift)}});
    out["dims"] = json::array();
    for (const auto& d : g.dims)
        out["dims"].push_back({{"a", vec2_json(d.a)}, {"b", vec2_json(d.b)}, {"offset", d.offset},
                               {"model_offset", d.model_offset}, {"text", d.text}, {"reference", d.reference}});
    out["hatches"] = json::array();
    for (const auto& h : g.hatches) {
        json pts = json::array();
        for (const auto& p : h.outline) pts.push_back(vec2_json(p));
        out["hatches"].push_back({{"outline", pts}});
    }
    const auto b = g.bbox();
    out["bbox"] = {{"lo", vec2_json(b.lo)}, {"hi", vec2_json(b.hi)}};
    return out;
}

json table_json(const drawing::Table& t) {
    return {{"title", t.title}, {"header", t.header}, {"rows", t.rows}, {"note", t.note}};
}

json rect_json(const drawing::Rect& r) { return {{"x0", r.x0}, {"y0", r.y0}, {"x1", r.x1}, {"y1", r.y1}}; }

assembly::TubeSignatureData sig_from(const json& j) {
    assembly::TubeSignatureData d;
    d.path = path_from_json(need(j, "path"));
    d.od = num(j, "od", 0);
    d.wall = num(j, "wall", 0);
    d.material = str(j, "material");
    d.supply_state = str(j, "supply_state");
    d.bending_tool = str(j, "bending_tool");
    d.coiling_tool = str(j, "coiling_tool");
    d.tooling_instance = str(j, "tooling_instance");
    const std::string f = str(j, "feed", "A");
    d.feed_from = (f == "B" || f == "Б") ? 'B' : 'A';
    d.both_orders = flag(j, "both_orders", false);
    d.cut_allowed = flag(j, "cut_allowed", true);
    if (j.contains("manual_A") && !j.at("manual_A").is_null()) d.manual_A = j.at("manual_A").get<double>();
    if (j.contains("manual_B") && !j.at("manual_B").is_null()) d.manual_B = j.at("manual_B").get<double>();
    d.E_A = num(j, "E_A", 0);
    d.E_B = num(j, "E_B", 0);
    d.insulation = str(j, "insulation");
    d.springback = flag(j, "springback", false);
    if (j.contains("manual_B_st")) {
        for (const auto& [k, v] : j.at("manual_B_st").items()) d.manual_B_st[std::stoi(k)] = v.get<double>();
    }
    return d;
}

drawing::UnfoldOptions unfold_from(const json& u) {
    drawing::UnfoldOptions o;
    if (u.is_null()) return o;
    const std::string f = str(u, "feed", "A");
    o.feed = (f == "B" || f == "Б") ? 'B' : 'A';
    o.end_A.E = num(u, "E_A", 0);
    o.end_B.E = num(u, "E_B", 0);
    if (u.contains("manual_A") && !u.at("manual_A").is_null()) o.end_A.manual = u.at("manual_A").get<double>();
    if (u.contains("manual_B") && !u.at("manual_B").is_null()) o.end_B.manual = u.at("manual_B").get<double>();
    o.N_first = num(u, "N_first", 0);
    o.N_last = num(u, "N_last", 0);
    o.cut_allowed = flag(u, "cut_allowed", true);
    o.dmin = num(u, "dmin", 0);
    o.point_end = flag(u, "point_end", true);
    o.point_mid = flag(u, "point_mid", false);
    o.chain = flag(u, "chain", false);
    o.angles = flag(u, "angles", true);
    o.numbers = flag(u, "numbers", true);
    o.arc_lengths = flag(u, "arc_lengths", false);
    o.direction = profile_from(str(u, "direction", "right"));
    o.minimize = flag(u, "minimize", true);
    if (u.contains("springback_K") && !u.at("springback_K").is_null()) o.springback_K = u.at("springback_K").get<double>();
    const std::string mb = str(u, "mark_base", "arc_start");
    o.mark_base = mb == "vertex" ? drawing::MarkBase::Vertex
                  : mb == "arc_mid" ? drawing::MarkBase::ArcMid
                                    : drawing::MarkBase::ArcStart;
    o.mark_offset = num(u, "mark_offset", 0);
    o.round_step = num(u, "round_step", 0.1);
    return o;
}

}  // namespace

// ---------- путь ----------
Path path_from_json(const json& j) {
    if (!j.is_array()) throw UserError("path должен быть массивом");
    Path p;
    for (const auto& e : j) {
        const std::string kind = str(e, "kind");
        if (kind == "straight") {
            p.push_back(Straight{num(e, "length", 0)});
        } else if (kind == "bend") {
            p.push_back(Bend{num(e, "radius", 0), num(e, "angle", 0), num(e, "rotation", 0)});
        } else if (kind == "spiral") {
            p.push_back(Spiral{num(e, "radius", 0), num(e, "pitch", 0), num(e, "turns", 0),
                               static_cast<int>(num(e, "direction", 1))});
        } else {
            throw UserError("неизвестный элемент пути: " + kind);
        }
    }
    return p;
}

json path_to_json(const Path& p) {
    json out = json::array();
    for (const auto& it : p) {
        if (const auto* s = std::get_if<Straight>(&it)) {
            out.push_back({{"kind", "straight"}, {"length", s->S}});
        } else if (const auto* b = std::get_if<Bend>(&it)) {
            out.push_back({{"kind", "bend"}, {"radius", b->R}, {"angle", b->B}, {"rotation", b->A}});
        } else if (const auto* sp = std::get_if<Spiral>(&it)) {
            out.push_back({{"kind", "spiral"}, {"radius", sp->R}, {"pitch", sp->p}, {"turns", sp->n},
                           {"direction", sp->direction}});
        }
    }
    return out;
}

// ---------- состояние ----------
struct Core::State {
    std::map<std::string, assembly::Registry> registries;  // по id документа
    std::map<std::string, std::function<json(const json&)>> commands;
};

namespace {

json instances_state(const assembly::Registry& r) {
    json inst = json::array();
    for (const auto& id : r.tree_order()) {
        const auto& in = r.instance(id);
        inst.push_back({{"id", id},
                        {"part", in.part_id},
                        {"primary", r.is_primary(id)},
                        {"adaptive", in.adaptive},
                        {"designation", in.designation},
                        {"editable", r.can_edit(id)}});
    }
    json pos = json::array();
    for (const auto& p : r.positions()) {
        pos.push_back({{"number", p.number},
                       {"designation", p.designation},
                       {"instances", p.instances},
                       {"quantity", p.quantity},
                       {"designations_differ", p.designations_differ}});
    }
    return {{"instances", inst}, {"positions", pos}};
}

}  // namespace

Core::Core() : state_(new State) {
    auto& c = state_->commands;
    State* st = state_;

    c["health"] = [](const json&) { return json{{"core", "bendcore"}, {"api", 1}}; };

    // ----- вывод гибов (3.5, 3.6, 7.6) -----
    c["bend.lra"] = [](const json& a) {
        const Path p = path_from_json(need(a, "path"));
        const auto rows = output::lra(p, feed_from(a), num(a, "E_A", 0), num(a, "E_B", 0));
        json out = json::array();
        for (const auto& r : rows)
            out.push_back({{"bend_no", r.bend_no}, {"L", r.L}, {"rotation", r.rotation}, {"angle", r.angle},
                           {"radius", r.radius}, {"is_end", r.is_end}});
        output::CsvOptions co;
        co.precision = static_cast<int>(num(a, "precision", 3));
        co.delimiter = str(a, "delimiter", ";").empty() ? ';' : str(a, "delimiter", ";")[0];
        return json{{"rows", out}, {"csv", output::lra_csv(rows, co)}};
    };
    c["bend.xyz"] = [](const json& a) {
        const Path p = path_from_json(need(a, "path"));
        const auto rows = output::xyz(p, feed_from(a), num(a, "E_A", 0), num(a, "E_B", 0),
                                      frame_from(a.value("frame", json())));
        json out = json::array();
        for (const auto& r : rows) {
            json row{{"label", r.label}, {"bend_no", r.bend_no}, {"p", vec_json(r.p)}};
            row["radius"] = r.radius ? json(*r.radius) : json();
            out.push_back(row);
        }
        output::CsvOptions co;
        co.precision = static_cast<int>(num(a, "precision", 3));
        co.delimiter = str(a, "delimiter", ";").empty() ? ';' : str(a, "delimiter", ";")[0];
        return json{{"rows", out}, {"csv", output::xyz_csv(rows, co)}};
    };
    c["bend.table"] = [](const json& a) {
        const Path p = path_from_json(need(a, "path"));
        const auto t = output::bend_table(p, feed_from(a), profile_from(str(a, "direction", "right")),
                                          flag(a, "minimize", true));
        json rows = json::array();
        for (const auto& r : t.rows)
            rows.push_back({{"bend_no", r.bend_no}, {"S", r.S}, {"A", r.A}, {"B", r.B}, {"side", side_name(r.side)},
                            {"R", r.R}, {"arc", r.arc}});
        return json{{"rows", rows}, {"last_S", t.last_S}, {"finished_length", t.finished_length}};
    };

    // ----- пакетная проверка (4.2, 4.5) -----
    c["check.run"] = [](const json& a) {
        std::vector<check::TubeInput> tubes;
        for (const auto& t : need(a, "tubes")) {
            check::TubeInput in;
            in.id = str(t, "id");
            in.name = str(t, "name", in.id);
            in.path = path_from_json(need(t, "path"));
            in.od = num(t, "od", 0);
            in.B_min = num(t, "B_min", 0);
            in.B_max = num(t, "B_max", 180);
            in.L_min = num(t, "L_min", 0);
            in.N_first = num(t, "N_first", 0);
            in.N_last = num(t, "N_last", 0);
            in.E_A = num(t, "E_A", 0);
            in.E_B = num(t, "E_B", 0);
            in.cut_allowed = flag(t, "cut_allowed", true);
            in.spiral_allowed = flag(t, "spiral_allowed", true);
            if (t.contains("spiral_min_pitch") && !t.at("spiral_min_pitch").is_null())
                in.spiral_min_pitch = t.at("spiral_min_pitch").get<double>();
            in.placement = frame_from(t.value("placement", json()));
            tubes.push_back(in);
        }
        check::Options opt;
        opt.clearance = num(a, "clearance", 5.0);
        opt.tube_to_tube = flag(a, "tube_to_tube", true);
        const auto r = check::run(tubes, opt);
        json v = json::array();
        for (const auto& x : r.violations) {
            v.push_back({{"tube", x.tube_id},
                         {"element", x.element},
                         {"severity", static_cast<int>(x.severity)},
                         {"class", check::severity_name(x.severity)},
                         {"rule", x.rule},
                         {"message", x.message},
                         {"value", x.value ? json(*x.value) : json()},
                         {"limit", x.limit ? json(*x.limit) : json()},
                         {"other", x.other_tube},
                         {"at_s", x.at_s ? json(*x.at_s) : json()}});
        }
        json tubes_out = json::object();
        for (const auto& [id, s] : r.tubes) {
            tubes_out[id] = {{"state", s.state == check::TubeState::Passed ? "passed" : "violations"},
                             {"worst", s.worst ? json(static_cast<int>(*s.worst)) : json()},
                             {"blocks_output", s.blocks_output},
                             {"fingerprint", std::to_string(s.fingerprint)}};
        }
        json counts = json::object();
        for (const auto& [sev, n] : r.counts) counts[std::to_string(static_cast<int>(sev))] = n;
        return json{{"violations", v}, {"tubes", tubes_out}, {"counts", counts}, {"pairs_tested", r.pairs_tested}};
    };

    // ----- экземпляры (5.8, 5.9) -----
    auto reg = [st](const json& a) -> assembly::Registry& { return st->registries[str(a, "doc", "default")]; };
    c["instances.add_part"] = [reg](const json& a) {
        auto& r = reg(a);
        r.add_part(str(a, "part"), str(a, "designation"), sig_from(need(a, "data")));
        return instances_state(r);
    };
    c["instances.insert"] = [reg](const json& a) {
        auto& r = reg(a);
        r.insert(str(a, "part"), str(a, "instance"), frame_from(a.value("placement", json())));
        return instances_state(r);
    };
    c["instances.set_primary"] = [reg](const json& a) {
        auto& r = reg(a);
        r.set_primary(str(a, "instance"));
        return instances_state(r);
    };
    c["instances.detach"] = [reg](const json& a) {
        auto& r = reg(a);
        r.detach(str(a, "instance"), str(a, "new_part"));
        return instances_state(r);
    };
    c["instances.edit_path"] = [reg](const json& a) {
        auto& r = reg(a);
        const auto upd = r.edit_path(str(a, "instance"), path_from_json(need(a, "path")));
        json out = instances_state(r);
        out["updated"] = upd;
        return out;
    };
    c["instances.set_adaptive"] = [reg](const json& a) {
        auto& r = reg(a);
        r.set_adaptive(str(a, "instance"), flag(a, "adaptive", true));
        return instances_state(r);
    };
    c["instances.remove"] = [reg](const json& a) {
        auto& r = reg(a);
        r.remove(str(a, "instance"));
        return instances_state(r);
    };
    c["instances.unify"] = [reg](const json& a) {
        auto& r = reg(a);
        r.unify_designation(need(a, "instances").get<std::vector<std::string>>(), str(a, "designation"));
        return instances_state(r);
    };
    c["instances.state"] = [reg](const json& a) { return instances_state(reg(a)); };
    c["instances.reset"] = [st](const json& a) {
        st->registries.erase(str(a, "doc", "default"));
        return json::object();
    };
    c["instances.compare"] = [](const json& a) {
        const auto m = assembly::compare(sig_from(need(a, "a")), sig_from(need(a, "b")));
        const char* names[] = {"different", "same", "same_reversed", "mirror"};
        return json{{"match", names[static_cast<int>(m)]}};
    };

    // ----- чертёж (7) -----
    c["drawing.part"] = [](const json& a) {
        drawing::DrawingInput in;
        in.path = path_from_json(need(a, "path"));
        in.od = num(a, "od", 0);
        in.wall = num(a, "wall", 0);
        in.standard = standard_from(str(a, "standard", "GOST"));
        in.format = str(a, "format", "A3");
        in.landscape = flag(a, "landscape", true);
        in.preset = str(a, "preset", "minimal") == "full" ? drawing::Preset::Full : drawing::Preset::Minimal;
        const std::string dm = str(a, "dims", "S");
        in.dims = dm == "L" ? drawing::DimMode::L : dm == "table" ? drawing::DimMode::TableOnly : drawing::DimMode::S;
        in.unfold = unfold_from(a.value("unfold", json()));
        in.precision = static_cast<int>(num(a, "precision", 1));
        in.has_errors = flag(a, "has_errors", false);
        if (a.contains("title")) in.title = a.at("title").get<std::map<std::string, std::string>>();
        const auto sheets = drawing::make_part_drawing(in);
        json out_sheets = json::array();
        for (const auto& ds : sheets) {
            json views = json::array();
            for (const auto& v : ds.views)
                views.push_back({{"id", v.id}, {"kind", kind_name(v.kind)}, {"label", v.label}, {"scale", v.scale},
                                 {"origin", vec2_json(v.origin)}, {"geom", geometry_json(v.geom)}});
            json tables = json::array();
            for (const auto& t : ds.tables)
                tables.push_back({{"table", table_json(t.table)}, {"top_left", vec2_json(t.top_left)}, {"width", t.width}});
            out_sheets.push_back({{"number", ds.number},
                                  {"count", ds.count},
                                  {"standard", str(a, "standard", "GOST")},
                                  {"projection", ds.projection == drawing::Projection::FirstAngle ? "first" : "third"},
                                  {"format", {{"name", ds.frame.format.name}, {"w", ds.frame.format.w}, {"h", ds.frame.format.h}}},
                                  {"border", rect_json(ds.frame.border)},
                                  {"title_block", rect_json(ds.frame.title_block)},
                                  {"views", views},
                                  {"tables", tables},
                                  {"title", ds.title},
                                  {"not_for_production", ds.not_for_production},
                                  {"warnings", ds.warnings}});
        }
        return json{{"sheets", out_sheets}};
    };
    c["drawing.release_check"] = [](const json& a) {
        drawing::ReleaseInputs in;
        in.tech_or_geom_errors = flag(a, "tech_or_geom_errors", false);
        in.lost_dim_refs = static_cast<int>(num(a, "lost_dim_refs", 0));
        in.unfold_sheet_changed = flag(a, "unfold_sheet_changed", false);
        in.stale_views = static_cast<int>(num(a, "stale_views", 0));
        in.missing_required_props = static_cast<int>(num(a, "missing_required_props", 0));
        const auto s = str(a, "strictness", "strict") == "soft" ? drawing::Strictness::Soft : drawing::Strictness::Strict;
        const auto r = drawing::release_check(in, s);
        return json{{"blocking", r.blocking},
                    {"warnings", r.warnings},
                    {"can_release", r.can_release},
                    {"not_for_production", r.not_for_production},
                    {"offer_update_and_release", r.offer_update_and_release},
                    {"can_unrelease", drawing::can_unrelease(str(a, "role"))}};
    };
}

Core::~Core() { delete state_; }

json Core::call(const std::string& command, const json& args) {
    const auto it = state_->commands.find(command);
    if (it == state_->commands.end()) return {{"ok", false}, {"error", "неизвестная команда: " + command}};
    try {
        return {{"ok", true}, {"data", it->second(args.is_null() ? json::object() : args)}};
    } catch (const std::exception& e) {
        // Ошибки ядра — понятный текст для пользователя; процесс продолжает работать.
        return {{"ok", false}, {"error", e.what()}};
    }
}

std::string Core::handle_line(const std::string& line) {
    json req;
    try {
        req = json::parse(line);
    } catch (const std::exception& e) {
        return json{{"id", nullptr}, {"ok", false}, {"error", std::string("неверный JSON: ") + e.what()}}.dump();
    }
    json out = call(req.value("command", ""), req.value("args", json::object()));
    out["id"] = req.value("id", json());
    return out.dump();
}

}  // namespace bendcore::api
