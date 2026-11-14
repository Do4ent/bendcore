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

Заявление «59 WASM-тестов проходят» (отчёт `BUILD_STATUS_2026-11-06.md`, удалён из репозитория
в `c593c1f` как устаревший) с заглушкой получено
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

## 8. Четвёртая очередь (2026-11-11): устаревший интерфейс `src/ui` и точки входа

Номера строк — по коммиту `a1bb972` (до изменений). Критерий: файл не достижим по импортам ни
от одной точки входа — окно Electron (`src/ui/index.tsx`, `webpack.config.js`), страницы
`web/{kernel-viewer,part-modeler,assembly,drawing}` (`src/ui/{kernel,modeling,assembly,drawing}`),
стенд `tests/ui_preview`, главный процесс `src/main.ts` — **или** выдаёт фиктивный результат.
Граф построен разбором `import`/`require`/`import()` во всех `*.ts(x)`, `*.js`, `*.mjs`, `*.html`
репозитория (кроме `node_modules`, `third_party`, `build*`). Каталоги `src/ui/{kernel,modeling,
assembly,drawing,app}`, `web/app`, `src/wasm` не затрагивались.

### 8.1 Удалено

| Что | Кто использовал | Доказательство |
|---|---|---|
| `src/ui/M22*`–`M25*` (40 компонентов и 2 CSS: файловые диалоги, настройки, инструменты эскиза, сборки, гибка, BOM, стоимость, NC-код, FEA, отчёты, траектория), `BendCoreLanding`, M22-диалоги `CloudSettings`, `PluginSettings`, `FileExplorer`, `SettingsDialog`, `IconThemeManager`, `RecentFilesManager`, `SaveDialog`, `ThemeSelector`, `UnitPreferences`, `PerformanceSettings`, `KeyboardShortcutEditor`, `FilePickerDialog`; тесты `tests/ui/*` | `index.tsx:28-31` — `lazy(() => import('./M2…'))`, ни разу не отрисованы; `BendCoreLanding` только реэкспортировался (`index.tsx:34`) | Только локальное состояние форм, ни вычислений, ни вызовов ядра: `M24BendAnalysis.tsx:10` — `results` никогда не присваивается; `M25CostCalculator.tsx` — сумма двух введённых чисел; `M25FEAResultsViewer`, `M25ToolpathPreview`, `M24ExplodedView` — `viewport-placeholder`; `PluginSettings.tsx:27-31`, `FileExplorer.tsx:34-35` — зашитые списки («Mock file system»); `M22Integration.tsx:70-75` — действие диалога только закрывает его |
| `PerformanceMonitoring.ts`, `MemoryManagement.ts`, `ComponentOptimization.tsx`, `DatabaseOptimization.ts`, `SerializationOptimization.ts`, `WebGLOptimization.ts`, `ConstraintSolverCache.ts`; `tests/performance-benchmark.test.ts` | первые три — `index.tsx` (печать времени запуска в консоль), остальные — только тест | `DatabaseOptimization.ts:308` — `ConnectionPool` раздаёт объекты `{id, available}` без базы данных; `SerializationOptimization.ts:29` — «сжатие» дописывает длину; `ComponentOptimization.tsx:107` — «This is a placeholder for the concept»; `MemoryManagement.ts:289` — «Placeholder» |
| `AnalyticsDashboard`, `KPICards`, `FunnelChart`, `ExperimentCard`, `EventTimeline`, `hooks/useAnalytics.ts`, `styles/`; `src/analytics.ts`, `src/growthbook.ts`, `src/experiment.ts`; каталог `ui/` (18 файлов); `tests/growthbook.e2e.tsx` | собственный тест; `ui/` не входил ни в одну сборку и ни в корни Jest | `useAnalytics.ts:74-144` — запросы к `/api/analytics/*`, которых не отдаёт ни один запущенный сервер (`backend/src/api/routes/analytics.ts` не подключён в `backend/src/index.ts` и возвращает зашитые эксперименты, `sampleSize: 5234`); `src/analytics.ts:294` — «In a real implementation, this would send to the GrowthBook API», пакет помечается `sent`; `ui/src/hooks/useIpc.ts:88` — канал `bendcore-api`, которого нет в `src/main.ts` |
| Калькуляторы смещения: `OffsetCalculatorTab`, `OffsetTypeSelector`, `OffsetResultsDisplay`, `OffsetVisualization`, `ParallelOffsetCalculator`, `Combined3DOffsetCalculator` (+ тесты), вкладка «Offset Calculator» `TubeDialog` | `TubeDialog.tsx:1437` | `OffsetCalculatorTab.tsx:121-131` — «Mock calculation for 3D»: `total_length = R·2 + offset·1,5`, `confidence: 82.5`; `ParallelOffsetCalculator.tsx:156` — `fetch('/api/bendcore/parallel-offset/calculate')` (маршрут ниже) |
| `src/api/*` (Express-маршруты), `tsconfig.api.json`, `tests/test_parallel_offset_api.ts` | ни один сервер их не подключает (поиск по репозиторию: только этот тест) | `routes/parallel-offset.ts:268, 293` — «Здесь была бы интеграция с C++ модулем», `(…)·(1 + k·0,1)`; `import_routes.ts:41-43` — `mockFormulas`; `formula_routes.ts:43` — «In production, this would call the compiled C++ module»; `template_marketplace_routes.ts:266, 377, 411` — зашитые рейтинги и подборки; `formula_api.cpp` вне целей CMake |
| `playwright.config.ts` (корневой), `tests/e2e/`, скрипты `test:e2e`, `test:e2e:{ui,headed,debug,report,parallel-offset,combined-3d,integration}` | — | наборы только для удалённых страниц смещения; `production.spec.ts` ищет кнопки «New Project», которых нет в интерфейсе; `webServer` — `npm run dev --workspace=@bendcore/backend` (рабочих пространств npm нет). Рабочие наборы — свои конфигурации `web/*/playwright.config.ts` |
| `src/ui/AdapterWeb.tsx` (вход `webpack.web.js`), `public/`, `index.html` в корне, `tests/web-app.spec.ts` (Playwright для этой страницы на порту 3000), скрипты `dev:web`, `dev:web:prod`, `build:web` | `webpack.web.js` | `AdapterWeb.tsx:27-104` — `createMockElectron`: любая команда «успешна», импорт возвращает трубу 100/R50·90°/100, масса `2.5` кг «Steel 1020» (`:88`), «Small gap detected»; `public/index.html:470, 704` — реклама «Real FEA Solver», «In production, would redirect to sign-up flow». `webpack.web.js` оставлен как общая основа страниц `web/*` без собственной точки входа |
| `src/index.tsx`, `src/App.tsx`, `src/screens/`, `src/ui/AppDemo.tsx`, `src/ui/components/*` (кроме `PropertiesPanel`) с тестами | ни одна сборка (`src/index.tsx` — не entry ни одного webpack) | `DrawingGenerationScreen.tsx:56-64` — «экспорт» с прогрессом `Math.random()·30` и сообщением «Exported» без экспорта; `WorkspaceScreen.tsx:37, 115`, `BendCreationScreen.tsx:33` — зашитые деревья и гибки; тесты `components/__tests__/*.spec.ts` — `// TODO: Implement test` + `expect(true).toBe(true)`, `Integration.test.ts`, `ThreeJsRenderer.test.tsx` проверяют собственные литералы |
| Прочие компоненты `src/ui` без потребителей: `DrawingEditor` (старые чертежи), `PathSketch3D` с `PathValidator`, `DimensionRenderer`, `PathSketch3D.types` (старый 3D-эскиз пути), `EnterpriseProfileManager`, `MaterialsPanel`, `FormulaEditor`, `TemplateLibraryMarketplace`, `TemplateLibraryView`, `TemplateEditor`, `TemplateCreator`, `PatternTemplates`, `ProfileDialog`, `ProfilesManager`, `SettingsStore`, `IconThemeSettings`, `IconThemeSystem`, `ToolbarCustomizationDialog`, `SearchPanel`, `ManualDimensionEditor`, `MenuBar`, `ImportProgress`, `ImportResults`, `hooks/useImport.ts`; их тесты, `tests/{templates,search,comments}.e2e.tsx` | только собственные тесты | Вдобавок к неиспользованию: `EnterpriseProfileManager.tsx:404` — «Load mock data»; `MaterialsPanel.tsx:72` — «In a real app, this would call the IPC handler»; `PathSketch3D.tsx:334` — «For now, simulate with simple validation»; `FormulaEditor`, `TemplateCreator`, `TemplateLibraryMarketplace`, `useImport` обращаются к маршрутам `/api/…`, которых нет. Замены: `src/ui/drawing`, `src/ui/modeling/sketch`. `comments.e2e.tsx` не импортировал ничего из `src` |
| Jest-наборы без кода проекта: `tests/AnalysisModule.test.ts`, `IPCIntegration.test.ts`, `cad-workflow.test.ts`, `ui-integration.test.tsx` | — | проверяют свои же объекты: `mockAnalysisModule`, `mockIPCBridge`, `MockApplication`, литерал `['step','stp']` |
| `src/features/**/*.ts(x)` (React-панели), `src/modules/`, `src/routing/` | никто (`BendCoreFeaturesApp` не импортировался) | `FEASimulator.tsx:46, 52` — `maxValue: Math.random()·500 + 100`; `BOM.tsx:22`, `PipeRouting.tsx:50` — `mockItems`, `mockFittings` с выдуманными ценами; `DWGDXFHandler.ts:105, 143` — случайный цвет DXF, «would use dwg library»; `AdvancedRoutingEngine.cpp` не входил ни в одну цель CMake. C++-файлы `src/features/*.cpp` (в `bendcore_core`) оставлены |
| `src/optimization/performance_monitor.cpp`, `include/bendcore/optimization/performance_monitor.hpp` | никто (заголовок не включается; тест удалён в 7.2) | `performance_monitor.cpp:207-216` — `executionTimeMs = 10`, `memoryPeakBytes = 1000000`, `cacheHitRate = 0.85`, `passedThresholds = true` при любом числе элементов; `:232` — `count / 100  // Simplified`; `recordMetric` отбрасывает значение. Строка убрана из `bendcore_core` в `CMakeLists.txt` |

