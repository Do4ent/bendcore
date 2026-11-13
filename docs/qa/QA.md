# Контроль качества bendcore

Владелец: инженер качества (`.claude/agents/cad-qa-engineer.md`). Основание:
`docs/PROJECT_PROCESS.md` (разделы 2, 3, 5), `docs/ROADMAP_GENERAL_CAD.md` (фаза 0, п. 6).

## 1. Проверка «никаких заглушек» — `scripts/check_no_stubs.sh`

### Режимы

```bash
scripts/check_no_stubs.sh                      # полная проверка src, include, tests, web/<модуль>
scripts/check_no_stubs.sh --diff origin/ccr-d188a25e-rml7z8   # + добавленные строки относительно базы
scripts/check_no_stubs.sh --list-rules         # список правил
```

* Полная проверка всегда сканирует **весь** `src`, `include`, `tests` — и новые модули, и
  старый код (трубный модуль, расчёты `src/simulation`, сервер, CLI, Electron `src/ui`,
  `core_api`, WASM-мост) — а также `web/{kernel-viewer,part-modeler,assembly,drawing}`;
  отсутствующие каталоги пропускаются. С 2026-11-13 (`docs/STUB_REMOVAL.md`, раздел 9) старый
  код очищен до 0 нарушений, поэтому новое нарушение в любом его файле роняет CI
  (`qa_check_no_stubs_tree`, задача `no-stubs`).
* `--diff <ref>` дополнительно проверяет **только добавленные строки** во всех файлах кода,
  изменённых относительно `merge-base(<ref>, HEAD)`, включая незакоммиченные изменения
  отслеживаемых файлов (неотслеживаемые новые файлы — после `git add`) — в том числе вне
  каталогов полной проверки (`scripts/`, `backend/`, `examples/` и др.).
* `--no-full` — только diff; `--root <dir>` — другой корень; `--paths <p>...` — свои пути.
* Файлы кода: C/C++ (`.c .cc .cpp .cxx .h .hh .hpp .hxx .ipp .inl .tpp`), CMake, `.sh`,
  `.py`, JS/TS (`.js .mjs .cjs .jsx .ts .mts .cts .tsx`). Исключены: `tests/qa/fixtures/`
  (намеренные нарушения), сам скрипт и его самотест (содержат шаблоны), `third_party/`,
  `node_modules/`. Документация (`*.md`) не проверяется.
* Тестовыми считаются файлы в каталогах `tests/`, `test/`, `__tests__/`, `e2e*/` и файлы
  `*.test.*`, `*.spec.*` (Jest, `node:test`, Playwright): для них не действуют `fake-value`,
  `empty-body`, `literal-return` (двойники законны), зато действуют `trivial-assert` и
  `no-assertions`.
* Вывод: `файл:строка: [правило] пояснение | фрагмент`. Код выхода: 0 — чисто,
  1 — нарушения, 2 — ошибка запуска (неизвестный аргумент, нет ссылки git).
  Требуются bash, perl 5, git.

### Правила

