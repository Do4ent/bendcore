# Документ детали (`bendcore_modeling_document`)

Фаза 1, пп. 1–6 `docs/ROADMAP_GENERAL_CAD.md`. Заголовки
`include/bendcore/modeling/part_document.hpp` (дерево, перестроение, отмена/повтор, файл),
`include/bendcore/modeling/part_sketch.hpp` (данные эскиза, результаты эскиза и
вспомогательной геометрии), `include/bendcore/modeling/part_features.hpp` (типы операций),
исходники `src/modeling/document/`, пространство имён `bendcore::modeling`, сборка —
`cmake/modeling_document.cmake` (подключается при `BENDCORE_WITH_OCCT=ON`, зависит от
`bendcore_kernel`, `bendcore_modeling_sketch`, `bendcore_modeling_features`,
`bendcore_formula`). Топологическое именование — `docs/modeling/TOPOLOGICAL_NAMING.md`,
операции — `docs/modeling/FEATURES.md`, эскиз и профиль — `docs/modeling/SKETCH_PROFILE.md`,
вспомогательная геометрия — `docs/modeling/REFERENCE_GEOMETRY.md`, эталонные детали —
`docs/modeling/REFERENCE_PARTS.md`.

## Назначение

Упорядоченное дерево операций детали. Узел дерева — операция: параметры (выражения), ссылки
на топологию (имена из карты элементов), опции, связи с более ранними узлами (эскиз,
вспомогательная плоскость/ось, исходная операция массива), у эскиза — данные эскиза.
Результат узла — тело (`NamedShape`: форма OCCT + карта элементов) или, у эскиза и
вспомогательной геометрии, их результат (тело проходит дальше без изменений); плюс состояние
и сообщения. Перестроение начинается с первой изменившейся операции; ошибка одной операции
не останавливает дерево. Все правки записываются в историю отмены/повтора.

## Интерфейс операции

```cpp
class Feature {
    virtual std::string type() const = 0;
    virtual std::vector<ParameterSpec> parameterSpecs() const = 0;  // имя + выражение по умолчанию
    virtual std::vector<ReferenceSlot> referenceSlots() const = 0;  // имя, вид, одна/несколько, необязательная
    virtual std::vector<OptionSpec> optionSpecs() const = 0;        // допустимые значения, первое — по умолчанию
    virtual std::vector<LinkSlot> linkSlots() const;                // связи с ранними узлами (по умолчанию нет)
    virtual bool hasSketchData() const;                             // узел несёт SketchData (эскиз)
    virtual Result<FeatureOutput> execute(const FeatureContext&) const = 0;
};
struct FeatureOutput {
    TopoDS_Shape shape;                     // пусто — тело не меняется (эскиз, плоскость, ось)
    handle<BRepTools_History> history;      // входы → shape
    std::vector<ElementRole> roles;         // элементы, названные самой операцией
    std::vector<std::string> warnings;
    std::vector<std::shared_ptr<const NamedShape>> namingInputs;  // доп. именованные входы истории
    TopoDS_Shape tool; std::optional<FeatureMode> toolMode;       // инструмент (для массивов/зеркала)
    std::shared_ptr<const SketchResult> sketch;
    std::shared_ptr<const DatumResult> datum;
};
```

`FeatureContext` даёт входное тело (последнее валидное тело до операции), вычисленные
параметры, разрешённые ссылки по слотам (расщеплённая ссылка — все части), опции, связанные
узлы (`link(slot)` → `LinkedFeature`: тело, инструмент и его именованная копия, результат
эскиза или вспомогательной геометрии) и, у эскиза, `SketchData` с вычисленными выражениями
(`sketchValues()` в порядке `sketchExpressions()`).

Имена элементов результата строятся `buildNamedShape` по истории от входного тела,
дополнительных именованных входов (`namingInputs`) и ролей. Так грани выдавливания получают
имена от сущностей эскиза (раздел «Именование от эскиза»).

## Работа с документом

```cpp
PartDocument doc(partRegistry());
doc.setVariable("W", "40");
doc.setVariable("D", "25");
SketchBuilder b;
b.rectangle("0", "0", "W", "D");                       // угол закреплён, размеры — ограничения
doc.addFeature({"Sketch", "Profile", "", {}, {}, {{"plane", "XY"}}, false, {}, b.build()});
doc.addFeature({"Extrude", "Body", "", {{"distance", "10"}}, {}, {}, false, {{"sketch", "Profile"}}});
doc.rebuild();
NamedShape s = doc.finalShape();
TopoReference edge = makeReference(s, verticalEdgeAtOrigin).value();  // имя "G[Body](Profile:V1_4)"
doc.addFeature({"Fillet", "Round", "", {{"radius", "3"}}, {{"edges", {edge}}}, {}, false});
doc.rebuild();
doc.setVariable("W", "60");          // размер эскиза
RebuildReport rep = doc.rebuild();   // скругление остаётся на том же ребре (метод Exact)
doc.undo();                          // W = 40, прежние результаты из пула — объёмы бит в бит
```

Редактирование: `setVariable`, `removeVariable`, `addFeature` (по умолчанию — в позицию
маркера отката, если он есть, иначе в конец; идентификатор `<Type><NNN>`, если не задан),
`removeFeature`, `moveFeature`, `setSuppressed`, `setParameter`, `setReferences`, `setOption`,
`setLink`, `setSketch`, `setLabel`, `setRollback`. Изменения вступают в силу при `rebuild()`.
Выражения и структура эскиза (номера точек и сущностей) проверяются сразу, значения — при
перестроении.

### Состояния

| `FeatureState` | Смысл | Результат |
|---|---|---|
| `Ok` | Построена; ссылки разрешены точно или по истории | своё тело / эскиз / плоскость |
| `Warning` | Построена с замечанием: ссылка разрешена по сигнатуре, избыточные ограничения эскиза, несколько тел | то же |
| `Error` | Не построена: ошибка выражения, не задана или неверна связь, сломанная ссылка, конфликт ограничений, отказ OCCT, невалидный результат (`BRepCheck_Analyzer`), нет тела с положительным объёмом | нет; следующие получают последнее валидное тело |
| `Suppressed` | Подавлена пользователем | нет; тело проходит дальше; связи на неё — ошибка «is suppressed» |
| `RolledBack` | За маркером отката | нет; не вычисляется |

### Перестроение

Для каждой активной операции вычисляется хэш входов (FNV-1a): хэш цепочки тела (хэш
последней операции, которая видит тело; у эскиза и вспомогательной геометрии без ссылок —
постоянная метка, см. «Инкрементальное перестроение»), идентификатор, тип, подавление, значения параметров (биты `double`), ошибки выражений,
опции, имена и виды ссылок, связи (слот, идентификатор и хэш связанного узла), у эскиза —
структура данных эскиза и значения всех его выражений. Далее:

1. хэш совпал с хэшем текущего результата — результат переиспользуется (`reused`);
2. совпал с записью кэша загруженного файла — результат из кэша (`fromCache`);
3. совпал с одной из последних 16 записей **пула результатов** узла — результат из пула
   (`reused`): так отмена/повтор и возврат прежнего значения переменной не пересчитывают
   операцию, а форма, карта элементов и объём совпадают бит в бит;
4. иначе операция выполняется (`recomputed`).

