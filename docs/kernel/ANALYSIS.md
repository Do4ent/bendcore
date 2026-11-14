# Ядро: материалы, массовые характеристики, экспорт сеток, измерения, сечение (`bendcore_kernel_analysis`)

Фаза 2, пп. 4, 6 (STL/3MF), 7 `docs/ROADMAP_GENERAL_CAD.md`. Библиотека
`bendcore_kernel_analysis` (STATIC), исходники `src/kernel/analysis/`, заголовки
`include/bendcore/kernel/{material,mass_properties,mesh_export,measure,section}.hpp`,
пространство имён `bendcore::kernel`, сборка — `cmake/kernel_analysis.cmake` (подключается
при `BENDCORE_WITH_OCCT=ON` после `kernel_core.cmake`). Зависимости: `bendcore_kernel`,
OCCT 8.0.1, zlib — только в native-сборке (deflate записей 3MF; в WASM записи хранятся без
сжатия, раздел «ZIP и zlib»), `third_party/nlohmann/json.hpp` (JSON материалов). Тест 3MF
использует системный expat как независимый разборщик XML.

Все функции возвращают `Result<T>`; исключения OCCT (`Standard_Failure`) перехватываются на
границе модуля и превращаются в `ErrorCode::OperationFailed` с текстом исключения.

Единицы: длины мм, плотность кг/м³, масса кг, моменты инерции кг·мм², модули и прочность
МПа, теплопроводность Вт/(м·К).

Существующие `src/materials/` и `include/bendcore/materials/` (библиотека раннего этапа
проекта для листового металла) не используются: у значений нет источников, есть оценочные
«рейтинги» и углеродный след без ссылок, и библиотека не связана с ядром OCCT.

## 1. Материалы (`material.hpp`)

```cpp
MaterialLibrary lib = MaterialLibrary::withBuiltIns();
const Material* s235 = lib.find("steel_s235");      // density 7850, E 210000, nu 0.3, ReH 235
lib.add(userMaterial);                               // проверка диапазонов, уникальность id
std::string json = lib.userMaterialsToJson();        // только пользовательские
other.addUserMaterialsFromJson(json);                // всё или ничего
```

`Material`: `id`, `name`, `category` (`steel`, `stainless_steel`, `aluminum`, `titanium`,
`plastic`, `other`), `density` (обязательна), `youngModulus`, `poissonRatio`,
`yieldStrength`, `thermalConductivity` (все `std::optional` — пусто, если источник не даёт
значения с уверенностью), `color` (RGB для отображения), `source`, `verified`, `builtIn`.

Проверки (`validateMaterial`): непустые id и имя; 0 < ρ ≤ 30000; E > 0; 0 < ν < 0,5;
предел текучести > 0; теплопроводность > 0. Встроенные материалы нельзя изменить или удалить.

JSON: `{"format":"bendcore-materials","version":1,"materials":[{"id","name","category",
"density_kg_m3","young_modulus_mpa","poisson_ratio","yield_strength_mpa",
"thermal_conductivity_w_mk","color":"#RRGGBB","source","verified"}]}`. Отсутствующее
необязательное свойство не пишется. Ошибки: `ParseError` (синтаксис, типы, неизвестная
категория, цвет), `InvalidInput` (диапазоны, повтор id), `Unsupported` (другая версия).

### Встроенные марки и источники

Все значения записаны по памяти из указанных источников; ни одно не сверено с печатным
изданием внутри проекта, поэтому у всех `verified = false` и все марки внесены в
`docs/STANDARDS_VERIFICATION.md` (требуют сверки). Цвета — условные цвета отображения.

