# Лечение геометрии и устойчивые булевы операции

Библиотека `bendcore_kernel_healing` (STATIC, `cmake/kernel_healing.cmake`), пространство имён
`bendcore::kernel`. Заголовки: `include/bendcore/kernel/healing.hpp`,
`include/bendcore/kernel/boolean.hpp`. Работает напрямую с `TopoDS_Shape` (OCCT 8.0.1).
Ошибки возвращаются через `Result<T>` (`include/bendcore/kernel/result.hpp`); исключения OCCT
(`Standard_Failure`) и `std::exception` перехватываются на границе модуля и превращаются в
`ErrorCode::OperationFailed`.

## 1. Лечение: `healShape`

```cpp
Result<HealingResult> healShape(const TopoDS_Shape& shape, const HealingOptions& options = {});
Result<HealingResult> convertToAnalytic(const TopoDS_Shape& shape, double tolerance = 1e-6);
TypeCounts countSurfaceTypes(const TopoDS_Shape&);   // грани по типу поверхности
TypeCounts countCurveTypes(const TopoDS_Shape&);     // невырожденные рёбра по типу 3D-кривой
```

### Шаги (в порядке выполнения, каждый отключается флагом)

| Шаг | Флаг | Алгоритм OCCT | Что меняет |
|---|---|---|---|
| 1 | `directFaces` | `ShapeCustom::DirectFaces` | Поверхности с левой системой координат (после зеркалирования) заменяются правыми; ориентация граней и проволок согласуется. Нужен перед булевыми операциями и экспортом. |
| 2 | `fixShape` | `ShapeFix_Shape` (`precision`, `maxTolerance`) | Порядок рёбер в проволоках, разрывы, отсутствующие pcurve, SameParameter, ориентация оболочек, допуски вершин/рёбер. |
| 3 | `removeSmallElements` | `ShapeFix_FixSmallFace`, затем `ShapeFix_Wireframe` (`ModeDropSmallEdges`, `FixSmallEdges`, `FixWireGaps`), затем `ShapeFix_Shape` | Грани-точки и грани-полосы уже `smallElementSize` удаляются, рёбра короче `smallElementSize` сливаются с соседними, концы стыкуются. Объём меняется на величину порядка `smallElementSize²·L`. |
| 4 | `convertToAnalytic` | `GeomConvert_SurfToAnaSurf`, `GeomConvert_CurveToAnaCurve`, `BRepTools_Modifier`, `GeomProjLib::Curve2d`, затем `ShapeFix_Shape` | B-сплайновые/Безье поверхности → плоскость, цилиндр, конус, сфера, тор; кривые → прямая, окружность, эллипс, если отклонение ≤ `analyticTolerance`. |
| 5 | `unifySameDomain` | `ShapeUpgrade_UnifySameDomain` (рёбра и грани, без внутренних рёбер) | Сливает соседние грани на одной поверхности и рёбра на одной кривой — например, после булевых операций или после шага 4. |
| итог | — | `BRepCheck_Analyzer`, `BRepGProp` | Валидность, объём, наибольший допуск. |

Защита от порчи: если форма перед шагом была валидной, а после шага стала невалидной, шаг
откатывается, в `report.warnings` пишется причина. Невалидный итог (например, вход был
невалиден и лечение не помогло) ошибкой не считается — смотрите `report.valid`.

### Как устроен перевод в аналитику (шаг 4)

В OCCT 8.0.1 нет готового инструмента «форма → аналитические поверхности» (есть только
распознавание `ShapeAnalysis_CanonicalRecognition`), поэтому шаг реализован как
`BRepTools_Modification` (`AnalyticModification` в `src/kernel/healing.cpp`):

1. **Поверхность грани.** `GeomConvert_SurfToAnaSurf::ConvertToAnalytical(tol, uv-границы грани)`;
   для замкнутых периодических поверхностей этот вариант возвращает пусто, тогда вызывается
   вариант без границ. Кандидат проверяется независимо: сетка 7×7 точек исходной поверхности в
   границах грани проецируется на новую (`GeomAPI_ProjectPointOnSurf`), максимум расстояния
   должен быть ≤ `tol`; сравниваются нормали — при противоположной нормали берётся
   `UReversed()`, чтобы ориентация граней не менялась.
