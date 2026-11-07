# Контроль качества bendcore

Владелец: инженер качества (`.claude/agents/cad-qa-engineer.md`). Основание:
`docs/PROJECT_PROCESS.md` (разделы 2, 3, 5), `docs/ROADMAP_GENERAL_CAD.md` (фаза 0, п. 6).

## 1. Проверка «никаких заглушек» — `scripts/check_no_stubs.sh`

### Режимы

```bash
scripts/check_no_stubs.sh                      # полная проверка каталогов нового кода
scripts/check_no_stubs.sh --diff origin/ccr-d188a25e-rml7z8   # + добавленные строки относительно базы
scripts/check_no_stubs.sh --list-rules         # список правил
```

* Полная проверка всегда сканирует `src/{kernel,modeling,assembly_v2,drawing_v2,wasm,ui/kernel}`
  (в `src/wasm` — `kernel_bindings.cpp` и JS API ядра),
  `include/bendcore/{kernel,modeling,assembly_v2,drawing_v2}`,
  `tests/{kernel,modeling,assembly_v2,drawing_v2,corpus,wasm,e2e-kernel}`, `web/kernel-viewer`;
  отсутствующие каталоги пропускаются.
* `--diff <ref>` дополнительно проверяет **только добавленные строки** во всех файлах кода,
  изменённых относительно `merge-base(<ref>, HEAD)`, включая незакоммиченные изменения
  отслеживаемых файлов (неотслеживаемые новые файлы — после `git add`). Старый код с
  нарушениями не мешает, пока его не трогают.
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
| `placeholder` | «placeholder» | не ловятся `std::placeholders`, CSS `::placeholder`, атрибут `placeholder="…"` и поле `placeholder: '…'` |
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

Контроль точности: на всём существующем коде (`--paths src include tests`) скрипт находит
~1100 нарушений; выборочная проверка показала, что это действительно заглушки старого кода
(«Placeholder implementation», `EXPECT_TRUE(true)`, «For now, just return…»). Ложные
срабатывания, найденные при выборке (`result.valid = true` перед проверками, фабрика
`success_result`, `SetUp() override {}` в тестах), устранены и закреплены в
`tests/qa/fixtures/clean/`.

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
   `tests/qa/fixtures/expected_violations.txt` (42 нарушения: все правила для C++ и
   JS/TS-правила на `src/ui/kernel/bad_viewer.ts`, `tests/wasm/bad_kernel.test.mjs`,
   `tests/e2e-kernel/no_checks.spec.ts`), код 1;
2. `tests/qa/fixtures/clean` (законные конструкции) → 0 нарушений, код 0;
3. `--diff` во временном git-репозитории: сообщаются только добавленные строки
   (закоммиченные и в рабочей копии, включая `.tsx`), не сообщаются старые нарушения базы,
   фикстуры и `.md`;
4. полная проверка без `--paths` находит нарушение в каждом каталоге нового кода
   (`src/wasm/kernel_bindings.cpp`, `.mjs`, `.d.mts`, `src/ui/kernel/*.tsx`, `src/modeling`,
   `src/assembly_v2`, `src/drawing_v2`, `tests/wasm`, `tests/e2e-kernel`, `web/kernel-viewer`)
   и не сканирует каталог вне списка;
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

## 4. CI — `.github/workflows/qa.yml`, `wasm.yml`

| Workflow / задача | Когда | Что делает |
|---|---|---|
| `qa.yml` / `no-stubs` | push, PR | самотест; `check_no_stubs.sh --diff origin/<base_ref>` на PR, `--diff <before>` на push (на новой ветке — только полная проверка); нарушения выводятся аннотациями `::error file=…,line=…` и роняют задачу |
| `qa.yml` / `occt-native` | push, PR | OCCT 8.0.1 скриптом `third_party/build_occt.sh` (29 тулкитов из `third_party/occt_toolkits.txt`) с `actions/cache` установки; ключ `occt-native-V<версия>-ubuntu-24.04-<hashFiles(occt_toolkits.txt, build_occt.sh)>` — старый кэш `occt-build-…-v3` (48 тулкитов) не совпадает; в сводке — число тулкитов в списке и установленных. Конфигурация `BENDCORE_WITH_OCCT=ON` (RelWithDebInfo), полная сборка, **весь** `ctest` по `build-occt` (`--parallel 4 -E '^performance$'`), затем `performance` отдельным последовательным шагом (выполняется и после падения предыдущего). Затем образ сервиса (`docker/Dockerfile`) и запросы к нему. Артефакты: свежий корпус (всегда), журналы OCCT/сборки/ctest (при падении) |
| `wasm.yml` / `kernel-wasm` | push в main, PR | emsdk 6.0.12; кэш `third_party/occt-install-wasm` и `third_party/occt-build-wasm/probe` с ключом `occt-wasm-V8.0.1-emsdk-6.0.12-<hashFiles(occt_toolkits.txt, build_occt_wasm.sh)>`; при промахе — `git clone --depth 1 --branch V8.0.1` и `third_party/build_occt_wasm.sh` (успех — код 0, внутри пробная программа печатает `OK` под node). Затем `emcmake cmake … -DBENDCORE_WITH_OCCT=ON`, цель `bendcore_kernel_wasm` (`cmake/wasm_kernel.cmake`), `ctest -R '^wasm_kernel_corpus$' --no-tests=error`. Размеры `.wasm`/`.mjs` (байт и gzip -9) и `sizes.txt` пробной программы — в сводку job и в артефакт `bendcore-kernel-wasm` (вместе с модулем) |
| `wasm.yml` / `kernel-viewer-e2e` | после `kernel-wasm` | модуль из артефакта; `npm ci --ignore-scripts` (postinstall electron тесту не нужен), `npx playwright install --with-deps chromium`, `npm run test:e2e:kernel -- --trace off` с `CI=true` и `BENDCORE_KERNEL_WASM_DIR`; при падении — артефакт `kernel-viewer-e2e-results` (`test-results/kernel-viewer/`: снимок `kernel_viewport.png` и `error-context.md` Playwright) |

