# STEP со структурой сборки, именами, цветами и слоями (`bendcore_kernel_xcaf`)

Библиотека `bendcore_kernel_xcaf` (STATIC), исходник `src/kernel/xcaf_io.cpp`, заголовок
`include/bendcore/kernel/xcaf_io.hpp`, пространство имён `bendcore::kernel`, сборка —
`cmake/kernel_xcaf.cmake` (подключается корневым `CMakeLists.txt` при
`BENDCORE_WITH_OCCT=ON`, после `kernel_core.cmake`; зависит от `bendcore_kernel`).

## Назначение

`readStep` из `step_io.hpp` возвращает только геометрию. Этот модуль читает и пишет STEP и
IGES вместе с тем, что OCCT хранит в документе XCAF: дерево сборки, имена, цвета, слои, единицы.
Результат чтения — `AssemblyDocument`, обычная структура значений, не зависящая от OCAF
(документ XCAF живёт только внутри вызова). Вход и выход — буферы в памяти; файловая система
не используется, кроме временного файла при чтении IGES (читатель IGES в OCCT читает только
файл, раздел «IGES»; в WASM это MEMFS).

## API

```cpp
#include <bendcore/kernel/xcaf_io.hpp>
using namespace bendcore::kernel;

Result<AssemblyDocument> doc = readStepAssembly(stepText);      // std::string_view
Result<std::string> text = writeStepAssembly(doc.value(),
    {.schema = StepSchema::AP242, .fileUnit = StepLengthUnit::Millimetre});
Result<std::vector<PlacedPart>> parts = flattenAssembly(doc.value());
```

### Структура

| Тип | Поля |
|---|---|
| `AssemblyDocument` | `definitions` (прототипы), `roots` (индексы свободных форм в порядке файла), `fileLengthUnits` (имена единиц длины файла, как их сообщает OCCT: `millimetre`, `metre`, `INCH`…), `fileUnitMm` (мм на единицу файла, если единица одна и известна), `warnings` (что было в файле, но не помещается в структуру) |
| `AssemblyDefinition` | `kind` (`Part`/`Assembly`), `name` (имя PRODUCT), `shape` (только деталь, в собственной СК), `components` (только сборка), `color` (цвет поверхности всего прототипа), `faceColors` (только деталь), `layers`, `productId` (PRODUCT `id` — обозначение; при записи `nullopt` → `id` = имя, при чтении заполнено всегда), `description` (PRODUCT `description` — наименование) |
| `AssemblyComponent` | вхождение: `name` (имя NEXT_ASSEMBLY_USAGE_OCCURRENCE), `definition` (индекс прототипа), `location` (`TopLoc_Location` относительно родительской сборки), `color` (переопределяет цвет прототипа), `layers` (только чтение, см. ограничения), `instanceId` (NAUO `id`; пустой при записи — нумерация OCCT `1`, `2`, …), `description` (NAUO `description`), `referenceDesignator` (NAUO `reference_designator`, необязательный; `$` в файле, если не задан) |
| `FaceColor` | `faceIndex` — индекс грани по `TopologyIndex` формы детали (`shape.hpp`), `color` |
| `AssemblyColor` | `r, g, b` — компоненты sRGB в [0, 1] (пространство `COLOUR_RGB` в STEP), `alpha` (1 — непрозрачный) |
| `PlacedPart` | вхождение детали в мировой СК: `root`, `componentPath`, `definition`, `path` (имена через `/`), `location` (произведение положений по пути), `shape` (форма детали, сдвинутая на `location`), `color` (действующий цвет), `faceColors` |

Один прототип с несколькими вхождениями хранится один раз (как в STEP и XCAF): у вхождений
одинаковый `definition`. Порядок `definitions` после чтения — обход в глубину от корней,
порядок вхождений — порядок компонентов в XCAF.

### Ошибки

