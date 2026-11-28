# Документ сборки `bendcore_assembly_document`

Фаза 3 `docs/ROADMAP_GENERAL_CAD.md` (пп. 1, 3, 4, 6, 7): документ сборки на геометрии OCCT
поверх решателя `bendcore_assembly_v2` (`docs/assembly/SOLVER.md`).

- Код: `include/bendcore/assembly_v2/{assembly_document,assembly_configurations,occt_entities,interference,bom,history}.hpp`,
  `src/assembly_v2/document/` (`serialization.cpp` — собственный формат, `history.cpp` — отмена,
  `configurations.cpp` — исполнения сборки).
- Сборка: `cmake/assembly_document.cmake`, цель `bendcore_assembly_document`. Зависит от
  `bendcore_assembly_v2`, `bendcore_kernel`, `bendcore_kernel_healing` (булевы операции),
  `bendcore_kernel_xcaf` (STEP), `bendcore_modeling_document` (карта имён элементов детали);
  `third_party/nlohmann/json.hpp` (MIT) — только внутри `serialization.cpp`.
- Тесты: `tests/assembly_v2/test_assembly_document_*.cpp`.
- Пространство имён: `bendcore::assembly_v2`. Все функции возвращают `kernel::Result`;
  исключения OCCT перехватываются и превращаются в `OperationFailed`.

## 1. Структура документа

Структура повторяет XCAF: **определения** (прототипы) и **вхождения**.

| Тип | Содержимое |
|---|---|
| `Definition` (деталь) | `shape` в собственной СК, `topology` (`kernel::TopologyIndex`), `elementMap` (карта имён `modeling::NamedShape`, может отсутствовать), `source` (путь/ID документа детали), `revision` (растёт при замене геометрии), `color`, `faceColors`, `properties` |
| `Definition` (сборка) | `occurrences` — вхождения в СК этой сборки |
| `Occurrence` | `id` (уникален в документе, не переиспользуется), `name`, `definition`, `placement` (относительно родителя), `fixed` (только верхний уровень), `color`, `pattern` (экземпляр массива), `suppressed`, `designation` (позиционное обозначение), `sourceId` (ID в файле внешней подсборки) |
| `PartProperties` | `designation` (обозначение), `name` (наименование), `material`, `density` (кг/м³) |

Определение 0 — корневая сборка (`root()`), её вхождения — верхний уровень (`topLevel()`).
Подсборка вставляется как вхождение определения-сборки; циклы и вставка корня отклоняются.
Одно определение может иметь сколько угодно вхождений — так считаются одинаковые детали в BOM.

**Подсборка для решателя — жёсткое тело.** Переменные решателя — только вхождения верхнего
уровня; положения внутри подсборки задаются её `placement`. Сопряжение может ссылаться на
грань детали внутри подсборки: путь `OccurrencePath` = `{вхождение верхнего уровня, …, вхождение
детали}`, сущность переводится в СК вхождения верхнего уровня произведением положений по пути.

### Источники деталей

`PartData{shape, elementMap}` — геометрия детали. `addPart(name, data, props)` — форма задана
напрямую; карта элементов, если есть, должна принадлежать этой же форме (`IsSame`).
`addPartFromSource(name, source, resolver)` — деталь по пути/ID: `PartResolver` — функция
`source → PartData`. Готовые:

- `partDataFromDocument(PartDocument)` — итоговая форма документа детали с картой имён;
- `filePartResolver(registry, baseDir)` — файл: `.json` (документ детали, перестраивается с
  `registry`), `.step/.stp` (`kernel::readStep`), `.brep` (текст или бинарный — по сигнатуре).
  Ошибки: `IoError`, `Unsupported` (расширение), ошибки разбора формата.

`reloadPart(def, resolver)` загружает источник заново и выполняет замену геометрии (раздел 4).

## 2. API

```cpp
#include <bendcore/assembly_v2/assembly_document.hpp>
using namespace bendcore::assembly_v2;

AssemblyDocument doc("ShaftAssembly");
auto bushing = doc.addPart("Bushing", {bushingShape, nullptr}).value();
auto shaft   = doc.addPart("Shaft", partDataFromDocument(shaftPartDocument).value()).value();
auto b = doc.addOccurrence(bushing, Transform::identity()).value();
auto s = doc.addOccurrence(shaft, Transform::fromTranslation({30, -15, 40})).value();
doc.setFixed(b, true);

GeometryRef side{{s}, ElementRef::named(ElementKind::Face, "Shaft:Side")};  // по имени
GeometryRef hole{{b}, ElementRef::face(holeIndex)};                         // по TopologyIndex
doc.addMate(MateSpec::concentric(side, hole));
doc.addMate(MateSpec::coincident({{s}, ElementRef::named(ElementKind::Face, "Shaft:Bottom")},
                                 {{b}, ElementRef::face(topIndex)}, Alignment::AntiAligned));
auto report = doc.solve();   // report.value().totalDof == 1; положения вхождений обновлены
```

| Функция | Назначение / ошибки |
|---|---|
| `addMate(MateSpec)` | Разрешает обе ссылки, строит сущности, проверяет `prepareMate`. Ссылка через экземпляр массива запоминает массив и ключ экземпляра (раздел 5). `InvalidInput`: путь не существует или не верхнего уровня в начале, у массива нет экземпляра с ключом, индекс вне диапазона, имя не найдено, нет карты имён; `Unsupported`: геометрия элемента |
| `resolveEntity`, `worldEntity` | Сущность ссылки в СК вхождения верхнего уровня / в мировой СК |
| `solve(options)` | Решает, записывает положения вхождений верхнего уровня, пересчитывает массивы. Отчёт `DocumentSolveReport`: подсистемы с ID вхождений и сопряжений документа, `totalDof`, `brokenMates` |
| `drag(occ, localPoint, target)` | Перетаскивание с учётом сопряжений (`assembly_v2::drag`) |
| `buildSolverModel()` | Модель решателя без решения (соответствие компонентов и сопряжений) |
| `replaceOccurrenceDefinition`, `replacePartGeometry`, `reloadPart` | Замена (раздел 4), отчёт `ReplaceReport{kept, remapped, broken, patternsUpdated}` |
| `addLinearPattern`, `addCircularPattern`, `addFeaturePattern`, `editPattern`, `removePattern`, `updatePatterns`, `patternInstance`, `instanceRefOf` | Массивы (раздел 5) |
| `placedParts(overrides)` | Все детали в мировой СК (`PlacedOccurrence`: путь, имя пути `A/B/C`, положение, форма) |
| `toXcaf`, `fromXcaf`, `exportStep`, `importStep` | Обмен (раздел 9) |
| `editMate(id, spec)` | Новое описание сопряжения с тем же ID; при ошибке документ не меняется |
| `setOccurrenceSuppressed`, `setMateSuppressed` | Подавление (раздел 10.4) |
| `setOccurrenceName`, `setOccurrenceDesignation`, `removeExplodedView` | Правка вхождения, вида |
| `toJson`, `fromJson`, `saveFile`, `loadFile`, `addSubassemblyFromSource` | Собственный формат, внешние подсборки (раздел 10) |
| `AssemblyHistory` (`history.hpp`) | Отмена/повтор (раздел 11) |
| `setConfigurationTable`, `activateConfiguration`, `configurationTable`, `configurationBase`, `configurationProperties`, `configurationTableToCsv`/`FromCsv`, `configurationBom` | Исполнения сборки (раздел 8.2) |
| `setOccurrenceConfiguration(id, cfg, resolver / LoadOptions)` | Исполнение детали (8.1) или внешней подсборки (8.2) у вхождения |
| `addExplodedView`, `explodedPlacements` | Разнесённый вид (раздел 7) |

