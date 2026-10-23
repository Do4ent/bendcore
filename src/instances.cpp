#include "bendcore/instances.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace bendcore::assembly {
namespace {

double angle_diff(double a, double b) {
    double d = std::fmod(std::abs(a - b), 360.0);
    return d > 180.0 ? 360.0 - d : d;
}

bool near_opt(const std::optional<double>& a, const std::optional<double>& b, double tol) {
    if (a.has_value() != b.has_value()) return false;
    return !a || std::abs(*a - *b) <= tol;
}

// Параметры, не зависящие от концов.
bool same_spec(const TubeSignatureData& a, const TubeSignatureData& b, double tol) {
    return std::abs(a.od - b.od) <= tol && std::abs(a.wall - b.wall) <= tol && a.material == b.material &&
           a.supply_state == b.supply_state && a.bending_tool == b.bending_tool &&
           a.coiling_tool == b.coiling_tool && a.tooling_instance == b.tooling_instance &&
           a.both_orders == b.both_orders && a.cut_allowed == b.cut_allowed && a.insulation == b.insulation &&
           a.springback == b.springback;
}

bool same_lra(const std::vector<output::LraRow>& a, const std::vector<output::LraRow>& b, double tl, double td,
              bool mirror) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].is_end != b[i].is_end) return false;
        if (std::abs(a[i].L - b[i].L) > tl) return false;
        if (a[i].is_end) continue;
        if (std::abs(a[i].radius - b[i].radius) > tl) return false;
        if (std::abs(a[i].angle - b[i].angle) > td) return false;
        const double rb = mirror ? -b[i].rotation : b[i].rotation;
        if (angle_diff(a[i].rotation, rb) > td) return false;
    }
    return true;
}

// Пути со спиралью сравниваются поэлементно только в прямом порядке.
bool same_items(const Path& a, const Path& b, double tl, double td) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].index() != b[i].index()) return false;
        if (const auto* s = std::get_if<Straight>(&a[i])) {
            if (std::abs(s->S - std::get<Straight>(b[i]).S) > tl) return false;
        } else if (const auto* x = std::get_if<Bend>(&a[i])) {
            const auto& y = std::get<Bend>(b[i]);
            if (std::abs(x->R - y.R) > tl || std::abs(x->B - y.B) > td || angle_diff(x->A, y.A) > td) return false;
        } else {
            const auto& x2 = std::get<Spiral>(a[i]);
            const auto& y2 = std::get<Spiral>(b[i]);
            if (std::abs(x2.R - y2.R) > tl || std::abs(x2.p - y2.p) > tl || std::abs(x2.n - y2.n) > 1e-9 ||
                x2.direction != y2.direction)
                return false;
        }
    }
    return true;
}

bool same_map(const std::map<int, double>& a, const std::map<int, double>& b, double td) {
    if (a.size() != b.size()) return false;
    for (const auto& [k, v] : a) {
        const auto it = b.find(k);
        if (it == b.end() || std::abs(it->second - v) > td) return false;
    }
    return true;
}

int bend_count(const Path& p) {
    int n = 0;
    for (const auto& it : p) {
        if (const auto* b = std::get_if<Bend>(&it)) n += std::abs(b->B) >= kDegenerateBDeg;
    }
    return n;
}

}  // namespace