| Функция | Код | Когда |
|---|---|---|
| `readStepAssembly` | `ParseError` | пустой вход, текст не разбирается (`ReadStream` ≠ `IFSelect_RetDone`), нет корней для передачи, исключение OCCT при разборе |
| | `OperationFailed` | `Transfer` в XCAF не удался, нет свободных форм, у вхождения нет прототипа, у детали нет формы |
| `writeStepAssembly`, `flattenAssembly` | `InvalidInput` | нет прототипов или корней; индекс вне диапазона; цикл; прототип не корень и не используется; корень используется вхождением; повторный корень; пустая форма детали; сборка без вхождений или с формой; цвет вне [0, 1] или не число; индекс грани вне диапазона или не по возрастанию/повтор |
| `writeStepAssembly` | `Unsupported` | слои у сборки или у вхождения |
| | `OperationFailed` | XCAF отверг форму/вхождение, `STEPCAFControl_Writer::Transfer` не удался, исключение OCCT |
| | `IoError` | `WriteStream` не записал поток |

Исключения OCCT (`Standard_Failure`) и `std::exception` перехватываются внутри функций.

## Алгоритм

**Чтение.** `XCAFApp_Application::NewDocument("MDTV-XCAF")`, единица документа — мм
(`XCAFDoc_DocumentTool::SetLengthUnit(doc, 1, Millimeter)`): `STEPCAFControl_Reader` берёт
её как системную и пересчитывает файл в мм. `ReadStream` из `std::istringstream`, режимы
имён, цветов, слоёв включены, `Transfer(doc)`. Далее преобразование:

- корни — `XCAFDoc_ShapeTool::GetFreeShapes`;
- метка-сборка (`IsAssembly`) → `Assembly`, вхождения — `GetComponents` (без рекурсии),
  прототип — `GetReferredShape`, положение — `GetLocation`; метки уже преобразованных
  прототипов запоминаются (по `TDF_Tool::Entry`), поэтому общий прототип не дублируется;
- иначе → `Part`, форма — `GetShape`;
- имя — `TDataStd_Name` (UTF-8 через `ToUTF8CString`);
- цвет — `XCAFDoc_ColorSurf`, иначе `XCAFDoc_ColorGen`; из `Quantity_ColorRGBA` в sRGB
  (`Quantity_TOC_sRGB`) и альфу;
- цвета граней — метки подформ детали (`GetSubShapes`): метка грани даёт цвет этой грани;
  метка тела, оболочки или компаунда раскрывается на все свои грани, но собственный цвет
  грани имеет приоритет; индекс грани — `TopologyIndex::faceIndex`;
- слои — `XCAFDoc_LayerTool::GetLayers`;
- единицы — `STEPControl_Reader::FileUnits`; `fileUnitMm` — по таблице имён (SI-единицы
  `<приставка>metre`, `inch`, `foot`, `yard`, `mile`, `mil`);
- атрибуты PRODUCT и NAUO: после `Transfer` обходятся сущности модели STEP
  (`reader.ChangeReader().WS()->Model()`): для `PRODUCT_DEFINITION` метка находится как в
  `STEPCAFControl_Reader::ReadNames` (`TransientProcess::Find` → форма → `GetShapeLabelMap`
  или `ShapeTool::Search`), для NAUO — `STEPCAFControl_Reader::FindInstance`; берутся `id`,
  `name`, `description`, `reference_designator`. Строки декодируются как UTF-8, если кодовая
  страница модели не `NoConversion`. Имя вхождения — NAUO `name` (OCCT предпочитает
  `description`, но здесь это отдельный атрибут), пустое — как у OCCT (`description`, затем `id`);
- SHUO (стиль, привязанный к пути вхождений в глубину) в структуру не помещается: для
  каждого вхождения с SHUO добавляется строка в `warnings`.

