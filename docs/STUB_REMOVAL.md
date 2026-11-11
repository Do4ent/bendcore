# Удаление заглушек геометрии и экспорта чертежей (фаза 0, п. 3)

Основание: `docs/ROADMAP_GENERAL_CAD.md` (раздел 1, фаза 0 п. 3), правило «никаких
заглушек» `docs/PROJECT_PROCESS.md` (раздел 2). Номера строк — по коммиту `a0eb74a`
(состояние до удаления).

## 1. Что удалено

| Файл / функция | Доказательство (файл:строка до удаления) | Что удалено | Чем будет заменено |
|---|---|---|---|
| `BRepBuilder::create_cylinder/create_sphere/create_cone` | `src/geometry/brep_builder.cpp:347-360` — `// Simplified: create a box-like approximation`, вызов `create_box` | Весь класс `BRepBuilder` и структуры `Vertex/Edge/Face/Shell/Solid` (`src/geometry/brep_builder.cpp`, `include/bendcore/geometry/brep_builder.hpp`) | Абстракция `Shape` поверх `TopoDS_Shape` — фаза 0, п. 4; примитивы и операции — фаза 1 |
| `BRepBuilder::calculate_face_area / calculate_solid_volume` | `brep_builder.cpp:415` и `:420` — `return 1.0;  // Placeholder` | то же | `BRepGProp` — фаза 2, п. 4 |
| `BRepBuilder::boolean_union/difference/intersection` | `brep_builder.cpp:248-253` — `// In a real implementation, this would merge the B-Rep representations`; результат — копия `outer_shell` первого тела, объём = сумма / разность / минимум (`:263`, `:282`, `:301`) | то же | Булевы операции OCCT с лечением — фаза 0, п. 2; многотельная деталь — фаза 2, п. 3 |
| `BRepBuilder::create_cavity / create_hole` | `brep_builder.cpp:367-368` — `// In a real implementation…`, `return true`; `:378` — `add_inner_shell(solid, 0);  // Simplified` | то же | Отверстие — фаза 1, п. 3 |
| `Solid::euler_characteristic`, `BRepBuilder::create_shell` | `brep_builder.cpp:44-48` — всегда `return 2`; `:151` — `is_solid_closed(id)` получает id оболочки вместо id тела | то же | Проверка `BRepCheck_Analyzer` — фаза 0, п. 2 |
| `FeatureOperations::fillet / chamfer / shell` | `src/geometry/feature_operations.cpp:286-306`, `:310-330`, `:334-349` — после проверки параметров `return solid;` (вход без изменений) | Весь класс `FeatureOperations` (`src/geometry/feature_operations.cpp`, `include/bendcore/geometry/feature_operations.hpp`) | Скругление, фаска — фаза 1, п. 3; оболочка — фаза 2, п. 1 |
| `FeatureOperations::mirror / pattern` | `feature_operations.cpp:367-378` — `return solid;`; `:382-404` — массив из одного и того же id | то же | Фаза 1, п. 3 |
| `FeatureOperations::extrude / revolve / sweep / loft` | `feature_operations.cpp:97` — оболочка из одной нижней грани; `:125-132` — вращение = loft из двух одинаковых профилей, угол и ось не используются; `:141-142` `rotate_profile` возвращает вход; `:161-168` sweep не использует путь | то же | Выдавливание, вращение — фаза 1, п. 3; кинематика, по сечениям — фаза 2, п. 1 |
| `TopologicalReferenceManager`, `TopologicalSignature` | `src/topology/topological_reference.cpp:174-181` — `resolve_reference` возвращает сохранённый id без проверки, поэтому `update_references_after_rebuild` (`:355-368`) ничего не меняет; `:533-535` — оценка совпадения сравнивает сигнатуру саму с собой; `:539` — пустое тело, `// В реальной реализации записать в журнал`; сигнатура грани по площади `1.0` из `BRepBuilder` (`:55`, `:509-516`) | `src/topology/topological_reference.cpp`, `include/bendcore/topology/topological_reference.hpp` (единственный потребитель — удалённый тест) | Топологическое именование v1 — фаза 1, п. 4 |
| `exportToPDF/DWG/DXF/SVG/STEP/PNG/JPEG` (`ExportResult`) | `src/drawing/drawing_export.cpp:133, 153, 164, 175, 187, 198, 209` — `result.success = true;  // Placeholder implementation`, файл не пишется | Функции и их объявления в `include/bendcore/drawing/drawing_export.hpp` | Векторный PDF, DXF, SVG — фаза 4, п. 8 |
| `getSupportedFormats` | `drawing_export.cpp:312-322` — перечисляет 7 форматов, ни один не работает | Функция и объявление | Список появится вместе с реальными форматами — фаза 4, п. 8 |
| `exportDrawing` (`ExportResult`) | `drawing_export.cpp:37-58` — маршрутизация в функции выше | Маршрутизация; функция оставлена и возвращает явную ошибку (раздел 2) | Фаза 4, п. 8 |
| `exportToDWG/PDF/PNG/JPEG` (`bool`) | `src/drawing/drawing_extraction.cpp:481, 528, 585, 596` — `// This is a placeholder`, `return true` | Функции и объявления в `include/bendcore/drawing/drawing_extraction.hpp` | Фаза 4, п. 8 |
| `exportToDXF / exportToSVG` (`bool`) | `drawing_extraction.cpp:488-523`, `:534-576` — текст собирается в `std::ostringstream` и выбрасывается, `file_path` не используется, `return true` | то же | Фаза 4, п. 8 |
| `exportDrawing / exportView` (`bool`) | `drawing_extraction.cpp:418-477` — вызывают только функции выше | то же | Фаза 4, п. 8 |
| `extractViewsFromAssembly`, `generateCustomView`, `createDrawingSheet` и класс-заглушка `Assembly` | `include/bendcore/drawing/drawing_extraction.hpp:248-253` — `// These would be actual BendCore types - for now using placeholders`, пустой класс; `drawing_extraction.cpp:148-328` — параметр `assembly` не читается, виды возвращаются без геометрии; `createDrawingSheet` не раскладывает виды по проекционной связи, как обещано в заголовке | Функции, объявления, класс `Assembly` | Виды и HLR на OCCT — фаза 4, п. 2 |