2. **Кривая ребра.** `GeomConvert_CurveToAnaCurve::ConvertToAnalytical`; проверяются концы,
   отклонение в 15 внутренних точках и направление касательной. Кривая переводится, только если
   все грани ребра аналитические (или становятся ими) — иначе pcurve пришлось бы
   аппроксимировать на сплайновой поверхности с новой параметризацией.
3. **pcurve.** Для каждой пары (ребро, грань), где поменялась поверхность или параметризация
   кривой, строится новая pcurve: `GeomProjLib::Curve2d` (точная для элементарных поверхностей)
   и сдвиг на целое число периодов к опорной точке. Опорная точка — точка внутри грани рядом с
   серединой старой pcurve (сторона выбирается классификатором `BRepTopAdaptor_FClass2d`),
   переведённая в окно периода, центрированное на середине грани. Так две pcurve шва цилиндра
   ложатся на u и u+2π. Для вырожденных рёбер (полюса сферы) pcurve строится по точкам,
   смещённым внутрь грани, и выпрямляется на особую изолинию (`ShapeAnalysis_Surface::DegeneratedValues`).
   Отклонение pcurve от 3D-кривой проверяется в 201 точке и задаёт допуск ребра.
4. **Параметры вершин.** Новая кривая имеет новый диапазон; для замкнутых кривых конец
   выбирается по ориентации вершины относительно прямого ребра.
5. **Проверка шага.** `ShapeFix_Shape`, затем `BRepCheck_Analyzer` и сравнение объёма (адаптивное
   интегрирование, допустимо `10·max(tol, отклонение)·площадь`). Нарушение → откат шага.

### Допуски по умолчанию и почему

| Параметр | По умолчанию | Обоснование |
|---|---|---|
| `precision` | 1e-6 мм | Порядок `Precision::Confusion()` (1e-7) с запасом на данные STEP; ShapeFix не будет «чинить» то, что мельче. |
| `maxTolerance` | 1e-3 мм | Верхний предел роста допусков элементов. Больше 1e-3 мм — уже видимая неточность для механообработки (квалитеты IT5–IT6 на размерах до 10 мм — 4–6 мкм). |
| `smallElementSize` | 1e-5 мм | В 10 раз больше `precision`: такие рёбра/грани не несут конструктивного смысла, но ломают булевы операции и тесселяцию. |
| `analyticTolerance` | 1e-6 мм | Перевод NURBS→аналитика должен быть практически точным, чтобы не менять геометрию; сплайны из `BRepBuilderAPI_NurbsConvert` переводятся с отклонением ~1e-14. |
| `unifyLinearTolerance` / `unifyAngularTolerance` | 1e-6 мм / 1e-8 рад | Сливаются только действительно совпадающие поверхности. |

### Отчёт `HealingReport`

Типы поверхностей и кривых до/после (`"Plane"`, `"Cylinder"`, `"BSplineSurface"`, `"Line"`,
`"Circle"`…), числа граней и рёбер до/после, `directedFaces`, `smallFacesRemoved`,
`smallEdgesRemoved`, `surfacesConverted`, `curvesConverted`, `maxConversionDeviation`,
`facesMerged`, `validBefore`/`valid`, `volumeBefore`/`volumeAfter`/`volumeChange`,
`maxToleranceAfter`, список выполненных шагов и предупреждения.

### Пример

```cpp
#include <bendcore/kernel/healing.hpp>
using namespace bendcore::kernel;

HealingOptions opt;
opt.smallElementSize = 1e-4;          // импорт из системы с грубой точностью
auto healed = healShape(importedShape, opt);
if (!healed) { log(healed.error().message); return; }
const HealingReport& r = healed.value().report;
if (!r.valid) warn("модель осталась невалидной");
use(healed.value().shape);
```

## 2. Булевы операции: `booleanOp`

```cpp
enum class BooleanType { Fuse, Cut, Common };
Result<BooleanResult> booleanOp(BooleanType, const TopoDS_Shape& a, const TopoDS_Shape& b,
                                const BooleanOptions& = {});
Result<BooleanResult> booleanOp(BooleanType, const std::vector<TopoDS_Shape>& arguments,
                                const std::vector<TopoDS_Shape>& tools, const BooleanOptions& = {});
std::vector<double> fuzzyLadder(const BooleanOptions&);
```

Алгоритм:

1. Проверка входа: пустые формы и списки → `InvalidInput`. При `checkInputs` каждый аргумент и
   инструмент проходит `BRepAlgoAPI_Check` (малые рёбра, самопересечения); замечания →
   `InvalidInput` с перечнем статусов (`SelfIntersect`, `TooSmallEdge`…). Такой вход сначала
   лечат `healShape`.
