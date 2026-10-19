#include "bendcore/refs.hpp"

#include <cmath>
#include <map>
#include <stdexcept>

namespace bendcore {

std::string initial_mode(const std::string& origin) {
    if (origin == "assembly") return "адаптивна";
    if (origin == "recognized" || origin == "imported" || origin == "dragged" || origin == "table") return "заморожена";
    throw std::invalid_argument("неизвестное происхождение трубы: " + origin);
}

UpdateResult update_component(Tube& tube, const std::vector<std::pair<std::string, Vec3>>& features,
                              const PointSolver& solve, double tol, const std::set<std::string>& unloaded) {
    if (tube.mode == "заморожена") return {"заморожена", {}, {}, tube.points};

    std::map<std::string, Vec3> feat(features.begin(), features.end());
    std::vector<std::string> moved, lost;
    std::vector<Coincide> cons;
    bool skipped = false;
    for (auto& ref : tube.refs) {
        if (unloaded.count(ref.ref_id)) {
            ref.state = kNotLoaded;
            skipped = true;
            continue;
        }
        const auto it = feat.find(ref.ref_id);
        if (it == feat.end()) {
            ref.state = kLost;
            lost.push_back(ref.ref_id);
            continue;
        }
        if (norm(sub(it->second, ref.cached)) > tol) {
            ref.state = kOutdated;
            moved.push_back(ref.ref_id);
        } else {
            ref.state = kValid;
        }
        cons.push_back({ref.vertex, it->second});
    }
    if (!lost.empty()) return {"потеряны ссылки", moved, lost, tube.points};
    if (moved.empty()) return {skipped ? "не загружена" : "все в норме", {}, {}, tube.points};

    // пересчёт: вершины с ссылками садятся в новые признаки, остальные — фиксированы
    std::set<int> active;
    for (const auto& r : tube.refs) {
        if (!unloaded.count(r.ref_id)) active.insert(r.vertex);
    }
    std::vector<int> fixed;
    for (int i = 0; i < static_cast<int>(tube.points.size()); ++i) {
        if (!active.count(i)) fixed.push_back(i);
    }
    const auto new_points = solve(tube.points, cons, fixed);
    for (auto& ref : tube.refs) {
        if (unloaded.count(ref.ref_id)) continue;
        ref.cached = feat.at(ref.ref_id);
        ref.state = kValid;
    }
    tube.points = new_points;
    return {"вершины сдвинулись", moved, {}, new_points};
}

}  // namespace bendcore