Уточнение к аудиту: строки `drawing_extraction.cpp:481-596` относятся к экспорту
файлов, а не к проекции видов. Проекции в файле не было вообще: «извлечение видов»
(`:148-328`) задаёт только матрицы ориентации пустых видов.

Оставлено как рабочий код: модель 2D-вида и листа (`DrawingView`, `DrawingSheet`),
рамка, габарит, простановка габаритных размеров, раскладка и масштаб видов,
преобразования типов и форматов листов (`drawing_extraction.cpp`); проверка параметров
экспорта, имена файлов, расширения, резервная копия, признаки вектор/растр
(`drawing_export.cpp`); перечисление `ExportFormat`.

Зависимые файлы, из которых убраны ссылки на удалённое: `CMakeLists.txt` (источники
`bendcore_core`, цели `test_brep_builder`, `test_m2_feature_operations` и их строки в
`BENDCORE_UNVERIFIED_TARGETS`), `src/testing/test_suite.cpp`, `src/testing/launch_package.cpp`
(описание API `BRepBuilder`). `src/wasm/bindings.cpp`, `src/cli`, `src/server`, `src/api`
удалённые символы не используют.

## 2. Где теперь возвращается явная ошибка

Механизм модуля — `ExportResult{success = false, error_message}`.

| Место | Поведение |
|---|---|
| `BendCore::Drawing::exportDrawing(const DrawingSheet&, const ExportOptions&)`, `src/drawing/drawing_export.cpp` | Сначала проверка параметров (прежние сообщения), затем для любого формата: `"Drawing export to <ФОРМАТ> is not supported until roadmap phase 4 (drawings)"` |
| `exportView`, `batchExportDrawings` (`drawing_export.cpp`) | Передают ошибку `exportDrawing` (`batchExportDrawings`: `failed_exports` = число листов, `success = false`) |
| `publishToLocal`, `publishToNetwork`, `publishViaEmail`, `archiveDrawings`, `publishToCloud`, `publishToMultiple`, `batchExportMultipleFormats` (`src/drawing/publishing_manager.cpp`) | Не изменялись; получают ошибку экспорта: `success = false`. `publishToLocal`/`publishViaEmail` переносят `error_message`; `publishToCloud` и `archiveDrawings` возвращают `success = false` без текста (так было устроено и раньше) |

## 3. Удалённые тесты

