#pragma once

// Модель сборки для решателя: компоненты с жёстким преобразованием и сопряжения между ними.

#include <bendcore/assembly_v2/geometry.hpp>
#include <bendcore/assembly_v2/mate.hpp>
#include <bendcore/kernel/result.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace bendcore::assembly_v2 {

struct Component {
    ComponentId id = 0;
    std::string name;
    Transform pose;
    bool fixed = false;
};

struct SolveResult;

class Assembly {
public:
    // ID компонента равен его индексу в components().
    ComponentId addComponent(std::string name, const Transform& pose, bool fixed = false);

    // Проверяет сопряжение (prepareMate, существование компонентов), назначает ID.
    // Для Rigid без relative фиксирует текущее относительное положение.
    kernel::Result<MateId> addMate(Mate mate);
    // false, если сопряжения с таким ID нет.
    bool removeMate(MateId id);

    // false, если компонента с таким ID нет.
    bool setPose(ComponentId id, const Transform& pose);
    bool setFixed(ComponentId id, bool fixed);

    bool hasComponent(ComponentId id) const { return id < components_.size(); }
    const Component& component(ComponentId id) const { return components_.at(id); }
    const std::vector<Component>& components() const { return components_; }
    const std::vector<Mate>& mates() const { return mates_; }
    // nullptr, если нет.
    const Mate* findMate(MateId id) const;

    // Записывает положения из результата решения (размер должен совпадать с числом компонентов).
    kernel::Result<std::size_t> applyPoses(const SolveResult& result);

private:
    std::vector<Component> components_;
    std::vector<Mate> mates_;
    std::unordered_map<MateId, std::size_t> mateIndex_;
    MateId nextMateId_ = 1;
};

}  // namespace bendcore::assembly_v2
