# Сборка Open CASCADE для bendcore (native и WASM)

Фаза 0, п. 1 плана `docs/ROADMAP_GENERAL_CAD.md`. Ядро — OCCT 8.0.1 (LGPL 2.1 с
исключением OCCT), статические библиотеки `libTK*.a`.

## 1. Явный набор тулкитов

Единственный источник списка — `third_party/occt_toolkits.txt` (по одному тулкиту в
строке, `#` — комментарий). Его читают:

- корневой `CMakeLists.txt`: переменная `BENDCORE_OCCT_TOOLKITS`, функция
  `bendcore_resolve_occt_toolkits()` превращает её в полные пути
  `${BENDCORE_OCCT_DIR}/lib/lib<TK>.a`; из неё строятся `OpenCASCADE_LIBRARIES`
  (цель `bendcore_import`) и `BENDCORE_OCCT_LIBS` (цель `bendcore_occt` и модули
  `cmake/kernel_*.cmake`). Если библиотеки нет — `FATAL_ERROR` на этапе конфигурации
  с именем тулкита и путём. Изменение файла перезапускает конфигурацию
  (`CMAKE_CONFIGURE_DEPENDS`);
- `third_party/build_occt.sh` (native) и `third_party/build_occt_wasm.sh`: список
  передаётся в OCCT как `BUILD_ADDITIONAL_TOOLKITS` при выключенных модулях.

Раньше обе переменные заполнялись `file(GLOB …/libTK*.a)`: состав линковки зависел от
того, что лежит в каталоге установки (48 тулкитов, включая TKXml*, TKBin*, TKStd*,
TKTObj, TKDEVRML/OBJ/PLY, TKRWMesh, TKXMesh, TKExpress, TKHelix, TKDECascade).

### Тулкиты и зачем они bendcore

| Тулкит | Что даёт | Где нужно в bendcore |
|---|---|---|
| TKernel | `Standard_*`, `NCollection`, `TCollection`, `OSD`, `Message` | всё; исключения `Standard_Failure` на границе модулей |
| TKMath | `gp_*`, `math_*`, `Bnd_*`, `BSplCLib` | вся геометрия, габариты |
| TKG2d | `Geom2d_*` | p-кривые граней, эскиз на грани |
| TKG3d | `Geom_*`, `GeomAdaptor` | поверхности и кривые B-Rep, анализ типов поверхностей |
| TKGeomBase | `GeomAPI_*`, `GC_*`, `GCE2d`, `Extrema` | построение сплайнов/дуг (`GC_MakeArcOfCircle`, `GeomAPI_PointsToBSpline` в теле трубы) |
| TKBRep | `TopoDS`, `BRep_*`, `BRepTools`, `TopExp` | представление формы, сериализация BRep |
| TKGeomAlgo | пересечения, аппроксимация, `GeomFill`, `GeomPlate` | булевы операции, скругления, кинематика |
| TKTopAlgo | `BRepBuilderAPI_*`, `BRepCheck`, `BRepGProp`, `BRepExtrema`, `BRepLib` | построение граней/тел, проверка формы, масса и площадь, расстояния |
| TKPrim | `BRepPrimAPI_*` (box, cylinder, prism, revol) | выдавливание, вращение, примитивы |
| TKShHealing | `ShapeFix`, `ShapeUpgrade`, `ShapeAnalysis` | модуль `geometry_healing`, лечение при импорте |
| TKBO | `BRepAlgoAPI_*`, `BOPAlgo` | булевы операции (вырез, объединение, пересечение), интерференция в сборке |
| TKBool | `BRepFill`, старые булевы `BRepAlgo` | `BRepOffsetAPI_MakePipe` (тело трубы), зависимость TKFeat/TKFillet/TKOffset |
| TKFeat | `BRepFeat_*` | отверстия, карманы, рёбра жёсткости (фазы 1–2) |
| TKFillet | `BRepFilletAPI_*`, `ChFi3d` | скругления, фаски |
| TKOffset | `BRepOffsetAPI_MakeThickSolid`, `MakeOffsetShape`, `DraftAngle` | оболочка, тонкостенные операции, уклон |
| TKMesh | `BRepMesh_IncrementalMesh` | тесселяция для вьюпорта и STL |
| TKHLR | `HLRBRep_Algo`, `HLRBRep_PolyAlgo` | виды чертежей с удалением невидимых линий (фаза 4) |
| TKCDF, TKLCAF, TKCAF | OCAF: `TDocStd_Document`, `TDF_Label`, `TDataStd_Name`, `TNaming` | документ XCAF при импорте STEP/IGES со структурой и именами |
| TKXSBase | `XSControl`, `IFSelect`, `Interface`, `Transfer` | общая основа трансляторов STEP/IGES |
| TKDE | `DE_*` (провайдеры обмена) | зависимость TKDESTEP/TKDEIGES/TKDESTL |
| TKXCAF | `XCAFDoc_*`, `XCAFApp_Application` | структура сборки, имена, цвета в STEP (`src/step_reader.cpp`, `bendcore_kernel_xcaf`, docs/kernel/XCAF_IO.md) |
| TKVCAF | `TPrsStd_DriverTable` | **добавлен по требованию линковщика**, см. ниже |
| TKV3d | `AIS_*`, `Prs3d_*`, `SelectMgr_*` | **добавлен по требованию линковщика**, см. ниже |
| TKService | `Graphic3d_*`, `Aspect_*`, `Image_*` | **добавлен по требованию линковщика**, см. ниже |
| TKDESTEP | `STEPControl_*`, `STEPCAFControl_*` | импорт и экспорт STEP AP203/214/242 |
| TKDEIGES | `IGESControl_*`, `IGESCAFControl_*` | импорт IGES |
| TKDESTL | `StlAPI_*`, `RWStl` | экспорт STL (фаза 2) |