| id | ρ, кг/м³ | E, МПа | ν | σт (Rp0,2), МПа | λ, Вт/(м·К) | Источник |
|---|---|---|---|---|---|---|
| steel_s235 | 7850 | 210000 | 0,3 | 235 | 53,3 | EN 1993-1-1:2005 п. 3.2.6 (E, ν, ρ); EN 10025-2:2004 (ReH, t ≤ 16 мм); EN 1993-1-2:2005 п. 3.4.1.3 (λ при 20 °C) |
| steel_s355 | 7850 | 210000 | 0,3 | 355 | 53,3 | то же |
| steel_c245 | 7850 | 206000 | 0,3 | 245 | — | СП 16.13330.2017 (E, ν, ρ); ГОСТ 27772-2015 (Ryn, t 2–20 мм) |
| steel_45 | 7826 | 200000 | — | 355 | — | ГОСТ 1050-2013 (σт, нормализация); «Марочник сталей и сплавов» под ред. Зубченко (ρ, E) |
| stainless_12x18n10t | 7920 | — | — | 196 | — | Марочник (ρ); ГОСТ 5949 (σ0,2, сортовой прокат) |
| stainless_1_4301 | 7900 | 200000 | 0,3 | 210 | 15 | EN 10088-1:2014 (ρ, E, λ); EN 10088-2:2014 (Rp0,2, горячекатаный лист P); EN 1993-1-4:2006 (ν) |
| stainless_1_4404 | 8000 | 200000 | 0,3 | 220 | 15 | то же |
| al_6082_t6 | 2700 | 70000 | 0,3 | 260 | — | EN 1999-1-1:2007 п. 3.2.5 (E, ν), табл. 3.2a (f0); ASM Handbook vol. 2 (ρ) |
| al_5083_o | 2660 | 70000 | 0,3 | 125 | — | то же |
| al_6061_t6 | 2700 | 68900 | 0,33 | 276 | 167 | ASM Handbook vol. 2 (1990), типичные значения |
| al_2024_t3 | 2780 | 73100 | 0,33 | 345 | 121 | то же (аналог Д16Т) |
| al_7075_t6 | 2810 | 71700 | 0,33 | 503 | 130 | то же |
| al_amg6 | 2640 | — | — | — | — | ГОСТ 4784-2019 (марка); справочник по алюминиевым сплавам (ρ) |
| ti_6al4v | 4430 | 113800 | 0,342 | 880 | 6,7 | ASM Handbook vol. 2, отожжённый Ti-6Al-4V |
| plastic_abs | 1040 | 2300 | — | 40 | 0,17 | типичные значения ненаполненных марок (паспорта производителей различаются) |
| plastic_pa6 | 1130 | 3000 | — | — | 0,24 | то же (сухой) |
| plastic_pom_h | 1420 | 3100 | — | — | 0,31 | то же |
| plastic_pc | 1200 | 2350 | 0,37 | 65 | 0,20 | то же |
| plastic_pmma | 1190 | 3200 | 0,37 | — | 0,19 | то же |
| plastic_ptfe | 2170 | — | — | — | 0,25 | то же |
| plastic_pp | 905 | — | — | — | — | то же (гомополимер) |

Наиболее уверенные значения — константы проектирования EN 1993-1-1, EN 1993-1-4,
EN 1999-1-1, СП 16.13330 и минимальные пределы текучести по стандартам на прокат. Для
пластмасс значения зависят от марки производителя: для точной массы пользователь задаёт
плотность из паспорта своего материала.

## 2. Массовые характеристики (`mass_properties.hpp`)

```cpp
auto p = computeMassProperties(shape, 7850.0);       // или (shape, material)
// p.value(): mass, volume, area, centerOfMass, inertiaAtCenter, principal.moments/axes
auto multi = computeMassProperties({{steelBody, 7850}, {alBody, 2700}});
Matrix3 iO = inertiaAboutFrame(p.value(), gp_Ax3(origin, zDir, xDir));
auto total = combine({{partProps, placement1}, {partProps, placement2}});
```

Алгоритм. Для каждого тела (`TopExp_Explorer` по `TopAbs_SOLID`)
`BRepGProp::VolumeProperties(solid, props, OnlyClosed=true)` даёт объём, центр масс и
матрицу инерции относительно центра масс в осях, параллельных глобальным;
`BRepGProp::SurfaceProperties` — площадь. Масса = V·ρ·10⁻⁹, тензор умножается на ρ·10⁻⁹.
Тела, многотельная деталь и вхождения сборки складываются функцией `combine`: центр масс —
средневзвешенный, тензор — сумма собственных тензоров плюс перенос по Штейнеру
`m(|d|²E − d·dᵀ)`. Вхождение сборки: центр масс преобразуется `gp_Trsf`, тензор
поворачивается `R·I·Rᵀ` — без повторного интегрирования. Тензор относительно СК: перенос в
начало СК, затем компоненты в осях СК. Главные моменты и оси — `math_Jacobi`, моменты по
возрастанию, оси — правая тройка.

Соглашение о тензоре: Ixx = ∫(y²+z²)dm, Ixy = −∫xy dm (как в OCCT; закреплено тестом
повёрнутого бруса и тензором относительно угла бруса).

Допуски: по умолчанию гауссово интегрирование BRepGProp — для плоскостей и квадрик
совпадение с формулами ~10⁻⁹ относительно. Для B-сплайновых тел — `relativeTolerance > 0`
(адаптивное интегрирование с этой оценкой ошибки). Требование дорожной карты «масса в
пределах 0,1 %» проверяется явно в каждом тесте.

Ограничения: пересекающиеся тела в одном составном теле считаются дважды (сначала
объединить, `boolean.hpp`); тело с неположительным объёмом (вывернутое, незамкнутое) —
`InvalidShape`; масштабирующее размещение — `InvalidInput` (зеркальное допускается).

## 3. Экспорт STL и 3MF (`mesh_export.hpp`)