| Тест | Причина |
|---|---|
| `tests/geometry/test_brep_builder.cpp` (цель `test_brep_builder`, была в `BENDCORE_UNVERIFIED_TARGETS`) | Проверял заглушку `BRepBuilder` (в т. ч. `assert(result \|\| !result)`) |
| `tests/test_m2_feature_operations.cpp` (цель `test_m2_feature_operations`, была в `BENDCORE_UNVERIFIED_TARGETS`) | Проверял `FeatureOperations` и топологические ссылки; тесты GAP-007 — пустые («skipped for now») |
| Блоки `BRepBuilder` в `tests/integration/test_phase_d_integration.cpp` (тесты 2, 3, 9, 10), `tests/testing/test_integration_e2e.cpp` (тесты 4, 5), `tests/testing/test_multi_phase_integration.cpp`; `#include` в `tests/integration/test_regression_suite.cpp` | Цели непроверенные (не собираются); проверяли только заглушку |
| `test_drawing_export_m4`: `ExportToPDF`, `ExportToDXF`, `ExportToDWG`, `ExportToSVG`, `ExportToPNG`, `ExportToJPEG`, `BatchExportMultipleSheets`, `GetSupportedFormats`, `ExportSingleView`, `QualitySettingsDraft`, `QualitySettingsMaximum`, `ColorSettingsFullColor`, `ColorSettingsGrayscale`, `ColorSettingsMonochrome`, `PDFWithMetadata`, `PDFWithWatermark` | Сравнивали с `success = true` заглушки |
| `test_publishing_manager_m4`: `PublishToLocalDirectory`, `PublishToCloud`, `PublishViaEmail`, `ArchiveDrawings`, `PublishToMultipleDestinations`, `BatchExportMultipleFormats` | Успех зависел только от фиктивного экспорта |
| `test_drawing_extraction`: `test_extract_views_from_assembly`, `test_extract_isometric_views`, `test_custom_view_generation`, `test_custom_view_invalid_direction`, `test_custom_view_parallel_up_direction`, `test_create_drawing_sheet`, `test_export_drawing_formats`, `test_export_to_svg` (16 проверок из 92) | Проверяли удалённые функции; экспортные — с пометкой «basic placeholder tests» |

Добавлены тесты явной ошибки: `DrawingExportTest.ExportReportsUnsupportedFormat`,
`ExportViewReportsUnsupportedFormat`, `InvalidOptionsReportedBeforeUnsupported`,
`PublishingManagerTest.PublishToLocalReportsExportError`.

## 4. Проверка

| Конфигурация | До (`a0eb74a`) | После |
|---|---|---|
| (а) `build`, Release, без OCCT | сборка OK, ctest 74/74 | сборка OK, ctest 74/74 |
| (б) `build-occt`, Release, `BENDCORE_WITH_OCCT=ON` | сборка OK, ctest 84/84 | сборка OK, ctest 84/84 |
| WASM (`cmake --preset wasm`, цель `bendcore_wasm`) | не собирается и до изменений: ошибки Eigen в `src/sketch.cpp`, `src/simulation/cfd_solver.cpp`, `src/simulation/thermal_solver.cpp`, `third_party/planegcs/*`, `src/sketch2d.cpp` | те же 7 объектов не собираются; изменённые `drawing_export.cpp`, `drawing_extraction.cpp`, `publishing_manager.cpp` компилируются; `bindings.cpp` не затронут |

Число тестов ctest не изменилось: удалённые цели были в `BENDCORE_UNVERIFIED_TARGETS`
и в ctest не входили; изменилось число случаев внутри `drawing_export_m4` (32 → 19),
`publishing_manager_m4` (20 → 15), `drawing_extraction` (92 → 76 проверок).

## 5. Найдено, не удалено

Пересчитано 2026-11-08 после раздела 6; файлы, удалённые разделом 7, из списка ниже
поимённо не вычеркнуты (перечень удалённого — в разделе 7). Явные заглушки C++ вне задачи. Список получен
поиском по `Placeholder`, `In a real implementation`, `in real implementation`,
`в реальной реализации`, `not implemented`, `would send/upload/use`, `In production, would`
(без `src/ui`, `src/kernel`); строки — по текущему коду. Дополнительно
`scripts/check_no_stubs.sh --paths src include tests` (все правила по всему старому коду):
1058 нарушений до раздела 6, 1047 после (ушли 10 в `src/dxf_reader.cpp` и
`no-assertions` в удалённом `tests/test_dxf_reader_comprehensive.cpp`; в изменённых файлах
нарушений нет). Подтверждено чтением кода:

- Сняты разделом 7: `src/drawing/*`, `src/testing/test_suite.cpp`, решатель
  `src/assembly/assembly_constraint_system.cpp`, `third_party_stubs`, `third_party/zlib_stub.hpp`.