Связи проверяются при перестроении: узел должен существовать, стоять выше по дереву, быть
нужного типа (`LinkSlot::types`) и построенным. После разрешения ссылок документ обновляет
их сигнатуры (и имя — при неточном разрешении с единственным результатом); хэш
пересчитывается с обновлёнными данными, поэтому следующее перестроение без правок ничего не
пересчитывает.

### Инкрементальное перестроение

Фаза 5 (`docs/qa/PERFORMANCE.md`). Что пересчитывается и что берётся готовым:

- **Цепочка тела.** Операция с `Feature::readsInputBody() == false` (эскиз, `DatumPlane`,
  `DatumAxis`) и без ссылок на топологию тела не видит; её хэш не содержит цепочку тела, и она
  цепочку не продвигает. Изменение первого эскиза пересчитывает выдавливание и вырезы
  (их тело изменилось), а эскизы окружностей, не ссылающиеся на тело, переиспользуются.
  Эскиз со ссылками (плоскость на грани, проекции рёбер) зависит от тела, как прежде.
- **Изменение последней операции** меняет только её хэш: перестраивается она одна (и узлы,
  связанные с ней); вернувшееся значение берётся из пула (без пересчёта).
- **Перестроение без изменений** только сравнивает хэши (300 операций — < 1 мс).
- **Кэш фактов о формах** (`detail::ShapeFacts`, `src/modeling/features/feature_common.cpp`):
  для формы (сравнение `IsEqual`: TShape, положение, ориентация) запоминаются валидность
  `BRepCheck_Analyzer`, объём и признак «готова к булевой операции». Поэтому тело,
  полученное предыдущей операцией, не проверяется заново ни `BRepCheck_Analyzer` (проверки
  входа выдавливания, `applyTool`, документа), ни `BRepGProp` (объём до/после выреза, объём
  результата документа). Ёмкость — 64 последние формы, формы удерживаются кэшем.
- **`BRepAlgoAPI_Check` входа** (малые рёбра, самопересечения) выполняет
  `detail::booleanWithHistory`, а не ядро (`BooleanOptions::checkInputs = false`); результат
  `kernel::booleanOp` с нулевым нечётким допуском, прошедший `BRepCheck_Analyzer`, помечается
  `booleanReady` и как вход следующей булевой операции не проверяется: результат булевой
  операции над проверенными входами самопересечений не содержит. Инструменты (новые призмы)
  проверяются всегда; сообщения об ошибке — прежние (`argument #0 failed BRepAlgoAPI_Check: …`).
- **Сигнатуры элементов** (`buildNamedShape`): элемент, перешедший из входа без изменений
  (`IsEqual`), получает сигнатуру входа; `BRepGProp` считается только для новых и изменённых
  элементов.

- **Известные массы входов**: объём и площадь, вычисленные ядром для результата булевой
  операции (`BooleanResult::mass`), хранятся в кэше фактов и передаются следующей булевой
  операции как `argumentMass`/`toolMass` (`FEATURES.md`, раздел 1; `stress_part_300`
  13,6 → 11,7 с).

Не ускорено (в зоне ядра; масса аргумента теперь передаётся, см. выше): `kernel::booleanOp` каждый раз считает массу и площадь аргументов,
проверяет результат `BRepCheck_Analyzer` и объёмом дважды (до и после `SimplifyResult`) — после
изменений это ≈ 60 % времени выреза. Локальная булева операция для детали из одного тела не
применима: вырез меняет это тело.

### Выражения

Переменные документа, параметры операций и числа эскиза — выражения `bendcore::formula`
(`src/formula.cpp`, проверяется `test_formula_comprehensive`): `+ - * / ^`, скобки, `sqrt`,
`abs`, `sin`, `cos`, `tan`, `min`, `max`, `floor`, `ceil`, `round`; имена переменных — с `$`
или без. Переменные вычисляются в порядке зависимостей; цикл помечает все переменные цикла
ошибкой «cyclic dependency A -> B -> A», неизвестное имя — «unknown variable 'Q'».
Единицы: длины — мм, **углы параметров операций и ограничений эскиза — градусы**.

Прежние `src/parameters` (`ParameterStore`) и `src/rebuild` не использовались: `ParameterStore`
хранил формулу, но не вычислял её и для отсутствующего имени молча возвращал `0.0`; оба
модуля удалены (`docs/STUB_REMOVAL.md`, раздел 7). Граф
зависимостей детали — цепочка хэшей тела плюс хэши связанных узлов.

## Свойства документа

`DocumentProperties` (`part_document.hpp`): обозначение (ГОСТ 2.201), наименование, материал
(id материала библиотеки ядра `kernel::builtInMaterials()` или «custom» — плотность вручную),
плотность (кг/м³), разработчик, организация, примечание. Это то, что видят сборка, спецификация,
STEP и основная надпись чертежа.

| Функция | Назначение |
|---|---|
| `properties()`, `setProperties(p)` | чтение и замена; правка — запись отмены «properties», перестроение не нужно; те же значения — не правка. `InvalidInput`: плотность не положительное конечное число, перевод строки в любом поле, кроме примечания |
| `configurationDesignation(row)` | полное обозначение исполнения: суффикс («-01») дополняет **обозначение документа**, если оно задано, иначе — обозначение первой строки таблицы (прежнее правило ГОСТ 2.113); полное обозначение строки — как записано |
| `effectiveProperties()` | `ConfigurationProperties{designation, name, material, density}` в активном исполнении: его полное обозначение, наименование строки (пустое — наименование документа), ячейка материала (пустая — материал документа); плотность — библиотечного материала, если своей нет |

Куда передаются:

- **Сборка** (`assembly_v2::partDataFromDocument`): `PartData::properties` = действующие свойства
  (материал — наименование материала библиотеки, «custom» — без наименования, только плотность),
  примечание. `AssemblyDocument::addPart` дополняет ими пустые поля; определение без имени получает
  наименование (затем обозначение), вхождения — «<наименование>:<n>»; на странице сборки
  компонент называется наименованием детали (`docs/assembly/ASSEMBLY_DOCUMENT.md`, раздел 8).
- **Спецификация ГОСТ 2.106**: графы «Обозначение», «Наименование» — из свойств определения,
  «Примечание» — примечание детали, если в спецификации не задано своё (`SPECIFICATION.md`).
- **STEP** (`docs/kernel/XCAF_IO.md`): PRODUCT `id` — обозначение, `name` и `description` —
  наименование (`assembly_v2::partStepDocument`, экспорт страницы детали `exportFile('step')`).
- **Основная надпись чертежа детали** (`docs/drawing/DRAWING_DOCUMENT.md`, «Свойства детали в
  основной надписи»): графы 1, 2, 3, масса, 9 (организация), 11 (разработчик) — при создании и
  при каждом обновлении по модели. Ручная правка граф листа имеет приоритет.

Файл: необязательные поля `"properties"` (непустые `designation`, `name`, `developer`,
`organization`, `note`) и `"material": {"id", "density"}` — тот же вид, что страница детали
пишет с v3. Версия документа не меняется (v3–v5 по содержимому): прежние версии программы такие
поля игнорируют. Файл без полей — пустые свойства; повреждённое поле `material` игнорируется (как
на странице), поле свойств неверного типа — `ParseError` «properties: 'designation' must be a
string». Тест — `test_modeling_document_properties`.