### Тулкиты, добавленные к стартовому списку раздела 3 плана

Стартовый список (26 тулкитов) не линкуется: `test_step_reader`,
`test_import_workflow` и `e2e_import` (через `src/step_reader.cpp` →
`XCAFApp_Application`) падают с неразрешёнными символами из TKXCAF:

| Добавлен | Кто требует (объект в `libTKXCAF.a`) | Неразрешённые символы (примеры) |
|---|---|---|
| TKVCAF | `XCAFApp_Application.cxx.o`: конструктор `XCAFApp_Application()` регистрирует `XCAFPrs_Driver` в таблице драйверов представления | `TPrsStd_DriverTable::Get()`, `TPrsStd_DriverTable::AddDriver`, `TPrsStd_Driver::TPrsStd_Driver()` |
| TKV3d | `XCAFPrs_Driver.cxx.o`, `XCAFPrs_AISObject.cxx.o` (`XCAFPrs_AISObject` наследует `AIS_ColoredShape`) | `vtable for AIS_ColoredShape`, `vtable for AIS_Shape`, `Prs3d_Drawer::ShadingAspect()`, `SelectMgr_SelectableObject::~SelectMgr_SelectableObject()` |
| TKService | `XCAFDoc_VisMaterial.cxx.o`, `XCAFPrs_Texture.cxx.o` | `Graphic3d_MaterialAspect::SetColor`, `Graphic3d_PBRMaterial::Graphic3d_PBRMaterial()`, `Graphic3d_BSDF::CreateMetallicRoughness`, `vtable for Graphic3d_TextureMap` |

Это зависимость самого OCCT 8.0.1 (`src/DataExchange/TKXCAF/EXTERNLIB.cmake` перечисляет
TKService, TKV3d, TKVCAF), её нельзя убрать без патча OCCT. Сборка OCCT для WASM
добавляет эти три тулкита автоматически по тем же `EXTERNLIB`. Рендеринга в них bendcore
не использует; при статической линковке в программу попадают только объекты, на которые
есть ссылки (для WASM — после `wasm-ld --gc-sections`).

### Что исключено

- **Visualization** кроме TKService/TKV3d: TKOpenGl, TKOpenGles, TKMeshVS, TKIVtk,
  TKD3DHost — вьюпорт bendcore на three.js.
