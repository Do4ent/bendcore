# Ядро: Shape, обмен BRep/STEP, тесселяция (`bendcore_kernel`)

Фаза 0, пп. 4–5 `docs/ROADMAP_GENERAL_CAD.md`. Библиотека `bendcore_kernel` (STATIC),
исходники `src/kernel/`, заголовки `include/bendcore/kernel/`, пространство имён
`bendcore::kernel`, сборка — `cmake/kernel_core.cmake` (подключается при
`BENDCORE_WITH_OCCT=ON`). Основа — OCCT 8.0.1.

## Назначение

- `Shape` — значение-обёртка над `TopoDS_Shape`: проверка валидности, габарит, объём,
  площадь, центр масс, счётчики топологии.
- `TopologyIndex` — детерминированная нумерация граней, рёбер и вершин 0..n-1 с типами
  поверхностей и кривых. Эти индексы — общий язык для выбора во вьюпорте, именования и
  сериализации.
- `brep_io.hpp` — BRep в памяти: бинарный (BinTools) и текстовый (BRepTools).
- `step_io.hpp` — STEP из буфера и в буфер, без файловой системы, модель всегда в мм.
- `tessellation.hpp` — сетка для вьюпорта: треугольники сгруппированы по граням, полилинии
  рёбер, всё привязано к `TopologyIndex`.

Все функции возвращают `Result<T>` (`result.hpp`); исключения OCCT (`Standard_Failure`) и
`std::exception` перехватываются на границе модуля и превращаются в `Error` с кодом и текстом.

## API и примеры

### Shape и TopologyIndex (`shape.hpp`)

```cpp
#include <bendcore/kernel/shape.hpp>
using namespace bendcore::kernel;

Shape box(BRepPrimAPI_MakeBox(10, 20, 30).Shape());
ValidityReport check = box.checkValidity();      // check.valid, check.issues
auto v = box.volumeProperties();                 // v.value().volume == 6000, centerOfMass
auto s = box.surfaceProperties();                // area == 2200, centroid
auto b = box.boundingBox();                      // min {0,0,0}, max {10,20,30}
TopologyCounts c = box.counts();                 // 1 solid, 1 shell, 6 faces, 12 edges, 8 vertices

auto index = TopologyIndex::build(box);
for (const FaceInfo& f : index.value().faces())  // f.face, f.surface (SurfaceType)
    ...;
std::optional<std::uint32_t> i = index.value().faceIndex(someFace);  // ориентация не важна
```

| Метод | Ошибки |
|---|---|
| `checkValidity()` | не бросает; для пустой формы `valid=false` и причина «shape is null» |
| `boundingBox()` | `InvalidInput` (пустая форма), `InvalidShape` (нет геометрии) |
| `volumeProperties()` | `InvalidInput` (пустая форма или нет ни одного тела) |
| `surfaceProperties()` | `InvalidInput` (пустая форма или нет граней) |
| `TopologyIndex::build()` | `InvalidInput` (пустая форма) |

`SurfaceType`: `Plane, Cylinder, Cone, Sphere, Torus, BSpline, Other` (Other — Безье,
поверхности вращения и выдавливания, offset и прочие).
`CurveType`: `Line, Circle, Ellipse, BSpline, Other, Degenerated` (Degenerated — ребро,
стянутое в точку, например полюса сферы).

Строки `ValidityReport::issues` имеют вид `SHELL #0: BRepCheck_NotClosed` или
`EDGE #3 in FACE #1: BRepCheck_InvalidCurveOnSurface`, где номер — индекс подформы среди
подформ того же типа в порядке `TopExp::MapShapes`.

### BRep (`brep_io.hpp`)

```cpp
auto bytes = writeBrepBinary(shape);                     // Result<std::vector<uint8_t>>
auto back  = readBrepBinary(bytes.value());              // Result<Shape>
auto text  = writeBrepText(shape, {.withTriangles = true});
auto back2 = readBrepText(text.value());
```

`BrepWriteOptions::withTriangles` — сохранять ли триангуляцию граней и полигоны рёбер
(по умолчанию нет). Пустой вход, мусор, обрезанный файл → `ParseError`; пустая форма при
записи → `InvalidInput`.

### STEP (`step_io.hpp`)

```cpp
auto step = writeStep(shape, {.fileUnit = StepLengthUnit::Metre});  // Result<std::string>
auto read = readStep(step.value());                                 // Result<Shape>, в мм
```

Запись: AP214 IS, `STEPControl_AsIs`, единица файла — мм (по умолчанию), м или дюйм.
Чтение: все корни (`TransferRoots`), несколько корней — один компаунд
(`OneShape`). Нечитаемый текст, файл без корней → `ParseError`; корни без формы после
передачи → `OperationFailed`.

### Тесселяция (`tessellation.hpp`)