Остальные совпадения (требуют разбора владельцами модулей; часть может оказаться
текстом интерфейса, а не отсутствующей логикой):
`include/bendcore/geometry/boolean_operations.h:16`, `include/bendcore/performance_advanced.hpp:124`,
`include/bendcore/simulation/cfd_solver.hpp:134`, `src/analytics_event.cpp:49`,
`src/bend_calculator.cpp:152`, `src/bendcore/import_filter.cpp:249`, `src/bendcore/shortcuts.cpp:159`,
`src/constraint_system.cpp:39, 48, 104`, `src/cost_analysis.cpp:575`,
`src/deployment/LicenseManagement.cpp:219, 220, 268, 323, 324, 329`,
`src/deployment/ProductionBuild.cpp:110`,
`src/editing/direct_editing_manager.cpp:313, 381, 402, 625, 651, 662`,
`src/event_batch_sender.cpp:63`, `src/generative/constraint_handler.cpp:240`,
`src/generative/design_generator.cpp:193, 197, 201, 205, 251, 257, 263`,
`src/generative/visualization.cpp:306, 386, 391, 396`, `src/geometry/curve_operations.cpp:275`,
`src/growthbook_client.cpp:135`, `src/ipc_bridge.cpp:429`, `src/ipc_commands.cpp:930, 1262`,
`src/manufacturing/ManufacturingWorkflow.cpp:407`,
`src/optimization/computation_cache.cpp:294, 299`,
`src/optimization/memory_management.cpp:155, 343, 347`,
`src/optimization/optimization_profiles.cpp:75, 79`,
`src/optimization/performance_monitor.cpp:41`,
`src/performance_advanced.cpp:24, 73, 103, 119, 125, 268, 416`,
`src/plugins/plugin_loader.cpp:25`, `src/production/crash_recovery.cpp:117`,
`src/production/logging_monitoring.cpp:173`, `src/production/performance_tuning.cpp:69`,
`src/production/production_deployment.cpp:108`, `src/security.cpp:302, 402, 406, 464`,
`src/serializer.cpp:21, 27`, `src/simulation/result_visualization.cpp:293`,
`src/simulation/simulation_manager.cpp:378`, `src/sketch/constraint_diagnostics.cpp:116, 217`,
`src/surfaces/surface_operations.cpp:143`, `src/testing/launch_package.cpp:608`,
`src/topology_optimization/constraint_handling.cpp:185, 190`,
`src/topology_optimization/sensitivity_analysis.cpp:29`,
`src/topology_optimization/topology_optimizer.cpp:63, 182, 183`;
TypeScript: `src/analytics.ts:294`, `src/features/enterprise/BOM.tsx:160`,
`src/features/sheetmetal/SheetMetalDesign.tsx:205`,
`src/modules/collaboration/CollaborationManager.tsx:222`,
`src/modules/formats/DWGDXFHandler.ts:143`.

Сняты с учёта (раздел 6): `src/drawing/publishing_manager.cpp` (облако, почта,
`embedMetadata`, нумерация, проверка назначений), `src/dxf_reader.cpp`,
`src/import_workflow.cpp:243-244`.

## 6. Вторая очередь (2026-11-08): Eigen-заглушка, читатель DXF, публикация

Номера строк — по коммиту `f5583b3` (до изменений).

| Файл / функция | Доказательство (файл:строка до удаления) | Что сделано | Чем заменено |
|---|---|---|---|
| `third_party/eigen-5.0.0/Eigen/{Core,Dense,Eigen}`, `third_party/eigen-stub/Eigen/{Core,Dense}`, `third_party_stubs/include/Eigen/Dense` | `eigen-5.0.0/Eigen/Core:1` — `// Minimal Eigen Mock for Phase 4 Testing`; `CMakeLists.txt:25-31` — путь к заглушке в `find_path`, при неудаче `third_party_stubs` (только `WARNING`) | Каталоги удалены; поиск переписан (`CMakeLists.txt`, блок Eigen); проверка заглушки в `cmake/assembly.cmake:4-7` удалена; `third_party_stubs` убран из путей `bendcore_core` | Настоящий Eigen ≥ 3.3: `BENDCORE_EIGEN_DIR` → пакет `Eigen3` → `/usr/include/eigen3` и др. (видны и при кросс-компиляции WASM) → `BENDCORE_FETCH_EIGEN=ON` (3.4.0, SHA-256); иначе `FATAL_ERROR`. `docs/WASM_BUILD_GUIDE.md` |
| `DXFReader::extract_lines/circles/polylines/splines` | `src/dxf_reader.cpp:96-160` — на каждое вхождение подстроки `LINE`/`CIRCLE`/… добавляется примитив с фиксированными координатами (`{0,0}→{100,0}`, `r = 25`), `// Simple placeholder`; `POLYLINE` также считается `LINE` | Класс `DXFReader` и структуры удалены, API заменено | `parseDxf`/`readDxfFile` → `Result<Document>`: разбор групповых кодов (HEADER, LAYER, LINE, ARC, CIRCLE, LWPOLYLINE с выпуклостью, POLYLINE/VERTEX, TEXT, MTEXT), `bulgeToArc`, `scaleToMillimeters`; библиотека `bendcore_dxf` без OCCT. `docs/drawing/DXF_IMPORT.md` |
| `DXFReader::extract_dimensions` | `dxf_reader.cpp:208-211` — `// Set placeholder coordinates`, точки `{0,0}`, `{100,0}` | Удалено | DIMENSION учитывается в `Document::skipped` (не читается) |
| `DXFReader::extract_layers` | `dxf_reader.cpp:258-272` — слои `Layer0…LayerN` по числу вхождений подстроки `LAYER` | Удалено | Таблица LAYER: имя, цвет, тип линии, флаги |
| `ImportWorkflow::import_dxf_file` | `src/import_workflow.cpp:243-244` — `// In production, would extract dimensions` | Комментарий удалён; ошибки и `skipped` читателя переносятся в `warnings` | `readDxfFile` |
| `publishToCloud` | `src/drawing/publishing_manager.cpp:121-136` — `// in real implementation, would upload`, формируется `s3://…` без выгрузки | Функция, `CloudProvider`, `CloudPublishSettings`, `PublishingJob::cloud_settings`, `PublishingResult::published_urls` удалены | `publishToMultiple(CloudStorage)` → `success = false`, `"Publishing to cloud storage is not supported: …"` |
| `publishViaEmail` | `publishing_manager.cpp:187-188` — `email_sent = true` без отправки | Функция, `EmailPublishSettings`, `email_settings`, `email_sent`, `notify_on_completion`/`notification_email` удалены | `publishToMultiple(EmailRecipients)` → `"Publishing via e-mail is not supported: …"` |
| `embedMetadata` | `publishing_manager.cpp:457-462` — `// In real implementation, would write metadata`, `return {true, ""}` | Функция и объявление удалены | — (метаданные PDF пишет `drawing_v2::writePdf`) |
| `generateDrawingNumber` | `publishing_manager.cpp:432-434` — `// In real implementation, would check existing files`, всегда `sequential_start`; `date_format` не используется | Реализовано | max(номер в `archive_directory`) + 1, но не меньше `sequential_start`; `date_format` (YYYY, YY, MM, DD) |
| `validatePublishingDestination` | `publishing_manager.cpp:501` — `NetworkShare`: `// Would validate network connection`, всегда `true`; облако и почта — `true` при непустых полях | Исправлено | `NetworkShare` — проверка записи в каталог; облако и почта — `{false, "… is not supported …"}` |