| Правило | Что ловит | Как снижены ложные срабатывания |
|---|---|---|
| `todo` | `TODO`, `FIXME`, `XXX`, `HACK` | только заглавными целыми словами (`todo_list` не ловится) |
| `placeholder` | «placeholder» | не ловятся `std::placeholders`, CSS `::placeholder`, атрибут `placeholder="…"`/`placeholder={…}`, поле `placeholder: '…'`, тип свойства `placeholder?: string;` и обращение `x.placeholder` |
| `real-implementation` | «in a real/full/production implementation», «real implementation would/should» | только устойчивые обороты |
| `would` | «would normally/actually/typically/go here», «we would», «would be computed here/later», «real code would» | одиночное «would» («иначе нормаль была бы перевёрнута») не ловится |
| `stub` | «stub», «stubs», «stubbed», идентификаторы `zlib_stub` | `stubborn`, `check_no_stubs` не ловятся |
| `not-implemented` | «not (yet) implemented», «unimplemented», «not supported yet», «coming soon» | — |
| `simplified` | «simplified implementation/version/approach/…», комментарий, начинающийся с «Simplified», «simplified:» | глагол «simplify» в описании алгоритма не ловится |
| `temporary` | «for now», «for the time being», «for simplicity», «temporary hack/solution/…» | — |
| `fake-value` | «dummy/fake/hardcoded/mock value/result/data…», `return x; // hardcoded/assume/for now` | только код продукта: в тестах двойники законны |
| `trivial-assert` | `EXPECT_TRUE(true)`, `ASSERT_FALSE(false)`, `assert(true)`, `SUCCEED()`, `EXPECT_EQ(x, x)`; JS/TS: `expect(true).toBe(true)`, `expect(x).toEqual(x)`, `assert.ok(true)`, `assert(1, …)`, `assert.equal(x, x)` | — |
| `empty-body` | функция с пустым телом `{}`; JS/TS: `function f(…) {}`, метод `m(…) {}`, `const f = (…) => {}` | не ловятся конструкторы, деструкторы, `= default`, лямбды, списки инициализации; в JS/TS — анонимные обработчики (`() => {}` как аргумент или значение по умолчанию), `catch {}`, `constructor(…) {}`, сигнатуры интерфейсов и `abstract`; только код продукта |
| `literal-return` | функция **с параметрами**, тело которой — только `return <литерал>;`; JS/TS: также `(x) => 0`, `(x) => ({ ok: true })`, `return []`/`null`/`undefined` | функции без параметров (свойства вида `is_empty() { return true; }`) и тесты не ловятся |
| `literal-success` | `success/ok/valid/... = true;`, перед которым в теле только объявления, а после — сразу `return` | инициализация «верно, пока проверка не провалится» и фабрики `success_result`/`ok` не ловятся |
| `no-assertions` | тестовый файл (`main` или `TEST`) без единой проверки (`assert`, `EXPECT_*`, `check(`, `return 1`, `throw`…); JS/TS: файл с `test(`/`it(` без `expect`/`assert`/`throw`/`process.exit(1)` | — |
| `bad-suppression` | `qa-allow` без причины или с неизвестным правилом | — |

Многострочные правила (`empty-body`, `literal-return`, `literal-success`, `no-assertions`)
работают по тексту C++ с вырезанными комментариями и строками; для JS/TS (кроме `.d.ts`/`.d.mts`)
— `empty-body`, `literal-return`, `no-assertions` по тексту с вырезанными комментариями, строками
и шаблонными строками. На старом фронтенде (`src/ui`, `src/components`) JS/TS-правила находят
настоящие заглушки (`expect(true).toBe(true)` в `__tests__/*.spec.ts`, пустые
`dispose()`/`onKeyUp()`), ложных срабатываний при выборке не найдено.

Контроль точности: до очистки старого кода скрипт находил на `src include tests` ~1100
нарушений, к 2026-11-13 — 674; разбор каждого (`docs/STUB_REMOVAL.md`, раздел 9) подтвердил, что
почти все — настоящие заглушки («Placeholder implementation», `EXPECT_TRUE(true)`, «For now,
just return…»). Ложные срабатывания (`result.valid = true` перед проверками, фабрика
`success_result`, `SetUp() override {}` в тестах, свойство `placeholder?: string` и
`field.placeholder` в React) устранены в правилах и закреплены в `tests/qa/fixtures/clean/`;
подавлений `qa-allow` в старом коде не понадобилось.

### Подавление ложного срабатывания

На той же строке или на строке выше — комментарий с именем правила и **обязательной
причиной** (не короче 12 символов):

```cpp
int legacy_bridge(int /*code*/) {  // qa-allow(literal-return): ABI shim, value documented in module docs
  return 1;
}
```

`qa-allow(<правило>)` без причины или с неизвестным правилом само является нарушением
(`bad-suppression`). Подавление действует только на указанное правило. Ревьюер
(руководитель проекта) проверяет каждую причину при приёмке.

### Самотест

`tests/qa/test_check_no_stubs.sh` (ctest: `qa_check_no_stubs_selftest`):

1. `tests/qa/fixtures/violations` → множество `файл:строка:правило` точно совпадает с
   `tests/qa/fixtures/expected_violations.txt` (44 нарушения: все правила для C++ и
   JS/TS-правила на `src/ui/kernel/bad_viewer.ts`, `tests/wasm/bad_kernel.test.mjs`,
   `tests/e2e-kernel/no_checks.spec.ts`), код 1;