## Таблица параметров (исполнения)

Аналог Design Table SolidWorks и «Исполнений» КОМПАС. Заголовок
`include/bendcore/modeling/configuration_table.hpp`, исходники
`src/modeling/document/configuration_table.cpp` (заголовки столбцов, обозначения, CSV) и
`src/modeling/document/configurations.cpp` (проверка, подстановка, проверка исполнений).

**Модель.** `ConfigurationTable{columns, rows, active}`:

| Элемент | Содержимое |
|---|---|
| Столбец `ConfigurationColumn{kind, target}` | `Variable` — переменная документа `target`; `Suppression` — флаг подавления операции `target`; `Material` — id материала библиотеки ядра (`kernel::builtInMaterials()`, без цели) |
| Строка `Configuration{designation, name, cells}` | обозначение (полное «РЦ.080.00.010» или суффикс «-01»), наименование, по ячейке на столбец |
| Ячейка | выражение переменной (как `setVariable`); `1/0`, `true/false`, `yes/no`, `да/нет` у подавления; id материала; **пусто — базовое значение документа** |
| `active` | обозначение активного исполнения; пусто — базовые значения |

`fullDesignation(table, row)` — полное обозначение: суффикс строки, начинающийся с «-»,
дописывается к обозначению первой строки («РЦ.080.00.010» + «-01» → «РЦ.080.00.010-01»,
ГОСТ 2.113). Первая строка — базовое исполнение. Если у документа задано обозначение (раздел
«Свойства документа»), суффикс дополняет его: `PartDocument::configurationDesignation(row)` —
так считают проверка исполнений, сборка, спецификация и групповой чертёж.

**Подстановка.** Активное исполнение не переписывает переменные: при `rebuild()` непустые ячейки
его строки заменяют выражения переменных (`effectiveVariables`) и флаги подавления узлов
(`effectiveSuppressed`); базовые значения остаются в документе и в файле. Поэтому
переключение туда и обратно возвращает те же входы, и результаты берутся из пула узлов
(`recomputed` пуст, объём совпадает бит в бит), а ссылки на топологию (скругление на ребре)
разрешаются по тем же именам — `Exact`. `variables()` и `feature().suppressed` показывают
действующие значения. `setVariable`/`setSuppressed` значения, которое задаёт непустая ячейка
активного исполнения, пишут **эту ячейку** (правка «в этом исполнении»), иначе — базовое
значение. `configurationMaterial()` — id материала активного исполнения (пусто — нет ячейки).

**Правки** (каждая — запись отмены, состояние для отмены включает таблицу):
`setConfigurationTable` (вся таблица; так добавляются и удаляются строки),
`setConfigurationCell(row, column, value)`, `addConfigurationColumn(column)` (ячейки всех строк
получают текущее базовое значение: выражение переменной, `1`/`0` подавления, пустой материал),
`activateConfiguration(designation)` (пусто — базовые значения). Удаление переменной или
операции удаляет и её столбец (в той же записи отмены).

**Ошибки — явные** (`InvalidInput`, таблица не меняется): `column 1: unknown variable 'Q'`,
`column 4: unknown feature 'Nope'`, `configuration '-01', column 'W': invalid value '8O+': …`
(выражение не разбирается), `… invalid value 'maybe' (expected 1 or 0)`, `… unknown material
'unobtainium'`, пустое или повторённое обозначение, повторённый столбец, число ячеек не равно
числу столбцов, `no configuration '-07'`.

**Исполнение не перестраивается.** `checkConfigurations()` строит каждое исполнение по очереди
и возвращает `ConfigurationCheck{designation, fullDesignation, ok, problems, volume}`; причины —
ошибки переменных (цикл «cyclic dependency W -> W» из ячейки `W + 1`), узлы в состоянии `Error`
с их сообщениями («feature 'Body' is not built: …»), отсутствие тела. Затем восстанавливает
активное исполнение и перестраивает; история отмены не меняется.

**CSV** (`configurationTableToCsv`, `configurationTableFromCsv`; без внешних библиотек).
Разделитель «;», кавычки по RFC 4180 (ячейка с «;», кавычкой, переводом строки или пробелами по
краям — в кавычках, кавычка удваивается), строки CRLF, UTF-8 с BOM (`CsvOptions::byteOrderMark`,
Excel читает кириллицу). Заголовок: `designation;name;<столбцы>`, переменная — её имя (при чтении
допускается `$W`), подавление — `suppress:<id>`, материал — `material`; при чтении также
«Обозначение», «Наименование», «подавить:<id>», «Материал». Числа ячеек переменных пишутся с
десятичным разделителем `CsvOptions::decimal` (`.` или `,`); при чтении число вида `12,5`
становится `12.5` (выражения пишутся как есть: запятая внутри `max(3, W)` — разделитель
аргументов). Пустые строки пропускаются, BOM снимается. Ошибки чтения — `ParseError` с номером
строки: незакрытая кавычка, текст после закрывающей кавычки, первые столбцы не
обозначение/наименование, пустой заголовок, повтор столбца, число ячеек не как в заголовке.
Проверка по документу — при `setConfigurationTable`. Туда и обратно таблица не теряет ничего
(`CsvRoundTripWithoutLoss`, обе формы разделителя).

**Файл.** Таблица пишется полем `"configurations"`, документ с таблицей — версии 4; документ
без таблицы по-прежнему пишется как v3 (его прочтут прежние версии), файлы v1–v3 без таблицы
читаются как раньше:

```json
"configurations": {"columns": [{"kind": "variable", "target": "W"}, {"kind": "suppression", "target": "Hole"},
                               {"kind": "material", "target": ""}],
                   "rows": [{"designation": "РЦ.080.00.010", "name": "Брусок", "cells": ["60", "0", "steel_s235"]},
                            {"designation": "-01", "name": "Брусок большой", "cells": ["80", "0", "al_6061_t6"]}],
                   "active": "-01"}
```

Таблица из файла проверяется так же, как `setConfigurationTable`; ошибка — `ParseError`
«configurations: column 1: unknown variable 'Q'», неизвестный вид столбца — `ParseError`.
В кэше (`includeCache`) — результаты активного исполнения (хэш входов учитывает подстановку).

Исполнения в сборке и спецификации — `docs/assembly/ASSEMBLY_DOCUMENT.md` (раздел 8.1),
`docs/drawing/SPECIFICATION.md` («Исполнения деталей»).

## Эскиз (`Sketch`)

Данные (`SketchData`, `part_sketch.hpp`):

- точки `SketchPointDef{x, y, fixed}` — выражения; закреплённая точка стоит в этой позиции
  (её координаты — размеры), свободная — только начальное приближение решателя;
- кривые `SketchGeometryDef`: `Line(start, end)`, `Circle(center, radius)`,
  `Arc(center, start, end, counterClockwise)` (нативная дуга PlaneGCS),
  `Ellipse(center, focus, radius = малая полуось)`, `EllipticalArc(center, focus, radius,
  start, end, counterClockwise)`, `BSpline(poles, degree, knots, multiplicities, weights,
  periodic)`, флаг вспомогательной (`construction`); поля `focus` и сплайна пишутся в JSON
  только у этих видов, поэтому файлы с отрезками, окружностями и дугами не меняются;
  `projection >= 0` — проекция ребра модели номер `projection` из слота ссылок `projected`;
  идентификатор сущности = номер в списке + 1;
