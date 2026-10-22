// Bill of Materials (спецификация): groups identical tubes and specifies blank length (этап 5 фазы 23).
// Один TubeSpec на тип трубы (трубы с одинаковыми геометрией и припусками группируются).
// Подпись трубы (tube_signature) зависит от: ∅, толщина стенки, длина оси, углы и типы гибов,
// спирали, участки на концах (для припусков). Одинаковые подписи → одна строка спецификации.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "bendcore/blank.hpp"
#include "bendcore/tube_model.hpp"
#include "bendcore/tubegeom.hpp"

namespace bendcore {

// Идентификатор типа трубы: хеш геометрии и припусков.
using TubeSignature = std::string;

// Подпись трубы по её геометрии, припускам и параметрам концов.
// Зависит от: od, wall, path (ось и изгибы), EndRule на каждом конце, dmin.
TubeSignature tube_signature(double od, double wall, const Path& path, const EndRule& start,
                             const EndRule& end, double dmin = 0.0);

// Один тип трубы в спецификации: все параметры и количество.
struct TubeSpec {
    std::string id;                 // идентификатор первой трубы этого типа (для обратной связи с моделью)
    std::string designation;        // обозначение из TubeModel
    std::string source;             // источник (то же TubeModel.source)
    double od = 0.0;               // наружный диаметр, мм
    double wall = 0.0;             // толщина стенки, мм
    double blank_length = 0.0;     // длина заготовки с припусками, мм
    int quantity = 0;              // количество идентичных труб
    std::vector<Issue> issues;     // замечания по заготовке (ошибки припусков и т. д.)
};

// Группа труб одного типа.
struct TubeGroup {
    TubeSignature signature;
    std::vector<std::string> tube_ids;     // id всех труб в группе (по порядку, как в исходном списке)
    TubeSpec spec;                         // спецификация этого типа
};

// Спецификация: список типов труб, сгруппированные по подписи.
struct BOM {
    std::vector<TubeGroup> groups;   // по порядку первого появления трубы каждого типа
};

// Собрать BOM из списка труб. EndRule для обоих концов одинаковых (без разных концов).
BOM build_bom(const std::vector<TubeModel>& tubes, const EndRule& end_rule, double dmin = 0.0);

// Собрать BOM с разными правилами концов.
BOM build_bom_with_ends(const std::vector<TubeModel>& tubes, const EndRule& start_end,
                        const EndRule& other_end, double dmin = 0.0);

}  // namespace bendcore