Удалённые тесты:

| Тест | Причина |
|---|---|
| `tests/test_dxf_reader_comprehensive.cpp` (ctest `dxf_reader_comprehensive`, нативная и WASM) | Только печать строк, ни одной проверки |
| Старый `tests/test_dxf_reader.cpp` (ctest `dxf_reader`, только с OCCT) | `assert(geom.lines.size() >= 0)` и подобные; заменён новым с тем же именем |
| `test_publishing_manager_m4`: `PublishToCloudMissingCredentials`, `PublishViaEmailNoRecipients`, `EmbedMetadata`, `ValidatePublishingDestinationCloud`, `ValidatePublishingDestinationEmail` | Проверяли удалённые функции или фиктивный успех |
| `e2e_import`, тест 5 | `assert(geom.entity_count >= 0)` — заменён проверкой координат |

Добавлены: `test_dxf_reader` (11 случаев, все нативные конфигурации),
`test_dxf_reader_roundtrip` (лист из `bendcore_drawing_v2`; при `BENDCORE_PYTHON_EZDXF` —
файлы R12/R2000/R2018 от ezdxf), `PublishingManagerTest.PublishToMultipleReportsUnsupportedCloudAndEmail`,
`ValidateRejectsCloudAndEmail`, усилены `GenerateDrawingNumber`, `GenerateDrawingNumberWithDate`,
`test_import_workflow` (тест 2: `success`, число примитивов, предупреждение о SPLINE).

Проверка:

| Конфигурация | До (`f5583b3`) | После |
|---|---|---|
| `build`, Release, без OCCT | ctest 74/74 | ctest 74/74 (+`dxf_reader`, −`dxf_reader_comprehensive`) |
| `build-occt`, Release, OCCT, `BENDCORE_PYTHON_EZDXF` задан | ctest 110/110 | ctest 110/110 (+`dxf_reader_roundtrip`; `dxf_reader` заменён; −`dxf_reader_comprehensive`) |
| WASM, `cmake --preset wasm` без параметров | Eigen = заглушка: 8 объектов не компилируются, ctest 2/60 | Eigen 3.4.0 найден автоматически: сборка OK, ctest 59/59 |
| WASM, `BENDCORE_EIGEN_DIR=/usr/include/eigen3` (`build_wasm.sh`, CI) | ctest 60/60 | то же, что строкой выше |

Заявление «59 WASM-тестов проходят» (`BUILD_STATUS_2026-11-06.md`) с заглушкой получено
быть не могло; оно соответствует `scripts/build_wasm.sh`, который передаёт настоящий Eigen.
Точное число 59 на той версии не перепроверялось; на `f5583b3` таких тестов 60.

## 7. Третья очередь (2026-11-11): устаревшие подсистемы и непроверенные цели

Номера строк — по коммиту `f3e1b9a` (до изменений). Критерий удаления: модуль не
используется новым кодом (`src/kernel`, `src/modeling`, `src/assembly_v2`, `src/drawing_v2`),
CLI, сервером и WASM-мостом **и** является заглушкой либо содержит данные без источников.
Потребители проверены по `#include` во всём `src`, `include`, `tests` и по спискам целей
CMake; замыкание включений от `src/cli/main.cpp`, `src/server/*`, `src/wasm/bindings.cpp`,
`src/occt/*`, `bendcore_import` ни одного удалённого файла не содержит.

