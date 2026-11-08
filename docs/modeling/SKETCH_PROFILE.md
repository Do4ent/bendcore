# Эскиз в модели: размещение, профили, проекция рёбер (`bendcore_modeling_sketch`)

Фаза 1, п. 2 `docs/ROADMAP_GENERAL_CAD.md`. Библиотека `bendcore_modeling_sketch` (STATIC),
исходники `src/modeling/sketch/`, заголовок `include/bendcore/modeling/sketch_profile.hpp`,
пространство имён `bendcore::modeling`, сборка — `cmake/modeling_sketch.cmake` (подключается
при `BENDCORE_WITH_OCCT=ON`). Зависит от `bendcore_kernel` (OCCT) и `bendcore_sketch2d`
(PlaneGCS). Ошибки — `kernel::Result<T>` (`include/bendcore/kernel/result.hpp`), исключения
OCCT перехватываются и превращаются в `Error`.

## Назначение

Превращает решённый 2D-эскиз в то, с чем работают операции детали:

1. `SketchPlacement` — правая локальная система эскиза (начало, нормаль, ось X) на
   плоскости или на плоской грани формы.
2. `SketchProfile` — замкнутые контуры (`ProfileLoop`), их вложенность и области
   (`ProfileRegion`) с плоскими гранями OCCT; выбор областей для операции.
3. `checkSketchCurves` — диагностика с идентификаторами сущностей: разомкнутые контуры,
   ветвления, пересечения, перекрытия, самопересечения, вырожденные кривые.
4. `BoundSketch` — привязка к `sketch2d::Sketch`: номера сущностей профиля и их геометрия
   PlaneGCS (отрезки, окружности, нативные дуги, эллипсы, дуги эллипсов, B-сплайны).
   Решатель, ограничения, диагностика и инструменты редактирования — `SKETCH.md`.
5. `projectEdge` / `projectEdges` / `projectVertex` — проекция рёбер и вершин модели на
   плоскость эскиза в виде 2D-геометрии.

## API

### Размещение

```cpp
#include <bendcore/modeling/sketch_profile.hpp>
using namespace bendcore::modeling;

auto onXY   = SketchPlacement::fromPlane(gp_Pln());                       // всегда правая
auto tilted = SketchPlacement::fromAxes(gp_Pnt(5,5,5), gp_Dir(0,-0.5,0.866), gp_Dir(1,0,0));
auto onTop  = SketchPlacement::onFace(topologyIndex, faceIndex);         // Result
auto custom = SketchPlacement::onFace(face, {.origin = gp_Pnt(5,10,0), .xDirection = gp_Dir(0,1,0)});

gp_Pnt w = tilted.value().toWorld({30, 20});    // origin + 30 X + 20 Y, Y = N x X
Point2 l = tilted.value().toLocal(w);            // ортогональная проекция
gp_Trsf t = tilted.value().localToWorld();       // XY-плоскость -> плоскость эскиза
```

| Функция | Ошибки (`InvalidInput`) |
|---|---|
| `fromAxes` | ось X параллельна нормали (X проецируется на плоскость, иначе) |
| `fromFrame` | левая система (`!gp_Ax3::Direct()`) |
| `onFace` | грань пустая, без поверхности, неплоская; индекс вне диапазона |

Нормаль на грани — внешняя нормаль грани: нормаль поверхности `Du × Dv = X × Y` плоскости,
развёрнутая для грани `TopAbs_REVERSED`. Начало по умолчанию — точка привязки плоскости
(`gp_Pln::Location`), ось X — ось X плоскости; заданные `origin`/`xDirection` проецируются на
плоскость грани.

### Кривые эскиза

`Curve2d = variant<LineSegment2d, CircularArc2d, Circle2d, Ellipse2d, EllipticalArc2d, BSpline2d>`,
`SketchCurve{id, geometry, construction}`.

- `CircularArc2d{center, start, end, counterClockwise}` — радиус `|start − center|`, конец
  обязан лежать на той же окружности (в пределах допуска); направление выбирает одну из двух
  дуг.