- **ApplicationFramework**: форматы хранения OCAF TKBin*, TKXml*, TKStd*, TKTObj* —
  у bendcore собственный формат документа (фаза 1, п. 6); BRep сериализуется через
  `BRepTools`/`BinTools` из TKBRep/TKTopAlgo.
- **DataExchange**: TKDEVRML, TKDEOBJ, TKDEGLTF, TKDEPLY, TKDECascade, TKRWMesh,
  TKBinXCAF, TKXmlXCAF — форматы не входят в v1.0. glTF понадобится — добавить
  TKRWMesh + TKDEGLTF (и RapidJSON для записи).
- **ModelingAlgorithms**: TKXMesh (плагин сетки), TKExpress (генератор EXPRESS),
  TKHelix (спирали; понадобится для резьбы с геометрией витков — добавить тогда).
- **Draw** целиком (Tcl/Tk), сторонние библиотеки FreeType, FreeImage, FFmpeg,
  OpenVR, VTK, TBB, RapidJSON, Draco, Eigen.

## 2. Native

```bash
# OCCT (один раз; 29 тулкитов из occt_toolkits.txt, 249 МиБ статических библиотек;
# с проверками в Release и C++20 — раздел 6)
OCCT_INSTALL=$PWD/third_party/occt-install-checked third_party/build_occt.sh

cmake -S . -B build-occt -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBENDCORE_WITH_OCCT=ON -DBENDCORE_OCCT_DIR=$PWD/third_party/occt-install-checked
cmake --build build-occt
ctest --test-dir build-occt --output-on-failure
```

Если `BENDCORE_OCCT_DIR` не задан, а `third_party/occt-install-checked` (в Emscripten —
`third_party/occt-install-wasm-checked`) существует, конфигурация берёт его по умолчанию.

В рабочих копиях (git worktree) `BENDCORE_OCCT_DIR` — абсолютный путь к основной
установке `/home/user/bendcore/third_party/occt-install-checked`.

Цели, линкующие OCCT: `bendcore_import` (+ `test_occt_init`, `test_step_reader`,
`test_parametric_builder`, `test_geometry_cache`, `test_dxf_reader`,
`test_import_workflow`, `e2e_import`), `bendcore_occt` (+ `test_tube_occt`,
`test_geometry_analyzer_occt`), `bendcore_server_core` (+ `bendcore_server`,
`test_server`), модули `cmake/kernel_*.cmake`.

### `build_occt.sh`

Скрипт собирает OCCT 8.0.1 (`Release`, `BUILD_LIBRARY_TYPE=Static`) по тому же списку
`third_party/occt_toolkits.txt`, что и `build_occt_wasm.sh`: все `BUILD_MODULE_*=OFF`,
`BUILD_ADDITIONAL_TOOLKITS=<список>` (зависимости из `EXTERNLIB` OCCT добавляет сам),
`USE_TK/TCL/FREETYPE/FREEIMAGE/FFMPEG/OPENVR/RAPIDJSON/DRACO/TBB/VTK/EIGEN/OPENGL/GLES2=OFF`,
документация, тесты и Inspector выключены. После установки проверяет заголовки и наличие
каждого тулкита списка и печатает размеры `.a` с пометкой «listed» / «dependency added by
OCCT». Пути переопределяются переменными окружения (по умолчанию — рядом со скриптом):

| Переменная | По умолчанию |
|---|---|
| `OCCT_VERSION` | `8.0.1` (тег `V8.0.1`) |
| `OCCT_SOURCE` | `third_party/occt-<версия>`; если нет `CMakeLists.txt` — `git clone --depth 1` |
| `OCCT_BUILD` | `third_party/occt-build` (логи — `<OCCT_BUILD>/logs`) |
| `OCCT_INSTALL` | `third_party/occt-install` |
| `OCCT_JOBS` | `nproc` |

Раньше скрипт включал модули FoundationClasses, ModelingData, ModelingAlgorithms и
DataExchange целиком (48 тулкитов, 232 МБ).