- ограничения `SketchConstraintDef{type, a, b, c, value, driven}`: Coincident, Horizontal,
  Vertical, Distance (мм), Radius, Parallel, Perpendicular, EqualLength, Angle (градусы),
  PointOnLine, PointOnCircle, EqualRadius, Symmetric, Tangent, Equal, Concentric, Midpoint,
  PointOn, Fix, DistanceX, DistanceY, PointCurveDistance, Diameter, ArcLength, MajorRadius,
  MinorRadius, SymmetricPoint, Rotation (смысл — `SKETCH.md`); операнды — номера точек (с 0)
  или сущностей (с 1); `driven = true` — справочный размер (в JSON `"driven": true` только у
  таких);
- выбор областей `regions`: пусто — «материальные» области (внешние контуры и острова),
  иначе каждая сущность выбирает область, внешний контур которой её содержит (при двух —
  меньшую по площади).

`SketchBuilder` собирает данные в коде: `rectangle(x, y, w, h)` (закреплённый угол,
горизонтальность/вертикальность и два размера Distance), `fixedCircle`, `fixedPolygon`,
`projected`, `constrain`.

Перестроение эскиза:

0. **Вид контура** (опция `contour`): `closed` (по умолчанию) — кривые образуют области
   (профиль для выдавливания, вращения, …); `open` — кривые образуют одну цепочку, открытую
   или замкнутую (траектория кинематики, контур ребра жёсткости): `buildSketchPath`, областей
   нет; ветвление, пересечения и несколько цепочек — ошибка узла.
1. **Размещение** (опция `plane`): `XY` (нормаль +Z, X = +X), `XZ` (нормаль −Y, X = +X,
   Y = +Z), `YZ` (нормаль +X, X = +Y) со сдвигом `offset` по нормали; `face` — плоская грань
   (ссылка `face`, через карту элементов), нормаль — наружная, начало — проекция точки
   (`originX/Y/Z`), X — глобальная X, спроецированная на грань (глобальная Y, если X почти
   нормальна грани) — кадр не зависит от параметризации грани OCCT; `datum` — плоскость узла
   `DatumPlane` (связь `plane`).
2. **Проекции**: ссылки слота `projected` разрешаются по карте элементов входного тела,
   ребро проецируется (`projectEdge`), точки проецированной кривой закрепляются (прямая
   ориентируется от меньшего x, затем y), окружность получает размер радиуса.
3. **Решение** — `BoundSketch` (PlaneGCS, `src/sketch2d.cpp`), после решения каждое
   ограничение проверяется по невязке `Sketch::residual` (≤ 1e-6 × масштаб эскиза):
   решатель, не сошедшийся без сообщения, даёт ошибку с номером ограничения. Неверные
   операнды (касание двух отрезков, справочное не-размерное ограничение, радиус отрезка) —
   ошибка «sketch constraint N (тип): причина». Конфликт — ошибка со списком ограничений;
   избыточность — `Warning`. `SketchResult` содержит `dof`, `definition`
   (`fully-defined | under-defined | over-defined`), `redundant`, `partiallyRedundant`
   (номера ограничений с 1), `pointDefined` и `entityDefined` (определённость каждой точки и
   сущности для цветовой индикации) и `constraintValues` (значение каждого размера; у
   справочного — измеренное).
4. **Профиль** — `SketchProfile::build` (контуры, вложенность, области), выбор областей,
   грани областей с именами (ниже). Эскиз без профильных кривых допустим (точки для
   отверстий, оси); выдавливание такого эскиза — ошибка «has no closed profile».

### Именование от эскиза

Грани выбранных областей образуют `NamedShape` с именами от сущностей эскиза:
`<эскиз>:R<id>` (область; id — меньшая сущность её внешнего контура), `<эскиз>:E<id>`
(ребро сущности), `<эскиз>:V<id>_<id>` (вершина, общая для сущностей; совпадающие имена
различаются `;D<i>` по положению). Выдавливание и вращение передают эти грани в историю
(`namingInputs`), поэтому:

| Элемент выдавливания `Body` эскиза `Profile` | Имя |
|---|---|
| боковая грань от отрезка 3 | `G[Body](Profile:E3)` |
| верхний торец | `G[Body](Profile:R1)` |
| нижний торец (в плоскости эскиза) | `Profile:R1` |
| боковое ребро от вершины отрезков 1 и 4 | `G[Body](Profile:V1_4)` |
| кромка кармана в плоскости эскиза | `Profile:E3` |

Пересекающиеся кривые (`SKETCH_PROFILE.md`, «Пересекающиеся кривые») разрезаются на части;
у сущности из нескольких частей ребро — `E<id>.<часть>`, вершина — `V` и подписи частей,
сходящихся в ней (`V1.0_2.1`), область — `R` и подписи частей её контура через «_»
(`R1.0_2.0`), а области выбора, слитые в одну грань, — их имена через «+»
(`R1.0_2.0+R1.1_2.0+R1.1_2.1` — объединение двух окружностей). Сущности без разрезов
сохраняют прежние имена. Тест `ModelingDocumentSketch.CrossingCirclesUnionAndRebuild`:
выдавливание объединения двух окружностей, бобышка на верхнем торце и скругление ребра из
точки пересечения переживают изменение радиуса 10 → 11 → 9 → 10 (ссылки — `Exact`).

Имена не содержат размеров и переживают любые изменения размеров эскиза, пока не меняется
состав сущностей и их точек пересечения (проверено тестами `sketch`, `survival`,
`reference_parts`).

## Операции (`part_features.hpp`)

Каждая операция — обёртка над функцией `src/modeling/features/` или модулей эскиза и
вспомогательной геометрии; история — та, что возвращает функция (`FEATURES.md`).

