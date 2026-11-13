# Очистка устаревших документов (2026-11-13)

Владелец: технический писатель. Основание: `docs/PROJECT_LOG.md`, запись 2026-11-12 («в корне
репозитория и `docs/` остаются десятки устаревших отчётов о „завершении“ прежних этапов»),
`docs/ROADMAP_GENERAL_CAD.md`, раздел 1 («отчёты не отражают реальное состояние»).

## 1. Правило

Документ удалялся, если описывает то, чего в коде нет (удалено как заглушка или не
создавалось), или утверждает результаты, не подтверждённые тестами репозитория. Верная
информация переносилась в актуальный документ. Прежнее содержимое доступно в истории git:
`git show 1f6b1fd:<путь>`.

Новые отчёты о «завершении» этапов не создаются: результат работы фиксируется тестами,
документом модуля и записью в `docs/PROJECT_LOG.md` (`CONTRIBUTING.md`). Сводный статус
относительно плана — `docs/V1_READINESS.md`.

## 2. Оставлено и исправлено

| Документ | Что сделано |
|---|---|
| `docs/TESTING.md` | оставлен: сборка трубного модуля, `bendcore_cli`, стенд `tests/ui_preview` — сверено с `src/core_api.cpp`, `src/core_bridge.ts`, `tests/ui_preview/server.mjs` |
| `docs/WASM_BUILD_GUIDE.md` | оставлен (на него ссылаются `README.md`, `.github/workflows/qa.yml`); раздел CI переписан по `.github/workflows/wasm.yml` (задачи `wasm`, `wasm-modules`, `wasm-e2e`), удалены несуществующая задача `occt`, отметки «✅ tested» и ссылка на удалённый `docs/ARCHITECTURE.md` |
| `CONTRIBUTING.md` | переписан: прежний описывал удалённые каталоги (`src/modules`, `src/screens`, `docs/PHASE1…`), несуществующие скрипты `npm run coverage`, `lint:fix` и требование покрытия > 90 %; теперь — по `docs/PROJECT_PROCESS.md` |
| `README.md` | ссылка на `docs/V1_READINESS.md`; убрано упоминание «отчётов фаз в корне» |

Переноса содержимого из удалённых документов не потребовалось: верные сведения в них
(требования к Eigen, формат импорта трубы, сборка OCCT) уже есть в `docs/WASM_BUILD_GUIDE.md`,
`include/bendcore/tube_model.hpp` и `docs/kernel/OCCT_BUILD.md` в более точном виде.
Проверено на примерах: `docs/IMPORT_GUIDE.md` описывал CSV с колонками
`segment,type,parameter,value,unit`, а `import_csv` читает `kind,length,radius,angle,…`;
`docs/OCCT_INTEGRATION.md` заявлял импорт 100 МБ быстрее 1 с при замере 22 с на 65 МиБ.

## 3. Не тронуто (вне зоны или на проверке у владельца)

- `docs/qa/PERFORMANCE_BENCHMARKS.md`, `QA_RESULTS_SUMMARY.md`, `QA_TEST_PLAN.md`,
  `QA_TEST_SUITE_README.md` (2026-10-23) — те же признаки устаревания («400+ тестов»,
  «95/100 performance score», числа без тестов; `docs/qa/PERFORMANCE.md` прямо называет первый
  «не измерением»). Каталог `docs/qa/` — зона инженера качества; рекомендуется удалить.
- `backend/BACKEND_IMPLEMENTATION.md`, `backend/README.md`, документы в `sdk/`, `kubernetes/`,
  `terraform/` — вне корня и `docs/`; относятся к наследию, перечисленному в `README.md`
  («Ограничения»).
- Ссылки на удалённые файлы остаются в коде и скриптах (документами не являются):
  `scripts/check-stage2-readiness.sh` проверяет наличие `IMPLEMENTATION_STATUS.md`,
  `WORK_COMPLETED.md`, `docs/STAGE_2_3_ROADMAP.md` (скрипт прежнего этапа);
  комментарии `src/ui/ColorScheme.ts`, `src/ui/RibbonBar.tsx` ссылаются на
  `BENDCORE_SPECIFICATION_FINAL_LOCKED.md`; `docker/.dockerignore` — на `CHANGELOG.md`.
- `docs/STUB_REMOVAL.md` (зона агента очистки) упоминает удалённые `BUILD_STATUS_2026-11-06.md`
  и `docs/API_REFERENCE.md` как исторические источники заявлений — нужна пометка «удалён,
  см. `docs/DOCS_CLEANUP.md`».

## 4. Удалённые документы (208 файлов и каталог `launch_package/` из 10 файлов)