Job `occt` из `wasm.yml` удалён: его полный `ctest` и проверка образа сервиса теперь в
`occt-native`, так что native OCCT в CI собирается одной задачей. Каталог Eigen в новых
задачах не задаётся: его находит корневой `CMakeLists.txt` (`third_party/eigen-5.0.0`).

Порог роста размера `.wasm` не задан: размеры только публикуются. Снимки экрана на каждом
упавшем шаге Playwright делает только при `screenshot: 'only-on-failure'` в
`web/kernel-viewer/playwright.config.ts` (зона фронтенда); сейчас в артефакт попадают снимок
документационного теста и `error-context.md` упавших тестов.

### Проверено локально (2026-11-07, 4 ядра)

* YAML всех `.github/workflows/*.yml` разобран `python3 -c "import yaml"`.
* `occt-native`: конфигурация и полная сборка `build-occt` (429 шагов) с
  `/home/user/bendcore/third_party/occt-install` (это прежняя установка из 48 тулкитов;
  установка из 29 — `occt-install-min` — проверена инженером ядра, `OCCT_BUILD.md`, раздел 2);
  ровно те же команды ctest: 109 тестов параллельно без `performance` — все проходят,
  затем `performance` отдельно — проходит.
* `kernel-wasm`: `build_occt_wasm.sh` с `SKIP_OCCT_BUILD=1` на готовой `occt-install-wasm`
  (29 тулкитов) — пробная программа собрана в -O2/-Oz, обе печатают `OK`, код 0;
  `emcmake cmake` + сборка `bendcore_kernel_wasm` + `wasm_kernel_corpus` — проходит
  (3 с); шаг размеров: `bendcore_kernel.wasm` 12 625 582 байт, gzip -9 3 793 177;
  `bendcore_kernel.mjs` 168 676 / 29 265.
* `kernel-viewer-e2e`: `npm ci --ignore-scripts` в чистой рабочей копии, затем
  `npm run test:e2e:kernel -- --trace off` с `CI=true` — 4 из 4 (11 с). Браузер — локальный
  Chromium 1243 через `PLAYWRIGHT_CHROMIUM_EXECUTABLE`.
* `no-stubs`: самотест, полная проверка и `--diff ccr-d188a25e-rml7z8` — 0 нарушений.

### Только в CI (локально не воспроизводится)

* Запуск на `ubuntu-24.04`, попадание/промах `actions/cache` и вычисление `hashFiles`,
  `git fetch` базовой ветки и `github.event.before`, вывод аннотаций и сводки.
* Полная сборка OCCT из исходников (native ~30 мин, WASM ~25 мин на 4 ядрах без кэша),
  `git clone` OCCT и установка emsdk через `mymindstorm/setup-emsdk`.
* `npx playwright install --with-deps chromium` (браузер версии из `package-lock.json`,
  Playwright 1.64.1) и передача модуля между задачами через `upload/download-artifact`.
* Сборка образа сервиса и запросы к контейнеру (Docker здесь не проверялся).

## 5. Как запускать локально

```bash
# проверка заглушек
scripts/check_no_stubs.sh --diff ccr-d188a25e-rml7z8
bash tests/qa/test_check_no_stubs.sh

# корпус и регрессия (OCCT установлен в third_party/occt-install)
cmake -S . -B build-occt -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBENDCORE_WITH_OCCT=ON -DBENDCORE_OCCT_DIR=$PWD/third_party/occt-install
cmake --build build-occt --target generate_corpus test_step_corpus
ctest --test-dir build-occt -R 'corpus|qa_' --output-on-failure

# перегенерация закоммиченного корпуса
build-occt/generate_corpus tests/corpus/generated
```