| Тип | Параметры | Ссылки / связи / опции | Функция |
|---|---|---|---|
| `Sketch` | `offset, originX/Y/Z` | `face`, `projected` (необяз.); связь `plane`; `plane = XY\|XZ\|YZ\|face\|datum` | `SketchPlacement`, `BoundSketch`, `SketchProfile`, `projectEdge` |
| `DatumPlane` | `offset, angle` | `face, face2, edge, vertex1..3`; связи `plane, axis, point1..3`; `method = offset\|angle\|midway\|threePoints\|tangent`, `base`, `axis`, `tangent = line\|direction\|angle`, `flip` | `planeOffset`, `planeAtAngle`, `planeMidway`, `planeOfFace`, `planeThroughPoints`, `planeTangentToCylinder*` |
| `DatumAxis` | — | `face, edge`; связи `plane, plane2`; `method = X\|Y\|Z\|face\|edge\|planes` | `axisOfFace`, `axisOfEdge`, `axisOfPlaneIntersection` |
| `DatumPoint` | `x, y, z` | `vertex, edge, face`; связи `plane, csys`; `method = coordinates\|vertex\|midpoint\|center\|edgePlane`, `base` | `pointOnEdge(0.5)`, `pointCenterOfEdge`, `pointEdgePlaneIntersection` |
| `DatumCsys` | `x, y, z` | `vertex, edge, edge2, face`; связи `point, axis, axis2`; `method = directions\|faceEdge`, `origin = vertex\|datum\|coordinates`, `xDirection`, `xyDirection` | `coordinateSystemFromDirections`, `coordinateSystemFromFaceEdge` |
| `Extrude` | `distance, distance2, draftAngle, thickness` | связь `sketch`; `limit` (грань); `mode = boss\|cut\|newBody`, `extent` (все 7 условий), `reversed`, `draftDirection`, `thin = none\|inside\|outside` | `extrude` по каждой области, истории объединяются |
| `Revolve` | `angle, angle2, axisEntity` | связи `sketch, axis`; `axisEdge`; `mode`, `extent = full\|angle\|symmetric\|twoSided`, `axis = entity\|X\|Y\|Z\|edge\|datum`, `reversed` | `revolve` |
| `Hole` | `diameter, depth, cbDiameter, cbDepth, csDiameter, csAngle, drillAngle, x, y, z` | `face`; связь `sketch` (центры — точки эскиза, не занятые кривыми); `type = simple\|counterbore\|countersink`, `end`, `drillPoint` | `hole` по каждой точке, грань размещения отслеживается по истории |
| `Fillet` | `radius` | `edges` | `fillet` |
| `Chamfer` | `distance, distance2, angle` | `edges`, `face`; `type = equal\|twoDistances\|distanceAngle` | `chamfer` |
| `LinearPattern` | `count1, spacing1, count2, spacing2` | связь `feature` (+ `axis1/2`); `edge1/2`; `direction1/2 = X\|Y\|Z\|edge\|datum`, `reversed1/2` | `linearPattern` от инструмента исходной операции |
| `CircularPattern` | `count, angle` | связи `feature, axis`; `axisFace, axisEdge`; `axis`, `spacing = equal\|step` | `circularPattern` |
| `Mirror` | — | связи `feature, plane`; `face`; `what = feature\|body`, `plane`, `merge` | `mirrorFeature` / `mirrorBody` |
| `Shell` | `thickness` | `faces` (удаляемые, может быть пусто); `direction` | `shell` |
| `Draft` | `angle` | `faces`, `neutralFace`; связь `plane`; `neutral = face\|datum\|XY\|XZ\|YZ`, `reversed` | `draftFaces`; направление извлечения — от нейтральной плоскости внутрь тела (для грани — против её наружной нормали) |
| `Sweep` | `thickness` | связи `sketch` (профиль), `pathSketch`, `guideSketch` (эскизы `contour = open`); `pathEdges`, `guideEdges` (рёбра модели); `mode = boss\|cut\|newBody`, `path = sketch\|edges`, `orientation = correctedFrenet\|frenet\|fixed\|binormal\|guide`, `binormal = Z\|X\|Y`, `guide`, `thin` | `sweep` по каждой области профиля |
| `Loft` | — | связи `section1` … `section8` (эскизы, по одной области; 1 и 2 обязательны, без пропусков); `mode`, `surface = smooth\|ruled`, `closed` | `loft` |
| `Rib` | `thickness` | связь `sketch` (`contour = open`, открытая цепочка); `direction = parallel\|perpendicular`, `side = symmetric\|first\|second`, `flip = auto\|false\|true` | `rib`; роли `SideA`, `SideB`, `Top`, `Wall.E<id сущности>.A\|B\|End` |
| `ThreadedHole` | `nominal, pitch` (0 — крупный), `threadLength, drillDepth` (0 — длина + 3P), `drillDiameter` (0 — D − P), `x, y, z` | `face`; связь `sketch` (точки — центры); `end = blind\|through`, `hand = right\|left`, `drillPoint` | `threadedHole`; роли `Wall` (с атрибутами резьбы), `Tip`, `Bottom` (`H<k>.` при нескольких) |
| `GearTeeth` | `module, teeth` (целое), `shift`, `width` (0 — длина грани), `helix` (°), `offset`, `angle` (°, поворот зуба № 1) | `face` (цилиндр заготовки), `axisEdge`; связь `axis`; `mode = cut\|boss\|newBody`, `axis = face\|X\|Y\|Z\|edge\|datum`, `hand = right\|left` | `gearTeeth`; роли `T<k>.Root`, `T<k>.FilletR/L`, `T<k>.FlankR/L` (`T<k>.Tip`, `Start`, `End` — у boss/newBody) с атрибутами `gear.*`; `gearsOf` (`docs/modeling/FEATURES.md`, раздел 13) |
| `BodyBoolean` | — | `target` (грань тела), `tools` (грани тел); `operation = union\|subtract\|intersect`, `keepTools` | `bodyBoolean` |
| `DeleteBody` | — | `bodies` (грани тел) | `deleteBodies` |
| `BodyLinearPattern` | `count1, spacing1, count2, spacing2` | `bodies`, `edge1/2`; связи `axis1/2`; `direction1/2`, `reversed1/2`, `merge` | `copyBodies` |
| `BodyCircularPattern` | `count, angle` | `bodies`, `axisFace`, `axisEdge`; связь `axis`; `axis`, `spacing`, `merge` | `copyBodies` |
| `BodyMirror` | — | `bodies`, `face`; связь `plane`; `plane`, `merge` | `copyBodies` |
| `Box`, `Cylinder` | `length, width, height, x, y, z` / `radius, height, x, y, z` | — | примитив: профиль (прямоугольник/круг) → `extrude` без цели → `applyTool` (Fuse с входом) |

Имена, которые операции дают сами (`roles`): грани `Box` — `Top, Bottom, Front (YMin),
Back (YMax), Left (XMin), Right (XMax)`, `Cylinder` — `Top, Bottom, Side`; грани отверстия —
`Wall, CounterboreWall, CounterboreFloor, Countersink, Tip, Bottom` (у нескольких отверстий —
`H<k>.Wall` и т. д.). Инструмент операции (`tool`) хранится в узле с именами
`<id>:Tool.F<k>` (грани в каноническом геометрическом порядке: тип, направление, радиус,
центр); массив и зеркало передают его в историю, поэтому грани копий называются
`G[<массив>](<id>:Tool.F<k>);G<i>`.

`basic_features.hpp` оставлен для совместимости: `basic::basicRegistry()` возвращает
`partRegistry()` (Box, Cylinder, Fillet, Chamfer с прежними параметрами и именами).

## Вспомогательная геометрия

Узлы `DatumPlane`, `DatumAxis`, `DatumPoint`, `DatumCsys` (`sketch_features.cpp`) тело не
меняют (`readsInputBody() == false`) и дают `DatumResult` (`part_sketch.hpp`): `plane`, `axis`,
`point`, у ЛСК — `csys` и вместе с ним её плоскость XY (`plane`), ось Z (`axis`) и начало
(`point`). Поэтому ЛСК подходит любой связи, принимающей её тип: плоскость эскиза (`Sketch`,
связь `plane`, `plane = datum` — эскиз в плоскости XY с осями ЛСК), плоскость зеркала (`Mirror`,
`BodyMirror`), нейтраль уклона (`Draft`), база плоскости и точки — связи с типами
`{DatumPlane, DatumCsys}`; точки плоскости по трём точкам и начало ЛСК — `{DatumPoint,
DatumCsys}`. Оси массивов и вращения принимают только `DatumAxis`. Ссылки на вершины, рёбра,
грани — имена карты элементов, как у других операций; узел со ссылками зависит от тела и
пересчитывается при его изменении (раздел «Инкрементальное перестроение»).

