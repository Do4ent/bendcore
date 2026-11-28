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

#include <optional>
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

// Объём (BRepGProp::VolumeProperties); значение запоминается в кэше фактов о формах.
double volumeOf(const TopoDS_Shape& shape);

// Кэш фактов о формах (docs/modeling/PART_DOCUMENT.md, «Инкрементальное перестроение»).
// TopoDS_Shape неизменяема: форма с тем же TShape, Location и Orientation (IsEqual) имеет те же
// свойства. Кэш хранит до kShapeFactsCapacity последних форм (сами формы удерживаются, поэтому
// адрес TShape не переиспользуется), доступ под мьютексом.
//   valid        — BRepCheck_Analyzer(shape).IsValid() был true;
//   booleanReady — форма — результат kernel::booleanOp с нулевым нечётким допуском и прошла
//                  BRepCheck_Analyzer, поэтому как вход следующей булевой операции не проверяется
//                  BRepAlgoAPI_Check повторно (самопересечений результат булевой операции не
//                  содержит; вход, из которого он построен, уже прошёл эту проверку);
//   volume       — объём BRepGProp::VolumeProperties;
//   mass         — объём и площадь, которые kernel::booleanOp вычислил для проверки границ объёма
//                  (BooleanResult::mass); передаются следующей булевой операции как
//                  BooleanOptions::argumentMass / toolMass, чтобы ядро их не пересчитывало.
struct ShapeFacts {
    bool valid = false;
    bool booleanReady = false;
    std::optional<double> volume;
    std::optional<kernel::BooleanInputMass> mass;
};
constexpr std::size_t kShapeFactsCapacity = 64;
std::optional<ShapeFacts> knownFacts(const TopoDS_Shape& shape);
// Дополняет факты формы (флаги — логическое ИЛИ, объём и масса — если заданы).
void rememberFacts(const TopoDS_Shape& shape, const ShapeFacts& facts);
// BRepCheck_Analyzer с учётом кэша (успешная проверка запоминается).
bool isValidShape(const TopoDS_Shape& shape);
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

// Булева операция через kernel::booleanOp с историей ядра (BooleanResult::history —
// BRepAlgoAPI_BuilderAlgo::History(), объединённая с историей SimplifyResult).
// В history переносятся записи (Modified/Generated/IsRemoved) только для элементов входов
// из отслеживаемых групп (trackArguments/trackTools); untrackedImages получает грани
// результата, которые являются неизменёнными гранями или Modified-образами граней
// неотслеживаемых входов и не являются образами отслеживаемых.
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

// Тела формы с ограничивающими коробками, которые пересекаются с коробкой инструмента
// (с запасом tolerance): кандидаты многотельной булевой операции.
std::vector<TopoDS_Shape> bodiesNear(const std::vector<TopoDS_Shape>& bodies, const TopoDS_Shape& tool,
                                     double tolerance);

// Объединение историй независимых шагов над непересекающимися входами (каждая — от своих
// входов к своей части результата). Для элементов inputs: Modified/Generated — объединение
// образов всех шагов, IsRemoved — только если ни один шаг не дал образа и хотя бы один удалил.
History combineHistories(const std::vector<TopoDS_Shape>& inputs, const std::vector<History>& steps,
                         const TopoDS_Shape& result);

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