### 8.2 Исправлено в окне трубного приложения

| Место | Было | Стало |
|---|---|---|
| `src/main.ts:58-73` `handleCommand` | `new-document`, `new-sketch`, `new-tube` — `{ document: 'new_doc_001' }`, `{ tube: 'tube_001' }` без создания объекта | В ядро идут только команды «раздел.действие» (`src/core_api.cpp`); иное — `{status: 'error', message: 'Команда не поддерживается ядром: …'}` |
| `RibbonBar.tsx`, сохранение и создание трубы | ответ проверялся как `res.success === false`, поэтому `{status: 'error'}` считался успехом и диалог закрывался | проверяется `status !== 'ok'` |
| `RibbonBar.tsx:542-640`, панели «Gaps» и «Mass» | образцы «Gap Violations (3 found)» (`:546`), «0.54 kg» (`:612`), «7.85 g/cm³»; `setGapViolations`/`setMassResult` нигде не вызывались | панели и кнопки удалены |
| `RibbonBar.tsx:727-745`, «Tech Specs» | ограничения (`tubeMaxOD: 100`, `tubeMaxLength: 5000`), которые нигде не проверяются | удалено |
| `RibbonBar.tsx:139-173`, импорт STEP/DWFx | файл отправлялся несуществующей команде, затем `alert('… imported successfully!')` (`:161` — `// TODO: Handle imported geometry`) | ветка удалена: STEP открывает настоящий `ImportDialog` (`index.tsx`, сервис `bendcore_server`); кнопка DWFx удалена (команды в ядре нет) |
| `DrawingWorkspace.tsx:567-572`, `DrawingVersionPanel` | «Откат» и «Сравнить» — только `console.log`; `CoreClient.getDrawingVersions/compareVersions/restoreVersion` вызывали `drawing.get_versions/compare_versions/restore`, которых нет в `core_api.cpp` | кнопки и методы удалены; панель показывает список версий ассоциативного чертежа и состояние |

