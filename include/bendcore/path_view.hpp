// Доступ к элементам пути (Item = variant<Straight, Bend, Spiral>) без std::visit в прикладном коде.
// Для элемента не того типа возвращается 0 — так прикладной код может суммировать без ветвлений.
#pragma once

#include <variant>

#include "bendcore/tubegeom.hpp"

namespace bendcore {

inline bool is_bend(const Item& it) { return std::holds_alternative<Bend>(it); }
inline bool is_straight(const Item& it) { return std::holds_alternative<Straight>(it); }
inline bool is_spiral(const Item& it) { return std::holds_alternative<Spiral>(it); }

// Радиус гиба или витка, мм.
inline double item_R(const Item& it) {
    if (const auto* b = std::get_if<Bend>(&it)) return b->R;
    if (const auto* s = std::get_if<Spiral>(&it)) return s->R;
    return 0.0;
}
// Угол гиба B, градусы (со знаком).
inline double item_B(const Item& it) {
    if (const auto* b = std::get_if<Bend>(&it)) return b->B;
    return 0.0;
}
// Поворот A перед гибом, градусы.
inline double item_A(const Item& it) {
    if (const auto* b = std::get_if<Bend>(&it)) return b->A;
    return 0.0;
}
// Длина прямого участка S, мм.
inline double item_S(const Item& it) {
    if (const auto* s = std::get_if<Straight>(&it)) return s->S;
    return 0.0;
}

}  // namespace bendcore