2. `tests/qa/fixtures/clean` (законные конструкции) → 0 нарушений, код 0;
3. `--diff` во временном git-репозитории: сообщаются только добавленные строки
   (закоммиченные и в рабочей копии, включая `.tsx`), не сообщаются старые нарушения базы,
   фикстуры и `.md`;
4. полная проверка без `--paths` находит нарушение в каждом каталоге нового кода
   (`src/wasm/kernel_bindings.cpp`, `.mjs`, `.d.mts`, `src/ui/kernel/*.tsx`, `src/modeling`,
   `src/assembly_v2`, `src/drawing_v2`, `tests/wasm`, `tests/e2e-kernel`, `web/kernel-viewer`)
   и в старом коде (`src/legacy`, `src/server`, `src/ui/RibbonBar.tsx`, `include/bendcore`,
   `tests/test_*.cpp`) и не сканирует `examples/` и `tests/qa/fixtures/`;
5. ошибки запуска → код 2.

При изменении фикстур `expected_violations.txt` правится вручную — только после проверки,
что каждое новое нарушение действительно ожидаемо.

## 2. Корпус STEP — `tests/corpus/`

Описание, источники и лицензии: `tests/corpus/README.md`. Кратко:

* `generated/` — 13 моделей от `tests/corpus/generate_corpus.cpp` (OCCT): коробка,
  цилиндр, труба, пластина с отверстием, тор, шар, усечённый конус, две детали со
  скруглениями, уголок, многотельная деталь, сборка XCAF с именами и цветами в AP214 и
  AP242. Протоколы AP203/AP214/AP242.
* `manifest.json` — ожидаемые значения **по формулам** (объём, площадь, габариты, число
  тел/граней, имена, цвета, `FILE_SCHEMA`), не по измерению OCCT.
* `external/` — для публичных моделей сторонних CAD; на 2026-11-06 загрузка NIST заблокирована
  сетевым прокси (403), подробности в README корпуса.

## 3. Регрессия импорта — `test_step_corpus`

`tests/corpus/test_step_corpus.cpp`, цели и тесты в `cmake/qa.cmake` (только при
`BENDCORE_WITH_OCCT=ON`):

| ctest | Что делает |
|---|---|
| `corpus_step_committed` | импорт закоммиченного `tests/corpus/generated` |
| `corpus_generate` → `corpus_step_fresh` | генерация корпуса текущим OCCT в `build-occt/corpus-fresh` и его импорт (fixture ctest) |
| `corpus_step_external` | только если есть `tests/corpus/external/manifest.json` |
| `qa_check_no_stubs_selftest`, `qa_check_no_stubs_tree` | самотест и полная проверка заглушек |

Для каждой модели: `STEPReader::read` успешен; число свободных форм; имя верхней формы
(`shape_0_name`); `solid_count`, `face_count`; `BRepCheck_Analyzer` формы из
`get_shape_data()`; объём и площадь (`BRepGProp`) с допуском из манифеста; точные габариты
(`BRepBndLib::AddOptimal`, ±1e-4 мм); габариты читателя содержат точные и шире не более чем
на 5% диагонали; `FILE_SCHEMA` в заголовке; для сборок — имена деталей, число вхождений и
цвета (±1e-3).

### Известные ограничения читателя (`src/step_reader.cpp`)

`STEPReader` возвращает только верхние (свободные) формы и их имена; **имена деталей
сборки и цвета он не отдаёт**. Поэтому тест проверяет их, читая тот же файл через XCAF
(`STEPCAFControl_Reader`) напрямую. Когда читатель начнёт отдавать структуру сборки, эти
проверки нужно перевести на его API.

## 4. CI — `.github/workflows/qa.yml`, `wasm.yml`, `production-deploy.yml`

