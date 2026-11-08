# STEP со структурой сборки, именами, цветами и слоями (`bendcore_kernel_xcaf`)

Библиотека `bendcore_kernel_xcaf` (STATIC), исходник `src/kernel/xcaf_io.cpp`, заголовок
`include/bendcore/kernel/xcaf_io.hpp`, пространство имён `bendcore::kernel`, сборка —
`cmake/kernel_xcaf.cmake` (подключается корневым `CMakeLists.txt` при
`BENDCORE_WITH_OCCT=ON`, после `kernel_core.cmake`; зависит от `bendcore_kernel`).

## Назначение

`readStep` из `step_io.hpp` возвращает только геометрию. Этот модуль читает и пишет STEP
вместе с тем, что OCCT хранит в документе XCAF: дерево сборки, имена, цвета, слои, единицы.
Результат чтения — `AssemblyDocument`, обычная структура значений, не зависящая от OCAF
(документ XCAF живёт только внутри вызова). Работа только с буферами в памяти — файловая
система не используется (нужно для WASM и сервера).

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

## Как тестировать

```bash
cmake -S . -B build-occt -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBENDCORE_WITH_OCCT=ON -DBENDCORE_OCCT_DIR=/home/user/bendcore/third_party/occt-install
cmake --build build-occt --target test_kernel_xcaf test_kernel_xcaf_roundtrip
ctest --test-dir build-occt -R '^kernel_xcaf' --output-on-failure
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
