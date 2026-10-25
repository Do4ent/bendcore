#pragma once
#include "../document/entity_id.hpp"
namespace BendCore::CadCore {
class StressAnalyzer {
public:
    StressAnalyzer(const EntityId& id) : geometry_id_(id), max_stress_(0.0) {}
    bool runSimulation() noexcept;
    [[nodiscard]] double getMaxStress() const noexcept { return max_stress_; }
private:
    EntityId geometry_id_;
    double max_stress_;
};
}
