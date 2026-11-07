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

* Полная проверка всегда сканирует `src/{kernel,modeling,assembly_v2,drawing_v2}`,
  `include/bendcore/{kernel,modeling,assembly_v2,drawing_v2}`,
  `tests/{kernel,modeling,assembly_v2,drawing_v2}`; отсутствующие каталоги пропускаются.
* `--diff <ref>` дополнительно проверяет **только добавленные строки** во всех файлах кода,
  изменённых относительно `merge-base(<ref>, HEAD)`, включая незакоммиченные изменения
  отслеживаемых файлов (неотслеживаемые новые файлы — после `git add`). Старый код с
  нарушениями не мешает, пока его не трогают.
* `--no-full` — только diff; `--root <dir>` — другой корень; `--paths <p>...` — свои пути.
* Файлы кода: C/C++ (`.c .cc .cpp .cxx .h .hh .hpp .hxx .ipp .inl .tpp`), CMake, `.sh`,
  `.py`, JS/TS. Исключены: `tests/qa/fixtures/` (намеренные нарушения), сам скрипт и его
  самотест (содержат шаблоны), `third_party/`, `node_modules/`. Документация (`*.md`) не
  проверяется.
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
| `trivial-assert` | `EXPECT_TRUE(true)`, `ASSERT_FALSE(false)`, `assert(true)`, `SUCCEED()`, `EXPECT_EQ(x, x)` | — |
| `empty-body` | функция с пустым телом `{}` | не ловятся конструкторы, деструкторы, `= default`, лямбды, списки инициализации; только код продукта |
| `literal-return` | функция **с параметрами**, тело которой — только `return <литерал>;` | функции без параметров (свойства вида `is_empty() { return true; }`) и тесты не ловятся |
| `literal-success` | `success/ok/valid/... = true;`, перед которым в теле только объявления, а после — сразу `return` | инициализация «верно, пока проверка не провалится» и фабрики `success_result`/`ok` не ловятся |
| `no-assertions` | тестовый файл (`main` или `TEST`) без единой проверки (`assert`, `EXPECT_*`, `check(`, `return 1`, `throw`…) | — |
| `bad-suppression` | `qa-allow` без причины или с неизвестным правилом | — |

Многострочные правила (`empty-body`, `literal-return`, `literal-success`, `no-assertions`)
работают по тексту C++ с вырезанными комментариями и строками.

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
   `tests/qa/fixtures/expected_violations.txt` (23 нарушения, все правила), код 1;
2. `tests/qa/fixtures/clean` (законные конструкции) → 0 нарушений, код 0;
3. `--diff` во временном git-репозитории: сообщаются только добавленные строки
   (закоммиченные и в рабочей копии), не сообщаются старые нарушения базы, фикстуры и `.md`;
4. ошибки запуска → код 2.

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

## 4. CI — `.github/workflows/qa.yml`

| Задача | Когда | Что делает |
|---|---|---|
| `no-stubs` | push, PR | самотест; `check_no_stubs.sh --diff origin/<base_ref>` на PR, `--diff <before>` на push (на новой ветке — только полная проверка); нарушения выводятся аннотациями `::error file=…,line=…` и роняют задачу |
| `occt-native` | push, PR | OCCT 8.0.1 из исходников (`third_party/build_occt.sh`) с `actions/cache` (ключ общий с job `occt` в `wasm.yml`), конфигурация `BENDCORE_WITH_OCCT=ON`, полная сборка, `ctest -R 'occt\|kernel\|corpus\|import'`; свежий корпус выгружается артефактом |

Не проверено локально (GitHub Actions здесь не запускается): реальный запуск на
`ubuntu-24.04`, попадание/промах кэша, `git fetch` базовой ветки и значения
`github.event.before`, вывод аннотаций. Синтаксис YAML проверен `python3 -c "import yaml"`,
ключевые шаги (самотест, `--diff`, конфигурация и сборка с OCCT, ctest) выполнены локально.

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