| Workflow / задача | Когда | Что делает |
|---|---|---|
| `qa.yml` / `no-stubs` | push, PR | самотест; `check_no_stubs.sh --diff origin/<base_ref>` на PR, `--diff <before>` на push (на новой ветке — только полная проверка); нарушения выводятся аннотациями `::error file=…,line=…` и роняют задачу |
| `qa.yml` / `occt-native` | push, PR | apt: Ninja, GTest, Eigen, **zlib и expat** (3MF в `bendcore_kernel_analysis`, без них тест молча пропускается), **`fonts-dejavu-core`** (системный шрифт нативных тестов чертежа). OCCT 8.0.1 скриптом `third_party/build_occt.sh` (с проверками в Release и C++20, `docs/kernel/OCCT_BUILD.md`, раздел 6) в `third_party/occt-install-checked` с `actions/cache` установки (ключ `occt-native-checks<OCCT_RELEASE_CHECKS>-V<версия>-ubuntu-24.04-<hashFiles(occt_toolkits.txt, build_occt.sh)>`). Полная сборка `build-occt` (RelWithDebInfo), `ctest --parallel 4 -E '^(performance\|offset_performance\|stress_.*)$'`, затем **отдельным последовательным шагом** (выполняется и после падения предыдущего) `-R '^(performance\|offset_performance\|stress_.*)$'` с `BENDCORE_STRESS_TIME_SCALE=1.5`; отчёт нагрузки (`stress-report.jsonl`) — таблицей в сводку и артефактом `stress-report`. Затем образ сервиса и запросы к нему. Артефакты: свежий корпус (всегда), журналы и входы дефектов fuzz (при падении) |
| `qa.yml` / `fuzz` | после `occt-native` | clang 18 + `libclang-rt-18-dev`; OCCT из кэша `occt-native` (`fail-on-cache-miss`); конфигурация `build-fuzz` с `-DBENDCORE_FUZZ=ON`, ASan/UBSan (`-fno-sanitize-recover=undefined`); `ctest -L fuzz` (мутационный тест, регрессия, прогон фаззеров по затравочному корпусу); libFuzzer по 60 с на цель. Находки — артефакт `fuzz-findings` (раздел 7) |
| `wasm.yml` / `wasm` | push в main, PR | прежняя сборка `scripts/build_wasm.sh` и проверка `web/index.html` (не менялась) |
| `wasm.yml` / `wasm-modules` | push в main, PR, вызов из `production-deploy.yml` | apt: Ninja, Eigen, **`fonts-dejavu-core`** (`cmake/wasm_drawing.cmake` кладёт `DejaVuSans.ttf` и лицензию рядом с модулем чертежа; шаг проверяет, что оба файла есть). emsdk 6.0.12; кэш `third_party/occt-install-wasm-checked` и `third_party/occt-build-wasm/probe` (ключ `occt-wasm-checks<OCCT_RELEASE_CHECKS>-V8.0.1-emsdk-6.0.12-<hashFiles(occt_toolkits.txt, build_occt_wasm.sh)>`), при промахе — `git clone` OCCT и `build_occt_wasm.sh`. **Одна** сборка `build-wasm-modules` с четырьмя целями `bendcore_kernel_wasm`, `bendcore_modeling_wasm`, `bendcore_assembly_wasm`, `bendcore_drawing_wasm` (библиотеки ядра компилируются один раз, а не четыре); node-тесты `ctest -R '^wasm_' --no-tests=error` (и проверка, что их не меньше 5: `wasm_kernel_corpus`, `wasm_modeling_document`, `wasm_sketch_editor`, `wasm_assembly_document`, `wasm_drawing_document`); размеры `.wasm`/`.mjs` всех модулей (байт и gzip -9) и `sizes.txt` пробной программы — в сводку и `sizes.md`; артефакт `bendcore-wasm-modules` (четыре каталога модулей) |
| `wasm.yml` / `wasm-e2e` (матрица `kernel`, `modeling`, `assembly`, `drawing`, `app`) | после `wasm-modules` | модули из артефакта; `npm ci --ignore-scripts`, `npx playwright install --with-deps chromium`, `npm run test:e2e:<набор> -- --trace off` с `CI=true` и каталогом модуля в `BENDCORE_{KERNEL,MODELING,ASSEMBLY,DRAWING}_WASM_DIR` (набор `app` — оболочка приложения, `docs/ui/APP_SHELL.md`, — получает все четыре; перед запуском проверяется, что `.wasm` на месте); `fail-fast: false`. При падении — артефакт `e2e-<набор>-results` = `test-results/{kernel-viewer,part-modeler,assembly,drawing,app}/`: снимки экрана упавших тестов (`screenshot: 'only-on-failure'` в конфигурациях модуля детали, сборки и чертежа), снимки самих тестов и `error-context.md` |
| `production-deploy.yml` | только вручную | см. ниже |