Match compare(const TubeSignatureData& a, const TubeSignatureData& b, double tl, double td) {
    if (!same_spec(a, b, tl)) return Match::Different;

    std::vector<output::LraRow> la, lb_fwd, lb_rev;
    bool lra_ok = true;
    try {
        la = output::lra(a.path);
        lb_fwd = output::lra(b.path);
        lb_rev = output::lra(b.path, output::Feed::B);
    } catch (const output::OutputError&) {
        lra_ok = false;
    }
    if (!lra_ok) {
        const bool fwd = a.feed_from == b.feed_from && near_opt(a.manual_A, b.manual_A, tl) &&
                         near_opt(a.manual_B, b.manual_B, tl) && std::abs(a.E_A - b.E_A) <= tl &&
                         std::abs(a.E_B - b.E_B) <= tl && same_map(a.manual_B_st, b.manual_B_st, td) &&
                         same_items(a.path, b.path, tl, td);
        return fwd ? Match::Same : Match::Different;
    }

    // Прямое совпадение: концы А↔А.
    const bool ends_fwd = a.feed_from == b.feed_from && near_opt(a.manual_A, b.manual_A, tl) &&
                          near_opt(a.manual_B, b.manual_B, tl) && std::abs(a.E_A - b.E_A) <= tl &&
                          std::abs(a.E_B - b.E_B) <= tl && same_map(a.manual_B_st, b.manual_B_st, td);
    // Обратное: труба b развёрнута концами, параметры концов и номера гибов переставляются.
    std::map<int, double> b_st_rev;
    const int nb = bend_count(b.path);
    for (const auto& [k, v] : b.manual_B_st) b_st_rev[nb + 1 - k] = v;
    const char feed_rev = b.feed_from == 'A' ? 'B' : 'A';
    const bool ends_rev = a.feed_from == feed_rev && near_opt(a.manual_A, b.manual_B, tl) &&
                          near_opt(a.manual_B, b.manual_A, tl) && std::abs(a.E_A - b.E_B) <= tl &&
                          std::abs(a.E_B - b.E_A) <= tl && same_map(a.manual_B_st, b_st_rev, td);

    if (ends_fwd && same_lra(la, lb_fwd, tl, td, false)) return Match::Same;
    if (ends_rev && same_lra(la, lb_rev, tl, td, false)) return Match::SameReversed;
    if ((ends_fwd && same_lra(la, lb_fwd, tl, td, true)) || (ends_rev && same_lra(la, lb_rev, tl, td, true)))
        return Match::Mirror;
    return Match::Different;
}

// ---------- Registry ----------

Instance& Registry::inst(const std::string& id) {
    const auto it = instances_.find(id);
    if (it == instances_.end()) throw InstanceError("нет экземпляра " + id);
    return it->second;
}

const Instance& Registry::instance(const std::string& id) const {
    const auto it = instances_.find(id);
    if (it == instances_.end()) throw InstanceError("нет экземпляра " + id);
    return it->second;
}

const Part& Registry::part(const std::string& id) const {
    const auto it = parts_.find(id);
    if (it == parts_.end()) throw InstanceError("нет детали " + id);
    return it->second;
}

void Registry::add_part(const std::string& part_id, const std::string& designation, const TubeSignatureData& data) {
    if (part_id.empty()) throw InstanceError("пустой id детали");
    if (parts_.count(part_id)) throw InstanceError("деталь уже есть: " + part_id);
    Part p;
    p.id = part_id;
    p.designation = designation;
    p.data = data;
    parts_[part_id] = p;
}

void Registry::insert(const std::string& part_id, const std::string& instance_id, const output::Frame& placement) {
    auto pit = parts_.find(part_id);
    if (pit == parts_.end()) throw InstanceError("нет детали " + part_id);
    if (instance_id.empty() || instances_.count(instance_id)) throw InstanceError("id экземпляра занят: " + instance_id);
    output::validate_frame(placement);
    Instance in;
    in.id = instance_id;
    in.part_id = part_id;
    in.placement = placement;
    in.designation = pit->second.designation;
    instances_[instance_id] = in;
    pit->second.instances.push_back(instance_id);
    if (pit->second.primary.empty()) pit->second.primary = instance_id;
    order_.push_back(instance_id);
}

void Registry::remove(const std::string& instance_id) {
    Instance& in = inst(instance_id);
    Part& p = parts_.at(in.part_id);
    const bool was_adaptive = in.adaptive;
    p.instances.erase(std::find(p.instances.begin(), p.instances.end(), instance_id));
    order_.erase(std::find(order_.begin(), order_.end(), instance_id));
    if (p.primary == instance_id) {
        p.primary = p.instances.empty() ? std::string() : p.instances.front();
        // Роль главного переходит к следующему экземпляру; адаптивность — вместе с ролью.
        if (!p.primary.empty()) instances_.at(p.primary).adaptive = was_adaptive;
    }
    instances_.erase(instance_id);
}

bool Registry::is_primary(const std::string& instance_id) const {
    const Instance& in = instance(instance_id);
    return parts_.at(in.part_id).primary == instance_id;
}

const std::string& Registry::primary_of(const std::string& part_id) const { return part(part_id).primary; }

void Registry::set_primary(const std::string& instance_id) {
    Instance& in = inst(instance_id);
    Part& p = parts_.at(in.part_id);
    if (p.primary == instance_id) return;
    Instance& old = instances_.at(p.primary);
    in.adaptive = old.adaptive;
    old.adaptive = false;
    p.primary = instance_id;
}