### 8.3 Оставлено и почему

| Что | Почему |
|---|---|
| `src/ui/index.tsx`, `RibbonBar`, `RibbonButton`, `RibbonTab`, `ColorScheme`, `ContextMenu`, `StatusBar`, `PropertiesPanel`, `PropertyGroups`, `components/PropertiesPanel`, `KeyboardShortcuts`, `WorkspaceState`, `FeatureTree`, `SearchBar`, `TubeDialog`, `NewTubeDialog`, `TemplateSuggestionsTab`, `TemplateMatcherView`, `hooks/useTemplates` | оболочка окна Electron трубного модуля (вход `webpack.config.js`, `npm run build` в CI); несёт настоящие панели ниже; покрыта Jest |
| `CheckPanel`, `BendDataDialog`, `InstancesPanel`, `DrawingWorkspace`, `LayersPanel`, `CommentsPanel`, `CommentBubble`, `DrawingVersionPanel`, `SketchEditor` (2D-эскиз на листе трубы; результат уходит в `drawing.add_sketch`), `SketchControls`, `core/*` | вызывают существующие команды ядра (`check.run`, `bend.lra`, `bend.xyz`, `instances.*`, `drawing.part`, `drawing.make_associative`, `drawing.release_check`, `drawing.add_sketch`, `drawing.sketch_bbox`, слои, комментарии) через `bendcore_cli` (Electron) или `dispatch` модуля `bendcore_wasm` (`src/wasm/bindings.cpp`); `tests/node/ui_core.test.ts` проверяет клиент на настоящем `bendcore_cli`, стенд `tests/ui_preview` — панели |
| `ImportDialog`, `tubeService`, `Viewport3D` с `OrbitCamera`, `MouseGestures`, `SnapConstraints`, `SKeyMenu` | импорт STEP и распознавание труб через `bendcore_server` (`/v1/analyze`), отображение сеток сервиса в three.js; тесты `tests/ImportDialog.test.tsx`, `OrbitCamera`, `MouseGestures`, `SnapConstraints` |
| `webpack.web.js` | общая основа `web/*/webpack.config.js` |
| `tests/SketchEditor.e2e.tsx` | проверяет оставленные `SketchEditor`/`DrawingWorkspace` (вне Jest, как и раньше); удалён только случай, проверявший собственный мок `restoreVersion` |

