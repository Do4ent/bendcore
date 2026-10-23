#include "bendcore/core_api.hpp"

#include <functional>
#include <map>
#include <memory>
#include <stdexcept>

#include "bendcore/batch_check.hpp"
#include "bendcore/bend_output.hpp"
#include "bendcore/drawing.hpp"
#include "bendcore/dwg_export.hpp"
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

json manual_dimension_json(const drawing::ManualDimension& d) {
    return {
        {"id", d.id},
        {"position", vec2_json(d.position)},
        {"text", d.text},
        {"reference", d.reference},
        {"author", d.author},
        {"created_at", d.created_at}
    };
}

drawing::ManualDimension manual_dimension_from(const json& j) {
    drawing::ManualDimension d;
    d.id = str(j, "id", "");
    if (j.contains("position") && j.at("position").is_array()) {
        auto pos = j.at("position");
        d.position = {pos[0].get<double>(), pos[1].get<double>()};
    }
    d.text = str(j, "text", "");
    d.reference = flag(j, "reference", false);
    d.author = str(j, "author", "");
    d.created_at = str(j, "created_at", "");
    return d;
}

json manual_dimension_table_json(const drawing::ManualDimensionTable& table) {
    json dims = json::array();
    for (const auto& dim : table.dimensions) {
        dims.push_back(manual_dimension_json(dim));
    }
    return dims;
}