std::vector<std::string> Registry::edit_path(const std::string& instance_id, const Path& path) {
    if (!is_primary(instance_id)) {
        throw InstanceError("копия только для чтения: правьте главный экземпляр " +
                            primary_of(instance(instance_id).part_id) + " или отвяжите копию");
    }
    Part& p = parts_.at(instance(instance_id).part_id);
    p.data.path = path;
    return p.instances;
}

void Registry::set_adaptive(const std::string& instance_id, bool adaptive) {
    if (adaptive && !is_primary(instance_id)) {
        throw InstanceError("адаптивной может быть только главная труба детали");
    }
    inst(instance_id).adaptive = adaptive;
}

void Registry::detach(const std::string& instance_id, const std::string& new_part_id) {
    Instance& in = inst(instance_id);
    Part& old = parts_.at(in.part_id);
    if (old.instances.size() == 1) throw InstanceError("экземпляр единственный: отвязывать нечего");
    if (parts_.count(new_part_id)) throw InstanceError("деталь уже есть: " + new_part_id);
    // Сначала убираем из старой детали (с передачей роли главного), затем создаём новую.
    const bool was_primary = old.primary == instance_id;
    const bool was_adaptive = in.adaptive;
    old.instances.erase(std::find(old.instances.begin(), old.instances.end(), instance_id));
    if (was_primary) {
        old.primary = old.instances.front();
        instances_.at(old.primary).adaptive = was_adaptive;
    }
    Part np;
    np.id = new_part_id;
    np.designation = old.designation;  // совпадёт с исходной позицией по сигнатуре, пока не изменят
    np.data = old.data;
    np.primary = instance_id;
    np.instances = {instance_id};
    parts_[new_part_id] = np;
    in.part_id = new_part_id;
    in.adaptive = false;  // независимая копия по умолчанию заморожена (9.7)
}

void Registry::unify_designation(const std::vector<std::string>& instance_ids, const std::string& designation) {
    std::set<std::string> touched;
    for (const auto& id : instance_ids) touched.insert(instance(id).part_id);
    for (const auto& pid : touched) {
        Part& p = parts_.at(pid);
        p.designation = designation;
        for (const auto& i : p.instances) instances_.at(i).designation = designation;
    }
}

std::vector<Position> Registry::positions() const {
    // Сравнение дорогое (LRA), поэтому трубы сначала раскладываются по корзинам с точными ключами,
    // которые у одинаковых труб обязаны совпадать; полное сравнение — только внутри корзины.
    auto bucket_key = [](const Part& p) {
        return std::to_string(bend_count(p.data.path)) + "|" + p.data.material + "|" + p.data.supply_state + "|" +
               p.data.bending_tool + "|" + p.data.tooling_instance + "|" + p.data.insulation + "|" +
               (p.data.springback ? "1" : "0");
    };
    std::vector<Position> out;
    std::map<std::string, std::vector<std::size_t>> buckets;  // ключ → номера позиций
    std::vector<std::string> rep_part;                        // деталь-представитель позиции
    std::map<std::string, std::size_t> part_pos;              // деталь → позиция (экземпляры одной детали)
    for (const auto& id : order_) {
        const Instance& in = instances_.at(id);
        const Part& p = parts_.at(in.part_id);
        std::optional<std::size_t> found;
        if (const auto it = part_pos.find(p.id); it != part_pos.end()) {
            found = it->second;
        } else {
            auto& bucket = buckets[bucket_key(p)];
            for (std::size_t k : bucket) {
                const Match m = compare(parts_.at(rep_part[k]).data, p.data);
                if (m == Match::Same || m == Match::SameReversed) {
                    found = k;
                    break;
                }
            }
            if (!found) {
                Position pos;
                pos.number = static_cast<int>(out.size()) + 1;
                pos.designation = in.designation;
                out.push_back(pos);
                rep_part.push_back(p.id);
                found = out.size() - 1;
                bucket.push_back(*found);
            }
            part_pos[p.id] = *found;
        }
        Position& pos = out[*found];
        pos.instances.push_back(id);
        pos.quantity++;
        if (in.designation != pos.designation) pos.designations_differ = true;
    }
    return out;
}

}  // namespace bendcore::assembly