Проверка (2026-11-07): сборка в отдельный каталог, не трогая основную установку,

```bash
OCCT_SOURCE=/home/user/bendcore/third_party/occt-8.0.1 \
OCCT_BUILD=/home/user/bendcore/third_party/occt-build-min \
OCCT_INSTALL=/home/user/bendcore/third_party/occt-install-min OCCT_JOBS=3 \
  third_party/build_occt.sh
```

— 5112 единиц трансляции, около 28 минут на 3 потоках, 1 предупреждение компилятора
(`-Warray-bounds` в заголовке libstdc++), ошибок нет. Установлено ровно 29 тулкитов
списка (OCCT ничего не добавил: TKService/TKV3d/TKVCAF уже в списке), 224 923 268 байт
`.a` (215 МБ против 232 МБ у прежней установки). Затем bendcore (`RelWithDebInfo`,
`-DBENDCORE_OCCT_DIR=…/occt-install-min`): все 397 шагов сборки без ошибок, `ctest -j3` —
101 тест, 100 проходят, `performance` падает под параллельной нагрузкой и проходит
отдельно (как и с прежней установкой, см. ниже). Среди прошедших — все OCCT-зависимые:
`occt_init`, `step_reader`, `import_workflow`, `e2e_import`, `tube_occt`,
`geometry_analyzer_occt`, `server`, `kernel_*` (включая `kernel_xcaf*`),
`corpus_step_committed`, `corpus_step_fresh`, `corpus_generate`.

Проверка замены (2026-11-06, `build-occt`, RelWithDebInfo): полная сборка всех целей
без ошибок; `ctest` — 84 теста, результат по каждому совпадает с исходным (до замены
`file(GLOB)`): 83 проходят, `performance` (`BenchmarkSuiteTest.ScaledTargets`,
OCCT не использует) падает при параллельном запуске `ctest -j` под нагрузкой и
проходит отдельно — и до, и после изменения.

## 3. WebAssembly

```bash
source /root/emsdk/emsdk_env.sh      # emsdk 6.0.12 (как в CI); скрипт делает это сам
OCCT_SOURCE=/home/user/bendcore/third_party/occt-8.0.1 \
OCCT_WASM_INSTALL=/home/user/bendcore/third_party/occt-install-wasm-checked \
OCCT_WASM_BUILD=/home/user/bendcore/third_party/occt-build-wasm-checked \
  third_party/build_occt_wasm.sh
```

По умолчанию пути — `third_party/occt-8.0.1`, `third_party/occt-build-wasm`,
`third_party/occt-install-wasm` рядом со скриптом (все под `.gitignore`).
`SKIP_OCCT_BUILD=1` — не пересобирать OCCT, только пробную программу.

Что делает скрипт:

1. `emcmake cmake` для исходников OCCT 8.0.1: `BUILD_LIBRARY_TYPE=Static`, все
   `BUILD_MODULE_*=OFF`, `BUILD_ADDITIONAL_TOOLKITS=<список из occt_toolkits.txt>` (OCCT
   сам добавляет зависимости из `EXTERNLIB`), `USE_TK/TCL/FREETYPE/FREEIMAGE/FFMPEG/
   OPENVR/RAPIDJSON/DRACO/TBB/VTK/EIGEN=OFF`, документация, тесты и Inspector выключены,
   `-fexceptions`. OpenGL для Emscripten OCCT выключает сам (`USE_OPENGL=OFF`,
   `USE_GLES2=ON` действует только при сборке TKOpenGles, которой нет в наборе).
2. Сборка и `cmake --install`; проверка, что каждый тулкит списка установлен; печать
   размеров `.a` с пометкой «listed» / «dependency added by OCCT».
3. Пробная программа `tests/kernel/wasm/occt_size_probe.cpp` линкуется только со
   списком тулкитов (`-O2` и `-Oz`, `-sENVIRONMENT=node`), запускается под `node`;
   скрипт завершается ошибкой, если программа не напечатала `OK`.