### 8.4 Проверка

| Конфигурация | До (`a1bb972`) | После |
|---|---|---|
| `npm run typecheck` | OK | OK (без `tsconfig.api.json`) |
| Jest (полный) | наборов 56/56, тестов 1269/1269 | наборов 18/18, тестов 335/335 (−38 наборов удалённого кода и самопроверяющих) |
| `npm run build` (tsc + webpack окна Electron) | — | OK |
| `build`, Release, без OCCT | ctest 59/59 (7.4) | ctest 59/59 |
| `build-occt`, Release, OCCT | ctest 120/120 (7.4) | ctest 120/120 (удалённый `performance_monitor` тестов не имел) |
| Playwright `test:e2e:{kernel,modeling,assembly,drawing}` | — | WASM-модули (`build-wasm`, emsdk, OCCT WASM, `BENDCORE_EIGEN_DIR=/usr/include/eigen3`) собраны; kernel 4/4, modeling 4/4, assembly 2/2, drawing 1/1 |
| `check_no_stubs.sh`, `--diff ccr-d188a25e-rml7z8` | 0 | 0 |
| `check_no_stubs.sh --paths src include tests` | 891 | 675 |

### 8.5 Найдено, не удалено

- Кнопки ленты окна Electron без команды ядра (`new-tube`, `edit-tube`, `open`, `save`, эскиз,
  опорная геометрия, `measure`, `generate-report` и др.), `SearchBar` (`search-commands`),
  `FeatureTree` (`get-feature-tree`, `rename-feature`) теперь получают явную ошибку главного
  процесса; канал `bendcore:set-property` (`index.tsx`) не обрабатывается. Оболочку заменит
  приложение `src/ui/app`.