**Запись.** Проверка структуры (как у `flattenAssembly`), новый документ XCAF в мм,
прототипы создаются от корней в глубину, дети раньше родителей: деталь —
`AddShape(shape, makeAssembly=false)`, цвета граней — `AddSubShape` + `SetColor(…,
ColorSurf)`; сборка — `NewShape` + `AddComponent(label, ref, location)`; цвета, слои;
`UpdateAssemblies`. Меткам прототипов и вхождений даются временные уникальные имена-маркеры
(`bendcore-xcaf-definition-<i>`, `bendcore-xcaf-component-<i>-<k>`): `STEPCAFControl_Writer`
пишет имя метки в PRODUCT `name` и `id` и в NAUO `name`. После `Transfer` в модели STEP
сущность с маркером получает настоящие `name`, `id` (`productId` или имя), `description`, у
NAUO — `id` (если задан), `description`, `reference_designator` (`Init` заново, поля-флаги
необязательных атрибутов выставляются). Маркеры дают точное соответствие и при одинаковых
именах прототипов; если для прототипа не нашёлся PRODUCT или для вхождения не ровно один
NAUO — `OperationFailed` (маркеры в файл не попадают — тест проверяет). Корни создаются в порядке `roots`, поэтому свободные формы в файле идут
в том же порядке. `STEPCAFControl_Writer` с режимами имён, цветов и слоёв,
`DESTEP_Parameters` (`WriteSchema` = AP214IS или AP242DIS, `WriteUnit` = мм/м/дюйм),
`Transfer(doc, params, AsIs)`, `WriteStream(std::ostringstream)`.

**Развёртка** (`flattenAssembly`). Обход от каждого корня в глубину, положение — произведение
`TopLoc_Location` родителя и вхождения. Действующий цвет — как в
`XCAFPrs_DocumentExplorer` (OCCT): унаследованный от родителя, его заменяет цвет прототипа,
а тот — цвет вхождения. Цвета граней детали действуют поверх цвета вхождения.

## Допуски и единицы

- Длины в мм. При чтении файл в метрах или дюймах пересчитывается OCCT; тест проверяет
  габариты после записи в м/дюймах с допуском 1e-6 мм.
- Положения вхождений после кругового прохода совпадают поэлементно с допуском 1e-9.
- Цвет: `Quantity_Color` хранит линейный RGB в `float`, перевод sRGB ↔ линейный даёт
  погрешность ~1e-6…1e-5; тесты сверяют 0/1 с допуском 1e-6, промежуточные значения
  (0,5) — 1e-4.

## Ограничения

- **Слои** записываются только для деталей: `STEPCAFControl_Writer::writeLayers`
  пропускает метки сборок и вхождений («there is no way to assign layer to instance in
  STEP»). Поэтому `writeStepAssembly` отвечает `Unsupported`, если слои есть у сборки или
  вхождения, вместо тихой потери. Читаются слои на любых метках, которые создаёт OCCT.
  Слои граней не поддерживаются.
- **Цвета**: только цвет поверхности (ColorSurf/ColorGen); цвет кривых (ColorCurv),
  видимость, материалы (`XCAFDoc_VisMaterial`) не переносятся.
- **SHUO** (стиль конкретного пути вхождений в глубину) не представим в структуре: при
  чтении — предупреждение в `warnings`, при записи не создаётся.
- Цвет тела/оболочки внутри детали при чтении раскрывается в цвета граней; при записи
  такие цвета пишутся на грани (визуально то же, в файле — другие сущности).
- Внешние ссылки STEP (многофайловые сборки) не поддерживаются: `WriteStream` OCCT их не
  пишет, при чтении внешние файлы недоступны (нет файловой системы).
- PMI/GD&T, виды, прочие метаданные PRODUCT (категории, версии `PRODUCT_DEFINITION_FORMATION`,
  пользовательские свойства) не читаются и не пишутся; из PRODUCT переносятся `name`, `id`,
  `description`, из NAUO — `id`, `name`, `description`, `reference_designator`.