Пробная программа (без внешних файлов, всё в памяти): брусок 40×30×20 минус сквозное
отверстие R5 (`BRepPrimAPI`, `BRepAlgoAPI_Cut`) → STEP AP214 в `std::ostringstream`
(`STEPControl_Writer::WriteStream`) → чтение из буфера (`STEPControl_Reader::ReadStream`)
→ проверки против аналитики: `BRepCheck_Analyzer`, объём `40·30·20 − π·5²·20`
(относительно 1e-6), 7 граней → `BRepMesh_IncrementalMesh` (прогиб 0,1, угол 0,5) →
число треугольников и площадь сетки против аналитической площади поверхности
(допуск 0,1 %).

Модель исключений: `-fexceptions` (исключения Emscripten на JS), как в корневом
`CMakeLists.txt` для цели `bendcore_wasm`: OCCT сообщает об ошибках через
`Standard_Failure`, а смешивать `-fexceptions` и `-fwasm-exceptions` в одной линковке
нельзя. Переход на `-fwasm-exceptions` (меньше и быстрее) — отдельное решение для всего
WASM-кода bendcore.

### Результаты

Измерено 2026-11-06, emsdk 6.0.12, node v22.23.4, OCCT собран `Release`
(`-O3 -DNDEBUG` из тулчейна Emscripten, плюс `-DNo_Exception` от OCCT), `-fexceptions`.

Сборка OCCT: 5112 единиц трансляции, около 25 минут на 4 ядрах (ядра делились с
другими сборками). Установлено 29 тулкитов (26 из стартового списка + TKService,
TKV3d, TKVCAF, добавленные OCCT как зависимости TKXCAF), 188 МБ `.a`
(native-установка из 48 тулкитов — 232 МБ). Предупреждений компилятора — 3
(неиспользуемые глобальные переменные в TKBool), ошибок нет.

Вывод пробной программы под node (одинаков для `-O2` и `-Oz` и совпадает с native-сборкой
той же программы против `occt-install`):

```
OCCT 8.0.1
STEP bytes: 19030
volume: 22429.203673 (analytic 22429.203673)
faces: 7
triangles: 120
nodes: 130
mesh area: 5671.235556 (analytic 5671.238898)
OK
```

Размер `.wasm` (JS-обёртка ~160 КБ не включена):

| Программа | Линковка | `.wasm`, байт | gzip -9, байт |
|---|---|---|---|
| `occt_size_probe` (булева + STEP запись/чтение + BRepMesh) | `-O2` | 18 489 733 (17,6 МиБ) | 4 794 651 (4,6 МиБ) |
| `occt_size_probe` | `-Oz` | 15 879 908 (15,1 МиБ) | 4 627 505 (4,4 МиБ) |
| то же без STEP (только булева + BRepMesh), разовый замер | `-Oz` | 8 826 605 (8,4 МиБ) | 2 895 253 (2,8 МиБ) |

Разбор `-Oz`: секция кода 13,7 МБ, данных 2,1 МБ. STEP (TKDESTEP + TKXSBase) добавляет
около 7 МБ сырого и 1,7 МБ сжатого размера: протокол STEP регистрирует все типы
сущностей AP214/AP242, поэтому код читателя целиком достижим из `STEPControl_Reader`.
Исключения Emscripten на JS (`-fexceptions`) оборачивают вызовы в `invoke_*` и тоже
увеличивают код; оценка выигрыша от `-fwasm-exceptions` требует пересборки OCCT и
bendcore с этим флагом и не делалась.

Повторить: `third_party/build_occt_wasm.sh` печатает таблицу и пишет её в
`occt-build-wasm/probe/sizes.txt`.

## 4. Нужные изменения CI (зона инженера качества)

`.github/workflows/` в этой задаче не менялись. Нужно:

1. Job `occt` в `wasm.yml`: ничего не менять в сборке OCCT; конфигурация bendcore
   теперь сама проверяет наличие каждого тулкита из `third_party/occt_toolkits.txt`.
   Native-скрипт `build_occt.sh` теперь собирает только список: ключ кэша
   `occt-build-…` нужно сменить на зависящий от
   `hashFiles('third_party/occt_toolkits.txt', 'third_party/build_occt.sh')`, иначе
   останется старая установка из 48 тулкитов (она тоже работает, но больше).