- **`DatumPoint`**: `coordinates` — `(x, y, z)`; со связью `csys` — координаты в ЛСК
  (`gp_Trsf::SetTransformation`, обратное преобразование); `vertex`; `midpoint` — точка на
  половине длины ребра; `center` — центр окружности или дуги; `edgePlane` — пересечение ребра с
  плоскостью `base` (`XY|XZ|YZ|face|datum`, грань — слот `face`, вспомогательная — связь
  `plane`; прямое ребро — по его прямой).
- **`DatumPlane`**: `threePoints` — точка `i` из связи `point<i>`, если она задана, иначе из
  вершины `vertex<i>`; `tangent` — цилиндрическая грань `face`; `tangent = line` — линия из
  опции `axis` (`edge` — слот `edge`, `datum` — связь `axis`, X/Y/Z), `direction` — направление
  оси из той же опции, `angle` — база `base` (грань — слот `face2`, вспомогательная — связь
  `plane`) и параметр `angle` (°); `flip = true` — второе решение (для `angle` — угол с
  обратным знаком).
- **`DatumCsys`**: начало `origin` — вершина `vertex`, связь `point` или `(x, y, z)`;
  `directions` — X из `xDirection` (`edge` — слот `edge`, `datum` — связь `axis`, X/Y/Z),
  второе направление из `xyDirection` (слот `edge2`, связь `axis2`); `faceEdge` — плоская грань
  `face` и прямое ребро `edge`, начало проецируется на грань.

Сериализация — общая: опции, параметры, ссылки и связи узла пишутся в `features`; результат
пересчитывается при загрузке (в кэш не пишется). Старые файлы с `DatumPlane` загружаются:
недостающие опции получают значения по умолчанию.

## Многотельная деталь

Тело документа — по-прежнему один `NamedShape`; у многотельной детали это составное из тел
(`TopoDS_Solid`). Каждое тело имеет имя (`NamedShape::bodyNames`, `TOPOLOGICAL_NAMING.md`):
тело, созданное операцией, называется её идентификатором (`Peg`; несколько новых —
`<id>;B<i>` в геометрическом порядке), дальше имя наследуется по истории (то же тело, его
`Modified`-образ или образы его граней), при слиянии остаётся меньшее из имён, части
разрезанного тела — `<имя>;S<i>`. `PartDocument::bodies()` — список тел итоговой формы
(имя, объём, число граней).

* Новое тело — `mode = newBody` у `Extrude`, `Revolve`, `Sweep`, `Loft` (`FeatureMode::NewBody`).
* Операции Boss/Cut на многотельной детали действуют только на задетые тела (`FEATURES.md`,
  раздел 1); отверстия, скругления и прочие — как раньше, по ссылкам на элементы.
* Узлы тел (`BodyBoolean`, `DeleteBody`, `BodyLinearPattern`, `BodyCircularPattern`,
  `BodyMirror`) получают тела ссылками на **любую грань тела** — обычные ссылки карты
  элементов, поэтому разрешаются по имени, истории и сигнатуре и переживают изменения
  размеров. `makeBodyReference(shape, "Peg")` строит такую ссылку (грань тела с наименьшим
  именем). Тело с одной гранью у узла тел, которому нужно ≥ 2 тела, — ошибка узла.
* Сохранение: имена тел — поле `bodies` записи кэша; запись без него пересчитывается.

## Условная резьба

`ThreadedHole` кладёт атрибуты `thread.*` (`FEATURES.md`, раздел 11) в запись стенки
отверстия; атрибуты переходят по истории к изменённым и расщеплённым образам грани и
сохраняются в кэше (`attributes` записи элемента). `threadsOf(NamedShape)` — грани с резьбой:
индекс, имя, тело, `ThreadSpec` — вход для изображения резьбы на чертеже (ГОСТ 2.311).

## Отмена и повтор

Механизм `src/document` (`DocumentTransaction`, `UndoRedoManager`) работает со снимками
`CadDocument` (другая модель сущностей; `test_cad_document` в списке непроверенных целей) и к
документу детали не применим, поэтому история встроена в `PartDocument`
(`src/modeling/document/transactions.cpp`):

- запись — полное **определение** документа (переменные, маркер отката, определения узлов:
  параметры, ссылки, опции, связи, данные эскиза, подавление) до правки; каждая успешная
  правка — отдельная запись, правки между `beginTransaction(name)` и `commitTransaction()` —
  одна; `abortTransaction()` восстанавливает состояние начала без перестроения;
- `undo()` / `redo()` восстанавливают определение и вызывают `rebuild()`; узлы с теми же
  идентификаторами сохраняют свои пулы результатов, поэтому восстановленное состояние берётся
  из пула (`recomputed` пуст, объём совпадает бит в бит);
- `undoHistory()`, `redoHistory()` (имя и число правок), `canUndo/canRedo`, `clearHistory()`;
  до 200 записей; загрузка файла начинает с пустой истории; новая правка очищает повтор.

## Формат файла

JSON, `"format": "bendcore-part"`, `"version": 3`; с таблицей параметров — `"version": 4`
(поле `"configurations"` — раздел «Таблица параметров»); со справочными сетками — `"version": 5`
(`PartDocument::kFormatVersion`, поле `"referenceMeshes"` — раздел «Справочная сетка»). Документ
пишется наименьшей версией, вмещающей его содержимое, поэтому прежние версии программы отказывают
файлу с сетками («newer than the supported»), а не теряют сетку молча:

```json
{"format": "bendcore-part", "version": 3,
 "variables": [{"name": "W", "expression": "40"}], "rollback": null,
 "features": [
  {"id": "Profile", "type": "Sketch", "label": "Profile", "suppressed": false,
   "parameters": {"offset": "0", "originX": "0", ...}, "options": {"plane": "XY"},
   "references": {"face": [], "projected": []}, "links": {},
   "sketch": {"points": [{"x": "0", "y": "0", "fixed": true}, ...],
              "geometry": [{"type": "line", "start": 0, "end": 1, "center": -1, "radius": "1",
                            "counterClockwise": true, "construction": false, "projection": -1}, ...],
              "constraints": [{"type": "distance", "a": 0, "b": 1, "c": -1, "value": "W"}, ...],
              "regions": []}},
  {"id": "Body", "type": "Extrude", ..., "links": {"sketch": "Profile"}},
  {"id": "Round", "type": "Fillet", ..., "references": {"edges": [{"name": "G[Body](Profile:V1_4)",
     "kind": "edge", "signature": {...}}]}}],
 "cache": {"Body": {"hash": "…", "state": "ok", "brep": "<base64>", "tool": "<base64>",
           "toolMode": "Boss", "elements": {...}, "deletions": {...}, "bodies": ["Body"], ...}}}
```

- Режим инструмента в кэше — `Boss`/`Cut`/`NewBody` (`featureModeFromString`; раньше чтение
  сравнивало с `"cut"`, и закэшированный вырез читался как бобышка — массив из кэша
  объединял бы копии вместо вычитания). Записи элементов могут иметь `attributes` (пары
  ключ–значение), запись кэша — `bodies` (имена тел).