- `Ellipse2d{center, majorDirection, majorRadius, minorRadius}`; `EllipticalArc2d` — параметры
  `t` от `startParameter` до `endParameter` (> start, размах ≤ 2π) в системе (U, V), V = U
  повёрнутая на +90°. Если `minorRadius > majorRadius`, система пересчитывается (U' = V,
  t' = t − π/2), геометрия та же.
- `BSpline2d` — полюса, веса (пусто — нерациональный), различные узлы с кратностями,
  `periodic`. Замкнутым считается периодический сплайн или сплайн с совпадающими концами.
- `construction = true` — вспомогательная геометрия: в профиль не входит и в проверках не
  участвует.

### Профиль

```cpp
std::vector<SketchCurve> curves = /* прямоугольник 1..4, окружность 5 */;
auto profile = SketchProfile::build(placement, curves);     // Result<SketchProfile>
if (!profile.ok()) std::cerr << profile.error().message;    // перечень проблем с ID

const auto& loops = profile.value().loops();     // entities, wire, area, parent, depth, children
const auto& regions = profile.value().regions(); // outerLoop, holeLoops, face, area, depth
auto material = profile.value().makeMaterialFaces();        // области чётной глубины
auto chosen   = profile.value().makeFaces({0, 2});          // по индексам
auto picked   = profile.value().makeFacesAt({{50, 45}});    // по точкам
std::optional<SketchEntityId> id = profile.value().entityOf(someEdge);
```

- Контур (`ProfileLoop`) — простой замкнутый цикл кривых. `entities` — в порядке обхода,
  начиная с наименьшего ID; `wire` ориентирован против часовой стрелки при взгляде с
  нормали; `area > 0`.
- Вложенность любой глубины: `depth` — число охватывающих контуров, `parent` — ближайший
  (наименьший по площади) охватывающий контур, `children` — обратная связь.
- Область `i` ограничена контуром `i` снаружи и его детьми изнутри; областей столько же,
  сколько контуров. `depth` области: 0 — внешний контур, 1 — отверстие, 2 — остров и т. д.
  `materialRegions()` — области чётной глубины (обычный выбор для выдавливания).
- Грани областей построены на `Geom_Plane(placement.frame())`: нормаль грани = нормаль
  эскиза, параметры (u, v) поверхности совпадают с координатами эскиза. Каждая сущность даёт
  одно ребро (`edges()`), общее для всех проводов и граней, где оно встречается, — смежные
  области делят ребро.
- Порядок контуров и областей детерминирован: по наименьшему ID сущности контура. При
  изменении размеров эскиза (без изменения топологии) номера контуров, областей и состав
  `entities` сохраняются — это проверяет тест `BoundSketchRebuildAfterDimensionChange`.
- `makeFaces`: ошибка при пустом выборе, индексе вне диапазона, повторе; `makeFacesAt`: точка
  вне всех областей или на границе. Одна область — `TopoDS_Face`, несколько — `TopoDS_Compound`.

### Диагностика

`checkSketchCurves(curves, options)` → `vector<ProfileIssue>{kind, entities, location, detail}`;
пустой результат — профиль корректен. `SketchProfile::build` при наличии проблем возвращает
`InvalidInput` с сообщением вида

```
sketch profile is invalid:
  open contour [entities 10, 11, 12]: contour is not closed: free end at (120, 0)
  intersection [entities 2, 5]: curves 2 and 5 meet at (10, 5) which is not a shared end point
```

| `ProfileIssueKind` | Условие | `entities` |
|---|---|---|
| `DuplicateId` | одинаковый ID у нескольких кривых | ID |
| `Degenerate` | нулевая длина/радиус, конец дуги не на окружности, начало = конец дуги, неверные данные сплайна | кривая |
| `OpenContour` | у цепочки есть свободный конец | все кривые цепочки |
| `Branching` | в одной точке сходятся > 2 концов | кривые узла |
| `Intersection` | кривые пересекаются/касаются не в общем конце | пара |
| `Overlap` | кривые имеют общий участок | пара |
| `SelfIntersection` | сплайн пересекает сам себя | кривая |
| `Empty` | нет ни одной не вспомогательной кривой | — |

`ProfileOptions::tolerance` (по умолчанию 1e-6 мм) — допуск слияния концов и пересечений.

### Привязка к PlaneGCS (`BoundSketch`)

```cpp
BoundSketch bound;
auto& sk = bound.sketch();                       // sketch2d::Sketch для ограничений
auto p0 = sk.add_point(0, 0), p1 = sk.add_point(45, 1), ...;
auto l0 = bound.addLine(p0, p1);                 // ID сущности 1, 2, 3, ...
auto hole = bound.addCircle(pc, 4);
auto arc = bound.addArc(center, start, end, /*ccw*/ true);   // нативная дуга PlaneGCS
auto oval = bound.addEllipse(center, focus, /*minor*/ 3);
auto spline = bound.addBSpline(data);            // полюса — точки эскиза
bound.addLine(p0, p2, /*construction*/ true);
sk.constrain_tangent(bound.entity(l0).geo, bound.entity(arc).geo);
sk.constrain_distance(p0, p1, 50); ...
bound.solve();                                   // sketch2d::Report: dof, статусы элементов
auto curves = bound.curves();                    // Result<vector<SketchCurve>>
auto profile = SketchProfile::build(placement, curves.value());
```

Сущность (`BoundSketch::Entity`) хранит вид, ссылку `geo` на геометрию `sketch2d` и номера
точек (`start`, `end`, `center`, `focus`). Признак вспомогательной геометрии хранится в
`sketch2d` (`setConstruction` меняет оба). `removeEntity` удаляет сущность (номер не
переиспользуется, `curves()` её пропускает), `setEndPoints` меняет концы отрезка или дуги
(инструменты редактирования). `curve(id)` / `curves()` строят `SketchCurve` по текущим
координатам: дуга — `CircularArc2d`, эллипс — `Ellipse2d` (большая полуось — к фокусу),
дуга эллипса — `EllipticalArc2d` (параметры концов в системе эллипса, по часовой стрелке —
переставлены), сплайн — `BSpline2d`. Ошибка, если конец дуги (дуги эллипса) дальше допуска от
своей кривой (эскиз не решён).

### Проекция рёбер модели

```cpp
auto e = projectEdges(placement, topologyIndex, {3, 7});   // Result<vector<ProjectedEdge>>
// ProjectedEdge{edgeIndex, curve (Curve2d) | point (Point2)}
auto v = projectVertex(placement, topologyIndex, 0);       // Result<Point2>
```

Ортогональная проекция вдоль нормали эскиза: `GeomProjLib::ProjectOnPlane` (с сохранением
параметризации), затем `GeomAPI::To2d` в координаты эскиза и разбор типа кривой:

| Ребро | Результат |
|---|---|
| отрезок | `LineSegment2d` (концы по параметризации кривой ребра) |
| окружность на параллельной плоскости | `Circle2d` / `CircularArc2d` (направление — по виду с нормали эскиза) |
| окружность на наклонной плоскости, эллипс | `Ellipse2d` / `EllipticalArc2d` (или окружность, если радиусы равны) |
| плоская кривая, видимая «с ребра» | `LineSegment2d` между крайними точками проекции |
| ребро вдоль нормали | `point` |
| прочие (сплайн, Безье, offset) | `BSpline2d` |

Результат — те же типы, что вход профиля, поэтому проекцию можно сразу использовать как
кривую эскиза (тест `ArcKeepsEndPointsAndSense` строит из проекции полукруг). Ошибки:
вырожденное ребро, ребро без 3D-кривой, индекс вне диапазона.

## Алгоритм профиля и выбор инструментов OCCT

1. Каждая кривая → `Geom2d_*` на отрезке параметров (`Geom2d_Line`, `Geom2d_Circle`,
   `Geom2d_Ellipse`, `Geom2d_BSplineCurve`), проверка вырожденности.
2. Концы незамкнутых кривых сливаются в узлы (допуск `tolerance`).
3. Попарные пересечения `Geom2dAPI_InterCurveCurve` (с отсечением по `Bnd_Box2d`). Точка или
   короткий участок касания допустимы только у узла, где кончаются обе кривые (радиус
   `max(1000·tol, 1e-4)` — точность пересечения касательных кривых, например отрезка,
   касательного к дуге). Иное — `Intersection`/`Overlap`. Самопересечения сплайнов —
   `Geom2dAPI_InterCurveCurve(C)`.
4. Степени узлов: 1 → `OpenContour` (вся цепочка, через объединение компонент), > 2 →
   `Branching`.
5. Обход циклов; 3D-рёбра: `GeomAPI::To3d` на плоскость эскиза +
   `BRepBuilderAPI_MakeEdge(curve, V1, V2, p1, p2)` с общими вершинами узлов (допуск вершины
   покрывает разброс слитых концов); провод — `BRepBuilderAPI_MakeWire`.
6. Ориентация и площадь — формула Грина `∮ x dy` квадратурой Гаусса–Лежандра (5 точек ×
   64 подотрезка, для сплайнов больше): точно для отрезков, ~1e-15 для дуг. Провод с
   отрицательной площадью разворачивается.
7. Вложенность: внутренняя точка каждого контура классифицируется
   `BRepClass_FaceClassifier` относительно грани каждого другого контура. Так как контуры не
   пересекаются, одной точки достаточно.
8. Грани областей: `BRepBuilderAPI_MakeFace(plane, outerWire)` + `Add(childWire.Reversed())`,
   проверка `BRepCheck_Analyzer`; площадь области = площадь контура − площади детей, сверяется
   с `BRepGProp` (расхождение > 0,1 % — ошибка).

Почему не `BOPAlgo_Tools::EdgesToWires/WiresToFaces` и не `BRepAlgoAPI_Splitter`: они
сначала сливают/расщепляют рёбра общим булевым построителем. Пересечения и перекрытия при
этом «молча» превращаются в новые вершины и области, а связь «ребро → сущность эскиза» теряется
или требует разбора истории. Регламент требует явной ошибки с перечнем сущностей и
однозначного соответствия ребро ↔ сущность для топологического именования; собственный граф
узлов на 2D-кривых даёт и то, и другое, а OCCT используется там, где он точнее (пересечения,
классификация, построение рёбер и граней).

## Ограничения

- Контуры должны быть простыми и не касаться друг друга: эскиз, где отрезок делит
  прямоугольник на две области (общая граница, узлы степени 3), отклоняется как `Branching`.
  Выбор областей, разделённых общими рёбрами, — следующая итерация (через расщепление с
  сохранением истории).
- Точечные сущности эскиза в профиль не входят (они нужны только ограничениям).
- Непериодический сплайн `BoundSketch` — зажатый (концы — крайние полюса).
- Проекция плоской кривой «с ребра» даёт отрезок по крайним точкам (уточнение золотым
  сечением между 64 выборками); кратность проекции (например, окружность → дважды пройденный
  диаметр) не сохраняется.
- Площадь области из `BRepGProp` по умолчанию менее точна для сплайновых границ (~1e-5
  относительно); `ProfileRegion::area` вычислена по границе точнее.

## Как тестировать

```bash
cmake -S . -B build-occt -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBENDCORE_WITH_OCCT=ON \
  -DBENDCORE_OCCT_DIR=/home/user/bendcore/third_party/occt-install \
  -DBENDCORE_EIGEN_DIR=/home/user/bendcore/third_party/eigen-5.0.0
cmake --build build-occt --target test_modeling_sketch_profile test_modeling_sketch_projection \
  test_modeling_sketch_reference
ctest --test-dir build-occt -R modeling_sketch --output-on-failure
```

`tests/modeling/test_modeling_sketch_profile.cpp` (эталоны — формулы площадей и аналитические
векторы):

- прямоугольник 100×60 с отверстием R10: 6000 − 100π; два провода, нормаль +Z, `regionAt`;
- отверстие R20 с островом R8: области 6000 − 400π, 336π, 64π; выбор кольца точкой;
- вложенность 3 уровня (квадраты 100/60/30 и круг R5, в перемешанном порядке): глубины 0–3,
  родители, площади областей;
- дуги в обоих направлениях: обвод 40×20 (800 + 100π) с дугой по/против часовой, выпуклый и
  вогнутый торец (800 ± 50π);
- эллипс, эллиптическая дуга (с `minor > major`), периодический сплайн (площадь сверена с
  высокоточным `BRepGProp`);
- разомкнутый контур → `OpenContour` с ID 10, 11, 12 и текст ошибки; ветвление, пересечение,
  перекрытие, самопересекающийся сплайн «восьмёрка», вырожденные кривые, повтор ID, пустой
  профиль;
- вспомогательная геометрия не влияет на профиль;
- эскиз на плоскости под 30°: нормаль грани, площадь 600 − 16π, габарит, вершины в плоскости;
- эскиз на верхней, нижней и боковой гранях коробки: внешняя нормаль, площадь 16 − π,
  выдавливание по нормали наружу (объём и центр масс), параметры `origin`/`xDirection`;
- `BoundSketch`: прямоугольник с ограничениями и размерами, смена размера 50 → 80 →
  перестроение с теми же ID контуров и новой площадью 2400 − 25π; дуга PlaneGCS (50π); дуга до
  решения — ошибка.

`test_modeling_sketch_tools.cpp` (см. `SKETCH.md`): профиль паза из отрезков, касательных к
нативным дугам (2RL + πR²), профиль со сплайном (площадь против интеграла Симпсона),
вспомогательная геометрия вне профиля, профили после инструментов редактирования.

`test_modeling_sketch_projection.cpp`: рёбра коробки (8 отрезков, 4 точки), окружность →
окружность / эллипс с малой полуосью 5·cos30°, дуга с направлением (сверху/снизу),
сплайн (полюса проецируются аффинно), окружности цилиндра «с ребра» → диаметры.