2. Новый job (или шаг в WASM-job) «OCCT для WASM»: установить emsdk 6.0.12; кэш
   `third_party/occt-build-wasm` и `third_party/occt-install-wasm` с ключом от
   `hashFiles('third_party/occt_toolkits.txt', 'third_party/build_occt_wasm.sh')` и версии
   OCCT/emsdk; получить исходники OCCT 8.0.1 (тот же `git clone --depth 1 --branch
   V8.0.1`, что в `build_occt.sh`); запустить `third_party/build_occt_wasm.sh`
   (успех = код возврата 0: внутри проверяется вывод `OK` пробной программы под node);
   опубликовать `occt-build-wasm/probe/sizes.txt` в сводке job и как артефакт.
3. Порог размера: при желании — ошибка, если gzip `.wasm` пробной программы вырос
   больше чем на N % относительно значения в этом документе.
4. Время: полная сборка OCCT для WASM без кэша — около 25 минут на 4 ядрах (раздел 3);
   на 2-ядерном раннере GitHub ожидать около часа, `timeout-minutes: 120`.

## 5. Известные проблемы и ограничения

- **Основная установка `third_party/occt-install`** собрана прежним скриптом (48
  тулкитов). Она остаётся рабочей: линкуется только явный список. Пересборка
  `build_occt.sh` в тот же каталог даст установку из 29 тулкитов (лишние `.a` прежней
  установки при этом не удаляются — для чистой установки удалить каталог заранее).
- **TKXCAF тянет визуализацию** (TKVCAF, TKV3d, TKService) — свойство OCCT 8.0.1, см.
  раздел 1.
- **Размер пробной программы — нижняя граница.** `wasm-ld` удаляет неиспользуемый код,
  поэтому размер отражает только достижимое из пробной программы (STEP, булевы,
  BRepMesh). Скругления, смещения, HLR, IGES, XCAF добавят размер, когда появятся в
  WASM-биндингах ядра.
- Однопоточная сборка: без `-pthread` (иначе нужны заголовки COOP/COEP), параллельный
  режим `BRepMesh` и `BOPAlgo` в браузере не используется.
- Проверки OCCT в Release и стандарт C++ — раздел 6 (с 2026-11-12 обе установки собираются с
  проверками и как C++20).

## 6. Проверки OCCT в Release и стандарт C++ (2026-11-12)

### Проблема

Fuzz-проверка загрузчиков (`docs/qa/QA.md`, раздел 7) нашла 229 падений (SIGSEGV, SIGABRT,
> 8 ГиБ памяти) внутри читателей OCCT на искажённых файлах BRep, STEP и BRep, встроенном в
документ сборки: повреждённый файл пользователя ронял приложение.

### Что включает проверки в OCCT 8.0.1 (по исходникам)

- **`BUILD_RELEASE_DISABLE_EXCEPTIONS`** (`CMakeLists.txt`, по умолчанию `ON`;
  `adm/cmake/occt_defs_flags.cmake`) добавляет `-DNo_Exception` в `CMAKE_CXX_FLAGS_RELEASE`.
  Каждое исключение `Standard_*.hxx` определяет макрос `<Имя>_Raise_if(условие, текст)`, который
  при `No_Exception` (или `No_<Имя>`) пуст. В исходниках 8.0.1 — 1 517 вызовов `*_Raise_if` в
  498 файлах: `Standard_OutOfRange_Raise_if` 298 (индексы `NCollection_Array1/2`, `Sequence`,
  `IndexedMap::FindKey`, `TColStd`…), `StdFail_NotDone_Raise_if` 278, `Standard_NoSuchObject`
  149, `Standard_ProgramError` 106, `Standard_ConstructionError` 104 (нулевое направление
  `gp_Dir`), `Standard_DomainError` 93, `Standard_DimensionError` 65, `Standard_RangeError` 49,
  `Standard_NullObject` 42 и др. Флаг действует только на единицы трансляции OCCT; код bendcore,
  подключающий заголовки OCCT, `No_Exception` не определяет, и встроенные проверки заголовков в
  нём и раньше работали. Часть HLR (`HLRBRep_*`, `Intrv_Intervals`) определяет `No_Exception`
  в самом файле — там проверок нет при любом флаге.