- `XCAFApp_Application` — глобальный объект OCCT; одновременные вызовы из разных потоков
  требуют внешней синхронизации.

## IGES

```cpp
Result<AssemblyDocument> doc = readIgesAssembly(igesText);              // IgesReadOptions
Result<std::string> text = writeIgesAssembly(doc.value(),
    {.mode = IgesWriteMode::BRep, .fileUnit = StepLengthUnit::Millimetre});
```

Та же структура `AssemblyDocument`, что у STEP; трансляторы — `IGESCAFControl_Reader` /
`IGESCAFControl_Writer` (TKDEIGES), режимы имён и цветов включены.

**Чтение.** Читатель IGES в OCCT работает только с файлом (`IGESFile_Read` открывает путь, у
`IGESSelect_WorkLibrary` нет чтения из потока), поэтому текст пишется во временный файл
`std::filesystem::temp_directory_path()/bendcore-iges-<случайное>-<счётчик>.igs` (в WASM — MEMFS
`/tmp`) и удаляется после чтения. До записи — предел размера (`maxInputBytes`, как у STEP) и
проверка первой записи: в столбце 73 должна стоять буква раздела S или G (IGES — записи по
80 столбцов). Далее `ReadFile` → `Transfer(doc)` в документ XCAF в мм (файл в дюймах или метрах
пересчитывается) и то же преобразование меток, что у STEP (`XcafToDocument`, без атрибутов
PRODUCT/NAUO — в IGES их нет; `productId` пустой). Единица файла — `GlobalSection().UnitName()`
в `fileLengthUnits`, `fileUnitMm` — по флагу единиц глобального раздела (IGES 5.3,
параметр 14: 1 дюйм, 2 мм, 4 фут, 5 миля, 6 м, 7 км, 8 mil, 9 мкм, 10 см, 11 микродюйм).

Два шага после передачи:

1. **Сшивка.** Деталь, у которой есть грани, но нет тела (IGES из обрезанных поверхностей,
   тип 144 — так пишет большинство CAD), сшивается `BRepBuilderAPI_Sewing` с допуском
   `sewingTolerance` (по умолчанию 10⁻³ мм); каждая замкнутая оболочка превращается в тело
   (`BRepBuilderAPI_MakeSolid` + `ShapeFix_Solid` для ориентации наружу). Цвета граней
   переносятся по истории сшивки. Если хотя бы одна оболочка не замкнута, деталь остаётся
   гранями. Каждый случай — строка в `warnings`. `sewingTolerance = 0` отключает шаг.
2. **Разбиение на детали.** IGES не хранит структуру изделия: `IGESCAFControl_Writer` (и
   другие системы) пишут тела сборки под одним корнем, и читатель возвращает одну деталь из
   нескольких тел. Если `splitSolids` (по умолчанию), корневая деталь из ≥ 2 тел становится
   сборкой: по детали на тело, имя — из метки XCAF этого тела (имена тел IGES), иначе
   `<имя>_<k>`; цвета граней тела, совпадающие у всех граней, становятся цветом детали.
   Положения — единичные (тела уже в мировой СК). `splitSolids = false` оставляет
   многотельную деталь.

**Запись.** Проверка структуры как у `writeStepAssembly`, документ XCAF (`DocumentToXcaf`),
метки получают настоящие имена (`useDocumentNames`: писатель IGES берёт имя метки);
`IGESControl_Controller::Init()`, статические параметры `write.iges.unit` (`MM`, `M`, `INCH`) и
`write.iges.brep.mode` (1 — тела как manifold solid B-rep object, тип 186; 0 — обрезанные
поверхности, тип 144), `Transfer(doc)`, `Write(ostream)`. Слои не пишутся. Вхождения сборки
пишутся копиями тел на своих местах; при чтении они возвращаются отдельными деталями
(шаг 2), общий прототип не восстанавливается.