```cpp
TessellationParams p;
p.linearDeflection = 0.05;   // мм (или доля размера при relative = true)
p.angularDeflection = 0.2;   // рад
auto mesh = tessellate(shape, p);
for (const FaceSpan& span : mesh.value().faceSpans)
    // треугольники грани span.faceIndex: indices[span.firstIndex .. + span.indexCount)
```

`Mesh`:

| Поле | Содержимое |
|---|---|
| `positions` | float xyz на вершину, глобальные координаты, мм |
| `normals` | единичные float xyz на вершину, наружу из материала |
| `indices` | uint32, тройки; обход против часовой стрелки при взгляде снаружи |
| `faceSpans` | `{faceIndex, firstIndex, indexCount}` в порядке `TopologyIndex::faces()`; диапазоны идут подряд без перекрытий и покрывают все `indices` |
| `edgePolylines` | `{edgeIndex, points}` — точки xyz полилинии ребра |
| `missingFaces` | индексы граней, для которых BRepMesh не построил триангуляцию |
| `missingEdges` | индексы невырожденных рёбер без полигона |

Вершины не разделяются между гранями (у каждой грани свои вершины): иначе нормали на
острых рёбрах усреднялись бы. Индексы `faceIndex`/`edgeIndex` — это `TopologyIndex`, т. е.
выбор треугольника во вьюпорте даёт грань B-rep.

Ошибки: пустая форма, нет граней и рёбер, неположительный или NaN прогиб → `InvalidInput`;
сбой BRepMesh → `OperationFailed`; при `requireAllFaces = true` (по умолчанию) хотя бы одна
грань без триангуляции → `OperationFailed` с числом и номерами граней. При
`requireAllFaces = false` такие грани перечислены в `missingFaces`, остальная сетка
возвращается.

## Алгоритмы

- **Валидность**: `BRepCheck_Analyzer(shape, GeomControls=true)`; при `IsValid()==false`
  обходятся все подформы (`TopExp::MapShapes`), из `BRepCheck_Result` собираются статусы
  самой подформы и статусы в контексте (`InitContextIterator`), текст — `BRepCheck::Print`.
- **Габарит**: `BRepBndLib::AddOptimal(shape, box, useTriangulation=false,
  useShapeTolerance=false)` — точный габарит по геометрии без допусков.
- **Свойства**: `BRepGProp::VolumeProperties` / `SurfaceProperties`. Объём знаковый:
  у вывернутого тела (`Reversed()`) он отрицательный.
- **Индексация**: `TopExp::MapShapes(shape, TopAbs_FACE|EDGE|VERTEX)`; индекс = позиция в
  `NCollection_IndexedMap` минус 1. Порядок определяется обходом структуры формы
  (`TopExp_Explorer`), поэтому одна и та же форма и её круговой проход через BRep дают те же
  индексы. Подформы различаются по `IsSame` (TShape + Location): одно тело, вставленное
  дважды с разными положениями, даёт две группы граней. Тип поверхности — `BRepAdaptor_Surface::GetType`,
  тип кривой — `BRepAdaptor_Curve::GetType`, вырожденность — `BRep_Tool::Degenerated`.
- **BRep-ввод**: данные читаются через собственный `std::streambuf` поверх буфера (без
  копирования, с поддержкой `seekg`). В маске исключений потока стоят `failbit|badbit`:
  `BRepTools::Read` на обрезанном тексте иначе зацикливается в
  `BRepTools_ShapeSet::ReadGeometry` (не проверяет состояние потока), а так первое
  неудачное извлечение выходит исключением и превращается в `ParseError`.
- **STEP**: `STEPControl_Reader::ReadStream(name, istream)` (сигнатура 8.0.1), затем
  `SetSystemLengthUnit(1.0)` — модель в мм, `TransferRoots`, `OneShape`. Запись:
  `STEPControl_Writer`, `Model()->SetLocalLengthUnit(1.0)`, `DESTEP_Parameters::WriteUnit`
  задаёт единицу файла, `Transfer(shape, STEPControl_AsIs, params)`, `WriteStream(ostream)`.
- **Тесселяция**: `BRepTools::Clean` (удаляет прежнюю триангуляцию, чтобы результат зависел
  только от параметров; триангуляция граней без поверхности сохраняется), затем
  `BRepMesh_IncrementalMesh(shape, IMeshTools_Parameters{Deflection, Angle, Relative,
  InParallel})`. Для каждой грани из `TopologyIndex`:
  - `BRep_Tool::Triangulation(face, loc)`; узлы переводятся в глобальные координаты
    `loc.Transformation()`;
  - у грани `TopAbs_REVERSED` порядок узлов треугольника меняется (n2↔n3) и нормаль
    поверхности разворачивается — триангуляция OCCT ориентирована по параметризации
    поверхности (Du×Dv), а не по грани;
  - нормаль — по поверхности в UV-узле: `BRepLProp_SLProps` на `BRepAdaptor_Surface(face)`
    (адаптер учитывает положение грани, нормаль сразу глобальная). Где нормаль не определена
    (полюс сферы) или у грани нет поверхности/UV (триангуляция из сеточного импорта) —
    взвешенная по площади сумма нормалей соседних треугольников.
  - Полилиния ребра: `BRep_Tool::PolygonOnTriangulation(edge, tri, loc)` на триангуляции
    первой смежной грани (`TopExp::MapShapesAndAncestors`), для свободного ребра —
    `BRep_Tool::Polygon3D`. Вырожденные рёбра полилинии не имеют и в `missingEdges` не
    попадают.