- **`OCCT_DEBUG`** — только в конфигурации Debug и при `BUILD_WITH_DEBUG=ON`: включает
  отладочный вывод алгоритмов (`std::cout`), а не проверки; для Release неприменим.
- **`Standard_ASSERT_*`** (`Standard_Assert.hxx`) ведут себя одинаково в Release и Debug (в
  Debug добавляется только сообщение) — от флагов не зависят.
- **`OCC_CONVERT_SIGNALS`** OCCT определяет сам для GCC/Clang на POSIX: точки
  `OCC_CATCH_SIGNALS` (301 в `.cxx`, в т. ч. `STEPControl_ActorRead`, `StepToGeom`,
  `StepToTopoDS_*`, `BinTools_ShapeReader`, `GeomTools_CurveSet::ReadCurve`) превращают сигнал
  в исключение, если установлен обработчик сигналов (`OSD::SetSignal`). bendcore ставит свой,
  ограниченный вызовами чтения (`docs/kernel/KERNEL_CORE.md`, «Повреждённые файлы»).
- **`BUILD_CPP_STANDARD`** (по умолчанию `C++17`). Читатели BRep (`BinTools_*Set`,
  `TopTools_*Set`, `BRepTools_ShapeSet` — 15 мест) читают ключевые слова разделов
  `IS >> buffer` в `char buffer[255]`. В C++17 это `operator>>(istream&, char*)` без предела
  (слово длиннее 254 байт — переполнение стека, `stack smashing detected`, 5 падений
  `brep_binary`, неустранимых защитой ядра); в C++20 — `operator>>(istream&, char(&)[N])`
  (P0487), чтение ограничено размером массива, длинное слово просто не совпадает с ключевым и
  OCCT бросает `Standard_Failure`. OCCT 8.0.1 собирается в C++20 без изменений (1 предупреждение
  компилятора, как и в C++17).

### Решение

`third_party/build_occt.sh` и `third_party/build_occt_wasm.sh`:

| Переменная | По умолчанию | Передаётся в OCCT |
|---|---|---|
| `OCCT_RELEASE_CHECKS` | `1` (`0` — прежнее поведение OCCT) | `BUILD_RELEASE_DISABLE_EXCEPTIONS=OFF` (`ON` при 0) |
| `OCCT_CXX_STANDARD` | `C++20` (`C++17`, `C++23`) | `BUILD_CPP_STANDARD` |

После конфигурации скрипт проверяет `build.ninja`: при `OCCT_RELEASE_CHECKS=1` там не должно
быть `-DNo_Exception` (кэшированный параметр мог остаться от прежней конфигурации), при `0` —
должен быть. В установку пишется `bendcore_occt_build.txt`
(`OCCT 8.0.1 release_checks=1 cxx=C++20`).

Корневой `CMakeLists.txt`:

- `BENDCORE_OCCT_DIR` по умолчанию — `third_party/occt-install-checked` (Emscripten —
  `third_party/occt-install-wasm-checked`), если каталог существует;
- `bendcore_check_occt_release_checks()` читает установленный
  `lib/cmake/opencascade/OpenCASCADECompileDefinitionsAndFlags-release.cmake` OCCT (там записаны
  флаги Release) и при `No_Exception` останавливает конфигурацию с объяснением; принять такую
  установку можно явно: `-DBENDCORE_OCCT_REQUIRE_CHECKS=OFF` (тогда предупреждение).

