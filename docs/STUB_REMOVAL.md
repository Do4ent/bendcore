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

Явные заглушки C++ вне задачи. Список получен поиском по `Placeholder`,
`In a real implementation`, `в реальной реализации`, `not implemented`, `would send/upload/use`
(без `src/ui`, `src/kernel`); строки — после удаления. Подтверждено чтением кода:

- `src/drawing/publishing_manager.cpp:121` — облачная выгрузка не выполняется
  (`in real implementation, would upload`), формируется только адрес `s3://…`;
  `:187-188` — `email_sent = true` без отправки; также `:433`, `:457-459`.
- `src/drawing/print_manager.cpp:28, 170, 351, 356`; `src/drawing/file_handler.cpp:135-136`;
  `src/drawing/annotation_dimension.cpp:288, 347`.
- `src/testing/test_suite.cpp:351, 373, 448, 497, 521` — тесты (`CrashRecoveryTest` и др.)
  возвращают успех без проверок, `// In real implementation, would …`.
- Решатель сборок `src/assembly/assembly_constraint_system.cpp` — только валидирует
  (раздел 1 плана).

Остальные совпадения (требуют разбора владельцами модулей; часть может оказаться
текстом интерфейса, а не отсутствующей логикой):
`include/bendcore/geometry/boolean_operations.h:16`, `include/bendcore/performance_advanced.hpp:124`,
`include/bendcore/simulation/cfd_solver.hpp:134`, `src/analytics_event.cpp:49`,
`src/bend_calculator.cpp:152`, `src/bendcore/shortcuts.cpp:159`,
`src/constraint_system.cpp:39, 48, 104`, `src/cost_analysis.cpp:575`,
`src/deployment/LicenseManagement.cpp:219, 220, 268, 323, 324, 329`,
`src/deployment/ProductionBuild.cpp:110`, `src/dxf_reader.cpp:97, 101, 116, 131, 147, 208`,
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
`src/production/production_deployment.cpp:108`, `src/security.cpp:402`,
`src/serializer.cpp:21, 27`, `src/simulation/result_visualization.cpp:293`,
`src/simulation/simulation_manager.cpp:378`, `src/sketch/constraint_diagnostics.cpp:116, 217`,
`src/surfaces/surface_operations.cpp:143`, `src/testing/launch_package.cpp:608`,
`src/topology_optimization/constraint_handling.cpp:185, 190`,
`src/topology_optimization/sensitivity_analysis.cpp:29`,
`src/topology_optimization/topology_optimizer.cpp:63, 182, 183`.
