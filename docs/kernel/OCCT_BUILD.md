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
# OCCT (один раз; 29 тулкитов из occt_toolkits.txt, 215 МБ статических библиотек)
third_party/build_occt.sh            # -> third_party/occt-install

cmake -S . -B build-occt -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBENDCORE_WITH_OCCT=ON -DBENDCORE_OCCT_DIR=$PWD/third_party/occt-install
cmake --build build-occt
ctest --test-dir build-occt --output-on-failure
```

В рабочих копиях (git worktree) `BENDCORE_OCCT_DIR` — абсолютный путь к основной
установке `/home/user/bendcore/third_party/occt-install`.

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
OCCT_WASM_INSTALL=/home/user/bendcore/third_party/occt-install-wasm \
OCCT_WASM_BUILD=/home/user/bendcore/third_party/occt-build-wasm \
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
- `BUILD_RELEASE_DISABLE_EXCEPTIONS` оставлен по умолчанию OCCT (ON), как и в native:
  в Release часть проверок аргументов OCCT (`Standard_OutOfRange` и т. п.) не
  компилируется; ошибки алгоритмов (`Standard_Failure` из `StdFail_NotDone` и др.)
  бросаются как обычно.