- `useTemplates` и `NewTubeDialog` читают `/data/templates.json`, `./data/bending_profiles.json`,
  которые `npm run build` не кладёт в `dist/web`.
- `backend/` (Express): маршрут `analytics.ts` с зашитыми экспериментами; собирается в CI.
- ~~`docs/API_REFERENCE.md` (раздел «Optimization APIs») описывал несуществующий
  `bendcore/optimization/performance_monitor.h`~~ — документ удалён в `c593c1f`.
- Зависимость `@growthbook/growthbook` в `package.json` больше не используется (lock-файл не
  менялся).
- `.github/workflows/production-deploy.yml` вызывает `npm run test:e2e` (удалён вместе с
  корневым `playwright.config.ts`) и `test:integration`, `test:smoke`, `test:performance`
  (не существовали и раньше) — для инженера качества.

## 9. Пятая очередь (2026-11-13): весь старый код, `check_no_stubs.sh` на `src include tests`

Номера строк — по коммиту `1f6b1fd` (до изменений). `scripts/check_no_stubs.sh --paths src include tests`
до: **674** нарушения в 134 файлах (433 `trivial-assert`, 95 `simplified`, 34 `placeholder`,
27 `no-assertions`, 23 `real-implementation`, 20 `temporary`, 17 `stub`, 17 `would`, прочие —
единицы); после: **0**. Каждое нарушение разобрано: настоящая заглушка → реализовано с тестом
против независимого эталона или удалено с потребителями и объявлениями; описание реального
поведения маркерным словом → текст исправлен. Подавлений `qa-allow` не понадобилось: два вида
ложных срабатываний (`placeholder?: string`, `field.placeholder`) устранены в правиле.
Потребители проверены по `#include` во всём `src`, `include`, `tests`, `python`, `examples`, `sdk`,
`backend` и по целям CMake; замыкание включений `bendcore_cli`, `bendcore_server`,
`bendcore_wasm`, `core_api` ни одного удалённого файла не содержит.

Полная проверка скрипта теперь охватывает весь `src`, `include`, `tests` (раздел 1 `docs/qa/QA.md`),
так что новое нарушение в старом коде роняет `qa_check_no_stubs_tree` и задачу CI `no-stubs`.

### 9.1 Реализовано