Ссылки на `patterns()`, `mates()`, `definitions()` действительны до следующего изменения
документа (векторы могут перераспределяться).

## 3. Сущности сопряжений из геометрии OCCT

`entityFromElement(element, use)` (`occt_entities.hpp`); элемент берётся из `TopologyIndex`
детали (положение `TopLoc_Location` учтено, координаты — в СК формы детали).

| Элемент | `Auto` | `Axis` | `Point` |
|---|---|---|---|
| Плоская грань | Plane: точка — центр масс площади, нормаль `X×Y` плоскости, развёрнута для `REVERSED` (наружу из материала) | — | — |
| Цилиндрическая грань | Cylinder: ось, радиус; точка — проекция центра грани на ось | Axis | — |
| Коническая грань | Axis (точка — проекция центра грани) | Axis | — |
| Сферическая грань | Sphere (центр, радиус) | — | Point (центр) |
| Прямое ребро | Axis через середину ребра | Axis | — |
| Круговое ребро | Axis с точкой в центре окружности | Axis | Point (центр) |
| Вершина | Point | — | Point |

«—», тор, B-сплайн, Безье, поверхности вращения/выдвижения, эллипс, вырожденное ребро →
`Unsupported` с названием геометрии. Нормаль грани наружу позволяет задавать совпадение
торцов как `AntiAligned` («лицом к лицу»).

Разрешение ссылки `ElementRef`: `index` (индекс `TopologyIndex`) или `name` — имя из карты
элементов документа детали (`NamedShape::find`, затем `resolveReference` — псевдонимы и
история). При добавлении сопряжения запоминается `BoundElement`: индекс, геометрическая
сигнатура (`modeling::computeSignature`), определение и его ревизия.

## 4. Замена компонента

После любой замены (`replaceOccurrenceDefinition` — вхождение получает другое определение;
`replacePartGeometry`/`reloadPart` — у определения новая форма, ревизия +1) все сопряжения
перепроверяются. Сторона не затронута, если путь существует и конечное определение и ревизия
совпадают с привязкой. Иначе элемент ищется заново:

1. Ссылка по имени и у новой детали есть карта имён → `resolveReference(имя, сигнатура)`
   (точно → история → сигнатура); нужен ровно один элемент. Имя ссылки обновляется на текущее.
2. Иначе — тот же индекс, если элемент с ним существует и совместим по сигнатуре (тот же вид и
   тип геометрии, направление с косинусом ≥ 0,995: `signatureDistance` конечно).
3. Иначе — ближайшая сигнатура среди элементов того же вида: расстояние ≤ 0,35 и меньше 0,6 ×
   второго (как в `modeling::ResolveOptions`).

Затем новые сущности проверяются `prepareMate`. Неразрешимое сопряжение помечается
`brokenReason` (причина — от шага, на котором не нашлось элемента), остаётся в документе и
пропускается решателем (`DocumentSolveReport::brokenMates`). При следующей замене оно
проверяется снова и восстанавливается, если ссылки разрешились (тест: вал → коробка → вал).
Замена исходного вхождения массива меняет определение и у экземпляров.

## 5. Массивы компонентов

Массив хранит исходное вхождение (`seed`), необязательное опорное вхождение (`reference`) и
преобразования `L_i` в СК опоры. Поза экземпляра:

```
P_i = P_ref · L_i · P_ref⁻¹ · P_seed
```

Экземпляры — вхождения верхнего уровня с полем `pattern`; они не переменные решателя (их
положение задаёт массив: `setPlacement` и `drag` экземпляра — `InvalidInput`). `solve`/`drag`
после решения вызывают `updatePatterns`, поэтому экземпляры следуют за исходным и опорным
вхождениями.

**Ключи экземпляров** (`ComponentPattern::keys`, по одному на экземпляр): линейный — «i,j»
(номера по первому и второму направлению), круговой — «k», по отверстиям — имя первой грани
отверстия в карте элементов, если оно не повторяется у других отверстий, иначе «@dx,dy,dz»
(смещение оси от исходного отверстия, округление 1e-6 мм), по массиву детали — ключ копии
(«i,j» / «k», `PartFeaturePattern::keys`). При пересчёте массива (`updatePatterns`,
`editPattern`, замена геометрии опоры) экземпляр с тем же ключом сохраняет своё вхождение (ID),
исчезнувшие ключи удаляются, новые получают новые вхождения. Файлы без ключей (до этой версии)
получают ключи по порядку экземпляров при первом пересчёте; поле `"keys"` массива необязательное,
номер версии формата не меняется.

**Сопряжения с гранями экземпляров.** `GeometryRef::instance = {pattern, key}` — ссылка через
экземпляр; `addMate` заполняет его сам, если `path[0]` — экземпляр (или берёт `path[0]` по
заданному ключу при пустом пути). После каждого пересчёта массива (`refreshInstanceRefs`)
`path[0]` переводится на вхождение ключа: изменение числа экземпляров ссылку не ломает. Если
экземпляра с ключом больше нет (или массив удалён), сопряжение становится **висячим**:
`brokenReason` = «dangling pattern instance: pattern 1 has no instance '2,0'», решатель его
пропускает (`DocumentSolveReport::brokenMates`); когда экземпляр возвращается, ссылка
привязывается заново и сопряжение решается. В решателе экземпляр, на который ссылается
действующее сопряжение, — **неподвижный компонент** на позе массива; после решения массив
пересчитывается от нового положения исходного/опорного вхождения, и решение повторяется, пока
позы таких экземпляров не перестанут меняться (≤ 8 проходов; `DocumentSolveReport::passes`,
`patternsConverged`, при несходимости `success = false`). В файле — поле `"instance"` ссылки.

`editPattern(id, spec)` — новое описание массива того же вида и с тем же исходным вхождением
(число, шаг, ось, отверстие, массив детали); ошибки — как у `add*Pattern`, при ошибке массив не
меняется.

| Вид | `L_i` |
|---|---|
| Линейный | перенос `i·s₁·d₁ + j·s₂·d₂`, `0 ≤ i < count`, `0 ≤ j < count2`, кроме (0, 0) |
| Круговой | поворот на `k·Δφ` вокруг оси `(p, d)`: `R`, перенос `p − R p`, `k = 1…count−1` |
| По отверстиям | перенос между осями отверстий (ниже) |

**По массиву операции детали** (`FeaturePatternSpec::partPattern` — id операции `LinearPattern`,
`CircularPattern`, `BodyLinearPattern` или `BodyCircularPattern` документа детали; `seedHole` —
любая грань этой детали, обычно грань исходного элемента массива, путь — к детали-опоре).
`partDataFromDocument` переносит копии построенных массивов (`FeatureInfo::copies`, зеркальные
не переносятся) в `PartData::patterns` → `Definition::partPatterns` (в файле сборки —
`"partPatterns"` определения; источник, который загрузился, заменяет их своими). Преобразование
экземпляра в СК опоры `L = T·C·T⁻¹`: `T` — положение детали в СК вхождения верхнего уровня, `C` —
копия в СК детали; ключи — «i,j» (линейный, порядок копий операции: j внешний, i внутренний,
без (0, 0)) и «k» (круговой). Изменение числа копий в детали и замена геометрии (`reloadPart`,
`replacePartGeometry`) пересчитывают экземпляры с сохранением ключей. Нет такой операции —
`InvalidInput` «part 'Плита' has no pattern feature 'Nope' (patterns: Arr)».

