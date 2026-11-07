#pragma once

// Общие части операций детали: проверка входа, история, булева операция с
// восстановлением истории, проверка результата (docs/modeling/FEATURES.md, раздел 1).

#include <bendcore/kernel/boolean.hpp>
#include <bendcore/modeling/features.hpp>

#include <BRepTools_History.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_List.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopExp.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>

#include <string>
#include <vector>

namespace bendcore::modeling::detail {

using History = opencascade::handle<BRepTools_History>;
using ShapeMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;
using ShapeList = NCollection_List<TopoDS_Shape>;

constexpr double kLinearTol = 1e-7;    // совпадение точек, мм
constexpr double kAngularTol = 1e-9;   // параллельность, рад

// Проверка профиля: грань не пуста, плоская, в плоскости placement, все контуры замкнуты,
// площадь > 0, BRepCheck валиден. Возвращает плоскость профиля.
Result<gp_Pln> validateProfile(const Profile& profile);

// Тело (Solid/CompSolid/Compound с телами), BRepCheck валидно.
std::string checkSolidInput(const TopoDS_Shape& shape, const char* role);

// Результат операции: валиден, содержит тела с положительным объёмом.
std::string checkSolidResult(const TopoDS_Shape& shape);

double volumeOf(const TopoDS_Shape& shape);
int countSolids(const TopoDS_Shape& shape);
// Диагональ габарита объединения форм (пустые пропускаются).
double boundingDiagonal(const std::vector<TopoDS_Shape>& shapes);
// Интервал проекций углов габарита формы на ось (origin, dir).
void projectionRange(const TopoDS_Shape& shape, const gp_Pnt& origin, const gp_Dir& dir, double& lo, double& hi);

// Является ли s подформой формы (TopExp::MapShapes, ориентация не учитывается).
bool containsSubShape(const TopoDS_Shape& whole, const TopoDS_Shape& s);

History newHistory();
// Копия истории (Merge в пустую историю).
History copyHistory(const History& h);
// a := a ∘ b (Merge), пустые истории допустимы.
void mergeInto(History& a, const History& b);

// Копия формы с преобразованием; map(исходная подформа) → подформа копии.
struct TransformedCopy {
    TopoDS_Shape shape;
    std::vector<std::pair<TopoDS_Shape, TopoDS_Shape>> pairs;  // (исходная, копия) для V/E/F/Solid
};
Result<TransformedCopy> transformCopy(const TopoDS_Shape& shape, const gp_Trsf& trsf);

// Булева операция через kernel::booleanOp с историей, восстановленной геометрически:
// элемент результата, совпадающий с элементом входа, — без изменений; лежащий на элементе
// входа — Modified; ребро результата на двух гранях входа без ребра-прообраза —
// Generated из этих граней; элемент входа без образов — Remove.
// Записываются только элементы входов из tracked; untrackedImages получает грани
// результата, полученные только из входов вне tracked.
struct BooleanStep {
    TopoDS_Shape shape;
    History history;
    std::vector<TopoDS_Shape> untrackedImages;
    std::vector<std::string> warnings;
    double fuzzyValue = 0.0;
};

Result<BooleanStep> booleanWithHistory(kernel::BooleanType type, const std::vector<TopoDS_Shape>& arguments,
                                       const std::vector<TopoDS_Shape>& tools, bool trackArguments,
                                       bool trackTools);

// Применение инструмента к целевому телу (Boss/Cut) с объединением истории
// инструмента (toolHistory: профиль → tool) и истории булевой операции.
// Для Cut без пересечения с телом — ошибка.
Result<FeatureResult> applyTool(FeatureMode mode, const TopoDS_Shape& target, const TopoDS_Shape& tool,
                                const History& toolHistory, std::vector<std::string> warnings);

// История заметания (BRepPrimAPI_MakePrism / MakeRevol) от исходного профиля к телу:
// начальный торец — Modified(элемент профиля), конечный торец, боковые грани и рёбра,
// тело — Generated(элемент профиля). copy — копия профиля, которую заметали.
template <class Sweep>
History sweepHistory(Sweep& sweep, const TopoDS_Shape& tool, const TransformedCopy& copy) {
    History h = newHistory();
    ShapeMap toolMap;
    TopExp::MapShapes(tool, toolMap);
    for (const auto& [orig, cp] : copy.pairs) {
        if (orig.ShapeType() == TopAbs_SOLID) continue;
        const TopoDS_Shape first = sweep.FirstShape(cp);
        const TopoDS_Shape last = sweep.LastShape(cp);
        const bool hasFirst = !first.IsNull() && toolMap.Contains(first);
        if (hasFirst) h->AddModified(orig, first);
        const bool hasLast = !last.IsNull() && toolMap.Contains(last) && !(hasFirst && last.IsSame(first));
        if (hasLast) h->AddGenerated(orig, last);
        for (const TopoDS_Shape& g : sweep.Generated(cp)) {
            if (g.IsNull() || !toolMap.Contains(g)) continue;
            if ((hasFirst && g.IsSame(first)) || (hasLast && g.IsSame(last))) continue;
            h->AddGenerated(orig, g);
        }
    }
    return h;
}

// Нормализация истории алгоритма OCCT: образ того же типа, что и элемент входа, считается
// Modified (BRepOffsetAPI_DraftAngle сообщает изменённые грани как Generated), образ другого
// типа — Generated; элемент входа без Modified, которого нет в result, — Remove (даже если у
// него есть Generated-образы: BRepOffsetAPI_MakeThickSolid не помечает удалённые грани).
History normalizeHistory(const TopoDS_Shape& input, const TopoDS_Shape& result, const History& raw);

// История алгоритма OCCT со стандартными Modified/Generated/IsDeleted, нормализованная.
template <class Algo>
History algoHistory(const TopoDS_Shape& input, Algo& algo) {
    ShapeList args;
    args.Append(input);
    History raw = new BRepTools_History(args, algo);
    return normalizeHistory(input, algo.Shape(), raw);
}

// Для каждой пары (исходный элемент, копия): Generated(исходный) → образы копии в result
// (сама копия, если она есть в result, иначе её Modified в stepHistory).
void addCopyGenerations(History& history, const std::vector<std::pair<TopoDS_Shape, TopoDS_Shape>>& pairs,
                        const TopoDS_Shape& result, const History& stepHistory);

// Сообщение для исключений OCCT.
std::string occtMessage(const char* where, const char* what);

}  // namespace bendcore::modeling::detail