### 7.1 Подсистемы

| Подсистема | Кто использовал | Доказательство фиктивности | Что сделано |
|---|---|---|---|
| `src/materials`, `include/bendcore/materials` | только `tests/materials/test_material_library.cpp`; `material_property_database` — только тест `GAP-052`, не входивший в сборку | `material_library.cpp:500-508` и далее: «рейтинги» обрабатываемости/свариваемости 0–100 и `carbon_footprint` без единой ссылки на источник | Удалено. Замена — `bendcore_kernel_analysis` (`include/bendcore/kernel/material.hpp`, значения с источниками) |
| `CadDocument`, транзакции, история, команды: `src/cad_document.cpp`, `entity_tree`, `document_event_bus`, `command_manager`, `transaction`, `history_stack`, `src/document/*`, `src/commands/*`, `src/api/document_api.cpp`, `src/serialization/document_serializer.cpp`, `src/serializer.cpp`, `src/atomic_save.cpp`, `src/recovery.cpp`, `src/legacy_adapter.cpp`, `src/wasm_bindings.cpp` | только собственные тесты, `src/testing`, старая сборка | `src/serializer.cpp:21, 27` — `// JSON format - simplified placeholder`, пишется строка `{"version": 1, "revision": 0}`; два варианта одного класса (`src/cad_document.cpp` и `src/document/cad_document.cpp`, собирался один); большинство тестовых целей не компилировалось (`'bendcore::document' is not a namespace-name` и т. п.); `src/wasm_bindings.cpp` включает несуществующие заголовки `*.h` | Удалено. Документ детали — `src/modeling` |
| `ParameterStore` (`src/parameter_store.cpp`, `src/parameters/`) | тесты (цель `test_parameter_store` не компилировалась) | `parameter_store.cpp:11-17` — `get()` для неизвестного имени молча возвращает `0.0` | Удалено. Переменные и выражения — `bendcore_modeling_document` |
| Граф зависимостей и перестроение: `src/dependency_graph.cpp`, `src/dependencies/`, `src/rebuild_engine.cpp`, `src/rebuild/` | тесты; `test_rebuild_engine` был закомментирован в `CMakeLists.txt` («Missing source files») | перестраивались только числа параметров, геометрии нет (раздел 1 плана); копии заголовка графа в `dependency/` и `dependencies/` | Удалено. Перестроение — дерево операций `src/modeling` |
| Старая сборка `src/assembly`, `include/bendcore/assembly`, `src/main.cpp` | собственные тесты, `hierarchy_adapter.hpp` (удалён) | `assembly_constraint_system.cpp:237-262` — «решатель» вызывает `validate()` и ставит `SATISFIED`, положения компонентов не меняет; `:310-311` — присваивание поля самому себе; часть заголовков без реализаций (`mate_solver.hpp`, `fixture_designer.hpp`) | Удалено. Замена — `src/assembly_v2` |
| `src/features/assembly` (React) | `src/features/BendCoreFeaturesApp.tsx` (сам никем не импортируется) | `Assembly.tsx:36-42` — сопряжение всегда между `face-0` и `face-0`, `status: 'satisfied'` без расчёта | Удалено; вкладка убрана; тип `FEAResult` перенесён в `src/features/simulation/types.ts` |
| Старые чертежи `src/drawing`, `include/bendcore/drawing` | только 8 собственных тестов | после раздела 1 `exportDrawing` возвращал ошибку для любого формата, поэтому публикация, печать (`print_manager.cpp:28, 170, 351, 356`) и `file_handler.cpp:135-136` работали с пустым результатом; `annotation_dimension.cpp:288, 347` — заглушки | Удалено целиком. Замена — `src/drawing_v2` (виды, аннотации, PDF/DXF/SVG). Чертёж трубы `src/drawing.cpp` оставлен |
| `third_party_stubs/include/nlohmann/json.hpp` | никто | 133-строчная замена; все потребители уже включают `third_party/nlohmann/json.hpp` (MIT) | Удалено |
| `third_party_stubs/glm` и потребители: `src/bendcore/{camera_manager,snap_system,viewport_manager}.*`, `src/ipc_commands.{cpp,hpp}` | ни одна цель CMake | `snap_system.cpp:192-201` — привязки к концам и центрам возвращают `{}`; `viewport_manager.cpp:56-63` — «вписать в вид» ставит фиксированную камеру; настоящей GLM в системе нет | Удалено вместе с потребителями (вьюпорт — three.js в `src/ui`) |
| `third_party/zlib_stub.hpp` | только удалённый `document_serializer.cpp` | — | Удалено |