**По отверстиям** (`FeaturePatternSpec{seed, seedHole, radius, tolerance}`, `partPattern` пуст): `seedHole` —
цилиндрическая грань отверстия детали-опоры (опора — `seedHole.path[0]`). Отбираются
цилиндрические грани детали с радиусом `radius` (по умолчанию радиус `seedHole`) с допуском
`tol·max(1, r)` и осью, параллельной оси `seedHole` (`|d₀ × d| ≤ tol`). Соосные грани (точка оси
на расстоянии ≤ `tol·max(1, r)` от оси группы) объединяются в одно отверстие — сквозное
отверстие из нескольких граней даёт один экземпляр. Для каждого другого отверстия
`L = перенос(Δ − d₀(d₀·Δ))`, `Δ` — разность точек осей: смещение берётся перпендикулярно оси,
поэтому осевое положение компонента сохраняется (отверстия, начинающиеся с одной плоскости).
Порядок экземпляров — по расстоянию от исходного отверстия, затем по координатам. При замене
геометрии опоры грань исходного отверстия ищется заново (раздел 4), число экземпляров
меняется вместе с числом отверстий.

## 6. Проверка пересечений

`checkInterference(document, options)` (`interference.hpp`):

1. Развёртка всех деталей в мировую СК (`placedParts`).
2. Габариты `BRepBndLib::AddOptimal` (точные по геометрии), расширенные на `contactTolerance`,
   чтобы касающиеся детали остались кандидатами. Отбор пар: сортировка по `xmin` и проход
   (sweep-and-prune по X) с проверкой `Bnd_Box::IsOut` по всем осям — `O(n log n + k)`.
3. **Габариты граней** (по требованию, один раз на деталь, `AddOptimal`, расширены на
   `contactTolerance`). Если ни одна грань детали A не пересекает габарит B, граница A дальше
   допуска от B: B целиком внутри A или вне его — классификация вершины B
   (`BRepClass3d_SolidClassifier`); вне — пары нет, внутри — расчёт ниже. Симметрично для B.
   Отбор применяется, когда все грани обеих деталей принадлежат телам.
4. **Куски крупной детали.** Деталь с числом граней > 32, участвующая хотя бы в двух парах,
   один раз разрезается сеткой плоскостей (`BRepAlgoAPI_Splitter`; ≈ 4 грани на ячейку,
   тонкие направления не делятся). Куски не перекрываются, их объединение — деталь (сумма
   объёмов проверяется, иначе используется вся деталь). Для пары берутся куски, чей габарит
   пересекает габарит другой детали: `A ∩ B` лежит в них, остальные куски дальше допуска.
5. Для каждой пары кусков с пересекающимися габаритами — `kernel::booleanOp(Common)`
   (лестница нечёткого допуска, проверка объёма; по умолчанию без `BRepAlgoAPI_Check` входов и
   без `SimplifyResult`); объём пары — сумма. Объём `> volumeTolerance` (1e-6 мм³) —
   **пересечение**.
6. Иначе расстояние — минимум `BRepExtrema_DistShapeShape` (многопоточный) по тем же парам
   кусков: точки наибольшего сближения лежат в выбранных кусках, а грани разреза находятся
   внутри детали и не дают ложного сближения. `≤ contactTolerance` (1e-6 мм) — **касание**
   (нулевой объём), больше — пары нет в отчёте.

Результат совпадает с прямым расчётом по целым деталям (Common и расстояние всей пары) —
`LocalAlgorithmMatchesWholeShapeReference`: те же пары и виды, объёмы до 1e-9 отн., расстояния
до 1e-9 мм. Сборка из 300 компонентов (плита с 299 отверстиями и 299 штифтов): 58 с → 7 с
(`docs/qa/PERFORMANCE.md`).

Отчёт: `parts`, `pairs{a, b, kind, volume, distance}`, `totalPairs`, `candidatePairs`,
`totalVolume`. `includeSameTopLevel = false` исключает пары деталей одной подсборки.

## 7. Разнесённый вид

`ExplodedView{name, steps}`, шаг — `{вхождения верхнего уровня, направление (мировая СК),
расстояние}`. `explodedPlacements(doc, view, progress)` возвращает мировые положения всех
вхождений верхнего уровня: шаг `s` выполнен на `clamp(progress − s, 0, 1)`, смещения шагов
складываются (вхождение может двигаться в нескольких шагах). Документ не меняется — исходные
положения сохраняются; формы для показа — `placedParts(&placements)`.

## 8. Состав изделия (BOM)

`flatBom(doc)` — только детали; количество — число вхождений по всему дереву (вхождение
подсборки умножает количества её состава). Порядок — первое появление при обходе в глубину.
`structuredBom(doc)` — по уровням: на каждом уровне вхождения сгруппированы по определению,
количество — в родительской сборке, у сборок `children`, позиции `1`, `2.1`, …

Свойства детали из документа детали (`docs/modeling/PART_DOCUMENT.md`, «Свойства документа»):
`PartData::properties` дополняет пустые поля свойств определения в `addPart` и
`addPartFromSource` (обозначение, наименование, материал, плотность, примечание
`PartProperties::note`, в файле — необязательное `"note"`); определение без имени называется
наименованием детали (затем обозначением), его вхождения — «<наименование>:<n>»; страница сборки
называет вставленный компонент так же. Примечание идёт в графу «Примечание» спецификации ГОСТ
2.106, если в спецификации не задано своё. При перезагрузке источника свойства определения не
меняются (их можно править в сборке).

Масса детали: `|V| · 10⁻⁹ · ρ` кг, `V` — `BRepGProp::VolumeProperties` (мм³), `ρ` — плотность
кг/м³ из `PartProperties`. Масса сборки — сумма по составу; если плотность не задана хотя бы у
одной детали, масса (строки сборки и `Bom::totalMass`) — `nullopt`, а не ноль. Обозначение и
наименование — из `PartProperties` (пустое наименование — имя определения).

### 8.1 Исполнения деталей

Документ детали может нести таблицу параметров (`docs/modeling/PART_DOCUMENT.md`, «Таблица
параметров»). Исполнение задаётся **источником** определения: `«<путь>#<обозначение
исполнения>»` (`configurationSource`, `splitConfigurationSource`); источник без «#» — деталь
как сохранена (активное исполнение файла). Одно определение — одно исполнение, поэтому BOM,
масса, спецификация и STEP различают исполнения без отдельной логики.

- `partDataFromDocument(document, configuration)` делает исполнение активным, перестраивает и
  возвращает форму, карту имён и `PartData::properties` исполнения: полное обозначение
  («РЦ.080.00.010-01»), наименование строки, материал — наименование материала библиотеки
  ядра («6061-T6»), плотность — его плотность. Исполнение, которое не перестраивается, —
  `InvalidInput` «configuration '-03' does not build: feature 'Body': …»; нет такого —
  «the part has no configuration '-09'». Однопараметрическая `partDataFromDocument(document)`
  тоже отдаёт свойства активного исполнения, если оно есть.
- `filePartResolver` разбирает «#»: `.json` — исполнение документа детали, у STEP/BRep
  исполнения нет («configuration '-01' needs a part document (.json), not '.step'»).