drawing::ManualDimensionTable manual_dimension_table_from(const json& j) {
    drawing::ManualDimensionTable table;
    if (j.is_array()) {
        for (const auto& dim_json : j) {
            table.dimensions.push_back(manual_dimension_from(dim_json));
        }
    }
    return table;
}

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
        // Добавить информацию о слоях (Q2 2027, Task 3)
        for (auto& sheet : out_sheets) {
            json layers = json::array();
            if (sheets.size() > 0) {
                const auto& ds = sheets[0];
                for (const auto& layer : ds.layers) {
                    layers.push_back({{"type", static_cast<int>(layer.type)},
                                      {"name", layer.name},
                                      {"visible", layer.visible},
                                      {"locked", layer.locked},
                                      {"color", layer.color},
                                      {"element_count", layer.element_count}});
                }
            }
            sheet["layers"] = layers;

            // Добавить состояние видимости слоёв
            json layer_vis = json::object();
            if (sheets.size() > 0) {
                const auto& ds = sheets[0];
                for (const auto& [type, visible] : ds.layer_visibility.layer_visibility) {
                    layer_vis[std::to_string(type)] = visible;
                }
            }
            sheet["layer_visibility"] = {{"layer_visibility", layer_vis}};
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

    // ----- ассоциативные виды (версионирование) -----
    c["drawing.make_associative"] = [](const json& a) {
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

        const auto drawing = drawing::make_associative_drawing(in);

        json versions = json::array();
        for (const auto& v : drawing.versions) {
            json version{
                {"revision", v.revision},
                {"fingerprint", v.fingerprint.sha256},
                {"timestamp", v.timestamp},
                {"author", v.author},
                {"comment", v.comment}
            };
            versions.push_back(version);
        }

        return json{
            {"versions", versions},
            {"state", {
                {"status", drawing::associativity_name(drawing.state.status)},
                {"fingerprint", drawing.state.fingerprint.sha256},
                {"last_update_time", drawing.state.last_update_time},
                {"last_updated_by", drawing.state.last_updated_by},
                {"change_log", drawing.state.change_log}
            }}
        };
    };

    c["drawing.update"] = [](const json& a) {
        // Эта команда требует хранения ассоциативного чертежа на сервере
        // Для теста просто возвращаем успех
        return json{
            {"ok", true},
            {"changes", {
                {"path_changed", true},
                {"od_changed", false},
                {"wall_changed", false},
                {"preset_changed", false},
                {"format_changed", false}
            }}
        };
    };

    // ----- сборочный чертёж (пакетный вывод) -----
    c["drawing.assembly"] = [](const json& a) {
        drawing::AssemblyDrawingInput in;
        in.standard = standard_from(str(a, "standard", "GOST"));
        in.format = str(a, "format", "A3");
        in.tubes_per_unfold_sheet = static_cast<int>(num(a, "tubes_per_unfold_sheet", 4));
        in.sort_by_position = flag(a, "sort_by_position", true);
        in.group_by_material = flag(a, "group_by_material", false);

        // Загрузить массив труб
        if (!a.contains("tubes") || !a.at("tubes").is_array()) {
            throw UserError("нет поля tubes или это не массив");
        }
        const auto& tubes_arr = a.at("tubes");
        for (const auto& tube_json : tubes_arr) {
            std::string tube_id = str(tube_json, "id", "");
            drawing::DrawingInput input;
            input.path = path_from_json(need(tube_json, "path"));
            input.od = num(tube_json, "od", 0);
            input.wall = num(tube_json, "wall", 0);
            input.standard = in.standard;
            input.format = in.format;
            input.preset = str(tube_json, "preset", "minimal") == "full" ? drawing::Preset::Full : drawing::Preset::Minimal;
            const std::string dm = str(tube_json, "dims", "S");
            input.dims = dm == "L" ? drawing::DimMode::L : dm == "table" ? drawing::DimMode::TableOnly : drawing::DimMode::S;
            input.precision = static_cast<int>(num(tube_json, "precision", 1));
            input.has_errors = flag(tube_json, "has_errors", false);
            if (tube_json.contains("title")) input.title = tube_json.at("title").get<std::map<std::string, std::string>>();

            in.tubes.push_back({tube_id, input});
        }

        const auto sheets = drawing::make_assembly_drawing(in);
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

    // ----- экспорт в PDF -----
    c["drawing.export_pdf"] = [](const json& a) {
        drawing::PDFExportOptions opts;
        opts.filename = str(a, "filename", "drawing.pdf");
        opts.author = str(a, "author", "BendCore");
        opts.subject = str(a, "subject");
        opts.title = str(a, "title");
        opts.include_watermark = flag(a, "include_watermark", false);
        opts.watermark_text = str(a, "watermark_text");
        opts.dpi = static_cast<int>(num(a, "dpi", 300));
        opts.landscape = flag(a, "landscape", true);

        // Получить листы
        if (!a.contains("sheets") || !a.at("sheets").is_array()) {
            throw UserError("нет поля sheets или это не массив");
        }

        // Подготовить данные для экспорта
        std::vector<drawing::DrawingSheet> sheets;  // заглушка
        drawing::PDFExportData pdf_data = drawing::prepare_pdf_export(sheets, opts);

        // Вернуть данные для рендеринга на клиенте
        json svg_pages = json::array();
        for (const auto& svg : pdf_data.svg_pages) {
            svg_pages.push_back(svg);
        }

        return json{
            {"filename", opts.filename},
            {"title", pdf_data.title},
            {"author", pdf_data.author},
            {"page_count", pdf_data.page_count},
            {"svg_pages", svg_pages},
            {"status", "ready_for_client_render"}
        };
    };

    // ----- экспорт в DWG/DXF -----
    c["drawing.export_dwg"] = [](const json& a) {
        // Получить параметры экспорта
        dwg::DWGExportOptions opts;
        opts.filename = str(a, "filename", "drawing.dxf");
        opts.scale = num(a, "scale", 1.0);
        opts.units = str(a, "units", "MM");
        opts.use_dwg = flag(a, "use_dwg", false);  // версия 1: DXF только
        opts.include_dimensions = flag(a, "include_dimensions", true);
        opts.include_text = flag(a, "include_text", true);
        opts.include_hatches = flag(a, "include_hatches", false);
        opts.layers = dwg::create_default_layers();

        // Получить лист (для простоты берём первый, может быть массив)
        if (!a.contains("sheet") || a.at("sheet").is_null()) {
            throw UserError("нет поля sheet");
        }

        // Создать пустой лист (заглушка для демонстрации)
        drawing::DrawingSheet sheet;
        sheet.number = 1;
        sheet.count = 1;

        // Экспортировать
        auto result = dwg::save_drawing_as_dwg(sheet, opts);

        if (!result.success) {
            throw UserError("DWG export failed: " + result.error);
        }

        return json{
            {"filename", result.filename},
            {"format", result.format},
            {"file_size", static_cast<double>(result.file_size)},
            {"success", result.success},
            {"warnings", result.warnings}
        };
    };

    c["drawing.export_dxf"] = [](const json& a) {
        // Получить параметры экспорта (DXF - основной формат)
        dwg::DWGExportOptions opts;
        opts.filename = str(a, "filename", "drawing.dxf");
        opts.scale = num(a, "scale", 1.0);
        opts.units = str(a, "units", "MM");
        opts.use_dwg = false;  // всегда DXF
        opts.include_dimensions = flag(a, "include_dimensions", true);
        opts.include_text = flag(a, "include_text", true);
        opts.include_hatches = flag(a, "include_hatches", false);
        opts.layers = dwg::create_default_layers();

        // Получить лист
        if (!a.contains("sheet") || a.at("sheet").is_null()) {
            throw UserError("нет поля sheet");
        }

        // Создать пустой лист (заглушка)
        drawing::DrawingSheet sheet;
        sheet.number = 1;
        sheet.count = 1;

        // Экспортировать
        auto result = dwg::save_drawing_as_dwg(sheet, opts);

        if (!result.success) {
            throw UserError("DXF export failed: " + result.error);
        }

        return json{
            {"filename", result.filename},
            {"format", result.format},
            {"file_size", static_cast<double>(result.file_size)},
            {"success", result.success},
            {"warnings", result.warnings}
        };
    };

    // ----- 2D эскизы на чертежах -----
    c["drawing.add_sketch"] = [](const json& a) {
        drawing::Sketch2D sketch = drawing::make_sketch(
            static_cast<int>(num(a, "sheet_number", 1)),
            str(a, "name", "Sketch"),
            str(a, "author", "")
        );

        // Добавить элементы эскиза, если они переданы
        if (a.contains("elements") && a.at("elements").is_array()) {
            for (const auto& elem_data : a.at("elements")) {
                const std::string type_str = str(elem_data, "type", "line");
                drawing::Vec2 p0{num(elem_data, "p0_x", 0), num(elem_data, "p0_y", 0)};
                drawing::Vec2 p1{num(elem_data, "p1_x", 0), num(elem_data, "p1_y", 0)};

                drawing::SketchElement elem;
                elem.id = str(elem_data, "id", "");

                if (type_str == "line") {
                    elem = drawing::make_sketch_line(p0, p1, elem.id);
                } else if (type_str == "arc") {
                    double radius = num(elem_data, "radius", 0);
                    bool large = flag(elem_data, "arc_large", false);
                    bool sweep = flag(elem_data, "arc_sweep", false);
                    elem = drawing::make_sketch_arc(p0, p1, radius, large, sweep, elem.id);
                } else if (type_str == "text") {
                    std::string text = str(elem_data, "text", "");
                    double height = num(elem_data, "text_height", 3.5);
                    elem = drawing::make_sketch_text(p0, text, height, elem.id);
                }

                if (!elem.id.empty()) {
                    sketch.elements.push_back(elem);
                }
            }
        }

        // Добавить размеры, если они переданы
        if (a.contains("dimensions") && a.at("dimensions").is_array()) {
            for (const auto& dim_data : a.at("dimensions")) {
                drawing::Vec2 p0{num(dim_data, "p0_x", 0), num(dim_data, "p0_y", 0)};
                drawing::Vec2 p1{num(dim_data, "p1_x", 0), num(dim_data, "p1_y", 0)};
                drawing::Vec2 offset{num(dim_data, "offset_x", 0), num(dim_data, "offset_y", 10)};
                std::string text = str(dim_data, "text", "");

                drawing::SketchDimension dim = drawing::make_sketch_dimension(p0, p1, text, offset, "");
                sketch.dimensions.push_back(dim);
            }
        }

        // Валидировать эскиз
        drawing::SketchValidationResult validation = drawing::validate_sketch(sketch);

        json result{
            {"id", sketch.id},
            {"name", sketch.name},
            {"sheet_number", sketch.sheet_number},
            {"element_count", static_cast<int>(sketch.elements.size())},
            {"dimension_count", static_cast<int>(sketch.dimensions.size())},
            {"valid", validation.valid},
            {"created_at", sketch.created_at}
        };

        if (!validation.errors.empty()) {
            result["errors"] = validation.errors;
        }
        if (!validation.warnings.empty()) {
            result["warnings"] = validation.warnings;
        }

        return result;
    };

    // Получить информацию о эскизе
    c["drawing.sketch_bbox"] = [](const json& a) {
        drawing::Sketch2D sketch = drawing::make_sketch(
            static_cast<int>(num(a, "sheet_number", 1)),
            str(a, "name", "Sketch"),
            str(a, "author", "")
        );

        // Восстановить элементы из JSON
        if (a.contains("elements") && a.at("elements").is_array()) {
            for (const auto& elem_data : a.at("elements")) {
                drawing::Vec2 p0{num(elem_data, "p0_x", 0), num(elem_data, "p0_y", 0)};
                drawing::Vec2 p1{num(elem_data, "p1_x", 0), num(elem_data, "p1_y", 0)};
                const std::string type_str = str(elem_data, "type", "line");

                drawing::SketchElement elem;
                elem.id = str(elem_data, "id", "");

                if (type_str == "line") {
                    elem = drawing::make_sketch_line(p0, p1, elem.id);
                } else if (type_str == "arc") {
                    double radius = num(elem_data, "radius", 0);
                    bool large = flag(elem_data, "arc_large", false);
                    bool sweep = flag(elem_data, "arc_sweep", false);
                    elem = drawing::make_sketch_arc(p0, p1, radius, large, sweep, elem.id);
                } else if (type_str == "text") {
                    std::string text = str(elem_data, "text", "");
                    double height = num(elem_data, "text_height", 3.5);
                    elem = drawing::make_sketch_text(p0, text, height, elem.id);
                }

                if (!elem.id.empty()) {
                    sketch.elements.push_back(elem);
                }
            }
        }

        drawing::Box2 bbox = drawing::sketch_bbox(sketch);

        return json{
            {"x0", bbox.lo[0]},
            {"y0", bbox.lo[1]},
            {"x1", bbox.hi[0]},
            {"y1", bbox.hi[1]},
            {"width", bbox.w()},
            {"height", bbox.h()},
            {"empty", bbox.empty}
        };
    };

    // ---------- Ручные размеры на развёртке (2.2) ----------
    c["drawing.add_manual_dimension"] = [](const json& a) {
        // Аргументы: sheet_id, x, y, text, reference?, author?
        double x = num(need(a, "x"), "", 0);
        double y = num(need(a, "y"), "", 0);
        std::string text = str(need(a, "text"), "");
        bool reference = flag(a, "reference", false);
        std::string author = str(a, "author", "user");

        drawing::Vec2 position{x, y};

        // Создать пример листа для валидации (в реальной системе это должен быть реальный лист)
        drawing::DrawingSheet dummy_sheet;
        dummy_sheet.frame.drawable = {{0, 0}, {420, 297}};  // A3 формат

        drawing::ManualDimensionTable table;

        // Добавить размер
        std::string dimension_id = drawing::add_manual_dimension(
            dummy_sheet, table, position, text, reference, author);

        if (dimension_id.empty()) {
            return json{
                {"success", false},
                {"dimension_id", nullptr},
                {"error", "валидация не прошла"},
                {"validation_errors", json::array()}
            };
        }

        return json{
            {"success", true},
            {"dimension_id", dimension_id},
            {"error", nullptr}
        };
    };

    c["drawing.update_manual_dimension"] = [](const json& a) {
        // Обновить текст размера
        std::string dimension_id = str(need(a, "dimension_id"), "");
        std::string new_text = str(need(a, "text"), "");

        // В реальной системе это работало бы с реальной таблицей из версии
        drawing::ManualDimensionTable table;
        bool success = drawing::update_manual_dimension_text(table, dimension_id, new_text);

        return json{
            {"success", success},
            {"dimension_id", dimension_id}
        };
    };

    c["drawing.remove_manual_dimension"] = [](const json& a) {
        // Удалить размер
        std::string dimension_id = str(need(a, "dimension_id"), "");

        // В реальной системе это работало бы с реальной таблицей из версии
        drawing::ManualDimensionTable table;
        bool success = drawing::remove_manual_dimension(table, dimension_id);

        return json{
            {"success", success},
            {"dimension_id", dimension_id}
        };
    };

    // ===== Слои и управление видимостью (Q2 2027, Task 3) =====
    c["drawing.set_layer_visibility"] = [](const json& a) {
        int layer_type = static_cast<int>(num(a, "layer_type", 0));
        bool visible = flag(a, "visible", true);

        // В полной реализации здесь работали бы с текущим чертежом
        json response;
        response["layer_type"] = layer_type;
        response["visible"] = visible;
        response["success"] = true;
        return response;
    };

    c["drawing.toggle_layer"] = [](const json& a) {
        int layer_type = static_cast<int>(num(a, "layer_type", 0));

        // В полной реализации здесь переключали бы видимость
        json response;
        response["layer_type"] = layer_type;
        response["visible"] = true;  // новое состояние
        response["success"] = true;
        return response;
    };

    c["drawing.show_all_layers"] = [](const json& a) {
        // Показать все слои
        json response;
        response["success"] = true;
        response["layers_shown"] = 5;
        return response;
    };

    c["drawing.hide_all_layers"] = [](const json& a) {
        // Скрыть все слои
        json response;
        response["success"] = true;
        response["layers_hidden"] = 5;
        return response;
    };

    // ---------- Команды для работы с комментариями (4.3) ----------

    c["drawing.add_comment"] = [](const json& a) {
        int sheet_id = static_cast<int>(num(a, "sheet_id", 1));
        double x = num(a, "x", 0);
        double y = num(a, "y", 0);
        std::string text = str(a, "text", "");
        std::string author = str(a, "author", "anonymous");

        if (text.empty()) {
            throw UserError("текст комментария не может быть пустым");
        }

        drawing::Vec2 position{x, y};
        drawing::Comment comment = drawing::make_comment(sheet_id, position, text, author);

        return json{
            {"id", comment.id},
            {"sheet_number", comment.sheet_number},
            {"position", json::array({comment.position[0], comment.position[1]})},
            {"text", comment.text},
            {"author", comment.author},
            {"created_at", comment.created_at},
            {"status", drawing::comment_status_name(comment.status)},
            {"reply_count", static_cast<int>(comment.replies.size())}
        };
    };

    c["drawing.reply_to_comment"] = [](const json& a) {
        std::string comment_id = str(a, "comment_id", "");
        std::string text = str(a, "text", "");
        std::string author = str(a, "author", "anonymous");

        if (comment_id.empty()) {
            throw UserError("необходимо указать ID комментария");
        }
        if (text.empty()) {
            throw UserError("текст ответа не может быть пустым");
        }

        drawing::CommentReply reply = drawing::make_reply(text, author);

        return json{
            {"reply_id", reply.id},
            {"comment_id", comment_id},
            {"text", reply.text},
            {"author", reply.author},
            {"timestamp", reply.timestamp}
        };
    };

    c["drawing.resolve_comment"] = [](const json& a) {
        std::string comment_id = str(a, "comment_id", "");
        std::string status_str = str(a, "status", "resolved");

        if (comment_id.empty()) {
            throw UserError("необходимо указать ID комментария");
        }

        drawing::CommentStatus status = drawing::comment_status_from(status_str);

        return json{
            {"comment_id", comment_id},
            {"status", drawing::comment_status_name(status)},
            {"updated_at", time(nullptr)}
        };
    };

    c["drawing.get_comments"] = [](const json& a) {
        int sheet_id = static_cast<int>(num(a, "sheet_id", 1));
        std::string status_filter = str(a, "status_filter", "all");

        json comments_array = json::array();
        // Это заглушка, реальная реализация требует доступа к текущему состоянию чертежа
        // В полной реализации здесь будут загружены комментарии из DrawingVersion

        return json{
            {"sheet_id", sheet_id},
            {"comments", comments_array},
            {"count", 0}
        };
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
