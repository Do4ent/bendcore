#include <bendcore/assembly_v2/assembly.hpp>
#include <bendcore/assembly_v2/solver.hpp>

#include <string>
#include <utility>

namespace bendcore::assembly_v2 {

using kernel::ErrorCode;

ComponentId Assembly::addComponent(std::string name, const Transform& pose, bool fixed) {
    Component c;
    c.id = static_cast<ComponentId>(components_.size());
    c.name = std::move(name);
    c.pose = pose;
    c.pose.rotation.normalize();
    c.fixed = fixed;
    components_.push_back(std::move(c));
    return components_.back().id;
}

kernel::Result<MateId> Assembly::addMate(Mate mate) {
    if (!hasComponent(mate.a.component) || !hasComponent(mate.b.component)) {
        return kernel::Result<MateId>::failure(ErrorCode::InvalidInput,
                                               std::string(toString(mate.type)) + ": unknown component id");
    }
    auto prepared = prepareMate(std::move(mate));
    if (!prepared) return prepared.error();
    Mate m = std::move(prepared).value();
    if (m.form == MateForm::Rigid && !m.relative) {
        m.relative = components_[m.a.component].pose.inverse() * components_[m.b.component].pose;
    }
    m.id = nextMateId_++;
    mateIndex_[m.id] = mates_.size();
    mates_.push_back(std::move(m));
    return mates_.back().id;
}

bool Assembly::removeMate(MateId id) {
    auto it = mateIndex_.find(id);
    if (it == mateIndex_.end()) return false;
    mates_.erase(mates_.begin() + static_cast<std::ptrdiff_t>(it->second));
    mateIndex_.clear();
    for (std::size_t i = 0; i < mates_.size(); ++i) mateIndex_[mates_[i].id] = i;
    return true;
}

bool Assembly::setPose(ComponentId id, const Transform& pose) {
    if (!hasComponent(id)) return false;
    components_[id].pose = pose;
    components_[id].pose.rotation.normalize();
    return true;
}

bool Assembly::setFixed(ComponentId id, bool fixed) {
    if (!hasComponent(id)) return false;
    components_[id].fixed = fixed;
    return true;
}

const Mate* Assembly::findMate(MateId id) const {
    auto it = mateIndex_.find(id);
    return it == mateIndex_.end() ? nullptr : &mates_[it->second];
}

kernel::Result<std::size_t> Assembly::applyPoses(const SolveResult& result) {
    if (result.poses.size() != components_.size()) {
        return kernel::Result<std::size_t>::failure(
            ErrorCode::InvalidInput, "applyPoses: result has " + std::to_string(result.poses.size()) +
                                         " poses, assembly has " + std::to_string(components_.size()) +
                                         " components");
    }
    for (std::size_t i = 0; i < components_.size(); ++i) components_[i].pose = result.poses[i];
    return components_.size();
}

}  // namespace bendcore::assembly_v2
