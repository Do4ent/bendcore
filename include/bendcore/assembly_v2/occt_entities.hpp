#pragma once

// Сущности решателя сопряжений из граней, рёбер и вершин OCCT (docs/assembly/ASSEMBLY_DOCUMENT.md,
// раздел 3). Координаты сущности — в системе координат формы, к которой принадлежит элемент
// (положение TopLoc_Location элемента учтено).

#include <bendcore/assembly_v2/geometry.hpp>
#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/shape.hpp>
#include <bendcore/modeling/naming.hpp>

#include <TopoDS_Shape.hxx>

#include <cstddef>
#include <cstdint>

namespace bendcore::assembly_v2 {

using modeling::ElementKind;

// Какую сущность взять из элемента.
//  Auto:  плоская грань → Plane; цилиндрическая → Cylinder; коническая → Axis;
//         сферическая → Sphere; прямое ребро → Axis; круговое ребро → Axis с точкой в центре;
//         вершина → Point.
//  Axis:  цилиндрическая и коническая грань, прямое и круговое ребро → Axis.
//  Point: сферическая грань и круговое ребро → центр; вершина → точка.
enum class EntityUse { Auto, Axis, Point };

const char* toString(EntityUse use) noexcept;

// Ошибки: InvalidInput — пустой элемент или не грань/ребро/вершина;
// Unsupported — геометрия, для которой нет сущности (тор, B-сплайн, эллипс, плоскость для
// Axis/Point и т. п.); сообщение называет тип геометрии и запрошенное использование.
kernel::Result<Entity> entityFromElement(const TopoDS_Shape& element, EntityUse use = EntityUse::Auto);

// Элемент kind с индексом index из TopologyIndex. Ошибка InvalidInput — индекс вне диапазона.
kernel::Result<Entity> entityFromIndex(const kernel::TopologyIndex& topology, ElementKind kind,
                                       std::uint32_t index, EntityUse use = EntityUse::Auto);

// Элемент по индексу (ориентация и положение как у TopExp_Explorer); пустая форма вне диапазона.
TopoDS_Shape elementAt(const kernel::TopologyIndex& topology, ElementKind kind, std::uint32_t index);
std::size_t elementCount(const kernel::TopologyIndex& topology, ElementKind kind);

// Сущность в другой СК: точка и направление преобразуются, радиус сохраняется.
Entity transformEntity(const Entity& entity, const Transform& transform);

}  // namespace bendcore::assembly_v2