```cpp
MeshExportParams prm{.linearDeflection = 0.05, .angularDeflection = 0.35};
auto stl = exportStlBinary(shape, prm);              // std::string с байтами файла
auto txt = exportStlAscii(shape, prm, "part");
auto pkg = export3mf({{"Body", body, Rgb{200, 30, 40}}, {"Lid", lid, std::nullopt}}, prm);
```

Сетка: `kernel::tessellate` (BRepMesh_IncrementalMesh, треугольники против часовой стрелки
снаружи), затем сварка совпадающих вершин (пространственный хеш, допуск 10⁻⁶ мм) и удаление
выродившихся треугольников (полюса сфер, швы). Так замкнутое тело даёт замкнутую сетку.

- STL двоичный: заголовок 80 байт (не начинается с `solid`), число треугольников, по 50 байт
  на треугольник; нормаль вычисляется по треугольнику. ASCII: `solid/facet/outer loop`,
  числа `%.9g`.
- 3MF: ZIP с `[Content_Types].xml`, `_rels/.rels`, `3D/3dmodel.model`; `unit="millimeter"`,
  по одному `<object type="model">` и `<item>` на входной объект; цвета — `<basematerials>`
  с `displaycolor="#RRGGBBFF"` и `pid/pindex` объекта. Записи ZIP — `zipDefaultMethod()`:
  deflate в native-сборке, без сжатия (stored) в WASM; CRC-32 — собственная реализация
  (раздел «ZIP и zlib»); дата DOS 1980-01-01, чтобы одинаковая модель давала одинаковые байты.
- `readStl` (двоичный/ASCII), `meshVolume`, `checkClosure`, `readZip` (stored/deflate, без
  ZIP64 и шифрования, CRC проверяется; deflate без zlib — `Unsupported`) — для проверки
  результата.

### ZIP и zlib (`zip_archive.cpp`)

```cpp
std::uint32_t c = crc32("123456789");                     // 0xCBF43926
auto zip = writeZip({{"a.txt", "abc"}}, ZipMethod::Stored);
bool deflate = zipDeflateAvailable();                      // true в native, false в WASM
```

Порт zlib для Emscripten (`-sUSE_ZLIB=1`) скачивает исходники zlib 1.3.2 из репозитория
madler/zlib на GitHub при сборке; из рабочего окружения прокси на этот адрес отвечает 403
(одна попытка 2026-11-14: `curl` архива тега `v1.3.2` → `403`, состояние прокси: «gateway
answered 403 to CONNECT»); системного zlib при кросс-компиляции нет. Поэтому:

- **CRC-32** считается собственной табличной реализацией во всех сборках: отражённый полином
  0xEDB88320, начальное значение и финальный XOR 0xFFFFFFFF (ISO 3309 / ITU-T V.42 — тот же
  CRC, что в ZIP и zlib). Таблица на 256 элементов строится `constexpr`. Тест сверяет с
  опубликованным контрольным значением CRC-32/ISO-HDLC (`"123456789"` → `0xCBF43926`), с
  известными значениями других строк и в native-сборке — с `crc32` из zlib на случайных данных.
- **Сжатие** — за интерфейсом `ZipMethod`: в native-сборке (`BENDCORE_ANALYSIS_HAS_ZLIB=1`)
  `writeZip(…, Deflate)` и чтение deflate-записей используют zlib (raw deflate, окно 15);
  без zlib `export3mf` пишет записи stored (метод 0, «version needed» 1.0), `writeZip(…,
  Deflate)` и чтение deflate-записи возвращают `Unsupported` с причиной. Stored-архив —
  обычный ZIP: его читают `unzip`, Python `zipfile`, программы подготовки печати; файл больше
  deflate-варианта в несколько раз (XML сетки сжимается хорошо).
- Если zlib для WASM станет доступен (`ZLIB_ROOT` или кэш порта Emscripten), CMake найдёт его
  (`find_package(ZLIB)`) и модуль соберётся с deflate без изменений кода.

Точность: сетка вписана в поверхность с прогибом ≤ `linearDeflection`, поэтому для
выпуклых тел 0 ≤ V − V_сетки ≤ S·δ (S — площадь поверхности). Координаты STL хранятся во
float (~10⁻⁷ относительно).

## 4. Измерения (`measure.hpp`)

| Функция | Что | Точки для отображения |
|---|---|---|
| `minimumDistance(a, b)` | `BRepExtrema_DistShapeShape` для любых элементов | ближайшие точки, число решений |
| `angleBetweenFaces(f1, f2)` | угол между внешними нормалями плоских граней, 0…180° | середина общего ребра или середина между центрами, центры граней |
| `angleBetweenEdges(e1, e2)` | прямые рёбра: при общей вершине — угол лучей из неё, иначе угол направлений рёбер | вершина/середина, точки на рёбрах |
| `edgeLength(e)` | `GCPnts_AbscissaPoint::Length` | начало, конец, середина по длине |
| `faceArea(f)` | `BRepGProp::SurfaceProperties` | центр тяжести грани |
| `radiusOf(e или f)` | окружность/дуга, цилиндр, сфера, конус (радиус в опорной плоскости), тор (малый радиус) | центр, ось, точка на элементе |
| `vertexCoordinates(v)` | координаты вершины | — |