- `addPartFromSource` дополняет пустые поля свойств свойствами источника.
- `setOccurrenceConfiguration(occurrence, configuration, resolver)` — вхождение переходит на
  определение с источником `«<путь>#<исполнение>»`: существующее (одно определение на
  исполнение — количества в BOM складываются) или новое. Свойства нового — свойства детали как
  сохранена (определение с источником без «#», иначе прежнее определение), поверх — непустые
  свойства исполнения; исполнение без материала сохраняет материал и плотность детали. Затем —
  как `replaceOccurrenceDefinition`: ссылки сопряжений ищутся заново (имена от эскиза
  устойчивы к размерам), экземпляры массива следуют за исходным. `configuration` пусто —
  возврат к детали как сохранена. Ошибки `InvalidInput`: не деталь, экземпляр массива, нет
  источника («has no part document source»), нет исполнения, исполнение не строится; при
  ошибке документ не меняется.
- В файле сборки исполнение — часть `source`; при загрузке источник перестраивается в этом
  исполнении, отпечаток сверяется как обычно (`Loaded`/`Changed`). Неиспользуемые
  определения (вхождение перешло на другое исполнение) остаются в документе и в BOM не входят.
- Масса строки BOM — `V·ρ` исполнения (`V` — объём его формы, `ρ` — плотность его материала).

### 8.2 Исполнения сборки

Таблица исполнений сборки — аналог таблицы параметров детали (Design Table SolidWorks,
«Исполнения» КОМПАС) для состава. Заголовок `include/bendcore/assembly_v2/assembly_configurations.hpp`
(модель), API — `AssemblyDocument` и свободные функции в `assembly_document.hpp`, исходник
`src/assembly_v2/document/configurations.cpp`; состав всех исполнений — `configurationBom`
(`bom.hpp`, `bom.cpp`); файл — `serialization.cpp`.

**Модель.** `AssemblyConfigurationTable{columns, rows, active}`:

| Элемент | Содержимое |
|---|---|
| Столбец `{kind, target}` | `Suppression` — вхождение исключено или включено (`target` — ID вхождения); `Configuration` — исполнение детали или внешней подсборки у вхождения (`target` — ID вхождения); `MateValue` — значение сопряжения Distance (мм), Angle (градусы) или Gear (передаточное отношение) (`target` — ID сопряжения) |
| Строка `{designation, name, cells}` | обозначение (полное «РЦ.100.00.000» или суффикс «-01»), наименование, по ячейке на столбец |
| Ячейка | `1`/`0` (также true/false, yes/no, да/нет — `modeling::parseSuppressionCell`) у подавления; обозначение исполнения документа компонента или `#` (компонент как сохранён, источник без «#») у исполнения; число с точкой или запятой у значения; **пусто — базовое значение** |
| `active` | обозначение активного исполнения; пусто — базовые значения |

Переменных у документа сборки нет: параметры состава — значения сопряжений; переменные
деталей меняются исполнениями деталей (столбец `Configuration`). Полное обозначение —
`fullDesignation(table, row, rootDesignation)`: суффикс дописывается к обозначению первой
строки, а если и она суффикс — к обозначению сборки; `configurationFullDesignation(row)`,
`configurationProperties()` (свойства корня с обозначением и наименованием активного
исполнения — заголовок спецификации, свойства подсборки-исполнения).

**Базовые значения и правки.** Активное исполнение подставляет непустые ячейки своей строки
в документ (флаг `Occurrence::suppressed`, определение вхождения, `MateSpec::value`), пустые —
базовые значения (`configurationBase()`, по одному на столбец; при `setConfigurationTable` —
прежние для общих столбцов, текущее состояние документа — для новых). Документ всегда хранит
действующие значения, поэтому решатель, развёртка, пересечения, BOM, масса и STEP исполнения
не знают отдельной логики. Правка документа при активном исполнении (`setOccurrenceSuppressed`,
`editMate`, `setOccurrenceConfiguration`) попадает в непустую ячейку этого исполнения, иначе — в
базовое значение (как у детали): `configurationTable()` возвращает таблицу, синхронизированную
с документом, и при уходе с исполнения правка не теряется. Текст ячейки, задающий то же
значение («30» градусов), сохраняется как записан.

**Активация** — `activateConfiguration(designation, LoadOptions, ActivationOptions{solve,
solver})` → `ConfigurationActivation{designation, fullDesignation, excluded, disabledMates,
replaced, solve}`:

1. текущее состояние записывается в таблицу (см. выше), положения вхождений верхнего уровня
   запоминаются за уходящим исполнением;
2. значения столбцов ставятся по порядку: подавление — флагом; исполнение —
   `setOccurrenceConfiguration(id, исполнение, options)` (деталь — по `options.parts`,
   подсборка — по `options.assemblies`; определение исполнения уже есть — загрузки нет);
   значение — `spec.value` (угол из градусов в радианы);
3. положения, которые были у этого исполнения при уходе с него, восстанавливаются, массивы
   пересчитываются, затем `solve`. Поэтому переключение туда и обратно даёт **те же**
   положения бит в бит (решение стартует из решённого состояния; свободные вращения не
   «уплывают»);
4. отчёт: `excluded` — исключённые вхождения; `disabledMates` — сопряжения, у которых
   вхождение на любом уровне пути исключено, вместе с этим вхождением: они не решаются
   (`DocumentSolveReport::suppressedMates`) и **не являются ошибкой**; `replaced` — отчёты
   замены (`ReplaceReport`: сломанные ссылки с причиной) у вхождений, сменивших исполнение.

Исключённые компоненты (подавленные) не участвуют в решателе (не переменные), проверке
пересечений и развёртке (`placedParts`), BOM, массе и STEP. Ошибка на любом шаге
(«no configuration '-07'», исполнение детали не строится, нет файла подсборки) — документ
возвращается к состоянию до вызова.

**Правки таблицы** — `setConfigurationTable(table, options, activation)`: проверка, возврат к
базовым значениям прежней таблицы, новые базовые значения, активация `table.active`. Ошибки
`InvalidInput` (документ не меняется): «column 2: unknown occurrence 9», «column 3: mate 4
(Coincident) has no value», «column 1: occurrence 'B1:2' is a pattern instance; it follows the
pattern seed» (экземпляры массива создаются массивом и следуют за исходным), «column 4:
occurrence 'Корпус' has no source document: configurations need one», вхождение внешней
подсборки («belongs to external subassembly … and is read-only»), «repeated column
'suppress:A1'», «configuration 2 has no designation», «repeated designation '-01'»,
«configuration '-01' has 2 cells, the table has 3 columns», «configuration '-01', column
'suppress:A1': invalid value 'maybe' (expected 1 or 0)», «…: invalid value 'x': expected a
number», «…: a gear ratio must be positive», «no configuration '-07'», ошибки активации
(«occurrence 'Вал', configuration '-09': the part has no configuration '-09'»). Удаление
вхождения или сопряжения удаляет его столбец.

**Подсборки-исполнения.** `setOccurrenceConfiguration(id, configuration, LoadOptions)` у
вхождения внешней подсборки: источник определения — `«<путь>#<исполнение>»`, как у деталей.
Документ подсборки читается (`options.assemblies(путь)`), в нём активируется исполнение
(подстановка и **решение его сопряжений**: положения внутри подсборки — исполнения), состав
вставляется как при `addSubassemblyFromSource`; свойства определения — свойства прежнего с
обозначением и наименованием исполнения подсборки («РЦ.300.00.000-01»). Одно определение на
исполнение (повторное переключение берёт готовое). Ссылки сопряжений внутрь подсборки
переносятся на вхождения нового состава по ID в файле подсборки (`sourceId`); вхождения нет в
новом составе — сопряжение сломано с причиной и восстанавливается при возврате. Ошибки:
«occurrence 3 is a subassembly without a source document: configurations need one»,
«subassembly 'unit.json', configuration '-05': no configuration '-05'». При загрузке файла
определение-сборка с источником «…#-01» загружается так же; цикл файлов определяется по пути
без исполнения. Встроенные детали (без источника) двух исполнений одной подсборки — разные
определения (раздел 12), в `configurationBom` они сводятся в одну строку.

**Состав всех исполнений** — `configurationBom(document, options)` (`bom.hpp`) для групповой
спецификации ГОСТ 2.113 (`docs/drawing/SPECIFICATION.md`, «Исполнения сборки»): на копии
документа исполнения активируются по очереди без решения (состав от положений не зависит),
документ не меняется.

```cpp
struct ConfigurationBomRow {   // одна составная часть
    DefinitionId definition; DefinitionKind kind;
    std::string designation, name, material;
    std::vector<int> quantities;      // по исполнению; 0 — нет в нём
    double unitVolume; std::optional<double> unitMass;
};
struct ConfigurationBomColumn { std::string designation, fullDesignation, name;
                                std::optional<double> totalMass; double totalVolume; int totalParts; };
struct ConfigurationBom {
    std::vector<ConfigurationBomColumn> configurations;  // строки таблицы по порядку
    std::vector<ConfigurationBomRow> rows;   // первый уровень (structuredBom, уровень 0) — строки спецификации
    std::vector<ConfigurationBomRow> parts;  // все детали изделия (flatBom)
};
kernel::Result<ConfigurationBom> configurationBom(const AssemblyDocument&, const LoadOptions& = {});
```

Строки — в порядке первого появления по исполнениям; одна строка на определение, определения с
теми же видом, обозначением, наименованием, материалом, объёмом и массой — одна строка. Таблица
без строк — один столбец с текущим составом. Ошибки — активации исполнения или BOM (деталь без
геометрии) с префиксом «configuration '-01': ».

**CSV** — тем же кодом, что у деталей (`modeling::configurationTableToCsv` /
`configurationTableFromCsv`: «;», кавычки RFC 4180, CRLF, UTF-8 с BOM, десятичная запятая у
чисел): `configurationTableToCsv(document, table, decimal, byteOrderMark)`,
`configurationTableFromCsv(document, csv)`, заголовок столбца — `configurationColumnTitle`.
Столбцы сборки записываются столбцами таблицы детали: подавление — `suppress:<вхождение>`,
исполнение и значение — `configuration:<вхождение>`, `mate:<сопряжение>` (при чтении также
«подавить:», «исполнение:», «сопряжение:», «Обозначение», «Наименование»). Вхождение и
сопряжение называются по имени, если оно однозначно в редактируемом составе, иначе `#<ID>`.
Ошибки чтения — `ParseError`: ошибки CSV с номером строки (как у деталей), «CSV column
'material': an assembly configuration has no material…», «CSV column 'W': expected
'suppress:<component>', 'configuration:<component>' or 'mate:<mate>'», «… no occurrence named
'X'», «… 2 occurrences are named 'A1'; use #<id>». Значения проверяет `setConfigurationTable`.
Туда и обратно таблица не теряет ничего (обе формы разделителя).

**Файл** (версия 3): поле `"configurations"`; состояние вхождений и сопряжений в файле —
активного исполнения, `base` — базовые значения, `placements` — положения других исполнений
(ключ «» — базовые значения):

```json
"configurations": {"columns": [{"kind": "suppression", "target": 5}, {"kind": "configuration", "target": 2},
                               {"kind": "mateValue", "target": 3}],
                   "rows": [{"designation": "РЦ.100.00.000", "name": "Редуктор", "cells": ["0", "#", "20"]},
                            {"designation": "-01", "name": "Редуктор с удлинённым валом", "cells": ["1", "-01", "40"]}],
                   "active": "-01", "base": ["0", "#", "20"],
                   "placements": [{"configuration": "РЦ.100.00.000",
                                   "occurrences": [{"occurrence": 2, "placement": {"position": [...], "rotation": [...]}}]}]}
```

Таблица из файла проверяется как `setConfigurationTable`; ошибка — `ParseError`
«configurations: column 1: unknown occurrence 9», неизвестный вид столбца — «configurations:
unknown column kind 'x'». Без `base` базовыми считаются значения в файле.

**Отмена.** `AssemblyHistory::setConfigurationTable`, `activateConfiguration`,
`setComponentConfiguration` — по шагу истории («Configuration table», «Activate configuration»,
«Component configuration»); решение входит в шаг, поэтому `undo()` возвращает и состав, и
положения.

## 9. Обмен STEP

`toXcaf()` строит `kernel::AssemblyDocument`: корень — корневая сборка, определения —
достижимые от корня (неиспользуемые в файл не попадают), вхождения — с `TopLoc_Location`
(`toLocation`: кватернион + перенос), имена, цвета определений, вхождений и граней.
`exportStep(options)` = `writeStepAssembly(toXcaf(), options)` (AP214/AP242, единицы файла).

`fromXcaf` / `importStep`: один корень-сборка становится корнем документа, его вхождения —
верхним уровнем; иначе (несколько корней или деталь) каждый корень — вхождение верхнего уровня
с единичным положением. `fromLocation` отклоняет масштаб и отражение (`Unsupported`).

**Обозначения и наименования.** `PartProperties::designation` пишется в PRODUCT `id`
(пустое — `id` = имя, умолчание OCCT), `PartProperties::name` — в PRODUCT `description`,
`Occurrence::designation` — в NAUO `reference_designator` (`docs/kernel/XCAF_IO.md`). При чтении
`id`, совпадающий с именем (так пишут OCCT и многие CAD), обозначением не считается.
Подавленные вхождения в STEP не попадают; деталь без геометрии — `InvalidInput`.

## 10. Собственный формат (JSON)

`toJson(SaveOptions{embedGeometry, indent})` / `saveFile(path)`;
`fromJson(text, LoadOptions{parts, assemblies})` / `loadFile(path, registry)` возвращают
`LoadedAssembly{document, report}`. `fileLoadOptions(registry, dir)` — источники из файлов
относительно каталога документа (детали — `filePartResolver`).

```json
{"format": "bendcore-assembly", "version": 3, "name": "Вал во втулке",
 "properties": {"designation": "СБ-100", "name": "...", "material": "", "density": null}, "color": null,
 "next": {"occurrence": 3, "mate": 3, "pattern": 1},
 "definitions": [
   {"id": 1, "kind": "part", "name": "Втулка", "properties": {...}, "color": [r, g, b, a],
    "source": "parts/bushing.step", "revision": 0, "faceColors": [{"face": 2, "color": [...]}],
    "fingerprint": {"faces": 4, "edges": 6, "vertices": 4, "area": ..., "volume": ..., "box": [6 чисел]},
    "brep": "<base64 BinTools>", "elementMap": {"face": [...], "edge": [...], "vertex": [...], "deletions": {}}},
   {"id": 2, "kind": "assembly", "name": "Узел", "source": "sub/unit.json"},
   {"id": 3, "kind": "assembly", "name": "Рама", "occurrences": [...]}],
 "occurrences": [{"id": 1, "name": "Втулка:1", "definition": 1, "fixed": true, "suppressed": false,
   "designation": "", "color": null, "pattern": 1,
   "placement": {"position": [x, y, z], "rotation": [w, x, y, z]}}],
 "mates": [{"id": 1, "type": "Concentric", "name": "", "value": 0, "alignment": "Closest",
   "internalTangency": false, "suppressed": false, "broken": "<причина, если сломано>",
   "a": {"path": [2], "kind": "face", "name": "Shaft:Side", "use": "Auto"},
   "b": {"path": [1], "kind": "face", "index": 1, "use": "Auto"},
   "bound": [{"index": 0, "signature": {...}}, {"index": 1, "signature": {...}}],
   "rigidRelative": {"position": [...], "rotation": [...]},
   "gearPhase": [θa0, θb0]}],
 "patterns": [{"id": 1, "kind": "Feature", "seed": 2, "reference": 1, "instances": [3, 4, 5],
   "spec": {...}, "holeBound": {...}}],
 "explodedViews": [{"name": "...", "steps": [{"occurrences": [2], "direction": [0, 0, 1], "distance": 40}]}]}
```

Передача (`"type": "Gear"`, `MateSpec::gear(a, b, ratio, internal)`): `value` — передаточное
отношение zb/za, `internalTangency` — внутреннее зацепление, `gearPhase` — углы осей a и b от
линии центров в момент добавления (`gearAngles`, `docs/assembly/SOLVER.md`, раздел 2);
сопряжение Gear без `gearPhase` при загрузке — ошибка разбора. Ссылки — оси или цилиндрические
грани (делительный цилиндр, посадочные поверхности валов). Пример — зацепление вала-шестерни с
выходным валом редуктора `examples/gearbox/` (u = 3).

### 10.1 Компоненты и геометрия

Деталь задаётся источником (`source` — относительный путь к документу детали `.json`,
STEP или BRep) и/или встроенным BRep (`brep`, base64 бинарного BinTools, с картой имён
`elementMap` — записи `NamedShape::fromRecords` с атрибутами элементов `attributes` — пары
[ключ, значение]: «bearing.*», «key.*», «thread.*», «gear.*» — и именами тел `bodies`; файлы без
этих полей читаются как прежде). При `embedGeometry = false` пишется только
источник. `embedSourcedGeometry = false` (при `embedGeometry = true`) — встраиваются только детали без
источника (крепёж библиотеки, формы, заданные напрямую), детали с источником пишутся ссылкой: так
сохраняется сборка редуктора `examples/gearbox/` (тест `EmbedOnlyPartsWithoutSource`). Числа — `double` с 17 значащими цифрами, кватернион положения — как есть, поэтому
«сохранение → загрузка» восстанавливает положения до ulp, а «→ решение» — с той же
невязкой (тесты: 1e-9). ID определений при загрузке перенумеровываются, ID вхождений,
сопряжений и массивов сохраняются; счётчики `next` не дают переиспользовать ID.

### 10.2 Перемещённые и изменённые файлы

Для каждой детали с источником пишется `fingerprint` (числа граней/рёбер/вершин, площадь,
объём, габарит). При загрузке (`LoadReport::sources`, `SourceStatus`):

| Ситуация | Статус | Что происходит |
|---|---|---|
| источник загружен, отпечаток совпал (допуск 1e-6 относительный) | `Loaded` | ревизия как в файле |
| источник загружен, отпечаток отличается | `Changed` | ревизия +1 → ссылки сопряжений ищутся заново (раздел 4) |
| источник не загрузился, есть `brep` | `MissingEmbedded` | взята встроенная геометрия, сопряжения сохраняются |
| источник не загрузился, `brep` нет | `Missing` | деталь пуста (`Definition::loadError`), сопряжения с ней сломаны |
| нет источника | `Embedded` | встроенная геометрия |

Кроме отпечатка проверяется сигнатура каждого привязанного элемента (`bound`): если по
сохранённому индексу лежит элемент с другой сигнатурой (`signatureDistance` > 1e-6),
сторона ищется заново, даже когда отпечаток совпал. **По каждому затронутому сопряжению**
`LoadReport::mates` содержит `MateLoadDiagnostic{mate, name, outcome (Kept/Remapped/Broken),
definitions, message}`; сообщение называет детали и их файлы, для сломанных — причину
(«no face of 'Втулка' matches the referenced Cylinder …», «part 'Вал' has no geometry: cannot
load 'parts/shaft.json' …»). Сломанное сопряжение сохраняется со ссылками и причиной и при
следующей загрузке (файл вернули) восстанавливается.

Деталь без геометрии не попадает в `placedParts` (и в проверку пересечений); `flatBom`,
`structuredBom`, `toXcaf` для неё отвечают `InvalidInput` с причиной.

### 10.3 Внешние подсборки (вложенные документы)

`addSubassemblyFromSource(name, source, options)` загружает документ сборки `source` и
вставляет его состав как определение-сборку (`Definition::source` = путь). Сопряжения
вложенного документа не решаются — подсборка жёсткое тело с положениями, сохранёнными в её
файле. Пути деталей вложенного документа пересчитываются относительно верхнего
(`sub/../parts/x.step` → `parts/x.step`); деталь с тем же путём, что уже есть в документе,
становится тем же определением (BOM). Состав внешней подсборки только для чтения
(`isReadOnlyAssembly`: вставка, удаление, перемещение, замена, подавление внутри — `InvalidInput`).
При сохранении внешняя подсборка пишется ссылкой (`source`), её внутренние определения — нет.
Ссылки сопряжений внутрь неё хранят путь из ID вхождений **в её файле** (`sourceId`), поэтому
они переживают перезагрузку. Пропавшее в файле подсборки вхождение — сопряжение сломано с
причиной «occurrence N no longer exists in '…'». Цикл файлов подсборок обнаруживается
(`cycle of external subassemblies: a.json -> b.json -> a.json`); недоступный файл подсборки —
`AssemblyMissing` (состав пуст), диагностика вложенных источников передаётся с префиксом
«in '<файл>': ».

### 10.4 Подавление

Подавленное вхождение (`setOccurrenceSuppressed`) — не переменная решателя, его сопряжения
пропускаются (`DocumentSolveReport::suppressedMates`), оно не попадает в развёртку, BOM,
пересечения и STEP. Подавленное сопряжение (`setMateSuppressed`) только пропускается
решателем. Оба флага сохраняются в файле.

### 10.5 Версии и миграция

`AssemblyDocument::kFormatVersion = 4`. Версия новее — `Unsupported`; старее — миграция при
загрузке (`LoadReport::migrated`), сохранение всегда в текущей версии.

- **v4**: справочные сетки `"referenceMeshes"` и счётчик `next.referenceMesh` (раздел 14); файл
  без сеток отличается от v3 только номером версии.

- **v3**: таблица исполнений сборки `"configurations"` (раздел 8.2); файл без неё отличается от
  v2 только номером версии. Файлы v1 и v2 читаются, таблица у них пуста.
- **v2**: привязки сопряжений `bound` с сигнатурами, подавление, позиционные обозначения,
  счётчики `next`.
- **v1** (исходная раскладка): положение — `{"matrix": [r00, r01, r02, tx, r10, …, tz]}`
  (3×4, проверяется ортонормированность и det > 0, переводится в кватернион); сопряжения без
  `bound` — элементы привязываются заново по ссылкам (`index`/`name`) на загруженной
  геометрии; нет `suppressed`, `designation`, `next`.
- Ошибки: не JSON, чужой `format`, нет обязательного поля, неверный вид элемента или тип
  сопряжения, повтор ID, цикл подсборок, деталь без источника и без `brep` → `ParseError`.

## 11. Отмена и повтор

`AssemblyHistory(document, HistoryOptions{maxDepth, solveAfterEdit, solver})` владеет
документом; изменения — транзакции: `insertComponent`, `removeComponent`, `replaceComponent`,
`moveComponent`, `setFixed`, `setComponentSuppressed`, `addMate`, `editMate`, `removeMate`,
`setMateSuppressed`, `addLinearPattern`, `addCircularPattern`, `addFeaturePattern`,
`removePattern`, `solve`, а также `apply(label, edit, solve)` для произвольной правки.
Перед транзакцией запоминается снимок документа (формы OCCT, карты имён и индексы
топологии — общие дескрипторы, копируются только значения); ошибка внутри транзакции
возвращает документ к снимку и шаг не записывается. При `solveAfterEdit` (по умолчанию)
изменения состава и сопряжений решаются в той же транзакции, поэтому `undo()` восстанавливает
положения до правки побитно, `redo()` — после неё. Новое изменение очищает стек повтора;
`maxDepth` (200) — число хранимых шагов. `undoLabel`/`redoLabel` — названия шагов.

## 12. Ограничения

- В STEP не пишутся сопряжения, фиксация, массивы, разнесённые виды, материал и плотность
  (они — в собственном формате, раздел 10).
- Внешняя подсборка только для чтения; её правка — в её собственном документе. Встроенные
  детали без источника, встречающиеся и в документе, и во внешней подсборке, остаются
  разными определениями (объединяются только детали с одинаковым путём источника).
- Подсборка решается как жёсткое тело; гибкие подсборки и сопряжения внутри определения
  подсборки не поддерживаются.
- Экземпляр массива в сопряжении для решателя неподвижен: связь «исходное вхождение →
  экземпляр» решается повторными проходами, а не одной системой, поэтому степени свободы
  исходного, ограниченные только через экземпляр, ранговый анализ не видит; сопряжение
  экземпляра с опорой массива может переопределить систему (поворот исходного вокруг оси).
  Перетаскивание учитывает экземпляры одним проходом. Массив по отверстиям берёт смещение
  перпендикулярно оси; отверстия на разных уровнях по оси дают компонент на уровне исходного.
  Массив по массиву детали — только операции массивов документа детали (не по эскизу и не по
  таблице точек).
- Повторное разрешение ссылки по индексу без карты имён опирается на совпадение индекса и
  типа геометрии; при перестановке граней одного типа выбирается тот же индекс.
- Пересечения проверяются точной булевой операцией для каждой пары-кандидата; для сотен
  деталей в плотной сборке время растёт с числом пар-кандидатов.
- Разнесённый вид смещает только вхождения верхнего уровня.
- Исполнение выбирается у вхождения детали с источником — документом детали; у встроенной
  детали без источника, STEP и BRep исполнений нет. Определения прежних исполнений остаются в
  документе (не удаляются автоматически).
- Исполнения сборки: столбцы — подавление, исполнение компонента и значение сопряжения;
  столбцов фиксации, положения и цвета нет; у экземпляров массива столбцов нет (они следуют за
  исходным компонентом). Исполнение есть только у внешней подсборки (документа в своём файле);
  у встроенной подсборки — нет. Положения неактивных исполнений хранятся с момента ухода с них:
  исполнение, ни разу не бывшее активным, решается из текущих положений.
- Справочная сетка (раздел 14) не участвует в сопряжениях: её положение задаётся только
  перемещением.

## 13. Как тестировать

```bash
cmake -S . -B build-occt -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBENDCORE_WITH_OCCT=ON -DBENDCORE_OCCT_DIR=/home/user/bendcore/third_party/occt-install
cmake --build build-occt --target test_assembly_document_mates test_assembly_document_patterns \
  test_assembly_document_interference test_assembly_document_bom test_assembly_document_exchange \
  test_assembly_document_io test_assembly_document_history test_assembly_document_configurations \
  test_assembly_document_assembly_configurations test_assembly_document_reference_mesh \
  test_assembly_document_part_properties test_assembly_document_pattern_mates
ctest --test-dir build-occt -R assembly_document --output-on-failure
```

| Тест | Что проверяет | Эталон |
|---|---|---|
| `assembly_document_mates` | сущности из плоской/цилиндрической/конической/сферической граней, прямых и круговых рёбер, вершин (нормаль наружу, центр грани, середина ребра, центр окружности), отказ для тора и B-сплайна; вал (документ детали, ссылки по именам `Shaft:Side`, `Shaft:Bottom`) во втулке (ссылки по индексам): торец на z = 30, ось +Z, середина вала z = 55, 1 степень свободы; перетаскивание поворачивает вал к цели (atan2 = π/2); ошибки ссылок; замена: длинный вал (z середины 70), втулка выше (торец z = 40), коробка → соосность сломана с причиной, возврат вала восстанавливает; документ детали из файла и `reloadPart` | геометрия по построению |
| `assembly_document_patterns` | плита с 4 отверстиями r = 5,5 и отверстием r = 3: болт сопряжён с одним отверстием, массив по отверстиям даёт 3 экземпляра в (20,60), (80,20), (80,60) на z = 10; пересечений нет, 4 касания головок; BOM 4 болта; поворот плиты на 90° — болты следуют; линейный двумерный массив, направление в СК опоры, круговой массив 6 × 60° | координаты отверстий |
| `assembly_document_interference` | коробки [0,20]³ и [10,30]³ → 1000 мм³; касание по грани → Touching, объём 0; зазор 0,5 → нет пары; отбор по габаритам (2 кандидата из 6); повёрнутая плита и штифт → π·r²·h; вложенный куб → его объём; детали внутри подсборки; плита с 30 отверстиями (сетка кусков) и штифты по месту / с зазором / с натягом, кубы внутри, на плите и над ней — совпадение с прямым расчётом по целым деталям | формулы объёма; прямой расчёт `Common` + `BRepExtrema` по целым формам |
| `assembly_document_bom` | плоский: количества 1/2/4/4 через подсборку, массы `V·ρ`, отсутствие массы без плотности; по уровням: позиции `2.1`, количества в сборке, масса колеса; разнесённый вид (частичный шаг, суммирование шагов, документ не меняется, формы сдвинуты) | формулы |
| `assembly_document_exchange` | `toLocation`/`fromLocation`, масштаб → Unsupported; STEP AP214/AP242: имя корня, вхождения и имена, общее определение подсборки, цвета, положения (1e-9), объёмы и габариты всех деталей, габарит повёрнутого штифта; STEP одной детали | исходный документ, геометрия |
| `assembly_document_io` | вал (документ детали `parts/shaft.json`) во втулке (`parts/bushing.step`): сохранение → загрузка → решение — положения 1e-9, 1 степень свободы, свойства и обозначения (кириллица), со встроенной геометрией и без; кривошип (палец, рычаг, угол 30°): 0 степеней свободы, начало рычага (20, 20, 10), cos угла; правка угла 60° и подавление (1 степень свободы); плита с болтами: массив по отверстиям, подавленный экземпляр, разнесённый вид, BOM 3 болта и объём `π·5²·30 + π·8²·6`; перемещённый файл — `MissingEmbedded`/`Missing`, диагностика по обоим сопряжениям с путём файла, возврат файла восстанавливает; изменённые файлы — длинный вал (`Changed`, торец `Remapped`), втулка → коробка (соосность `Broken` с причиной «Cylinder»), торец вала на z = 30; внешняя подсборка дважды, сопряжение на грань внутри неё, только чтение, общий BOM, перезагрузка, цикл файлов; миграция v1 (матрица, привязка без сигнатур); ошибки формата; STEP: обозначения/наименования/позиционные обозначения, подавленное вхождение не экспортируется | аналитика положений, формулы объёма |
| `assembly_document_history` | отмена двух сопряжений возвращает вал в исходное положение побитно, повтор — посаженное; правка (расстояние 5) и отмена; подавление; удаление компонента с сопряжениями и отмена (те же ID); замена на длинный вал (середина z = 70) и отмена; подавление компонента; ошибки не записываются; линейный и круговой массивы, удаление массива и отмена; предел глубины; откат частичной правки в `apply` | побитное сравнение со снимком, геометрия по построению |
| `assembly_document_configurations` | документ детали `block.json` (брусок W×D×H, отверстие r = 5; исполнения РЦ.080.00.010 60×40×20 S235, -01 80×50×30 6061-T6, -02 50×30×15 без отверстия и без материала, -03 H = −5): два вхождения разных исполнений — источник `block.json#-01`, обозначение, наименование, материал; BOM 2 строки, объёмы и массы `V·ρ`, общая масса; третье вхождение «-01» — то же определение, количество 2; «-02» берёт плотность детали; ошибки («-09», «-03» не строится с причиной `Body`, деталь без источника), неудачная смена ничего не меняет; возврат к «как сохранена»; сохранение со ссылками и загрузка из файла детали — `Loaded`, те же количества и масса; разбор источника и `filePartResolver` с «#» (STEP/BRep — ошибка) | формулы объёма и массы |
| `assembly_document_assembly_configurations` | редуктор-подобная сборка: корпус 100×60×20 (отверстие r = 10, четыре r = 5, ρ = 7200), вал — документ детали `shaft.json` (R10×60, «-01» R10×80, «-02» R10.5×60, S235), болты М10×30 и М10×40 (ρ = 7850); исполнения: базовое — два болта М10×30, вылет вала 20; «-01» — четыре болта М10×40, вал «-01», вылет 40; «-02» — пустые ячейки (базовые значения), вал «-02». По каждому: масса `Σ V·ρ` (объёмы по формулам цилиндров и коробки), строки и количества BOM, степени свободы (3 / 5 / 3 — вращения вала и болтов), невязка < 1e-9, высота торца вала (40 / 60 / 40), число пересечений (0 / 0 / 1, объём натяга `π(10.5² − 10²)·20`), отключённые сопряжения исключённых болтов (8 / 4); состав всех исполнений (`configurationBom`: количества по исполнениям). Переключение туда и обратно — положения побитно; правка значения при активном исполнении пишет его ячейку; ошибки (нет исполнения, неизвестное исполнение компонента, недопустимое значение, компонент без источника, повтор обозначения) не меняют документ; удаление вхождения удаляет столбец. Файл v3: таблица, базовые значения, активное исполнение и положения другого исполнения; v2 читается; повреждённый вид столбца — `ParseError`. CSV туда и обратно (точка и запятая), одинаковые имена — `#<ID>`, русские заголовки, ошибки разбора. Отмена активации и таблицы. Вложенная подсборка-исполнение (планка с двумя болтами, второй исключён в базовом, «-01» поднимает болт на 5 мм сопряжением подсборки): источник `unit.json#-01`, обозначение исполнения, сопряжение внутрь подсборки переносится по `sourceId`, BOM 1 → 2 болта и масса по формулам, подъём головки 5 мм, таблица верхней сборки со столбцом исполнения подсборки, файл | формулы объёма и массы, аналитика положений |
| `assembly_document_part_properties` | плита документа детали (АБВГ.715311.001, «Плита опорная», 6061-T6, примечание): `PartData::properties`, имя определения и вхождений «Плита опорная:1/2», свои свойства не заменяются, BOM — обозначение, наименование, 2 шт., масса `60·40·20·10⁻⁹·2700`; примечание в файле; источник `plate.json#-01` — «АБВГ.715311.001-01», наименование строки; STEP детали и сборки — PRODUCT `id`/`description` после записи и чтения | формулы, ГОСТ 2.113 |
| `assembly_document_pattern_mates` | колпачок на стойке-экземпляре «2,0» линейного массива: позиция (100, 25, 30), 1 степень свободы; число 3 → 5 — то же вхождение; 3 → 2 — висячая ссылка с причиной, 2 сопряжения пропущены; 2 → 3 — привязка заново; плита поднята — 2 прохода, колпачок z = 37; файл (ключи, ссылка); массив по отверстиям — ключ «@60,0,0» переживает добавление отверстия, удаление отверстия — висячая ссылка; массив по `LinearPattern` детали 3 × 2 (повёрнутая плита): положения `R·(15 + 30i, 15 + 25j, 10) + t`, 3 → 4 копий с сохранением ключей, файл; копии `CircularPattern` 4 × 90° — `(30cos kφ, 30sin kφ)` | формулы положения |
| `assembly_document_reference_mesh` | две детали 10×10×10 и сетка-куб 30 мм, накрывающая обе: BOM — 1 строка, 2 детали, объём 2000, масса `2000·10⁻⁹·7850`; развёртка — 2 детали; пересечений нет; STEP без сетки; мировая сетка [−5, 25]³, объём 27000; поворот 90° вокруг Z + перенос + дюймы через `AssemblyHistory::apply` и отмена; удаление и отмена; файл v4: имя, единицы, видимость, прозрачность, байты, положение, `migrated = false`, новый ID больше сохранённого; повреждённые данные — ошибка с именем файла; обрезанный STL и неизвестные единицы отклоняются | формулы объёма и массы, геометрия по построению |

## 14. Справочная сетка

`ReferenceMeshComponent` — файл STL/3MF, встроенный в документ сборки (`docs/kernel/ANALYSIS.md`,
раздел 7): скан помещения, оснастки, покупного узла без B-rep. Исходник —
`src/assembly_v2/document/reference_meshes.cpp`.

```cpp
ReferenceMeshId id = doc.addReferenceMesh("Скан", "scan.stl", kernel::MeshFileFormat::Stl, bytes, "mm",
                                          Transform::fromTranslation({0, 0, 50})).value();
doc.setReferenceMeshPlacement(id, Transform::fromAxisAngle(Vec3::UnitZ(), M_PI / 2));
doc.setReferenceMeshUnit(id, "in");
doc.setReferenceMeshDisplay(id, true, 0.4);
kernel::TriangleMesh world = doc.worldReferenceMesh(id).value();   // масштаб единиц, затем положение
```

**Решения.**

- **Не деталь.** Сетка не определение и не вхождение: её нет в `placedParts`, `flatBom` /
  `structuredBom` / `configurationBom`, массах, `toXcaf` и STEP, решателе, массивах, разнесённых
  видах, таблице исполнений.
- **Позиционирование — только перемещением.** Сопряжения к плоскости или точке сетки не
  реализованы: сущности сопряжений строятся из геометрии B-rep определений (`occt_entities`), а
  у сетки нет граней и рёбер — плоскость пришлось бы аппроксимировать по треугольникам, и
  ссылка не переживала бы замену файла. Сетка фиксирована относительно сборки; положение
  (перенос и кватернион, как у вхождений) меняется правкой, записанной в историю.
- **Пересечения не проверяются, даже как предупреждение.** Скан обычно не замкнут, у него нет
  объёма, а пересечение с деталями — нормальная ситуация (деталь вписывается в скан); точная
  проверка потребовала бы тела из сетки. Близость к сетке пользователь измеряет: расстояние от
  точки сетки до грани детали — в панели «Измерения».

Отмена и повтор — снимок документа `AssemblyHistory` (сетки общие у снимков, `shared_ptr`).
Ошибки вставки — ошибки читателя с префиксом `mesh '<файл>': `, неизвестные единицы, нечисловое
положение. Файл: `"referenceMeshes": [{"id", "name", "fileName", "format", "unit", "placement":
{"position", "rotation"}, "visible", "opacity", "data": "<base64>"}]`, `next.referenceMesh`;
при загрузке файл читается заново, ошибка — с кодом читателя.