2. Лестница допусков: по умолчанию `0, 1e-7, 1e-6, 1e-5, 1e-4` (`fuzzyStart`, `fuzzyFactor`,
   `maxFuzzyValue`) или явная `fuzzyLadder`, отсортированная и обрезанная по `maxFuzzyValue`.
3. Каждая попытка: `BRepAlgoAPI_Fuse/Cut/Common` через `SetArguments`/`SetTools`,
   `SetFuzzyValue`, `SetRunParallel`, `SetNonDestructive(true)`, `Build()`; затем `HasErrors`
   (тексты ошибок из отчёта), `HasWarnings` (сохраняются). Сразу после `Build()` (при
   `simplifyResult`) вызывается `BRepAlgoAPI_BuilderAlgo::SimplifyResult` (UnifySameDomain с
   нечётким допуском), и **проверяется один раз** упрощённый результат; неупрощённый
   проверяется, только если упрощённый отвергнут (тогда предупреждение «SimplifyResult
   rejected» и возвращается неупрощённый). Проверка результата: `BRepCheck_Analyzer`, объём
   (`BRepGProp::VolumeProperties`, нужен для `BooleanResult::volume`) и критерии ожидаемого
   результата:
   - `checkVolumeBounds` (только если все входы — тела): объём в пределах, допустимых для
     операции (Fuse: `[max Vi, ΣVi]`, Cut: `[Va − Vt, Va]`, Common: `[0, min]`) с запасом
     `(fuzzy+1e-6)·ΣA + 1e-9·ΣV`. Объём и площадь входов считаются один раз на вызов и только
     при этой проверке; известные значения передаются в `argumentMass`/`toolMass` (вектор по
     числу входов, `nullopt` — вычислить; неверный размер или отрицательное значение →
     `InvalidInput`). Площадь результата при этой проверке тоже считается и возвращается в
     `BooleanResult::mass` — её можно передать следующей операции цепочки как `argumentMass`;
   - `expectedSolidCount`: число тел;
   - `minSolidThickness`: тела с `2V/A` меньше порога считаются щелями-артефактами;
   - результат Fuse не может быть пустым.
4. Первая приемлемая попытка возвращается.
5. Все попытки неудачны → `OperationFailed`, сообщение содержит причину каждой попытки.

Стоимость (2026-11-12): раньше каждая принятая попытка проверялась дважды (`BRepCheck_Analyzer`
и масса до и после упрощения), а объём и площадь аргумента пересчитывались на каждом вызове —
по профилю около 60 % времени выреза в цепочке `stress_part_300` уходило на проверки и массу, а
не на булеву операцию. Замер после изменения — `docs/qa/PERFORMANCE.md`.

`BooleanResult`: форма, `fuzzyValue` удачной попытки, `attempts`, `simplifyApplied`, `volume`,
`mass` (объём и площадь — если проверялись границы объёма),
`warnings` (причины отказа прежних попыток и предупреждения OCCT), `history`.

### История (`BooleanResult::history`)

`opencascade::handle<BRepTools_History>` от элементов входов (аргументов и инструментов:
вершин, рёбер, граней, тел — `BRepTools_History::IsSupportedType`) к `shape`. Берётся из
`BRepAlgoAPI_BuilderAlgo::History()` удачной попытки (`SetToFillHistory(true)`), ядро ничего
не восстанавливает само.

Как OCCT 8.0.1 её строит (`BOPAlgo_Builder::PrepareHistory`, `BRepAlgoAPI_BuilderAlgo`):

- **Modified** — части (splits) элемента входа, попавшие в результат, с исправленной
  ориентацией (у Cut грани инструмента входят развёрнутыми); тело аргумента — Modified в тело
  результата.
- **Generated** — новые элементы, порождённые элементом входа: рёбра и вершины пересечения
  граней (ребро пересечения — Generated сразу из обеих граней), вершины пересечения рёбер.
- **IsRemoved** — элемент входа без Modified, которого нет в результате (торцы сверла снаружи
  плиты, общая грань двух слитых коробок).
