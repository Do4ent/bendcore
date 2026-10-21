#include "bendcore/dwfx/import_filter.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace bendcore::dwfx {
namespace {

char lower(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

bool glob_at(const std::string& p, std::size_t pi, const std::string& t, std::size_t ti) {
    while (pi < p.size()) {
        if (p[pi] == '*') {
            for (std::size_t k = ti; k <= t.size(); ++k) {
                if (glob_at(p, pi + 1, t, k)) return true;
            }
            return false;
        }
        if (ti >= t.size()) return false;
        if (p[pi] != '?' && lower(p[pi]) != lower(t[ti])) return false;
        ++pi;
        ++ti;
    }
    return ti == t.size();
}

std::vector<std::string> split_layers(const std::string& text) {
    std::vector<std::string> out;
    std::string token;
    auto flush = [&] {
        const auto begin = token.find_first_not_of(" \t");
        if (begin != std::string::npos) {
            const auto end = token.find_last_not_of(" \t");
            out.push_back(token.substr(begin, end - begin + 1));
        }
        token.clear();
    };
    for (char c : text) {
        if (c == ',') {
            flush();
        } else {
            token.push_back(c);
        }
    }
    flush();
    return out;
}

bool equal_ci(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (lower(a[i]) != lower(b[i])) return false;
    }
    return true;
}

double distance_point_segment(const Point3& p, const Point3& a, const Point3& b) {
    double ab[3], ap[3];
    double ab2 = 0.0;
    double dot = 0.0;
    for (int k = 0; k < 3; ++k) {
        ab[k] = b[k] - a[k];
        ap[k] = p[k] - a[k];
        ab2 += ab[k] * ab[k];
        dot += ab[k] * ap[k];
    }
    double t = ab2 > 0.0 ? dot / ab2 : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    double sum = 0.0;
    for (int k = 0; k < 3; ++k) {
        const double q = a[k] + t * ab[k] - p[k];
        sum += q * q;
    }
    return std::sqrt(sum);
}

double bbox_diagonal(const std::array<double, 6>& box) {
    const double dx = box[3] - box[0];
    const double dy = box[4] - box[1];
    const double dz = box[5] - box[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

}  // namespace

bool glob_match(const std::string& pattern, const std::string& text) { return glob_at(pattern, 0, text, 0); }

double distance_point_polyline(const Point3& point, const std::vector<Point3>& polyline) {
    if (polyline.empty()) return std::numeric_limits<double>::infinity();
    if (polyline.size() == 1) {
        double sum = 0.0;
        for (int k = 0; k < 3; ++k) sum += (point[k] - polyline[0][k]) * (point[k] - polyline[0][k]);
        return std::sqrt(sum);
    }
    double best = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i + 1 < polyline.size(); ++i) {
        best = std::min(best, distance_point_segment(point, polyline[i], polyline[i + 1]));
    }
    return best;
}

FilterResult apply_import_filter(const ImportSettings& settings, const std::vector<FilterComponent>& components,
                                 const std::vector<FilterSupportAxis>& supports,
                                 const std::map<std::string, bool>& manual) {
    FilterResult result;

    // Правила из настроек, которые эта версия не применяет: пользователь видит это в отчёте.
    if (settings.filter_simplify_outer_shell) result.not_applied.push_back("упрощение: внешняя оболочка");
    if (settings.filter_simplify_remove_small_mm > 0.0) result.not_applied.push_back("упрощение: мелкие элементы");
    if (settings.filter_simplify_decimate) result.not_applied.push_back("упрощение: децимация сетки DWF");
    if (settings.filter_simplify_replace_bbox) result.not_applied.push_back("упрощение: габаритное тело");

    const std::vector<std::string> layers = split_layers(settings.filter_layers);
    const bool neighborhood_active = settings.filter_preset == FilterPreset::TubesAndNeighborhood;

    // Опорные трубы, выбранные настройками, с радиусом окрестности.
    struct Corridor {
        const FilterSupportAxis* axis;
        double limit;  // R + D, мм
    };
    std::vector<Corridor> corridors;
    if (neighborhood_active) {
        for (const auto& axis : supports) {
            const bool allowed = axis.source == SupportSource::File ? settings.neighborhood_support_file_tubes
                                                                     : settings.neighborhood_support_target_tubes;
            if (!allowed || axis.polyline.empty() || axis.od_mm <= 0.0) continue;
            const double radius = axis.od_mm / 2.0;
            const double d = std::max(settings.neighborhood_min_od_factor * axis.od_mm,
                                      settings.clearance_mm + settings.neighborhood_margin_mm);
            corridors.push_back({&axis, radius + d});
        }
    }

    // Точки компонента для проверок: вершины сетки, иначе углы и центр габарита.
    auto sample_points = [](const FilterComponent& c) {
        if (!c.samples.empty()) return c.samples;
        std::vector<Point3> points;
        const auto& b = c.bbox;
        for (int corner = 0; corner < 8; ++corner) {
            points.push_back({corner & 1 ? b[3] : b[0], corner & 2 ? b[4] : b[1], corner & 4 ? b[5] : b[2]});
        }
        points.push_back({(b[0] + b[3]) / 2, (b[1] + b[4]) / 2, (b[2] + b[5]) / 2});
        return points;
    };

    auto near_corridor = [&](const FilterComponent& c) {
        const std::vector<Point3> points = sample_points(c);
        for (const auto& corridor : corridors) {
            // Грубый отбор по габаритам: коробка оси, расширенная на R + D, должна пересекать габарит компонента.
            double lo[3] = {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
                            std::numeric_limits<double>::infinity()};
            double hi[3] = {-std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
                            -std::numeric_limits<double>::infinity()};
            for (const auto& p : corridor.axis->polyline) {
                for (int k = 0; k < 3; ++k) {
                    lo[k] = std::min(lo[k], p[k]);
                    hi[k] = std::max(hi[k], p[k]);
                }
            }
            bool overlap = true;
            for (int k = 0; k < 3; ++k) {
                if (c.bbox[k] > hi[k] + corridor.limit || c.bbox[k + 3] < lo[k] - corridor.limit) overlap = false;
            }
            if (!overlap) continue;
            for (const auto& p : points) {
                if (distance_point_polyline(p, corridor.axis->polyline) <= corridor.limit) return true;
            }
        }
        return false;
    };

    // Правила вида, маски, слоя и размера. Пустая строка — правило не сработало.
    auto rule_failure = [&](const FilterComponent& c) -> std::string {
        if (settings.filter_only_visible && !c.visible) return "hidden";
        switch (c.type) {
            case FilterObjectType::Body:
                if (!settings.filter_type_bodies) return "type";
                break;
            case FilterObjectType::Surface:
                if (!settings.filter_type_surfaces) return "type";
                break;
            case FilterObjectType::Curve:
                if (!settings.filter_type_curves) return "type";
                break;
            case FilterObjectType::Annotation:
                if (!settings.filter_type_annotations) return "type";
                break;
        }
        if (!settings.filter_name_mask.empty() && !glob_match(settings.filter_name_mask, c.name)) return "name_mask";
        if (!layers.empty()) {
            bool in_list = false;
            for (const auto& layer : layers) {
                if (equal_ci(layer, c.layer)) in_list = true;
            }
            if (!in_list) return "layer";
        }
        if (settings.filter_min_bbox_mm > 0.0 && bbox_diagonal(c.bbox) < settings.filter_min_bbox_mm) {
            return "min_size";
        }
        return "";
    };

    enum class Stage { Decided, Pending };
    struct Pending {
        Stage stage = Stage::Decided;
        bool rules_pass = false;
        bool near = false;
    };
    std::vector<Pending> state(components.size());
    result.decisions.resize(components.size());

    // Первый проход: ручные решения, порты, правила и окрестность.
    for (std::size_t i = 0; i < components.size(); ++i) {
        const FilterComponent& c = components[i];
        FilterDecision& d = result.decisions[i];
        d.id = c.id;

        const auto manual_it = manual.find(c.id);
        if (manual_it != manual.end()) {
            d.include = manual_it->second;
            d.reason = d.include ? "manual_include" : "manual_exclude";
            continue;
        }
        if (c.is_port) {
            d.include = true;
            d.reason = "port";
            continue;
        }
        const std::string failure = rule_failure(c);
        if (!failure.empty()) {
            d.include = false;
            d.reason = failure;
            continue;
        }
        state[i].rules_pass = true;

        switch (settings.filter_preset) {
            case FilterPreset::TubesOnly:
                d.include = c.is_tube;
                d.reason = c.is_tube ? "tube" : "not_tube";
                break;
            case FilterPreset::AllSimplified:
                d.include = true;
                d.reason = "rules";
                break;
            case FilterPreset::TubesAndNeighborhood:
                if (c.is_tube) {
                    d.include = true;
                    d.reason = "tube";
                } else {
                    state[i].near = near_corridor(c);
                    state[i].stage = Stage::Pending;
                    d.include = state[i].near;
                    d.reason = state[i].near ? "neighborhood" : "outside_neighborhood";
                }
                break;
        }
    }

    // Второй проход: единица отбора «компонент» — решение общее для группы.
    if (neighborhood_active && settings.neighborhood_unit == NeighborhoodUnit::Component) {
        auto group_key = [&](std::size_t i) {
            return components[i].group_id.empty() ? components[i].id : components[i].group_id;
        };
        std::set<std::string> hit_groups;
        for (std::size_t i = 0; i < components.size(); ++i) {
            if (state[i].stage == Stage::Pending && state[i].near) hit_groups.insert(group_key(i));
        }
        for (std::size_t i = 0; i < components.size(); ++i) {
            if (state[i].stage == Stage::Pending && !state[i].near && hit_groups.count(group_key(i)) != 0) {
                result.decisions[i].include = true;
                result.decisions[i].reason = "group";
            }
        }
    }

    // Расширение по контакту (9.8): тела, касающиеся уже включённых, добавляются по уровням.
    // Касание — расстояние между точками поверхности не больше допуска сравнения длин.
    auto touches = [&](const FilterComponent& a, const FilterComponent& b, double tol) {
        for (int k = 0; k < 3; ++k) {
            if (a.bbox[k] > b.bbox[k + 3] + tol || b.bbox[k] > a.bbox[k + 3] + tol) return false;
        }
        const std::vector<Point3> pa = sample_points(a);
        const std::vector<Point3> pb = sample_points(b);
        for (const auto& p : pa) {
            for (const auto& q : pb) {
                double sum = 0.0;
                for (int k = 0; k < 3; ++k) sum += (p[k] - q[k]) * (p[k] - q[k]);
                if (std::sqrt(sum) <= tol) return true;
            }
        }
        return false;
    };
    if (neighborhood_active && settings.contact_levels > 0) {
        const double tolerance = settings.compare_linear_mm;
        for (int level = 1; level <= settings.contact_levels; ++level) {
            std::vector<std::size_t> added;
            for (std::size_t i = 0; i < components.size(); ++i) {
                if (result.decisions[i].include || result.decisions[i].reason != "outside_neighborhood") continue;
                for (std::size_t j = 0; j < components.size(); ++j) {
                    if (result.decisions[j].include && touches(components[i], components[j], tolerance)) {
                        added.push_back(i);
                        break;
                    }
                }
            }
            if (added.empty()) break;
            for (std::size_t i : added) {
                result.decisions[i].include = true;
                result.decisions[i].reason = "contact_" + std::to_string(level);
            }
        }
    }

    for (const auto& d : result.decisions) {
        if (d.include) result.included += 1;
    }
    return result;
}

}  // namespace bendcore::dwfx