CI: `qa.yml` (`occt-native`, `fuzz`) и `wasm.yml` собирают и кэшируют
`third_party/occt-install-checked` / `occt-install-wasm-checked`; переменная
`OCCT_RELEASE_CHECKS: "1"` входит в ключ кэша (`occt-native-checks1-V8.0.1-…`,
`occt-wasm-checks1-V8.0.1-…`) вместе с хэшем скриптов, так что прежняя установка без проверок
из кэша не берётся. `docker/Dockerfile` — та же установка.

Установки на машине разработки (2026-11-12): `/home/user/bendcore/third_party/occt-install-checked`
и `/home/user/bendcore/third_party/occt-install-wasm-checked`; прежние `occt-install` и
`occt-install-wasm` (без проверок, C++17) оставлены для сравнения.

### Цена

Машина: 4 vCPU, 15 ГиБ, gcc 13.3, emsdk 6.0.12; одна и та же ревизия bendcore
(`build-occt` RelWithDebInfo) против двух установок OCCT, нагрузочные тесты по одному (`-j1`),
без параллельной сборки. Подробно — `docs/qa/PERFORMANCE.md`, раздел 2.

| Замер | Без проверок, C++17 | С проверками, C++20 | Разница |
|---|---:|---:|---:|
| `part_300`: 300 операций с нуля / после правки эскиза, с | 12,99 / 12,94 (12,84) | 13,27 / 12,96 (12,55) | 0…+2 % (в шуме) |
| `assembly_300`: пересечения 299 кандидатов, с | 6,50 | 6,70 | +3 % |
| `step_large`: импорт 65 МиБ / тесселяция, с | 20,32 / 2,31 | 21,23 / 2,31 | +4,5 % / 0 |
| `hlr_1000`: изометрия, с (три прогона) | 6,12; 6,32; 6,19 | 7,13; 6,88; 6,85 | +10…+16 % |
| `hlr_1000`: точный вид сверху, с | 0,086 | 0,098 | +0,01 с |
| Сборка OCCT native (`-j2`) | — | около 75 минут, 5 112 единиц | — |
| `libTK*.a` native, 29 тулкитов | 224 923 268 байт | 261 338 332 байт | +16 % |
| `libTK*.a` WASM, 29 тулкитов | 187 936 950 байт | 208 051 348 байт | +11 % |
| Пробная программа WASM (`occt_size_probe`), `-Oz`: `.wasm` / gzip -9 | 15 879 908 / 4 627 505 | 17 113 804 / 4 784 616 | +7,8 % / +3,4 % |
| то же, `-O2` | 18 489 733 / 4 794 651 | 20 011 145 / 5 001 162 | +8,2 % / +4,3 % |

Вывод пробной программы под node (объём, 7 граней, 120 треугольников, площадь сетки) с
проверками тот же, что без них. Размеры модулей bendcore с новой установкой —
`docs/qa/PERFORMANCE.md`, раздел 5.

Решение: проверки включены по умолчанию в обеих установках. Цена — единицы процентов на
булевых операциях и импорте STEP, около 10 % на изометрическом HLR и +3–4 % сжатого `.wasm`;
взамен индексы вне диапазона в читателях становятся `Standard_OutOfRange` → `ParseError`, а не
чтением чужой памяти. Без проверок (`OCCT_RELEASE_CHECKS=0`) нельзя доверять чтению файлов из
внешних источников.

### Результат fuzz-проверки

| Конфигурация | OCCT без проверок, C++17 | + проверки | + защита ядра от сбоев | + C++20 и предварительная проверка STEP |
|---|---:|---:|---:|---:|
| native (gcc) | 229 | 59 (step 46, brep_text 7, brep_binary 6) | 8 (brep_binary 5 — стек, step 3 — память) | **0** |
| asan (clang, ASan/UBSan) | 229 | — | — | **1** |

Промежуточные столбцы измерены в native: «+ проверки» — та же ревизия с отключённой защитой ядра
(`runConvertingFaults` как простой вызов), «+ защита» — с ней, OCCT ещё C++17. Оставшийся
дефект ASan и его причина — `tests/regression/fuzz/known_defects.txt` и `docs/qa/QA.md`,
раздел 7.