- Элемент входа, не упомянутый в истории и присутствующий в `shape`, не изменился.
- Modified не означает изменения геометрии: если у грани слито или разбито граничное ребро
  (верхи двух соседних коробок с общим ребром x = 10), OCCT строит новую `TopoDS_Face` на той
  же поверхности и сообщает её как Modified. `UnifySameDomain` (SafeInputMode) так же
  сообщает Modified для геометрически прежних граней перестроенного тела (торцы тех же
  коробок). Ссылки на элементы поэтому разрешаются только через историю, не через `IsSame`.
- **SimplifyResult**: `BRepAlgoAPI_BuilderAlgo::SimplifyResult` запускает
  `ShapeUpgrade_UnifySameDomain` и **сам** выполняет `myHistory->Merge(историяУпрощения)`;
  `History()` после него — уже композиция «вход → результат операции → упрощённый
  результат». Компланарные грани, слитые в одну, — Modified в общую грань. Так как слияние
  меняет историю на месте, `booleanOp` перед `SimplifyResult` сохраняет копию истории
  (`Merge` в пустую `BRepTools_History`); если упрощённый результат отвергнут проверками,
  возвращается неупрощённая форма вместе с этой копией — история всегда соответствует `shape`.
- Пустой результат (Cut/Common без остатка) и ошибка — без истории (`history` null).
  Результат с гранями, но без истории OCCT — `OperationFailed`.

`SetNonDestructive(true)`: входы не меняются, поэтому ключи истории — элементы исходных форм
вызывающего кода.

### Воспроизводимый пример неточного входа

Коробка A = [0,10]³ и коробка B = [10+1e-6, 20]×[0,10]² (зазор 1 мкм, типичный для данных,
округлённых при экспорте). Точный `BRepAlgoAPI_Fuse` (FuzzyValue = 0) даёт **валидный, но
неожиданный** результат — два отдельных тела. С `expectedSolidCount = 1` лестница отвергает
попытки 0 и 1e-7 («result has 2 solids») и на 1e-6 получает одно валидное тело объёмом
2000 (+1e-4 закрытого зазора), после `SimplifyResult` — 6 граней. Это тест
`KernelBoolean.FuzzyLadderClosesMicroGap`.

Второй пример: Cut коробки той же коробкой, сдвинутой на (1e-6, 1e-6, 1e-6). Точная операция
оставляет L-образную щель толщиной 1 мкм (объём ≈ 3e-4); с `minSolidThickness = 1e-4` она
отвергается, нечёткая попытка даёт ожидаемый пустой результат
(`KernelBoolean.SliverSolidIsRejected`).

В OCCT 8.0.1 на простых примитивах с зазорами/наложениями 1e-6 (коробки, соосные и смещённые
цилиндры, сферы, повёрнутые на 1e-7 рад коробки) точная операция не давала невалидной формы —
отличие было только в числе тел, щелях и лишних гранях. Поэтому критерии «ожидаемого»
результата задаются опциями: валидности `BRepCheck_Analyzer` недостаточно.

### Риски роста допуска

- `FuzzyValue` сливает элементы, отстоящие меньше чем на допуск: пропадают настоящие зазоры и
  тонкие стенки (замер на OCCT 8.0.1: Cut цилиндра r=5 цилиндром r=5−1e-6 при 1e-6 даёт пустоту).
- Допуски рёбер и вершин результата вырастают до порядка `FuzzyValue`; последующие операции
  наследуют эти допуски, и ошибки накапливаются по дереву построения.
- Объём меняется на `FuzzyValue · площадь контакта` (в примере выше +1e-4 мм³).
- Поэтому по умолчанию верхняя ступень 1e-4 мм, а используемый допуск возвращается
  (`fuzzyValue`) — вызывающий код должен показывать его пользователю или фиксировать в истории.

### Пример

```cpp
#include <bendcore/kernel/boolean.hpp>
using namespace bendcore::kernel;

BooleanOptions opt;
opt.expectedSolidCount = 1;
auto r = booleanOp(BooleanType::Cut, {plate}, {hole1, hole2, hole3}, opt);
if (!r) { report(r.error().message); return; }
if (r.value().fuzzyValue > 0) note("использован допуск " + std::to_string(r.value().fuzzyValue));
```

## 3. Ограничения

- Перевод в аналитику обрабатывает только `Geom_BSplineSurface`/`Geom_BezierSurface` и
  `Geom_BSplineCurve`/`Geom_BezierCurve` (в том числе под `Trimmed`); поверхности вращения и
  выдавливания остаются как есть (их упрощает `ShapeCustom::SweptToElementary`, в модуль не
  включён).