Задачи `kernel-wasm` и `kernel-viewer-e2e` заменены `wasm-modules` и `wasm-e2e` (ядро — одна из
четырёх целей и один из четырёх наборов). В `web/kernel-viewer/playwright.config.ts` (зона
фронтенда) `screenshot: 'only-on-failure'` не задан — для вьюпорта ядра в артефакт попадают снимок
самого теста и `error-context.md`. Порог роста размера `.wasm` не задан: размеры только публикуются.

**`production-deploy.yml`** (решение 2026-11-11). Запускается только вручную; при `workflow_dispatch`
выполняются задачи `test` (lint, Jest), `e2e-test`, `security` и `build` (сборка и публикация образов
в Docker Hub при наличии секретов). Задачи `deploy`, `post-deploy-tests`, `metrics` имеют условие
`github.event_name == 'push'` и при ручном запуске не выполняются никогда; кластера и адреса
`bendcore.app`, на которые они ссылаются, у проекта нет. Прежняя задача `e2e-test` поднимала
`docker-compose` и вызывала `npm run test:e2e` — корневой набор Playwright удалён вместе со старым
интерфейсом. Решение: `e2e-test` теперь вызывает `wasm.yml` как переиспользуемый workflow
(`workflow_call`), то есть публикации образов предшествуют настоящие наборы
`test:e2e:{kernel,modeling,assembly,drawing,app}` на модулях из того же коммита. Шаг
`npm run test:integration` удалён: такого скрипта в `package.json` нет. Скрипты `test:smoke` и
`test:performance` в недостижимой `post-deploy-tests` тоже отсутствуют; задача оставлена как
есть до появления реального окружения развёртывания (решение руководителя проекта).

### Проверено локально (2026-11-11, 4 ядра, 15 ГиБ)

* YAML всех `.github/workflows/*.yml` разобран `python3 -c "import yaml"` (после каждого изменения).
* `occt-native`: `build-occt` с `/home/user/bendcore/third_party/occt-install`; полный
  `ctest -E 'wasm|performance' --parallel 2` — 123/123; `ctest -L stress` последовательно — 5/5
  (таблица — `docs/qa/PERFORMANCE.md`); `fuzz_loaders` и `fuzz_loader_regressions` — проходят.
* `fuzz`: clang 18.1.3 + `libclang-rt-18-dev`, ровно та же конфигурация `build-fuzz` и список
  целей, `ctest -L fuzz` — 10/10 (мутационный тест 175 с под ASan); libFuzzer 60 с:
  `fuzz_dxf` 703 645 запусков, `fuzz_drawing_json` 693 543, `fuzz_part_json` 78 678 — без находок.
  Полная сборка дерева в `build-fuzz` не нужна и не проверялась (задача собирает только цели fuzz).
* `wasm-modules`: `emcmake cmake` (как в задаче) на готовой `occt-install-wasm`, четыре цели в одном
  каталоге — собраны; `DejaVuSans.ttf` и лицензия на месте; `ctest -R '^wasm_' --no-tests=error` —
  5/5 (31 с), счётчик тестов — 5; шаг размеров выполнен локально (скрипт шага из YAML):

  | Модуль | `.wasm`, байт | gzip -9 | `.mjs`, байт |
  |---|---:|---:|---:|
  | `bendcore_kernel_wasm` | 12 625 582 | 3 793 177 | 168 676 |
  | `bendcore_modeling_wasm` | 19 873 786 | 5 736 884 | 203 201 |
  | `bendcore_assembly_wasm` | 29 238 501 | 7 927 427 | 235 445 |
  | `bendcore_drawing_wasm` | 21 656 992 | 6 276 570 | 206 823 |