| Файл | Причина |
|---|---|
| `ACCEPT-FINAL-REPORT.md` | отчёт о приёмке/завершении/замерах с непроверенными результатами («7 469 тестов», «0 дефектов»); фактическая история — `docs/PROJECT_LOG.md`, замеры — `docs/qa/PERFORMANCE.md` |
| `ACCEPTANCE_TEST_PLAN.md` | отчёт о приёмке/завершении/замерах с непроверенными результатами («7 469 тестов», «0 дефектов»); фактическая история — `docs/PROJECT_LOG.md`, замеры — `docs/qa/PERFORMANCE.md` |
| `BUILD_STATUS_2026-11-06.md` | отчёт о приёмке/завершении/замерах с непроверенными результатами («7 469 тестов», «0 дефектов»); фактическая история — `docs/PROJECT_LOG.md`, замеры — `docs/qa/PERFORMANCE.md` |
| `CAD-CORE-001-DATA-OWNERSHIP-AUDIT.md` | старая документная модель (CadDocument, команды, транзакции, ParameterStore) удалена как заглушка — `docs/STUB_REMOVAL.md`, раздел 7; документ детали — `docs/modeling/PART_DOCUMENT.md` |
| `CAD-CORE-004-UNIFIED-ENTITY-HIERARCHY.md` | старая документная модель (CadDocument, команды, транзакции, ParameterStore) удалена как заглушка — `docs/STUB_REMOVAL.md`, раздел 7; документ детали — `docs/modeling/PART_DOCUMENT.md` |
| `CAD-CORE-005-COMMAND-ENGINE.md` | старая документная модель (CadDocument, команды, транзакции, ParameterStore) удалена как заглушка — `docs/STUB_REMOVAL.md`, раздел 7; документ детали — `docs/modeling/PART_DOCUMENT.md` |
| `CAD-CORE-006-TRANSACTION-MANAGER.md` | старая документная модель (CadDocument, команды, транзакции, ParameterStore) удалена как заглушка — `docs/STUB_REMOVAL.md`, раздел 7; документ детали — `docs/modeling/PART_DOCUMENT.md` |
| `CAD-CORE-007-HISTORY-STACK.md` | старая документная модель (CadDocument, команды, транзакции, ParameterStore) удалена как заглушка — `docs/STUB_REMOVAL.md`, раздел 7; документ детали — `docs/modeling/PART_DOCUMENT.md` |
| `CAD-CORE-008-DOCUMENT-EVENT-BUS.md` | старая документная модель (CadDocument, команды, транзакции, ParameterStore) удалена как заглушка — `docs/STUB_REMOVAL.md`, раздел 7; документ детали — `docs/modeling/PART_DOCUMENT.md` |
| `CAD-CORE-009-PARAMETER-STORE.md` | старая документная модель (CadDocument, команды, транзакции, ParameterStore) удалена как заглушка — `docs/STUB_REMOVAL.md`, раздел 7; документ детали — `docs/modeling/PART_DOCUMENT.md` |
| `CADCORE_PHASE_D_REPORT.md` | старая документная модель (CadDocument, команды, транзакции, ParameterStore) удалена как заглушка — `docs/STUB_REMOVAL.md`, раздел 7; документ детали — `docs/modeling/PART_DOCUMENT.md` |
| `CADCORE_PHASE_F_CHECKLIST_REPORT.txt` | отчёт о «фазе F/I» старого ядра: заявленные модули (`src/testing`, производительность, публикация) удалены, результаты тестов не воспроизводятся |
| `CADCORE_PHASE_F_PERFORMANCE_REPORT.json` | отчёт о «фазе F/I» старого ядра: заявленные модули (`src/testing`, производительность, публикация) удалены, результаты тестов не воспроизводятся |
| `CADCORE_PHASE_F_REPORT.md` | отчёт о «фазе F/I» старого ядра: заявленные модули (`src/testing`, производительность, публикация) удалены, результаты тестов не воспроизводятся |
| `CADCORE_PHASE_F_TEST_REPORT.html` | отчёт о «фазе F/I» старого ядра: заявленные модули (`src/testing`, производительность, публикация) удалены, результаты тестов не воспроизводятся |
| `CADCORE_PHASE_I_INTEGRATION_REPORT.md` | отчёт о «фазе F/I» старого ядра: заявленные модули (`src/testing`, производительность, публикация) удалены, результаты тестов не воспроизводятся |
| `CADVCORE_COMPATIBILITY_ANALYSIS.md` | анализ совместимости с удалённым ядром; не отражает текущий код |
| `CHANGELOG.md` | описание выдуманных выпусков «v1.0.0-PRODUCTION»/«v8.0» и модулей M1–M25; состав — `README.md` |
| `DEPLOYMENT_PROCEDURES.md` | план запуска/развёртывания несуществующего сервиса; окружения развёртывания нет (`docs/qa/QA.md`, раздел 4) |
| `FEATURES.md` | описание выдуманных выпусков «v1.0.0-PRODUCTION»/«v8.0» и модулей M1–M25; состав — `README.md` |
| `FINAL-TESTING-OPTIMIZATION-REPORT.md` | отчёт о приёмке/завершении/замерах с непроверенными результатами («7 469 тестов», «0 дефектов»); фактическая история — `docs/PROJECT_LOG.md`, замеры — `docs/qa/PERFORMANCE.md` |
| `FINAL-TESTING-PHASE-PLAN.md` | отчёт о приёмке/завершении/замерах с непроверенными результатами («7 469 тестов», «0 дефектов»); фактическая история — `docs/PROJECT_LOG.md`, замеры — `docs/qa/PERFORMANCE.md` |
| `FINAL_SUMMARY.md` | отчёт о приёмке/завершении/замерах с непроверенными результатами («7 469 тестов», «0 дефектов»); фактическая история — `docs/PROJECT_LOG.md`, замеры — `docs/qa/PERFORMANCE.md` |
| `GITHUB_DEPLOYMENT_GUIDE.md` | план запуска/развёртывания несуществующего сервиса; окружения развёртывания нет (`docs/qa/QA.md`, раздел 4) |
| `IMPLEMENTATION_ROADMAP_WITH_UI_ANALYSIS.md` | прежний план, заменён `docs/ROADMAP_GENERAL_CAD.md` |
| `IMPLEMENTATION_STATUS.md` | отчёт о приёмке/завершении/замерах с непроверенными результатами («7 469 тестов», «0 дефектов»); фактическая история — `docs/PROJECT_LOG.md`, замеры — `docs/qa/PERFORMANCE.md` |
| `INCIDENT_RESPONSE_PLAN.md` | план запуска/развёртывания несуществующего сервиса; окружения развёртывания нет (`docs/qa/QA.md`, раздел 4) |
| `KARPATHY_GUIDELINES.md` | рекомендовал «создавать stubs» вместо реализации — противоречит правилу «никаких заглушек» (`docs/PROJECT_PROCESS.md`, раздел 2) |
| `LAUNCH_CHECKLIST.md` | план запуска/развёртывания несуществующего сервиса; окружения развёртывания нет (`docs/qa/QA.md`, раздел 4) |
| `LAUNCH_READINESS_REPORT.md` | план запуска/развёртывания несуществующего сервиса; окружения развёртывания нет (`docs/qa/QA.md`, раздел 4) |
| `M12_M20_LAUNCH_REPORT.md` | отчёт/план этапов M1–M25: модули M14–M25 (коллаборация, плагины, облако, мобильный UI и т. п.) удалены как заглушки, числа тестов не подтверждаются |
| `M14_M20_AUTONOMOUS_COMPLETION.md` | отчёт/план этапов M1–M25: модули M14–M25 (коллаборация, плагины, облако, мобильный UI и т. п.) удалены как заглушки, числа тестов не подтверждаются |
| `M15_M20_BATCH_GENERATOR.sh` | сценарий генерации модулей M12–M25, удалённых как заглушки (`docs/STUB_REMOVAL.md`, раздел 8) |
| `M15_M20_CONTINUATION_SESSION_REPORT.md` | отчёт/план этапов M1–M25: модули M14–M25 (коллаборация, плагины, облако, мобильный UI и т. п.) удалены как заглушки, числа тестов не подтверждаются |
| `M15_M20_IMPLEMENTATION_PLAN.md` | отчёт/план этапов M1–M25: модули M14–M25 (коллаборация, плагины, облако, мобильный UI и т. п.) удалены как заглушки, числа тестов не подтверждаются |
| `M15_M20_IMPLEMENTATION_STATUS.md` | отчёт/план этапов M1–M25: модули M14–M25 (коллаборация, плагины, облако, мобильный UI и т. п.) удалены как заглушки, числа тестов не подтверждаются |
| `M15_M20_SESSION_REPORT.txt` | отчёт/план этапов M1–M25: модули M14–M25 (коллаборация, плагины, облако, мобильный UI и т. п.) удалены как заглушки, числа тестов не подтверждаются |
| `M16_M20_RAPID_IMPLEMENTATION.md` | отчёт/план этапов M1–M25: модули M14–M25 (коллаборация, плагины, облако, мобильный UI и т. п.) удалены как заглушки, числа тестов не подтверждаются |
| `M17_IMPLEMENTATION_SUMMARY.txt` | отчёт/план этапов M1–M25: модули M14–M25 (коллаборация, плагины, облако, мобильный UI и т. п.) удалены как заглушки, числа тестов не подтверждаются |
| `M1_PHASE_REPORT.md` | отчёт/план этапов M1–M25: модули M14–M25 (коллаборация, плагины, облако, мобильный UI и т. п.) удалены как заглушки, числа тестов не подтверждаются |
| `M20_COMPLETION_REPORT.md` | отчёт/план этапов M1–M25: модули M14–M25 (коллаборация, плагины, облако, мобильный UI и т. п.) удалены как заглушки, числа тестов не подтверждаются |
| `M22_COMPLETION_REPORT.txt` | отчёт/план этапов M1–M25: модули M14–M25 (коллаборация, плагины, облако, мобильный UI и т. п.) удалены как заглушки, числа тестов не подтверждаются |
| `M22_IMPLEMENTATION_COMPLETE.md` | отчёт/план этапов M1–M25: модули M14–M25 (коллаборация, плагины, облако, мобильный UI и т. п.) удалены как заглушки, числа тестов не подтверждаются |
| `M23_M25_ACCELERATION_PLAN.md` | отчёт/план этапов M1–M25: модули M14–M25 (коллаборация, плагины, облако, мобильный UI и т. п.) удалены как заглушки, числа тестов не подтверждаются |
| `M23_M25_COMPLETION_REPORT.txt` | отчёт/план этапов M1–M25: модули M14–M25 (коллаборация, плагины, облако, мобильный UI и т. п.) удалены как заглушки, числа тестов не подтверждаются |
| `P1-02-COMPLETION-REPORT.md` | отчёт о приёмке/завершении/замерах с непроверенными результатами («7 469 тестов», «0 дефектов»); фактическая история — `docs/PROJECT_LOG.md`, замеры — `docs/qa/PERFORMANCE.md` |
| `P1-03-ENGINEERING-ACCEPTANCE-TESTING-COMPLETION.md` | отчёт о приёмке/завершении/замерах с непроверенными результатами («7 469 тестов», «0 дефектов»); фактическая история — `docs/PROJECT_LOG.md`, замеры — `docs/qa/PERFORMANCE.md` |
| `PERF-REPORT.md` | отчёт о приёмке/завершении/замерах с непроверенными результатами («7 469 тестов», «0 дефектов»); фактическая история — `docs/PROJECT_LOG.md`, замеры — `docs/qa/PERFORMANCE.md` |
| `PHASE-AB-PROGRESS-2026-10-25.md` | отчёт о «завершении» фазы прежнего плана; результаты не воспроизводятся |
| `PHASE-B-COMPLETION-2026-10-25.md` | отчёт о «завершении» фазы прежнего плана; результаты не воспроизводятся |
| `PHASE2_COMPLETION_REPORT.txt` | отчёт о «завершении» фазы прежнего плана; результаты не воспроизводятся |
| `PHASE4_COMPREHENSIVE_TEST_REPORT.md` | отчёт «Phase 4» прежней команды: заглушки Eigen/json, неподтверждённые числа тестов; сборка и тесты описаны в `README.md`, `docs/qa/QA.md` |
| `PHASE4_CURRENT_STATUS.md` | отчёт «Phase 4» прежней команды: заглушки Eigen/json, неподтверждённые числа тестов; сборка и тесты описаны в `README.md`, `docs/qa/QA.md` |
| `PHASE4_ENVIRONMENT_SETUP.md` | отчёт «Phase 4» прежней команды: заглушки Eigen/json, неподтверждённые числа тестов; сборка и тесты описаны в `README.md`, `docs/qa/QA.md` |
| `PHASE4_EXECUTION_PLAN.md` | отчёт «Phase 4» прежней команды: заглушки Eigen/json, неподтверждённые числа тестов; сборка и тесты описаны в `README.md`, `docs/qa/QA.md` |
| `PHASE4_EXECUTION_REPORT.md` | отчёт «Phase 4» прежней команды: заглушки Eigen/json, неподтверждённые числа тестов; сборка и тесты описаны в `README.md`, `docs/qa/QA.md` |
| `PHASE4_EXECUTION_SUMMARY.txt` | отчёт «Phase 4» прежней команды: заглушки Eigen/json, неподтверждённые числа тестов; сборка и тесты описаны в `README.md`, `docs/qa/QA.md` |
| `PHASE4_GEOMETRY_COMPLETION.md` | отчёт «Phase 4» прежней команды: заглушки Eigen/json, неподтверждённые числа тестов; сборка и тесты описаны в `README.md`, `docs/qa/QA.md` |
| `PHASE4_PRODUCTION_REPORT.md` | отчёт «Phase 4» прежней команды: заглушки Eigen/json, неподтверждённые числа тестов; сборка и тесты описаны в `README.md`, `docs/qa/QA.md` |
| `PHASE4_PROGRESS_REPORT.md` | отчёт «Phase 4» прежней команды: заглушки Eigen/json, неподтверждённые числа тестов; сборка и тесты описаны в `README.md`, `docs/qa/QA.md` |
| `PHASE4_REAL_EXECUTION_RESULTS.md` | отчёт «Phase 4» прежней команды: заглушки Eigen/json, неподтверждённые числа тестов; сборка и тесты описаны в `README.md`, `docs/qa/QA.md` |
| `PHASE4_STATUS_UPDATED.md` | отчёт «Phase 4» прежней команды: заглушки Eigen/json, неподтверждённые числа тестов; сборка и тесты описаны в `README.md`, `docs/qa/QA.md` |
| `PHASE4_TESTING_STRATEGY.md` | отчёт «Phase 4» прежней команды: заглушки Eigen/json, неподтверждённые числа тестов; сборка и тесты описаны в `README.md`, `docs/qa/QA.md` |
| `PHASE4_TEST_INVENTORY.md` | отчёт «Phase 4» прежней команды: заглушки Eigen/json, неподтверждённые числа тестов; сборка и тесты описаны в `README.md`, `docs/qa/QA.md` |
| `PHASE4_TEST_RESULTS.md` | отчёт «Phase 4» прежней команды: заглушки Eigen/json, неподтверждённые числа тестов; сборка и тесты описаны в `README.md`, `docs/qa/QA.md` |
| `PHASE4_TEST_STRATEGY.md` | отчёт «Phase 4» прежней команды: заглушки Eigen/json, неподтверждённые числа тестов; сборка и тесты описаны в `README.md`, `docs/qa/QA.md` |
| `PHASE4_WEEK2B_2C_PLAN.md` | отчёт «Phase 4» прежней команды: заглушки Eigen/json, неподтверждённые числа тестов; сборка и тесты описаны в `README.md`, `docs/qa/QA.md` |
| `PHASE4_WEEK2_EXECUTION_REPORT.md` | отчёт «Phase 4» прежней команды: заглушки Eigen/json, неподтверждённые числа тестов; сборка и тесты описаны в `README.md`, `docs/qa/QA.md` |
| `PHASE_F_COMPLETION_SUMMARY.md` | отчёт о «фазе F/I» старого ядра: заявленные модули (`src/testing`, производительность, публикация) удалены, результаты тестов не воспроизводятся |
| `PHASE_M12_M20_ORCHESTRATION.yml` | сценарий генерации модулей M12–M25, удалённых как заглушки (`docs/STUB_REMOVAL.md`, раздел 8) |
| `PHASE_M22_M25_ORCHESTRATION.yml` | сценарий генерации модулей M12–M25, удалённых как заглушки (`docs/STUB_REMOVAL.md`, раздел 8) |
| `POLISH_PREPARATION_REPORT.md` | отчёт о приёмке/завершении/замерах с непроверенными результатами («7 469 тестов», «0 дефектов»); фактическая история — `docs/PROJECT_LOG.md`, замеры — `docs/qa/PERFORMANCE.md` |
| `QA-SIGN-OFF.md` | отчёт о приёмке/завершении/замерах с непроверенными результатами («7 469 тестов», «0 дефектов»); фактическая история — `docs/PROJECT_LOG.md`, замеры — `docs/qa/PERFORMANCE.md` |
| `QUICKSTART.md` | быстрый старт для удалённого интерфейса и мок-сервера; актуальный — `docs/USER_GUIDE/01_QUICK_START.md` |
| `README_QUICK_START.md` | быстрый старт для удалённого интерфейса и мок-сервера; актуальный — `docs/USER_GUIDE/01_QUICK_START.md` |
| `RELEASE_NOTES.txt` | описание выдуманных выпусков «v1.0.0-PRODUCTION»/«v8.0» и модулей M1–M25; состав — `README.md` |
| `SEC-AUDIT.md` | отчёт о приёмке/завершении/замерах с непроверенными результатами («7 469 тестов», «0 дефектов»); фактическая история — `docs/PROJECT_LOG.md`, замеры — `docs/qa/PERFORMANCE.md` |
| `SESSION-2026-10-25-FINAL-REPORT.md` | отчёт о приёмке/завершении/замерах с непроверенными результатами («7 469 тестов», «0 дефектов»); фактическая история — `docs/PROJECT_LOG.md`, замеры — `docs/qa/PERFORMANCE.md` |
| `UPGRADE_GUIDE.md` | описание выдуманных выпусков «v1.0.0-PRODUCTION»/«v8.0» и модулей M1–M25; состав — `README.md` |
| `WINDOWS_SETUP.md` | ссылается на несуществующие `build_occt.ps1`, `dev-server.ps1`, `check-readiness.ps1`, мок-сервер; сборка под Windows не проверялась |
| `WORK_COMPLETED.md` | отчёт о приёмке/завершении/замерах с непроверенными результатами («7 469 тестов», «0 дефектов»); фактическая история — `docs/PROJECT_LOG.md`, замеры — `docs/qa/PERFORMANCE.md` |
| `production_launch.log` | журнал фиктивного «production launch»; развёртывания у проекта нет (`docs/qa/QA.md`, раздел 4) |
| `launch_package/ (10 файлов)` | выход удалённого генератора `src/testing/launch_package.cpp`: «v6.0.0 Production», коллаборация, FEA — нет в коде |
| `docs/AB_TESTING_GUIDE.md` | аналитика, A/B-тесты, GrowthBook, план вывода на рынок — модули удалены (`docs/STUB_REMOVAL.md`, раздел 8) |
| `docs/ANALYTICS_API_REFERENCE.md` | аналитика, A/B-тесты, GrowthBook, план вывода на рынок — модули удалены (`docs/STUB_REMOVAL.md`, раздел 8) |
| `docs/ANALYTICS_TRACKING.md` | аналитика, A/B-тесты, GrowthBook, план вывода на рынок — модули удалены (`docs/STUB_REMOVAL.md`, раздел 8) |
| `docs/API_REFERENCE.md` | справочник API/компонентов удалённого кода (Optimization APIs, M22–M25); API модулей — `docs/{kernel,modeling,assembly,drawing,ui}/` |
| `docs/ARCHITECTURE.md` | архитектура/спецификация «BendCore Web» (бэкенд, БД, микросервисы) — не соответствует коду; архитектура — `docs/ROADMAP_GENERAL_CAD.md`, раздел 3 |
| `docs/ARCHITECTURE/API_COMPLETE_REFERENCE.md` | архитектура/спецификация «BendCore Web» (бэкенд, БД, микросервисы) — не соответствует коду; архитектура — `docs/ROADMAP_GENERAL_CAD.md`, раздел 3 |
| `docs/ARCHITECTURE/BACKEND_WEB_IMPLEMENTATION_COMPLETE.md` | архитектура/спецификация «BendCore Web» (бэкенд, БД, микросервисы) — не соответствует коду; архитектура — `docs/ROADMAP_GENERAL_CAD.md`, раздел 3 |
| `docs/ARCHITECTURE/BENDCORE_2027_IMPLEMENTATION_ROADMAP.md` | архитектура/спецификация «BendCore Web» (бэкенд, БД, микросервисы) — не соответствует коду; архитектура — `docs/ROADMAP_GENERAL_CAD.md`, раздел 3 |
| `docs/ARCHITECTURE/BENDCORE_COMPLETE_SPECIFICATION.md` | архитектура/спецификация «BendCore Web» (бэкенд, БД, микросервисы) — не соответствует коду; архитектура — `docs/ROADMAP_GENERAL_CAD.md`, раздел 3 |
| `docs/ARCHITECTURE/BENDCORE_FINAL_SPECIFICATION.md` | архитектура/спецификация «BendCore Web» (бэкенд, БД, микросервисы) — не соответствует коду; архитектура — `docs/ROADMAP_GENERAL_CAD.md`, раздел 3 |
| `docs/ARCHITECTURE/BENDCORE_IMPLEMENTATION_COMPLETE.md` | архитектура/спецификация «BendCore Web» (бэкенд, БД, микросервисы) — не соответствует коду; архитектура — `docs/ROADMAP_GENERAL_CAD.md`, раздел 3 |
| `docs/ARCHITECTURE/BENDCORE_PROJECT_ROADMAP.md` | архитектура/спецификация «BendCore Web» (бэкенд, БД, микросервисы) — не соответствует коду; архитектура — `docs/ROADMAP_GENERAL_CAD.md`, раздел 3 |
| `docs/ARCHITECTURE/BENDCORE_SPECIFICATION_FINAL_LOCKED.md` | архитектура/спецификация «BendCore Web» (бэкенд, БД, микросервисы) — не соответствует коду; архитектура — `docs/ROADMAP_GENERAL_CAD.md`, раздел 3 |
| `docs/ARCHITECTURE/IMPLEMENTATION_SUMMARY.md` | архитектура/спецификация «BendCore Web» (бэкенд, БД, микросервисы) — не соответствует коду; архитектура — `docs/ROADMAP_GENERAL_CAD.md`, раздел 3 |
| `docs/CADCORE_PHASE_C_REPORT.md` | старая документная модель (CadDocument, команды, транзакции, ParameterStore) удалена как заглушка — `docs/STUB_REMOVAL.md`, раздел 7; документ детали — `docs/modeling/PART_DOCUMENT.md` |
| `docs/CHANGELOG.md` | журнал выдуманного выпуска «v8.0» |
| `docs/CLOUD_DEPLOYMENT.md` | развёртывание несуществующего облачного сервиса (`bendcore.app`, Kubernetes); не проверялось |
| `docs/COMPONENTS.md` | справочник API/компонентов удалённого кода (Optimization APIs, M22–M25); API модулей — `docs/{kernel,modeling,assembly,drawing,ui}/` |
| `docs/COMPONENTS_IMPLEMENTATION.md` | справочник API/компонентов удалённого кода (Optimization APIs, M22–M25); API модулей — `docs/{kernel,modeling,assembly,drawing,ui}/` |
| `docs/DEPLOYMENT.md` | развёртывание несуществующего облачного сервиса (`bendcore.app`, Kubernetes); не проверялось |
| `docs/DEPLOYMENT/COMPLETE_DEPLOYMENT_SUMMARY.md` | развёртывание несуществующего облачного сервиса (`bendcore.app`, Kubernetes); не проверялось |
| `docs/DEPLOYMENT/FRIDAY_DEPLOYMENT_CHECKLIST.md` | развёртывание несуществующего облачного сервиса (`bendcore.app`, Kubernetes); не проверялось |
| `docs/DEPLOYMENT/FRIDAY_DEPLOYMENT_RUNBOOK.md` | развёртывание несуществующего облачного сервиса (`bendcore.app`, Kubernetes); не проверялось |
| `docs/DEPLOYMENT/MONDAY_PHASE3_KICKOFF.md` | развёртывание несуществующего облачного сервиса (`bendcore.app`, Kubernetes); не проверялось |
| `docs/DEPLOYMENT/PHASE2_EXAMPLES.md` | развёртывание несуществующего облачного сервиса (`bendcore.app`, Kubernetes); не проверялось |
| `docs/DEPLOYMENT/PHASE2_IMPLEMENTATION.md` | развёртывание несуществующего облачного сервиса (`bendcore.app`, Kubernetes); не проверялось |
| `docs/DEPLOYMENT/PHASE2_README.md` | развёртывание несуществующего облачного сервиса (`bendcore.app`, Kubernetes); не проверялось |
| `docs/DEPLOYMENT/PHASE2_STATUS.md` | развёртывание несуществующего облачного сервиса (`bendcore.app`, Kubernetes); не проверялось |
| `docs/DEVELOPER_GUIDE.md` | руководство разработчика для прежней структуры (бэкенд, мок-данные, «Stage 2-3»); актуально — `README.md`, `CONTRIBUTING.md`, `docs/PROJECT_PROCESS.md` |
| `docs/DEVELOPMENT_GUIDE.md` | руководство разработчика для прежней структуры (бэкенд, мок-данные, «Stage 2-3»); актуально — `README.md`, `CONTRIBUTING.md`, `docs/PROJECT_PROCESS.md` |
| `docs/DEVOPS_README.md` | развёртывание несуществующего облачного сервиса (`bendcore.app`, Kubernetes); не проверялось |
| `docs/DOCKER.md` | развёртывание несуществующего облачного сервиса (`bendcore.app`, Kubernetes); не проверялось |
| `docs/DOCUMENTATION_COMPLETION_CHECKLIST.md` | отчёт о «завершении» документации/проверки с непроверенными результатами |
| `docs/FINAL_VERIFICATION_REPORT.md` | отчёт о «завершении» документации/проверки с непроверенными результатами |
| `docs/GROWTHBOOK_SETUP.md` | аналитика, A/B-тесты, GrowthBook, план вывода на рынок — модули удалены (`docs/STUB_REMOVAL.md`, раздел 8) |
| `docs/GUIDES/INSTALLATION_GUIDE.md` | руководство разработчика для прежней структуры (бэкенд, мок-данные, «Stage 2-3»); актуально — `README.md`, `CONTRIBUTING.md`, `docs/PROJECT_PROCESS.md` |
| `docs/GUIDES/PERFORMANCE_OPTIMIZATION_GUIDE.md` | оптимизации удалённых калькуляторов смещения (`docs/STUB_REMOVAL.md`, раздел 8) с незамеренными числами |
| `docs/GUIDES/PRODUCTION_DEPLOYMENT_GUIDE.md` | развёртывание несуществующего облачного сервиса (`bendcore.app`, Kubernetes); не проверялось |
| `docs/IMPORT_API.md` | описывает формат и API импорта, не совпадающие с кодом (`include/bendcore/tube_model.hpp`, `import_csv`/`import_json`); импорт CAD — `docs/kernel/`, `docs/USER_GUIDE/09_IMPORT_EXPORT.md` |
| `docs/IMPORT_API_REFERENCE.md` | описывает формат и API импорта, не совпадающие с кодом (`include/bendcore/tube_model.hpp`, `import_csv`/`import_json`); импорт CAD — `docs/kernel/`, `docs/USER_GUIDE/09_IMPORT_EXPORT.md` |
| `docs/IMPORT_GUIDE.md` | описывает формат и API импорта, не совпадающие с кодом (`include/bendcore/tube_model.hpp`, `import_csv`/`import_json`); импорт CAD — `docs/kernel/`, `docs/USER_GUIDE/09_IMPORT_EXPORT.md` |
| `docs/IMPORT_USER_GUIDE.md` | описывает формат и API импорта, не совпадающие с кодом (`include/bendcore/tube_model.hpp`, `import_csv`/`import_json`); импорт CAD — `docs/kernel/`, `docs/USER_GUIDE/09_IMPORT_EXPORT.md` |
| `docs/INTERFACE_SPECIFICATION.md` | справочник API/компонентов удалённого кода (Optimization APIs, M22–M25); API модулей — `docs/{kernel,modeling,assembly,drawing,ui}/` |
| `docs/MARKET_LAUNCH_PLAN.md` | аналитика, A/B-тесты, GrowthBook, план вывода на рынок — модули удалены (`docs/STUB_REMOVAL.md`, раздел 8) |
| `docs/MATERIALS_REFERENCE.md` | материалы и «проверка стандартов» трубного модуля без источников и тестов; материалы ядра — `docs/kernel/ANALYSIS.md`, сверка — `docs/STANDARDS_VERIFICATION.md` |
| `docs/OCCT_INTEGRATION.md` | устаревшая интеграция OCCT («импорт 100 МБ < 1 с» — по замеру 22 с на 65 МиБ, `docs/qa/PERFORMANCE.md`); сборка OCCT — `docs/kernel/OCCT_BUILD.md` |
| `docs/OFFSET_OPTIMIZATIONS.md` | оптимизации удалённых калькуляторов смещения (`docs/STUB_REMOVAL.md`, раздел 8) с незамеренными числами |
| `docs/PARASOLID_INTEGRATION.md` | интеграция Parasolid не входит в v1.0 (`docs/ROADMAP_GENERAL_CAD.md`, 2.5) и не реализована |
| `docs/PHASE2/COMPLETE_FUNCTION_MATRIX.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/API_REFERENCE.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/COLLABORATION_PROTOCOL.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/CUSTOMER_COMMUNICATION.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/DEPLOYMENT_CHECKLIST.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/FEA_SIMULATION_GUIDE.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/MANUFACTURING_WORKFLOW.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/MARCH2027_ROADMAP.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PERFORMANCE_BENCHMARKS.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE3_IMPLEMENTATION.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE4_TESTING_PLAN.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE5_DEPLOYMENT.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_27_README.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_29_ARCHITECTURE.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_29_COMMAND_REFERENCE.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_29_COMPLETION_REPORT.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_29_IMPLEMENTATION_PLAN.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_29_MANIFEST.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_29_QUICK_START.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_29_TEST_REPORT.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_30_COMPLETION_STATUS.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_30_PART_A_COMPLETION_REPORT.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_30_PLAN.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_31_COMPLETION_REPORT.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_31_PREVIEW.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_32_INTEGRATION.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_33_IMPLEMENTATION.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_34_IMPLEMENTATION.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_35A_IMPLEMENTATION.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_35B_IMPLEMENTATION.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_35C_IMPLEMENTATION.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PHASE_36_IMPLEMENTATION.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/PROJECT_STATUS.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/Q2-2027-FINAL-COMPLETION-REPORT.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/Q3-2027-EXECUTION-SUMMARY.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/Q3-2027-PROGRESS-TRACKER.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/Q3-2027-ROADMAP.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/Q4-2027-CONDITIONAL-PLAN.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/Q4_TASK_1_ADVANCED_DRAWING_FEATURES_SUMMARY.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/Q4_TASK_2_IMPLEMENTATION_SUMMARY.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/ROUTING_ALGORITHM.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/SECURITY_AUDIT.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/SUPPORT_PROCEDURES.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/TASK_1_5_1_8_COMPLETION_REPORT.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PHASE3/TASK_2_COMPLETION_REPORT.md` | отчёт/план прежних фаз 27–36 и кварталов 2027 (коллаборация, FEA-интерфейс, маршрутизация, Parasolid): функциональности нет или удалена |
| `docs/PRODUCTION_COMPONENTS_SUMMARY.md` | справочник API/компонентов удалённого кода (Optimization APIs, M22–M25); API модулей — `docs/{kernel,modeling,assembly,drawing,ui}/` |
| `docs/SECURITY_AUDIT_CHECKLIST.md` | чек-лист аудита безопасности удалённого модуля `security`; аудит не проводился |
| `docs/STAGE_2_3_ROADMAP.md` | руководство разработчика для прежней структуры (бэкенд, мок-данные, «Stage 2-3»); актуально — `README.md`, `CONTRIBUTING.md`, `docs/PROJECT_PROCESS.md` |
| `docs/STANDARDS_COMPLIANCE.md` | материалы и «проверка стандартов» трубного модуля без источников и тестов; материалы ядра — `docs/kernel/ANALYSIS.md`, сверка — `docs/STANDARDS_VERIFICATION.md` |
| `docs/TASK-1-PHASE-3-OCCT-DETAILED-SPEC.md` | устаревшая интеграция OCCT («импорт 100 МБ < 1 с» — по замеру 22 с на 65 МиБ, `docs/qa/PERFORMANCE.md`); сборка OCCT — `docs/kernel/OCCT_BUILD.md` |
| `docs/TASK-2-TEMPLATES-DETAILED-SPEC.md` | система шаблонов трубного модуля: заявленные проценты совпадения и библиотека не проверены тестами |
| `docs/TASK-3-SEARCH-DETAILED-SPEC.md` | спецификация поиска, которого нет в коде |
| `docs/TASK-4-COMPLETION-REPORT.md` | аналитика, A/B-тесты, GrowthBook, план вывода на рынок — модули удалены (`docs/STUB_REMOVAL.md`, раздел 8) |
| `docs/TASK-4-GROWTHBOOK-DETAILED-SPEC.md` | аналитика, A/B-тесты, GrowthBook, план вывода на рынок — модули удалены (`docs/STUB_REMOVAL.md`, раздел 8) |
| `docs/TEMPLATE_API_REFERENCE.md` | система шаблонов трубного модуля: заявленные проценты совпадения и библиотека не проверены тестами |
| `docs/TEMPLATE_MATCHING_ALGORITHM.md` | система шаблонов трубного модуля: заявленные проценты совпадения и библиотека не проверены тестами |
| `docs/TEMPLATE_SYSTEM_QUICKSTART.md` | система шаблонов трубного модуля: заявленные проценты совпадения и библиотека не проверены тестами |
| `docs/TEMPLATE_USER_GUIDE.md` | система шаблонов трубного модуля: заявленные проценты совпадения и библиотека не проверены тестами |
| `docs/audit/ACTION_PLAN_NEXT_PHASE.md` | аудиты и планы октября 2026 (Windows-сборка, «ZERO DEFECTS», шаблоны отчётов) по коду, который с тех пор удалён; выводы учтены в `docs/ROADMAP_GENERAL_CAD.md`, раздел 1 |
| `docs/audit/API_MIGRATION_PLAN.md` | аудиты и планы октября 2026 (Windows-сборка, «ZERO DEFECTS», шаблоны отчётов) по коду, который с тех пор удалён; выводы учтены в `docs/ROADMAP_GENERAL_CAD.md`, раздел 1 |
| `docs/audit/AUTONOMOUS_PROGRESS.md` | аудиты и планы октября 2026 (Windows-сборка, «ZERO DEFECTS», шаблоны отчётов) по коду, который с тех пор удалён; выводы учтены в `docs/ROADMAP_GENERAL_CAD.md`, раздел 1 |
| `docs/audit/CAD_ACCEPTANCE_TESTS.md` | аудиты и планы октября 2026 (Windows-сборка, «ZERO DEFECTS», шаблоны отчётов) по коду, который с тех пор удалён; выводы учтены в `docs/ROADMAP_GENERAL_CAD.md`, раздел 1 |
| `docs/audit/CAD_FILE_TEST_RESULTS_TEMPLATE.md` | аудиты и планы октября 2026 (Windows-сборка, «ZERO DEFECTS», шаблоны отчётов) по коду, который с тех пор удалён; выводы учтены в `docs/ROADMAP_GENERAL_CAD.md`, раздел 1 |
| `docs/audit/CI_WINDOWS_VALIDATION.md` | аудиты и планы октября 2026 (Windows-сборка, «ZERO DEFECTS», шаблоны отчётов) по коду, который с тех пор удалён; выводы учтены в `docs/ROADMAP_GENERAL_CAD.md`, раздел 1 |
| `docs/audit/DEFECT_REGISTER.md` | аудиты и планы октября 2026 (Windows-сборка, «ZERO DEFECTS», шаблоны отчётов) по коду, который с тех пор удалён; выводы учтены в `docs/ROADMAP_GENERAL_CAD.md`, раздел 1 |
| `docs/audit/EXECUTION_GUIDE_WINDOWS.md` | аудиты и планы октября 2026 (Windows-сборка, «ZERO DEFECTS», шаблоны отчётов) по коду, который с тех пор удалён; выводы учтены в `docs/ROADMAP_GENERAL_CAD.md`, раздел 1 |
| `docs/audit/FULL_AUDIT_REPORT.md` | аудиты и планы октября 2026 (Windows-сборка, «ZERO DEFECTS», шаблоны отчётов) по коду, который с тех пор удалён; выводы учтены в `docs/ROADMAP_GENERAL_CAD.md`, раздел 1 |
| `docs/audit/INDEPENDENT_REAUDIT_REPORT.md` | аудиты и планы октября 2026 (Windows-сборка, «ZERO DEFECTS», шаблоны отчётов) по коду, который с тех пор удалён; выводы учтены в `docs/ROADMAP_GENERAL_CAD.md`, раздел 1 |
| `docs/audit/PART_A_WINDOWS_DETAILED_STEPS.md` | аудиты и планы октября 2026 (Windows-сборка, «ZERO DEFECTS», шаблоны отчётов) по коду, который с тех пор удалён; выводы учтены в `docs/ROADMAP_GENERAL_CAD.md`, раздел 1 |
| `docs/audit/QUICK_REFERENCE_CARD.md` | аудиты и планы октября 2026 (Windows-сборка, «ZERO DEFECTS», шаблоны отчётов) по коду, который с тех пор удалён; выводы учтены в `docs/ROADMAP_GENERAL_CAD.md`, раздел 1 |
| `docs/audit/REMAINING_RISKS.md` | аудиты и планы октября 2026 (Windows-сборка, «ZERO DEFECTS», шаблоны отчётов) по коду, который с тех пор удалён; выводы учтены в `docs/ROADMAP_GENERAL_CAD.md`, раздел 1 |
| `docs/audit/TEST_VALIDATION_REPORT.md` | аудиты и планы октября 2026 (Windows-сборка, «ZERO DEFECTS», шаблоны отчётов) по коду, который с тех пор удалён; выводы учтены в `docs/ROADMAP_GENERAL_CAD.md`, раздел 1 |
| `docs/audit/TROUBLESHOOTING_GUIDE.md` | аудиты и планы октября 2026 (Windows-сборка, «ZERO DEFECTS», шаблоны отчётов) по коду, который с тех пор удалён; выводы учтены в `docs/ROADMAP_GENERAL_CAD.md`, раздел 1 |
| `docs/audit/WINDOWS_AND_CAD_CHECKLIST.md` | аудиты и планы октября 2026 (Windows-сборка, «ZERO DEFECTS», шаблоны отчётов) по коду, который с тех пор удалён; выводы учтены в `docs/ROADMAP_GENERAL_CAD.md`, раздел 1 |
| `docs/audit/WINDOWS_BUILD_TEST_RESULTS_TEMPLATE.md` | аудиты и планы октября 2026 (Windows-сборка, «ZERO DEFECTS», шаблоны отчётов) по коду, который с тех пор удалён; выводы учтены в `docs/ROADMAP_GENERAL_CAD.md`, раздел 1 |
| `docs/audits/BendCore_Latest_Audit_2026-10-30.md` | аудиты и планы октября 2026 (Windows-сборка, «ZERO DEFECTS», шаблоны отчётов) по коду, который с тех пор удалён; выводы учтены в `docs/ROADMAP_GENERAL_CAD.md`, раздел 1 |
| `docs/bendcore_analysis.md` | сверка «90 % реализовано» прежнего трубного кода со спецификацией октября 2026; не отражает текущий код |
| `docs/bendcore_detailed_verification.md` | сверка «90 % реализовано» прежнего трубного кода со спецификацией октября 2026; не отражает текущий код |