| Место | Было (доказательство) | Стало | Тест (эталон) |
|---|---|---|---|
| `analysis::Analyzer::checkGaps` (трубный модуль) | `src/analysis.cpp:738-752` — «generate sample gap violations for demonstration», зазор `2.5 + 0.8·k` | Зазор между соседними гибами = сумма прямых между ними; < `minimumGap/2` — ошибка, < `minimumGap` — предупреждение | `test_analysis`: путь S100 B S3 B S7 B B S40 B → предупреждение 3 мм (гибы 1–2), ошибка 0 мм (3–4) |
| `MassPropertiesCalculator::calculatePrincipalMoments`, `diagonalizeInertia` | `mass_properties.cpp:196-215` — моменты = отсортированная диагональ; `:315-322` — оси всегда x, y, z; `principal_axes` не заполнялись | Циклический метод Якоби (Golub–Van Loan, 8.5), оси в `MassProperties::principal_axes` | `test_mass_properties`: тензор [[2,1,0],[1,2,0],[0,0,5]] → 1, 3, 5; две массы в (±1,±1,0) → 0, 4m, 4m, ось (1,1,0)/√2 |
| `ThermalSolver` нестационарный | `thermal_solver.cpp:248-350` — явный шаг с множителем `0.01`, `dt` и `rho·c` не используются, «In a real solver, this would solve K*T = F» | Неявный Эйлер на том же операторе, что и стационарная задача; соседи — относительно шага узлов (точный 3/5/7-точечный Лаплас на регулярной сетке); поток — МНК-градиент | рекуррентность `400 − 100(1+2a·dt)^−n`, сходимость к `400 − 100·e^(−2at)`; поток `25k` на линейном профиле |
| `StructuralSolver` | `structural_solver.cpp:140-216` — матрица жёсткости не собиралась, `u = F/(E·0.01)`; `:35-43` — «главные напряжения» `max|σ|`, `sqrt(σ1)` | Линейные тетраэдры (постоянная деформация), `K = ΣV·BᵀDB`, разреженный LU, заданные перемещения, отказ для неподдерживаемых нагрузок, `ROLLER` и механизмов; главные напряжения — собственные значения | patch-тест куба из 6 тетраэдров: заданное удлинение и согласованные силы на грани → однородное одноосное напряжение по закону Гука; чистый сдвиг → ±τ, 0 |
| `CFDSolver`, модели турбулентности | `cfd_solver.hpp:134` — Spalart–Allmaras и k-ω SST молча считались как k-ε | `solve()` возвращает `false` | `UnsupportedTurbulenceModelsAreRejected` |
| `ColorMap::viridisColor` | `result_visualization.cpp:72` — 4 ступени, верхние две одинаковы | Опорные значения таблицы matplotlib (индексы 0, 32, …, 255), линейная интерполяция | значения в 0, 128/255, 1; монотонность зелёного канала |
| `ResultVisualization::exportToVTK` | `:250` — «Empty cells for now»; точки всех полей подряд при `POINT_DATA` одного поля (неверный файл при двух полях) | Ячейки `VTK_VERTEX`, все скалярные и векторные поля; поля на разных узлах — `false` | содержимое файла; отказ для поля на других узлах |
| `ReportingUIManager::exportReport` | `reporting_ui.cpp:85-91` — цикл по форматам пуст, всегда `true`; `reporting.cpp:221` — «% PDF export stub» | Пишет `<id>.json/.html/.csv` через `ReportGenerator`; G-code, PDF, DWG — `false` до записи; `ReportGenerator::exportReport` для PDF/DOCX/DWG/SVG — `std::invalid_argument` | `test_reporting_ui`: файлы записаны и содержат id; запрос с PDF отклонён |
| `drawing::make_sketch` | id `sketch_<секунды>` совпадал у эскизов одной секунды (найдено тестом `test_drawing_sketches`, который не собирался) | Счётчик в id | `test_drawing_sketches` |
| Дуги эскиза листа (`src/ui/SketchEditor.tsx`) | `:139` — «Simplified arc hit test»: p0 принят за центр; отрисовка — вокруг p1 | `src/ui/sketchArc.ts`: переход от концов к центру по SVG 1.1 F.6.5 (как `make_sketch_arc`), попадание по дуге, а не окружности | `tests/sketchArc.test.ts`: центры и углы, посчитанные вручную |

В сборку возвращены настоящие тесты, не входившие ни в одну цель: `test_manual_dimensions`
(исправлена инициализация `Rect`), `test_drawing_sketches`.

### 9.2 Удалено (потребителей вне собственных тестов нет)