- Кривая ребра не переводится, если хотя бы одна его грань остаётся сплайновой.
- Поверхность, где `SurfToAnaSurf` находит кандидата, но независимая проверка на сетке 7×7 его
  отвергает, остаётся сплайновой (без предупреждения: это не ошибка).
- Проверка шага по объёму делается только для форм с телами; для оболочек — только валидность.
- `removeSmallElements` может сдвинуть вершины на величину до `smallElementSize`.
- Булевы операции проверяют критерии на всём результате; для многотельного Fuse задавайте
  `expectedSolidCount` осознанно.
- Метрика толщины `2V/A` — грубая: тонкие, но настоящие пластины тоже под неё попадают;
  по умолчанию проверка выключена.

## 4. Как тестировать

```bash
cmake -S . -B build-occt -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBENDCORE_WITH_OCCT=ON -DBENDCORE_OCCT_DIR=/home/user/bendcore/third_party/occt-install-checked
cmake --build build-occt --target test_kernel_healing test_kernel_boolean
ctest --test-dir build-occt -R "kernel_(healing|boolean)" --output-on-failure
```

Эталоны в тестах независимы от кода:

- `tests/kernel/test_healing.cpp`: цилиндр, сфера, конус, тор → `BRepBuilderAPI_NurbsConvert` →
  после лечения типы граней Cylinder/Sphere/Cone/Torus/Plane и рёбер Circle/Line; объёмы против
  πr²h, 4/3πr³, πh/3(r1²+r1r2+r2²), 2π²Rr² (отн. 1e-6). Эллиптический «цилиндр» (растяжение
  1.02) остаётся B-сплайном. Fuse двух коробок 10 граней → 6; две половины цилиндра 6 → 3.
  Призма с ребром 2.8e-6 мм: 7 граней/15 рёбер → 6/12, объём 500. Зеркальный цилиндр: 3
  левые поверхности → 0. NURBS-фьюжн коробок: 10 B-сплайнов → 6 плоскостей. Пустая/нулевая
  форма и неверные опции → `InvalidInput`.
- `tests/kernel/test_boolean.cpp`: Fuse/Cut/Common коробок (1875/875/125), отверстие в плите
  (4000 − 90π), тело Штейнмеца 16/3 r³ и объединение цилиндров, бобышка, списки
  инструментов (три отверстия) и аргументов (три куба и брус), SimplifyResult (10 → 6
  граней), микрозазор и щель (раздел 2), исчерпание лестницы с перечнем причин,
  самопересекающийся вход (`BRepAlgoAPI_Check` → `InvalidInput`), пустые входы.
- `KernelBooleanHistory.*` (там же): сквозное отверстие r=3 в коробке 20×20×10 с
  `SimplifyResult` и без — верх и низ Modified в одну грань площадью 400 − 9π с двумя
  контурами, четыре боковые грани в результате без изменений, цилиндрическая грань результата —
  Modified из боковой грани цилиндра (площадь 2π·3·10), торцы цилиндра IsRemoved, ровно два
  Generated ребра пересечения (окружности длиной 6π на z = 0 и z = 10, общие для грани
  коробки и грани цилиндра), тело коробки Modified в тело результата. Слияние соседних
  коробок: без упрощения 10 граней, верхи Modified в грани 10×10 (их общее ребро x = 10
  слито операцией), торцы x = 0 и x = 20 неизменны; с упрощением 6 граней, верхи обеих
  коробок Modified в одну грань 20×10, торцы Modified в грани 10×10 той же плоскости; общие
  грани IsRemoved. Для всех вариантов проверяется
  согласованность: каждый образ лежит в результате, каждый элемент входа ровно в одном
  состоянии (удалён / изменён / сохранён) и каждая грань результата достижима из истории или
  является неизменённой гранью входа. Пустой результат — без истории.
- `KernelBooleanMass.*` (там же): масса результата выреза R5 в бруске 40×30×10 — объём
  12000 − 250π и площадь 3800 + 50π; переданная следующему вырезу как `argumentMass` даёт
  12000 − 500π и 3800 + 100π (отн. 1e-6). Заведомо неверная переданная масса (1 мм³) заставляет
  проверку границ отвергнуть каждую попытку — значит, она заменяет вычисление; без
  `checkVolumeBounds` масса не нужна и `mass` пуст. Неверный размер вектора или отрицательный
  объём → `InvalidInput`.
