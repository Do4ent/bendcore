#include "bendcore/bom.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <sstream>

namespace bendcore {

namespace {

// Форматировать EndRule в строку подписи.
std::string rule_str(const EndRule& rule) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "E%.1f_N%.1f_%d", rule.E, rule.N, rule.cut_allowed ? 1 : 0);
    return buf;
}

// Форматировать Path в строку подписи.
std::string path_str(const Path& path) {
    std::string out;
    for (std::size_t i = 0; i < path.size(); ++i) {
        const auto& item = path[i];
        if (const auto* s = std::get_if<Straight>(&item)) {
            char buf[32];
            std::snprintf(buf, sizeof buf, "S%.1f;", s->S);
            out += buf;
        } else if (const auto* bend = std::get_if<Bend>(&item)) {
            char buf[64];
            std::snprintf(buf, sizeof buf, "B%.1f_%.1f_%.1f;", bend->R, bend->B, bend->A);
            out += buf;
        } else if (const auto* spiral = std::get_if<Spiral>(&item)) {
            char buf[64];
            std::snprintf(buf, sizeof buf, "L%.1f_%.1f_%.1f_%d;", spiral->R, spiral->p, spiral->n,
                          spiral->direction);
            out += buf;
        }
    }
    return out;
}

}  // namespace

TubeSignature tube_signature(double od, double wall, const Path& path, const EndRule& start,
                             const EndRule& end, double dmin) {
    char buf[128];
    std::snprintf(buf, sizeof buf, "%.2f_%.2f", od, wall);
    std::string sig = buf;
    sig += "|" + path_str(path);
    sig += "|" + rule_str(start) + "|" + rule_str(end);
    char dmin_buf[32];
    std::snprintf(dmin_buf, sizeof dmin_buf, "|dmin_%.1f", dmin);
    sig += dmin_buf;
    return sig;
}

BOM build_bom(const std::vector<TubeModel>& tubes, const EndRule& end_rule, double dmin) {
    return build_bom_with_ends(tubes, end_rule, end_rule, dmin);
}

BOM build_bom_with_ends(const std::vector<TubeModel>& tubes, const EndRule& start_end,
                        const EndRule& other_end, double dmin) {
    BOM bom;

    // Группировка по подписи и вычисление спецификации.
    std::map<TubeSignature, TubeGroup> groups;
    std::vector<TubeSignature> order;   // порядок первого появления каждой подписи

    for (const auto& tube : tubes) {
        if (!tube.path) continue;   // пропустить трубы без пути
        const TubeSignature sig = tube_signature(tube.od, tube.wall, *tube.path, start_end, other_end, dmin);

        if (groups.find(sig) == groups.end()) {
            // Первое появление этой подписи.
            order.push_back(sig);
            TubeGroup group;
            group.signature = sig;
            group.tube_ids.push_back(tube.id);

            // Вычислить спецификацию (один раз на группу).
            // Используем blank_length для простого случая (одинаковые правила).
            BlankResult blank;
            if (start_end.E == other_end.E && start_end.N == other_end.N &&
                start_end.cut_allowed == other_end.cut_allowed) {
                blank = blank_length(*tube.path, start_end, other_end, dmin);
            } else {
                // Разные правила на концах: вычисляем отдельно для каждого конца.
                // Формула: длина оси + припуски концов.
                double axis = axis_length(*tube.path);
                auto [d_start, i1] = end_allowance(end_straight(*tube.path, true), start_end, dmin);
                auto [d_end, i2] = end_allowance(end_straight(*tube.path, false), other_end, dmin);
                blank.blank_length = axis + start_end.E + other_end.E + d_start + d_end;
                blank.delta_start = d_start;
                blank.delta_end = d_end;
                if (i1) blank.issues.push_back(*i1);
                if (i2) blank.issues.push_back(*i2);
            }
            group.spec.id = tube.id;
            group.spec.designation = tube.source;   // используем source как обозначение
            group.spec.source = tube.source;
            group.spec.od = tube.od;
            group.spec.wall = tube.wall;
            group.spec.blank_length = blank.blank_length.value_or(0.0);
            group.spec.quantity = 1;
            group.spec.issues = blank.issues;
            groups[sig] = group;
        } else {
            // Добавить трубу в существующую группу.
            groups[sig].tube_ids.push_back(tube.id);
            groups[sig].spec.quantity++;
        }
    }

    // Собрать результат в порядке первого появления.
    bom.groups.reserve(order.size());
    for (const auto& sig : order) {
        bom.groups.push_back(groups[sig]);
    }

    return bom;
}

}  // namespace bendcore