- Результаты при загрузке пересчитываются (`fromJson` вызывает `rebuild()`); кэш
  (`SaveOptions::includeCache`) хранит BRep тела и инструмента, карту элементов, журнал
  удалений, состояние, сообщения, отчёты ссылок узлов с телом. Эскизы и вспомогательная
  геометрия не кэшируются (пересчитываются быстро, их хэш тот же). Запись кэша
  используется, только если совпал хэш входов; повреждённая — игнорируется.
- **Миграция v1 → v2 → v3.** v1: переменные — числа в `"parameters"`, операции — в
  `"operations"` с `"params"` и `"refs"` (только имена). v2 → v3: тип `PrismCut` (вырез
  прямоугольником/кругом на грани) заменяется эскизом `<id>Sketch` на той же грани (начало —
  `centerX/Y/Z`, прямоугольник `sizeU × sizeV` или круг `radius` закреплёнными точками) и
  выдавливанием-вырезом `<id>` (`blind` на `depth` или `throughAll`, внутрь материала);
  имена ролей старого выреза в ссылках переписываются на имена новых граней
  (`<id>:Side+V` → `G[<id>](<id>Sketch:E3)`, `Start` → `<id>Sketch:R1`,
  `Floor` → `G[<id>](<id>Sketch:R1)`, производные имена пересортировываются); ссылка, имя
  которой всё же не совпало (кромка кармана теперь называется `<id>Sketch:E3`), при первом
  перестроении разрешается по сохранённой сигнатуре (`Warning`) и далее идёт по новому
  имени. Маркер отката сдвигается на число вставленных эскизов, кэш v2 отбрасывается.
  Сохранение — v3, а документа с таблицей параметров — v4.
- Ошибки: не JSON, чужой формат, нет обязательных полей, неверный вид элемента, неизвестный
  вид кривой/ограничения → `ParseError`; версия новее поддерживаемой, неизвестный тип
  операции → `Unsupported`.

## Справочная сетка

Файл STL/3MF, встроенный в документ детали для справки: скан, чужая модель, оснастка
(`docs/kernel/ANALYSIS.md`, раздел 7). Сетка хранится **вне дерева операций**: она не тело, не
входит в перестроение, хэши, массу, булевы операции и экспорт тела; узлы дерева на неё не
ссылаются. Исходник — `src/modeling/document/reference_meshes.cpp`.

```cpp
ReferenceMeshSpec spec;            // id (пусто — Mesh001…), label, fileName, format, data (байты файла),
spec.fileName = "scan.stl";        // unit (mm|cm|m|in), rotationDegrees (X, затем Y, затем Z),
spec.data = bytes;                 // translation (мм), visible, opacity [0.05, 1]
std::string id = doc.addReferenceMesh(spec).value();              // файл читается и проверяется сразу
doc.setReferenceMeshPlacement(id, "in", {0, 0, 90}, {100, 0, 0}); // единицы, поворот, смещение
doc.setReferenceMeshDisplay(id, true, 0.4);                       // видимость, прозрачность
ReferenceMeshInfo info = doc.referenceMesh(id).value();           // отчёт чтения, свойства размещённой сетки
auto mesh = doc.placedReferenceMesh(id).value();                  // треугольники в СК детали, мм
std::string sketch = doc.addMeshSectionSketch(id, "XY", 5.0).value();
```

- **Ошибки** вставки — ошибки читателя с префиксом `mesh '<файл>': ` (код сохраняется:
  `ParseError` для битого файла, `InvalidInput` для превышения пределов и пустой сетки),
  неизвестные единицы, нечисловое положение, прозрачность вне [0.05, 1], занятый идентификатор
  (идентификаторы сеток не совпадают с идентификаторами операций). При ошибке документ не меняется
  и история не пополняется.
- **Отмена и повтор.** Вставка, удаление, размещение, видимость, имя — правки истории
  (`State::meshes`); разобранная сетка и байты файла общие у снимков (`shared_ptr`), копируются
  только определения. Размещённая сетка пересчитывается при смене размещения.
- **Сечение в эскиз.** `addMeshSectionSketch(id, plane, offset)` режет размещённую сетку базовой
  плоскостью `XY|XZ|YZ`, сдвинутой на `offset` вдоль нормали эскиза (XY: +Z; XZ: −Y; YZ: +X — как у
  узла `Sketch`), и добавляет узел `Sketch` с опцией `plane`, параметром `offset` и геометрией из
  **вспомогательных отрезков** (`construction = true`) между закреплёнными точками в осях эскиза
  (XY: X, Y; XZ: X, Z; YZ: Y, Z). Вспомогательная геометрия не бывает профилем: эскиз — справочный
  контур, по которому пользователь чертит свой профиль (привязки, размеры). Точки на прямой
  соседей удаляются (допуск 1e-6 мм); больше 5000 отрезков или плоскость мимо сетки —
  `InvalidInput`. Эскиз — обычный узел: отмена, сохранение, правка.
- **Файл.** Поле `"referenceMeshes"`: `[{"id", "label", "fileName", "format": "stl"|"3mf",
  "unit", "rotation": [3], "translation": [3], "visible", "opacity", "data": "<base64 файла>"}]`.
  При загрузке файл читается и проверяется заново; повреждённые данные — ошибка загрузки с
  кодом читателя (`ParseError` для неверного base64 и битого STL). Байты файла сохраняются как
  есть (тот же файл можно извлечь `referenceMeshData`).

Проверка — `test_modeling_document_reference_mesh`: STL куба 10 мм (`exportStlBinary` бруса) —
12 треугольников, объём 1000, площадь 600; тело бруса 40×30×20 и его объём не меняются, дерево не
пересчитывается; дюймы + 90° вокруг Z + 100 мм по X → габарит [−154, 100]×[0, 254]×[0, 254];
отмена/повтор; сохранение (v5, base64) и загрузка — те же байты, свойства бит в бит; без сеток —
снова v3; повреждённые данные — ошибка с именем файла; обрезанный файл и предел размера
отклоняются, документ не меняется; сечения XY (3,7) и XZ (−5) — квадрат 10×10 из 4 закреплённых
вспомогательных отрезков, эскиз строится без ошибок; плоскость мимо — «plane XY at 50 mm does not
cut mesh 'Mesh001'».

## Ограничения

- Дерево линейное: операция работает с последним валидным телом (одно тело или составное из
  тел многотельной детали); операции Boss/Cut выбирают задетые тела по габариту.
- Узлы тел ссылаются на тела через грани; удаление и пересоздание узла, создавшего тело,
  меняет имя тела (ссылки разрешаются по сигнатуре грани).
- Возможности эскиза — `docs/modeling/SKETCH.md` (касательность, размеры по осям, эллипсы и
  сплайны поддерживаются); проекция ребра модели в эскиз даёт отрезок, окружность или дугу.
- Имена от эскиза устойчивы к размерам, но не к удалению и пересозданию сущностей (новые
  идентификаторы) — тогда ссылки ниже разрешаются по сигнатуре.
- Имена граней инструмента для массивов — по геометрическому порядку; если правка меняет
  порядок однотипных граней инструмента с одинаковым направлением, ссылка на копию
  спасается сигнатурой.
- Ссылка, разрешившаяся в несколько элементов, в слоте «одна ссылка» — ошибка; любая сломанная
  ссылка — `Error`, частичное применение не выполняется.