Ограничения: углы — только для плоских граней и прямых рёбер (`Unsupported` иначе). Точка
внутри тела имеет расстояние 0. Около минимума расстояние квадратично по положению точек,
поэтому на кривых поверхностях BRepExtrema находит ближайшие точки с погрешностью ~10⁻³ мм
вдоль поверхности (само расстояние — ~10⁻⁷ мм); точки при этом лежат на поверхностях.

## 5. Сечение плоскостью (`section.hpp`)

```cpp
SectionPlane pl{{0, 0, 12}, {0, 0, 1}};
auto clipped = clipByPlane(shape, pl);               // оставлено (p − o)·n ≤ 0
auto s = sectionByPlane(shape, pl);                  // edges, wires, faces, area, polylines
```

- `clipByPlane`: `BRepAlgoAPI_Cut` с параллелепипедом, одна грань которого лежит в плоскости;
  размер покрывает габарит формы (2 диагонали + расстояние от начала плоскости). Полностью
  удалённая форма — пустой compound.
- `sectionByPlane`: контур — `BRepAlgoAPI_Section(shape, gp_Pln)` с аппроксимацией; провода —
  `BOPAlgo_Tools::EdgesToWires`; грани сечения — `BRepAlgoAPI_Common` тел формы с
  покрывающей плоской гранью (даёт области с отверстиями); площадь — сумма
  `BRepGProp::SurfaceProperties` граней; полилинии — `GCPnts_QuasiUniformDeflection`.

Ограничения: грани и площадь строятся только для тел; для оболочек и граней возвращаются
рёбра и провода.

## 6. Как тестировать

```bash
cmake --build build-occt --target test_kernel_materials test_kernel_mass_properties \
  test_kernel_mesh_export test_kernel_zip_archive test_kernel_measure test_kernel_section
ctest --test-dir build-occt -R 'kernel_(materials|mass_properties|mesh_export|zip_archive|measure|section)' --output-on-failure
```

- `test_kernel_materials`: инварианты таблицы, опорные значения EN 1993-1-1/EN 1999-1-1,
  пользовательские материалы, JSON туда и обратно, ошибки.
- `test_kernel_mass_properties`: брус (в т. ч. тензор относительно угла с произведениями
  инерции −mab/4), повёрнутый брус (R·I·Rᵀ), цилиндр, полый цилиндр, сфера, два тела разных
  материалов, составное тело, сборка из двух смещённых и повёрнутых вхождений (Штейнер и
  сверка с BRepGProp по перемещённым телам), тензор в повёрнутой СК, ошибки.
- `test_kernel_mesh_export`: STL читается своим парсером, `RWStl` и `StlAPI_Reader`; объём
  сетки в границах прогиба для бруса, цилиндра, сферы, полого цилиндра; замкнутость сеток;
  3MF распаковывается (CRC), три XML разбираются expat, единицы, цвета, объекты, замкнутость
  и объём каждой сетки; порча байта обнаруживается. Пакет дополнительно проверен вручную
  `unzip -t` и Python `zipfile` + `xml.dom`.
- `test_kernel_zip_archive`: CRC-32 против опубликованных значений и zlib, заголовки
  stored-архива, разобранные вручную (сигнатура, метод, размеры, CRC), круговой проход,
  обнаружение порчи байта, deflate (native) или `Unsupported` (без zlib).
- `test_kernel_measure`, `test_kernel_section`: расстояния, углы, длины, площади, радиусы,
  площади сечений (брус, наклонная плоскость, цилиндр вдоль и поперёк оси, кольцо полого
  цилиндра, круг сферы) и объёмы отсечённых частей против формул.

## WASM

Модуль входит в WASM-сборку без zlib (раздел «ZIP и zlib»): его линкуют `bendcore_kernel_wasm`,
`bendcore_modeling_wasm` и `bendcore_assembly_wasm` для экспорта STL/3MF и массовых
характеристик в интерфейсе (`docs/ui/DATA_EXCHANGE.md`). CMake сообщает `zlib not found for
Emscripten - bendcore_kernel_analysis writes stored ZIP entries`; JS-модуль ядра отдаёт
`exchangeInfo().zipMethod === 'stored'`. Проверка в node — `tests/wasm/exchange.test.mjs` (ZIP
разбирается тестом, CRC каждой записи — через `zlib.crc32` node, XML разбирается, сетки
замкнуты; дополнительно Python `zipfile.testzip()` и `xml.dom.minidom`, если есть python3).