| Подсистема | Доказательство (файл:строка до удаления) |
|---|---|
| `src/solver` (GAP-030), `include/bendcore/solver`, `test_real_time_solver` | `real_time_solver.cpp:317` — ГУ обнуляют `F`, матрицу не меняют (решение с закреплениями неверно); `preconditioners.cpp:161` — ILU(k) = вызов ILU0 без сохранения, `:248` — AMG с попарной агрегацией по индексам; ILU0 дважды прибавляет `U(i,j)` (`add_value` накапливает); `performance_metrics.cpp:345` — ссылка на временный объект; `solver_scheduler.cpp:216` — `cancel_task` пуст; `:377` — оценка ошибки 0. Расчётный модуль использует Eigen |
| Связки `SimulationManager::applyCFDToStructural/ThermalToStructural/CFDToThermal`, `setupCoupling`, `aggregateResults`, `CouplingMethod`, VTK-экспорт менеджера | `simulation_manager.cpp:219-262` — давление усредняется в одну нагрузку без узла; тепловая связка пуста; `:378` MONOLITHIC пуст; `exportToVTK` пишет `POINTS 0`. `MULTI_PHYSICS` теперь — независимый запуск решателей |
| `ResultVisualization::generateIsoSurface` | `:292-310` — вершины «у изоуровня» по допуску 0,01 и треугольники из подряд идущих; связности ячеек у полей нет |
| `src/simulation/{bend_radius_simulation,load_case_handler,stress_analysis_framework}` + тесты GAP-050/051/053 | не собирались; `bend_radius_simulation.cpp:61` — пружинение «0.1–0.3·угол» |
| `src/fea`, `include/bendcore/fea`, `test_fea_m6`, заголовки `mesh_optimization` (без реализаций) | `fea_solver.cpp:407-409` — диагональная «жёсткость» `E·V/100`; `:526-533` — напряжения «simplified model» |
| `src/generative`, `test_generative_m6` | `design_generator.cpp:193-263` — пустые `add/modify/remove/reorder_feature`, геометрия — куб объёма; `visualization.cpp:306, 386-396` — чувствительность 1.0, пустой экспорт |
| `src/parametric_ai`, `test_parametric_ai_m6` | `classification_system.cpp:409` — уверенность 0.7; `design_learning.cpp:490` — «собственные значения» = диагональ |
| `src/optimization` (нет даже тестов) | `memory_management.cpp:334, 343, 347` — фрагментация 10 %, пустые GC и раскладка; `parallel_executor.cpp:308` — эффективность 1.0; `computation_cache.cpp:294, 299`, `optimization_profiles.cpp:75, 79` — пустые загрузка/выгрузка |
| `src/cloud`, тесты GAP-070, `test_cloud_storage`, `test_gap071…073` | `serialization.cpp:156` — «сжатие» дописывает маркер, `:176` — хеш, `:199` — схема; `cloud_storage_manager.cpp:30` — размер = длина пути; `analytics_monitoring.cpp:177` — активные пользователи = n/2 |
| `src/hardening`, `src/production` (+ тесты GAP-088…093, `tests/hardening`, `tests/production`) | GAP-089…093 без единой проверки; `crash_recovery.cpp:80` — «Non-blocking stub», `production/crash_recovery.cpp:117` — восстановление имитируется |
| `src/plugins` (+ GAP-065…069, `test_plugin_*`, `test_extension_points`) | `plugin_loader.cpp:25-26` — «Mock implementation», загрузка не выполняется |
| `src/editing` (`test_direct_editing_manager`) | `direct_editing_manager.cpp:313, 381, 402, 625, 651, 662` — параметры не обновляются, коллизия = длина > 1000, проверка геометрии всегда `true`, перестроение не выполняется |
| `src/features`, `src/{feature_system,loft,revolve,sweep}.cpp` (`test_advanced_features_m5`) | операции без ядра (замена — `src/modeling`); тест без проверок |
| `src/sketch` M2 (`test_sketch_m2`) | `edit_tools.cpp:212, 245, 294`, `constraint_diagnostics.cpp:116, 217` — «simplified»/«placeholder»; замена — `sketch2d` (PlaneGCS) и `src/modeling/sketch` |
| `src/ipc_bridge.cpp`, `src/constraint_system.cpp` | потребителей и тестов нет; `ipc_bridge.cpp:171-195` — «Would normally parse JSON», `:426-429` — пустой обработчик; `constraint_system.cpp:39, 48, 93, 104` — проверка и решатель «simplified» |
| `src/performance.cpp`, `test_performance`, `examples/performance_demo.cpp` | `performance.cpp:123, 151` — формулы «(mock)», «параллельный» анализ последовательный; бенчмарк — `sleep(50 мс)` и доля попаданий 0.75 |
| `src/security.cpp`, `test_security` | `security.cpp:402-416` — проверки стека и утечек пусты; `:423-433` — зашитый список зависимостей (CGAL, Taskflow, Protobuf не используются); `:438` — база CVE пуста |
| `CollaborationUIStubs` (`reporting_ui`), `BOMGenerator::generateFromAssembly` | всегда пустые списки; `bom.cpp:280-291` — `assemblyData` не читается |
| Несобираемые старые исходники: трубные `bend_calculator`, `tube_bending_sequence`, `tube_builder`, `tube_analyzer`, `tube_feature_node`, `geometry_ops`, `pipe_profile`; `geometry_engine`, `boolean_operations.h`, `format_exporters`, `formula_engine`, `performance_advanced`, `src/geometry`, `src/manufacturing/cnc_*`, `src/bendcore/{geometry_recognition,import_filter,shortcuts,sketch_solver}`, `src/mobile`; поиск OpenMP в CMake | ни в одной цели CMake; `tube_bending_sequence.cpp:82` — деформация `угол·0.1`, `:148` — пружинение; `bend_calculator.cpp:150-184` — сила гибки «simplified model»; `geometry_engine.cpp:118` — обратная матрица = единичная; `geometry_recognition.cpp:95-96` — ошибка 0.1, уверенность 85; `parallel_geometry.cpp:237` — `intersects = true`; `format_exporters.cpp:27` — «STEP» из строк; `formula_engine.cpp:508` — зашитая дата |
| Тесты без проверок или проверяющие свои литералы, не входившие в сборку: GAP-054…064, `analytics_*.test.cpp`, `test_gap074…078`, `tests/mobile`, `tests/hardening`, `phase4_*`, `tests/acceptance` (+ `scripts/run_acceptance_tests.sh`), `test_import_csv_json`, `test_geometry_recognition`, `test_import_filter`, `test_tube_builder`, `test_formula_engine`, `test_plugin_marketplace`; собиравшиеся `test_{dwfx_parser,formula,analysis,geometry_analyzer}_comprehensive`, `test_phase_i_main`; Jest `tests/integration/comprehensive.test.ts` (исключён из Jest, не запускался) | `EXPECT_TRUE(true)` ×36 в файле; только печать строк; `assert(true)`; классы-двойники внутри теста |