- Отмена/повтор — в пределах сессии (история не сохраняется в файл).
- Справочная сетка не участвует в построении: на её треугольники нельзя сослаться из операций
  (только сечение в эскиз и измерения); сечение в эскиз — только базовыми плоскостями XY/XZ/YZ.
- Таблица параметров: столбцы — переменные, подавление операций и материал; параметры
  операций, размеры эскиза и опции (не через переменную) столбцами не бывают — их выносят в
  переменную. Материал — только из встроенной библиотеки ядра (21 материал, `verified = false`).
  Базовый материал — `DocumentProperties::material` (поле `"material"` файла; страница детали
  записывает свой выбор поверх при сохранении): пустая ячейка материала — «как в детали».
- Копии массивов (`FeatureInfo::copies`) — только у `LinearPattern`, `CircularPattern`,
  `BodyLinearPattern`, `BodyCircularPattern` (зеркальные копии не перечисляются); по ним строятся
  массивы компонентов «по массиву детали» (`docs/assembly/ASSEMBLY_DOCUMENT.md`, раздел 5); в кэше
  файла — поле `"copies"` записи узла (матрицы 3×4).

## Как тестировать

```bash
cmake -S . -B build-occt -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBENDCORE_WITH_OCCT=ON \
  -DBENDCORE_OCCT_DIR=/home/user/bendcore/third_party/occt-install
cmake --build build-occt --target test_modeling_document_naming test_modeling_document_tree \
  test_modeling_document_io test_modeling_document_survival test_modeling_document_sketch \
  test_modeling_document_reference_parts test_modeling_document_advanced test_modeling_document_multibody \
  test_modeling_document_datum test_modeling_document_configurations
ctest --test-dir build-occt -R modeling_document --output-on-failure
```

Эталоны — аналитические формулы (`modeling_document_test_util.hpp`): потеря объёма
скругления прямого ребра 90° `(1 − π/4)·r²·L`, фаски `d²/2·L`, скругления кромки круга по
Паппу `(1 − π/4)·r²·2π(ρ + r(10 − 3π)/(3(4 − π)))`; положение граней проверяется прямым
поиском по геометрии, не через карту элементов.

- `test_modeling_document_tree` — коробка + скругление, высота 20 → 35 → 12 → 50;
  инкрементальность; карман (эскиз на верхней грани + вырез), вставленный выше по дереву, —
  ссылка точная; паз через ребро — по истории на 2 части; фаска выше по дереву удаляет ребро —
  скругление `Error` с причиной; перенос, подавление, откат; ошибки операций, связей и ссылок;
  переменные; замена базовой коробки — эскиз на грани разрешается по сигнатуре с `Warning`.
- `test_modeling_document_sketch` — имена граней и рёбер выдавливания от сущностей эскиза,
  скругление на ребре `G[Body](Profile:V1_4)` при изменении размеров эскиза (Exact); выбор
  областей; проекция окружности тела через карту элементов следует за радиусом; конфликт
  ограничений, неизвестная переменная, ошибки данных; вращение вокруг линии эскиза;
  эскиз на вспомогательной плоскости; связь «вниз по дереву» — ошибка; отмена/повтор,
  транзакции, отмена транзакции; миграция v2 с `PrismCut`.
- `test_modeling_document_io` — сохранение → загрузка: те же состояния, ссылки, объёмы, имена
  всех элементов; кэш (тела из кэша, эскизы пересчитаны; изменённая переменная — всё
  пересчитано; испорченный BRep — пересчитана только эта операция); миграция v1; ошибки.
- `test_modeling_document_survival` — модель с 6 ссылками (карман и отверстие — эскизы на
  гранях с вырезами, скругления, фаска), 25 сценариев + исходное + возврат:
  `[naming survival] scenarios: 27, references checked: 162, survived: 162, rate: 100.0 %`.
- `test_modeling_document_reference_parts` — 13 эталонных деталей (`REFERENCE_PARTS.md`):
  `[reference parts] parts: 13, scenarios: 118, references checked: 388, survived: 388, rate: 100.0 %`.
- `test_modeling_document_advanced` — кинематика по эскизу (отрезок + дуга) с резьбовым
  отверстием на торце: объём πR²(L + π/2·Rb) − отверстие; по сечениям с резьбовым отверстием на
  верхней грани; ребро жёсткости уголка с отверстием в его боковой грани; 13 сценариев
  размеров: `[advanced survival] scenarios: 13, references checked: 13, survived: 13, rate:
  100.0 %`; сохранение/загрузка атрибутов резьбы (с кэшем и без); ошибки (ветвящаяся и
  пересекающаяся траектория, одно сечение, номинал вне ISO 261).
- `test_modeling_document_datum` — точки (координаты, вершина, середина ребра, центр
  окружности бобышки, пересечение ребра с плоскостью H/3) после изменения W, H, D эскиза;
  плоскость через вершины (W,0,0), (0,D,0), (0,0,H): нормаль `(DH, WH, WD)/|…|`, расстояние от
  начала `WDH/|…|`; плоскость через три вершины верхней грани с эскизом и выдавливанием —
  объём `WDH + abh` и положение граней при трёх наборах размеров; по трём узлам `DatumPoint`;
  коллинеарные точки — ошибка; касательные к цилиндру R (через линию x = 20 — нормали
  `(cos φ, ±sin φ, 0)`, `cos φ = R/20`, параллельно X, под 30° к XZ; расстояние от оси = R) при
  R = 8 и 11, зеркало стержня в касательной плоскости — объём `2πR²L`, ось копии; ЛСК (X = +Y,
  XY через +X → Z = −Z; по рёбрам; по грани и ребру с началом в точке центра), точка в
  координатах ЛСК, эскиз на ЛСК и выдавливание — объём; ошибки; сохранение/загрузка с кэшем и
  без.
- `test_modeling_document_configurations` — брусок W×D×H со скруглением R = 2 верхнего
  переднего ребра и сквозным отверстием r = 5: три исполнения (60×40×20, 80×50×30,
  50×30×15 с подавленным отверстием) — объём каждого по формуле `W·D·H − (1 − π/4)·R²·W −
  π·r²·H`, скругление на том же ребре (`Exact`, ось на y = R, z = H − R); три круга
  переключений — объём бит в бит, `recomputed` пуст, имя ссылки то же; правка переменной и
  подавления в активном исполнении пишет ячейку, отмена по шагам; ошибки (неизвестная
  переменная и операция, недопустимые значения, материал, повтор обозначения, число ячеек,
  неизвестное активное); проверка исполнений — «-03» с H = −5 не строится, причина «feature
  'Body'», цикл из ячейки; удаление переменной/операции удаляет столбец; файл v4, старый v3
  без таблицы, испорченная таблица — `ParseError`; CSV туда и обратно с точкой и запятой,
  ручной файл с русскими заголовками, ошибки с номером строки.
- `test_modeling_document_multibody` — плита + шип (`newBody`), вычитание с сохранением шипа,
  массив шипа, зеркало плиты с объединением, удаление копии; объёмы по именам тел в 5
  сценариях (`[multibody survival] … references checked: 20, survived: 20, rate: 100.0 %`),
  сохранение/загрузка; пересечение и объединение тел; кинематика по рёбрам модели как новое
  тело; ошибки узлов тел.