Оставлено и почему: `include/bendcore/document/entity_id.hpp` + `src/entity_id.cpp`
(UUID-идентификатор, используют `src/features/{loft,sweep,revolve}` и
`src/editing/direct_editing_manager`); `src/instances.cpp` (экземпляры труб, `core_api`);
трубный модуль, CFD/тепло, `bendcore_formula`, `sketch2d`, PlaneGCS, сервер, CLI — на них
держатся `bendcore_cli`, `bendcore_server`, `bendcore_wasm`, `bendcore_import`.

### 7.2 Цели `BENDCORE_UNVERIFIED_TARGETS`

Каждая цель собрана (`ninja -k 0`), затем — с добавлением её исходников реализации
(они не входили в `bendcore_core`) и GTest. Список и опция `BENDCORE_BUILD_UNVERIFIED` удалены.

| Цель | Результат проверки | Решение |
|---|---|---|
| `test_ab_testing` | собирается с `src/ab_testing.cpp`, проходит; распределение по вариантам — хеш, без заглушек | Возвращена в сборку |
| `test_comments` | написана на GTest (не хватало библиотеки при линковке), 12 тестов проходят | Возвращена (под `GTest_FOUND`) |
| `test_layers` | не хватало `using bendcore::Path/Bend/Straight`; после правки 9 тестов проходят | Возвращена |
| `test_drawing_layers_manager` | собирается с `src/drawing_layers.cpp`, 16 тестов проходят | Возвращена |
| `test_growthbook_client`, `test_analytics_event`, `test_event_batch_sender`, `e2e_analytics` | собираются с реализациями; три проходят, `test_event_batch_sender` падает | Заглушки: `growthbook_client.cpp:135-140`, `event_batch_sender.cpp:63-69`, `analytics_event.cpp:49-52` — «simulate a successful fetch/send», сеть не используется. Тесты и исходники удалены |
| `test_gap083/085/086/087` (+ `tests/security/*`) | собираются и проходят | Заглушки: `src/security/compliance_standards.cpp:20-48` — `implementGDPRCompliance`, `verifyAccessControl`, `deletePersonalData` и др. возвращают `true`. `src/security`, `include/bendcore/security` удалены (`src/security.cpp` с тестом `security` оставлен) |
| `test_cad_core_041…044` | 041, 044 проходят, 042 падает, 043 не компилируется | Заглушки: `src/deployment/CloudDeployment.cpp:162, 296, 306, 380-393`, `Installers.cpp:134, 143, 235` — «would go here», `return true`. `src/deployment` удалён |
| `test_cad_core_037…040` | 037, 038, 040 не компилируются; 039 проходит | Данные без источников: `BOMGenerator.cpp:399-409` (цены и поставщики «MetalSupplies Inc.»), `:152` (50 $/ч), `GCodeGenerator.cpp:90-105`, `ManufacturingWorkflow.cpp:309, 407`. `src/manufacturing/{BOMGenerator,FEAIntegration,GCodeGenerator,ManufacturingPlanner,ManufacturingWorkflow}.*`, `src/fea/FEAEngine.hpp` удалены (`manufacturing/bom.cpp` с тестами оставлен) |
| `test_manufacturing_constraints` | собирается с GTest, падает | `manufacturing_constraints.cpp:920, 934, 954` — «Simplified formula» с подобранными коэффициентами; потребителей нет. Удалено |
| `test_multi_physics_coupling` | собирается, 41 проверка проходит | `multi_physics_coupling.cpp:304` — `residual_ = 0.0;  // Simplified residual` (сходимость фиктивна), `:88, 101, 243`; потребителей нет. Удалено |
| `test_m5_optimization`, `phase_f_test_suite` | первая проходит, вторая не компилируется | `performance_monitor.cpp:207-212` — бенчмарк возвращает `executionTimeMs = 10`, `cacheHitRate = 0.85`; `src/testing/test_suite.cpp:351-521` — тесты без проверок. `src/testing`, `include/bendcore/testing` удалены |
| `test_geometry_kernel`, `test_curve_operations`, `test_surface_operations`, `test_surfaces_m5` | первые две падают на `assert`, остальные не компилируются | `curve_operations.cpp:122, 275, 456, 525`, `surface_operations.cpp:84-199`, `nurbs_surface.cpp:443` — «Simplified/placeholder»; геометрия — OCCT (`src/kernel`). Исходники, заголовки, `src/surfaces` удалены |
| `test_topology_optimization_m8` | 44 ошибки компиляции в заголовках модуля; исходники были закомментированы в CMake | `src/topology_optimization`, заголовки и тест удалены |
| `test_search` | `src/search.cpp` не компилируется (`SearchCondition` не объявлен, обращение к закрытому методу) | Удалено с заголовком и тестом |
| `test_drawing_undo_redo` | 4 проверки из 12 падают: менеджер хранит только индекс, первое действие отменить нельзя; потребителей нет | Удалено |
| `test_gap025/026/027_*_comprehensive`, `test_cloud_collaboration` | не компилируются: написаны под API, которого нет (`ConstraintHandler()`, `CloudStorageManager`, …) | Удалены только тесты; модули FEA, generative, parametric AI проверяются `fea_m6`, `generative_m6`, `parametric_ai_m6` |
| 18 целей документа/команд/параметров/графа/интеграции (`test_cad_document`, `test_entity_id`, `test_hierarchy_extended`, `test_command_executor`, `test_history_stack`, `test_transaction_manager`, `test_document_event_bus`, `test_parameter_store`, `test_dependency_graph`, `test_phase_c_integration`, `test_phase_d_integration`, `test_integration_e2e`, `test_multi_phase_integration`, `test_stress`, `test_stress_tests`, `test_e2e_workflows`, `test_cross_platform_validation`, `test_regression_suite`) | не компилируются | Удалены вместе с подсистемой (7.1) |