* `wasm-e2e`: `npm ci --ignore-scripts`, затем для каждого набора `npm run test:e2e:<набор> -- --trace off`
  с `CI=true` и `BENDCORE_*_WASM_DIR` на каталоги сборки, после слияния оболочки приложения и
  пересборки модулей: kernel 4/4, modeling 4/4, assembly 2/2, drawing 1/1, app 3/3 (60 с). Браузер —
  локальный Chromium 1243 через `PLAYWRIGHT_CHROMIUM_EXECUTABLE`.
* `no-stubs`: самотест, полная проверка (расширенный список каталогов) и `--diff ccr-d188a25e-rml7z8` —
  0 нарушений.

### Только в CI (локально не воспроизводится)

* Запуск на `ubuntu-24.04`, попадание/промах `actions/cache` и вычисление `hashFiles`,
  `git fetch` базовой ветки и `github.event.before`, вывод аннотаций и сводки.
* Полная сборка OCCT из исходников (native ~30 мин, WASM ~25 мин на 4 ядрах без кэша),
  `git clone` OCCT и установка emsdk через `mymindstorm/setup-emsdk`.
* `npx playwright install --with-deps chromium` (браузер версии из `package-lock.json`,
  Playwright 1.64.1) и передача модуля между задачами через `upload/download-artifact`.
* Сборка образа сервиса и запросы к контейнеру (Docker здесь не проверялся).
* Вызов `wasm.yml` из `production-deploy.yml` (`workflow_call`) и публикация образов.
* `actions/cache/restore` с `fail-on-cache-miss` в задаче `fuzz` (кэш сохраняет `occt-native`).

## 5. Как запускать локально

```bash
# проверка заглушек
scripts/check_no_stubs.sh --diff ccr-d188a25e-rml7z8
bash tests/qa/test_check_no_stubs.sh

# корпус и регрессия (OCCT с проверками установлен в third_party/occt-install-checked)
cmake -S . -B build-occt -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBENDCORE_WITH_OCCT=ON -DBENDCORE_OCCT_DIR=$PWD/third_party/occt-install-checked
cmake --build build-occt --target generate_corpus test_step_corpus
ctest --test-dir build-occt -R 'corpus|qa_' --output-on-failure

# перегенерация закоммиченного корпуса
build-occt/generate_corpus tests/corpus/generated

# нагрузка и fuzz (разделы 6, 7)
ctest --test-dir build-occt -L stress --output-on-failure
ctest --test-dir build-occt -L fuzz --output-on-failure
build-fuzz/fuzz_replay brep_binary build-fuzz/fuzz-artifacts/crash-brep_binary-98.bin
```

## 6. Нагрузочные тесты

`stress_part_300`, `stress_assembly_300`, `stress_step_generate` → `stress_step_large`,
`stress_hlr_1000` (метка ctest `stress`, `cmake/qa_stress.cmake`): деталь из 300 операций,
сборка из 300 компонентов, сгенерированный STEP ≥ 50 МБ, вид HLR детали из 996 граней через
кэш. Эталоны, пороги, отчёт с метками времени и базовая линия — `docs/qa/PERFORMANCE.md`.

## 7. Fuzz-проверка загрузчиков

Файлы: `tests/regression/fuzz/` (`fuzz_targets.{hpp,cpp}` — цели, затравки, мутатор;
`test_fuzz_loaders.cpp`; `fuzz_main.cpp` — фаззер libFuzzer; `fuzz_seeds.cpp`;
`fuzz_replay.cpp`; `test_loader_regressions.cpp`; `known_defects.txt`), подключение —
`cmake/qa_fuzz.cmake`.

### Цели

| Цель | Загрузчик | После приёма |
|---|---|---|
| `part_json` | `PartDocument::fromJson` (с перестроением) | `toJson` обязан пройти |
| `assembly_json` | `AssemblyDocument::fromJson` (встроенный BRep деталей) | `toJson`, `solve`, `placedParts` — только явные ошибки |
| `drawing_json` | `DrawingDocument::fromJson` | `toJson` |
| `step` | `kernel::readStep` | `counts()` |
| `dxf` | `dxf::parseDxf` | `scaleToMillimeters` |
| `brep_binary`, `brep_text` | `kernel::readBrepBinary`, `readBrepText` | `counts()` |