Ошибки: пустой вход, не IGES, OCCT не разобрал файл, нет передаваемых сущностей —
`ParseError`; превышен предел размера — `InvalidInput`; временный файл не создан —
`IoError`; передача без форм — `OperationFailed`; запись — как у STEP.

Ограничения IGES: нет структуры изделия, атрибутов PRODUCT/NAUO и слоёв при записи; сборка
читается плоским списком деталей; статические параметры OCCT (`Interface_Static`) общие для
процесса — одновременная запись IGES из нескольких потоков не поддерживается (WASM-модули
однопоточны).

## Как тестировать

```bash
cmake -S . -B build-occt -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBENDCORE_WITH_OCCT=ON -DBENDCORE_OCCT_DIR=/home/user/bendcore/third_party/occt-install
cmake --build build-occt --target test_kernel_xcaf test_kernel_xcaf_roundtrip test_kernel_iges
ctest --test-dir build-occt -R '^kernel_(xcaf|iges)' --output-on-failure
```

- `test_kernel_xcaf` — корпус `tests/corpus/generated` (эталоны из `manifest.json`,
  вычисленные генератором по формулам): для всех 13 моделей — число корней, имя верхней
  формы, единица `millimetre`, суммарные объём и площадь, число тел и граней, габарит в
  мировой СК по развёртке; для сборок AP214 и AP242 — прототипы и число их вхождений,
  общий прототип у вхождений, имена вхождений `<ДЕТАЛЬ>_<k>`, цвета, объёмы прототипов по
  формулам, сдвиг и габарит каждого вхождения в мировой СК по построению генератора
  (`make_assembly`); круговой проход «чтение → запись AP214/AP242 → чтение» для всех
  моделей с поэлементным сравнением структуры, цветов, положений, объёмов и габаритов
  деталей и проверкой `FILE_SCHEMA`; мусор, пустой вход, файл без данных, обрезанный файл →
  `ParseError`.
- `test_kernel_xcaf_roundtrip` — структура из памяти: вложенная подсборка, вставленная
  дважды (второй раз с поворотом на 90° вокруг Z), цвета прототипов, вхождений и грани,
  полупрозрачность, слои детали, UTF-8 имена (кириллица); развёртка — габариты пяти
  вхождений против посчитанных вручную и действующие цвета; круговой проход AP214/AP242
  (окрашенная грань находится заново по геометрии); единицы м и дюйм (токен единицы в файле,
  `fileUnitMm`, габариты снова в мм); несколько корней сохраняют порядок; `Unsupported` для
  слоёв сборки/вхождения; все ошибки `InvalidInput` проверки структуры; атрибуты PRODUCT и
  NAUO в AP214 и AP242 (кириллица в `id`, `description`, `reference_designator`; два
  прототипа с одинаковым именем получают свои `id`; `id` по умолчанию = имя; нумерация NAUO
  OCCT; ASCII-атрибуты видны в тексте файла, маркеры — нет); атрибуты файла корпуса
  (`id` = имя, NAUO `1…4`, нет `reference_designator`).
- `test_kernel_iges` — IGES: брус 100×60×20 туда и обратно в режиме B-rep (тип 186 в файле,
  6 граней, объём `abc`, имя, единица мм, без предупреждений); цилиндр в режиме поверхностей
  (нет типа 186; сшит в одно тело с 3 гранями, объём πr²h с допуском 10⁻⁵, предупреждение;
  без сшивки — нет тел); сборка из двух брусков и штифта с цветами — 3 детали, 15 граней,
  суммарный объём, габарит 0…40 × 0…10 × 0…10, два красных и один синий, корень — сборка с
  именами BLOCK, BLOCK, PIN; без разбиения — одна деталь из 3 тел; файл в дюймах
  (`fileUnitMm = 25.4`, объём в мм³); ошибки; закоммиченные IGES корпуса (`manifest.json`,
  раздел `iges`: объём, число тел и граней — по формулам модели-источника).