### 9.3 Исправлен текст (поведение реально, маркер был неверным описанием)

`src/ui/index.tsx:86` (контекстное меню — всегда меню пустого места: у оболочки нет выбора),
`src/declarations.d.ts:12` (объявления типов Three.js), класс CSS `expand-toggle-placeholder` →
`expand-toggle-spacer`, `tests/ImportDialog.test.tsx:100`, `tests/NewTubeDialog.test.tsx:209`,
`tests/integration/collaboration-engine.integration.test.ts:6`, двойник решателя в
`tests/test_refs.cpp`; явные `return 1` в `main` двух тестов вместо тернарного оператора
(правило `no-assertions`); `bom.hpp`, `reporting*.hpp` — описания форматов.
Модель и ограничения расчётов — `docs/simulation/SIMULATION.md` вместо маркеров
«simplified»/«TODO»/«In a real solver».

### 9.4 Проверка

| Конфигурация | До (`1f6b1fd`) | После |
|---|---|---|
| `build`, Release, без OCCT | ctest 59/59 | ctest 41/41 (−20 тестов удалённых подсистем и тестов без проверок, +`manual_dimensions`, +`drawing_sketches`) |
| `build-occt`, Release, OCCT | ctest 136/136 | __OCCT__ |
| WASM `cmake --preset wasm`, `BENDCORE_EIGEN_DIR=/usr/include/eigen3` (трубный `bendcore_wasm`) | сборка OK, ctest 47/47 | __WASM__ |
| Jest (полный) | наборов 22/22, тестов 374/374 | наборов 23/23, тестов 380/380 (+`sketchArc`) |
| `npm run typecheck` | — | OK |
| `check_no_stubs.sh --paths src include tests` | 674 | 0 |
| `check_no_stubs.sh` (полная, теперь весь `src include tests`) | 0 (только каталоги нового кода) | 0 |
| `tests/qa/test_check_no_stubs.sh` | OK (42 нарушения фикстур) | OK (44) |

### 9.5 Найдено, не удалено

- `src/dwfx_parser.cpp` не входит ни в одну цель CMake, а его единственный тест (печать строк)
  удалён; нарушений правила в нём нет.
- `docs/TESTING.md:13` упоминает удалённый `test_performance` — зона технического писателя.
- `backend/`, `scripts/`, `examples/`, `sdk/` полной проверкой не охватываются (только `--diff`).