Затравки: закоммиченные `tests/wasm/fixtures/bracket.part.json`, `tests/assembly-fixtures/*.json`,
файлы корпуса STEP, а также документы, построенные через API модулей (сборка с двумя деталями и
сопряжением, чертёж с листом и видами, STEP/BRep бруска со скруглением и отверстием, DXF писателя
`drawing_v2` и DXF R12 с LWPOLYLINE и выпуклостью). Мутации (`Mutator`, `std::mt19937_64`):
обрезка, перестановка байтов и блока, случайные байты, замена числа на «интересное» (1e308,
1e999, −0, 2⁶⁴, NaN/inf текстом, 1e-320 …), повтор блока до 200 раз и строки 5000 раз, удаление
блока, вставка 1000/200 000 скобок; для JSON — замена значения случайного поля (null, тип,
строка 100 000 символов, ±экстремальные целые, 1e308, вложенность 100/5000) и удаление поля.

### Детерминированный мутационный тест — ctest `fuzz_loaders`

300 мутаций на цель, зерно 20261111 (`BENDCORE_FUZZ_ITERATIONS`, `BENDCORE_FUZZ_SEED`). Каждый
вход — в отдельном процессе (`fork`); исход: принят, явная ошибка, **exception** (исключение
наружу), **crash** (сигнал, отчёт ASan/UBSan), **hang** (> `BENDCORE_FUZZ_TIMEOUT_S` = 60 с,
процесс убивается), **memory** (пик > 4096 МиБ; адресное пространство ребёнка ограничено 8 ГиБ,
под ASan — `hard_rss_limit_mb=4096`). Вход каждого дефекта сохраняется в
`build-*/fuzz-artifacts/<вид>-<цель>-<номер>.bin` (артефакт CI) и воспроизводится
`fuzz_replay <цель> <файл>`. Тест требует, чтобы затравки принимались и чтобы были и приёмы, и
отказы (иначе мутации ничего не проверяют).

Дефекты, которые нельзя исправить в коде bendcore, перечислены в `known_defects.txt` для своей
конфигурации (`native` — gcc, `asan` — clang + ASan/UBSan). Совпадение требуется точное: новый
дефект роняет тест, исчезнувший (исправленный) — тоже, чтобы список не устаревал. Загрузчик в
самом процессе теста не запускается: потоки OCCT, запущенные в родителе, после `fork` оставили бы
захваченные мьютексы (первый вариант теста давал из-за этого ложные зависания).

### libFuzzer — конфигурация `build-fuzz`

```bash
CC=clang CXX=clang++ cmake -S . -B build-fuzz -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBENDCORE_WITH_OCCT=ON -DBENDCORE_OCCT_DIR=$PWD/third_party/occt-install-checked -DBENDCORE_FUZZ=ON \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined,fuzzer-no-link -fno-sanitize-recover=undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-fuzz --target test_fuzz_loaders test_loader_regressions fuzz_seeds fuzz_replay \
  fuzz_part_json fuzz_assembly_json fuzz_drawing_json fuzz_step fuzz_dxf fuzz_brep_binary fuzz_brep_text
ctest --test-dir build-fuzz -L fuzz --output-on-failure
build-fuzz/fuzz_dxf build-fuzz/fuzz-corpus/dxf -max_total_time=60     # поиск
```

Нужен `libclang-rt-18-dev` (libFuzzer). OCCT не инструментирован (установка native): санитайзеры
проверяют код bendcore и вызовы OCCT из него. `fuzz_seeds` пишет затравочный корпус в
`build-fuzz/fuzz-corpus/<цель>/` (в git не хранится), `fuzz_corpus_<цель>` — прогон фаззера по
нему (`-runs=0`). В CI (задача `fuzz`) — ещё 60 с поиска на цель; находки `part_json`,
`drawing_json`, `dxf` роняют задачу, находки целей, читающих через OCCT (`step`, `brep_*`,
`assembly_json`), — предупреждение и артефакт `fuzz-findings` (после перехода на OCCT с
проверками можно сделать и их блокирующими — решение инженера качества).

### Найдено и исправлено