Удалены также проходившие тесты удалённых подсистем: `m1_integration`, `transaction`,
`undo_redo_manager`, `document_serializer`, `document_api`, `rebuild_cascade`,
`part_reference`, `assembly_constraints`, `assembly_creation`, `assembly_validation`,
`material_library`, `view_management`, `drawing_extraction`, `drawing_workbench_m4`,
`annotation_dimension`, `drawing_export_m4`, `publishing_manager_m4`, `print_manager_m4`,
`file_handler_m4`.

### 7.3 Jest

| Тест | Причина падения | Решение |
|---|---|---|
| `src/ui/__tests__/ImportDialog.integration.test.tsx` (7 из 10 падали) | устаревший тест: свойство `isOpen`, подписи и `role="dialog"`, которых у компонента нет; «проходившие» случаи — `if (x) expect(...)` | Удалён. Компонент реален и покрыт `tests/ImportDialog.test.tsx`; туда добавлен случай отклонения не-CAD файла |
| `tests/web-app.spec.ts` | спецификация Playwright (`@playwright/test`), попадала в Jest по маске `*.spec.ts` | Исключена из Jest. В `playwright.config.ts` (`testDir: tests/e2e`) не входит — открытая задача для QA |
| `tests/qa/fixtures/**` | фикстуры самотеста `check_no_stubs.sh`, не тесты | Исключены из Jest |
| `tests/electron-smoke.test.ts` (4 из 5 падали) | нет `dist/` (нужен `npm run build`) и бинарника Electron; первый случай проходил даже при «Electron failed to install» | Код реален. Вынесен в `npm run test:electron-smoke` (сборка + запуск); отсутствие Electron теперь ошибка. В этой среде не запускался: нет дисплея и бинарника |

### 7.4 Проверка

| Конфигурация | До (`f3e1b9a`) | После |
|---|---|---|
| `build`, Release, без OCCT | ctest 74/74 | ctest 59/59 (−19 удалённых, +4 возвращённых) |
| `build-occt`, Release, OCCT | ctest 133/134 (`performance` упал при параллельной сборке, отдельно проходит) | ctest 120/120, включая тесты документа чертежа из слияния `ccr-d188a25e-rml7z8` |
| WASM, `cmake --preset wasm` | сборка OK, ctest 59/59 | сборка OK, ctest 47/47 |
| Jest (полный) | наборов 54/58, тестов 1234/1245 | наборов 53/53, тестов 1230/1230 |
| `check_no_stubs.sh`, `--diff ccr-d188a25e-rml7z8` | 0 | 0 |
| `check_no_stubs.sh --paths src include tests` (весь старый код) | 1140 | 891 |

### 7.5 Найдено, не удалено

- `src/optimization/performance_monitor.cpp:207-212` — `BenchmarkFramework` с зашитыми
  результатами (модуль в `bendcore_core`, тест удалён).
- `src/cloud/cloud_storage_manager.cpp:30` — «выгрузка» записывает длину пути как размер
  файла; сетевых операций в `src/cloud` нет.
- `src/features/{loft,sweep,revolve,feature_system}.cpp`, `src/feature_system.cpp`,
  `src/editing/direct_editing_manager.cpp`, React-панели
  `src/features/{modeling,advanced,simulation,enterprise}` (`FEAResult.isStub`) — старые
  операции без ядра; заменены `src/modeling`, но имеют проходящие тесты и в задачу не входили.
- `src/manufacturing/cnc_*`, `src/geometry/{mesh_cache,parallel_geometry}.cpp`,
  `src/bendcore/command_search.cpp` и другие файлы вне целей CMake.
- `tests/acceptance/*` (свой `CMakeLists.txt`, не подключён) включает заголовки, которых
  никогда не было (`bendcore/assembly_manager.hpp`, `bendcore/undo_redo_manager.hpp`).
- `src/drawing_layers.cpp`, `src/ab_testing.cpp` — рабочие, но без потребителей кроме тестов.