## Допуски и единицы

- Длины — миллиметры везде внутри bendcore; STEP пересчитывается при чтении из единицы
  файла (мм, м, дюйм и прочие, поддерживаемые OCCT) и записывается в выбранной единице.
- Углы — радианы.
- Свойства (`BRepGProp`) точны для аналитических поверхностей с относительной погрешностью
  ~1e-9 (тор ~1e-7); тесты сверяют с формулами с допуском 1e-9…1e-7.
- Сетка: `float` координаты (около 7 значащих цифр: при координатах ~1e3 мм погрешность
  ~1e-4 мм). Нормали единичные с погрешностью < 1e-5.
- Точность сетки задают `linearDeflection` (максимальное отклонение от поверхности) и
  `angularDeflection`; при грубом угловом прогибе (0.5 рад по умолчанию) площадь сферы
  занижена на проценты, при 0.004 мм / 0.5 рад — < 0.1 %.

## Ограничения

- `tessellate` изменяет триангуляцию, хранящуюся в TShape формы (стандартное поведение OCCT):
  формы, разделяющие TShape, видят новую триангуляцию. Для параллельной тесселяции одной и
  той же формы из разных потоков нужна внешняя синхронизация.
- Бесконечные грани (неограниченная плоскость) BRepMesh не триангулирует — они попадают в
  `missingFaces` или дают ошибку.
- STEP: читается только геометрия (без имён, цветов и структуры сборки — это XCAF, отдельная
  задача); при записи — AP214 IS без атрибутов.
- `BrepWriteOptions` не сохраняет нормали триангуляции (нормали вычисляются заново при
  тесселяции).
- Отчёт валидности перечисляет статусы BRepCheck; исправление формы — модуль лечения
  геометрии (`healing.hpp`), не этот.

## Как тестировать

```bash
cmake -S . -B build-occt -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBENDCORE_WITH_OCCT=ON -DBENDCORE_OCCT_DIR=/home/user/bendcore/third_party/occt-install
cmake --build build-occt --target test_kernel_shape test_kernel_brep_io \
  test_kernel_step_io test_kernel_tessellation
ctest --test-dir build-occt -R '^kernel_' --output-on-failure
```

Тесты (`tests/kernel/`, GoogleTest) сверяют с независимыми эталонами:

- `test_kernel_shape` — объём, площадь, центр масс коробки, цилиндра, сферы, конуса, тора
  против формул; габарит повёрнутой коробки ((a+b)/√2); число и типы граней и рёбер
  (включая B-сплайн после `NurbsConvert`, эллипс и «прочее» у выдавленного эллипса);
  отрицательный объём вывернутого тела; тело с незамкнутой оболочкой → `NotClosed`;
  детерминированность и поиск индексов.
- `test_kernel_brep_io` — круговой проход bin/text: объём, по индексу — тип и площадь каждой
  грани, ориентация, координаты вершин; сохранение положения; триангуляция пишется только
  по запросу; случайные байты, пустой и обрезанный вход → `ParseError`.
- `test_kernel_step_io` — круговой проход STEP коробки, цилиндра, сферы (объём, число и типы
  граней); единицы мм/м/дюйм: в файле нужная единица и координаты угла в ней, после чтения —
  габарит в мм; компаунд из двух тел; мусор и STEP без данных → `ParseError`.
- `test_kernel_tessellation` — диапазоны граней покрывают все индексы без перекрытий,
  индексы < числа вершин, нормали единичные и согласованы с обходом треугольников; объём по
  теореме Гаусса — Остроградского положителен и равен объёму тела (замкнутость и наружная
  ориентация, в т. ч. для граней `REVERSED`); нормали сферы радиальные, включая полюса;
  площадь сетки сходится к 4πr² при уменьшении прогиба; относительный режим не зависит от
  масштаба; поворот+сдвиг (TopLoc) применён к вершинам, нормалям и полилиниям; два положения
  одного тела; полилинии рёбер на цилиндре, свободное ребро, вырожденные рёбра сферы;
  бесконечная грань → ошибка или `missingFaces`; грань только с триангуляцией; неверные
  параметры.