1. `AssemblyDocument::fromJson` выпускал наружу `nlohmann::json::out_of_range` (406, переполнение
   числа при разборе: `1e999`): ловился только `parse_error`
   (`src/assembly_v2/document/serialization.cpp`). Теперь — `ParseError`. Регрессия —
   `fuzz_loader_regressions` (`test_loader_regressions.cpp`: тот же вход у загрузчиков сборки,
   детали и чертежа).

2. **229 падений внутри читателей OCCT** (каждая конфигурация; brep_binary 103, brep_text 69,
   step 47, assembly_json 10; 2026-11-11) — устранены 2026-11-12 (`docs/kernel/OCCT_BUILD.md`,
   раздел 6; `docs/kernel/KERNEL_CORE.md`, «Повреждённые файлы»):

   | Шаг | native: дефектов | Что устранил |
   |---|---:|---|
   | OCCT 8.0.1 с `No_Exception`, C++17 (было) | 229 | — |
   | OCCT с проверками в Release (`OCCT_RELEASE_CHECKS=1`) | 59 | индексы вне диапазона в `BinTools_*Set`/`TopTools_ShapeSet` (`Standard_OutOfRange` → `ParseError`), в т. ч. все 10 `assembly_json` и `BSplCLib::InsertKnots` |
   | + защита ядра от аппаратных сбоев (`src/kernel/occt_fault_guard.*`) | 8 | нулевые дескрипторы: `StepToGeom::Make*` на ссылках неверного типа (46 step), `Geom_TrimmedCurve`/`Geom_SurfaceOfLinearExtrusion` на нулевой базе (7 brep_text, 1 brep_binary) — SIGSEGV → исключение OCCT → ошибка |
   | + OCCT как C++20 (`OCCT_CXX_STANDARD=C++20`) | 3 | 5 brep_binary: `IS >> char[255]` ключевого слова 4 МБ — переполнение стека (`stack smashing detected`) |
   | + предварительная проверка STEP (`src/kernel/step_preflight.*`) | **0** | 2 step: строка из 200 000 `[` (> 8 ГиБ в лексере OCCT) — недопустимый символ вне строк; 1 step: вход 1 ГБ — предел 256 МиБ |

   ASan/UBSan (`build-fuzz`, OCCT с проверками, C++20): **1** дефект (было 229). Мутации, которые
   раньше роняли процесс, теперь явные отказы: у `step` в ASan 134 приёма и 166 отказов из 300.
   Регрессия — `kernel_malformed_input` и `kernel_fault_guard` (`tests/kernel/`), по одному
   детерминированному входу на класс дефектов.

### Найдено, не исправлено (OCCT) — `known_defects.txt`

| Конфигурация | Цель | № | Вид | Причина |
|---|---|---:|---|---|
| asan | `brep_binary` | 98 | crash (exit 1) | 4,8 КБ двоичного BRep с испорченным числом полюсов 2D B-сплайна (~2²⁸): `BinTools_Curve2dSet` выделяет `NCollection_Array1<gp_Pnt2d>(1, nbpoles)` по числу из файла до чтения полюсов. Без ASan память только резервируется, поток кончается на первых полюсах → `ParseError` (в WASM выделение > 4 ГиБ → `Standard_OutOfMemory` → `ParseError`); ASan трогает выделенное и превышает `hard_rss_limit_mb=4096`. Исправимо только в OCCT (сверка числа с длиной потока) или полным разбором формата BinTools до OCCT |

В native дефектов нет. Чтение недоверенных файлов в браузере по-прежнему стоит держать в Web
Worker: в WASM нет сигналов, и обращение OCCT по нулевому дескриптору там не переводится в
ошибку (слои «проверки», «C++20» и «предварительная проверка STEP» действуют).

Известная особенность самого теста: `fuzz_loaders` держит вход и его копии в памяти дочернего
процесса; мутация `step` #272 (повтор блока — 1 ГБ) даёт пик около 1,9 ГиБ дочернего процесса
даже при отказе ядра до разбора (предел размера), а `part_json` #246 — около 1,9 ГиБ. При
параллельном `ctest` под общим лимитом памяти тест может упасть из-за нехватки памяти машины,
отдельно (`-j1`) проходит. После изменения OCCT `known_defects.txt` перегенерируется: прогон
теста печатает строки `[fuzz] <конфигурация> <цель> #<номер>: <вид>`.
