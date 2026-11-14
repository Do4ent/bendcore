# bendcore

Ядро CAD на C++20 поверх Open CASCADE (OCCT) и интерфейс на React/TypeScript, работающий с
ядром через WebAssembly. Начиналось как система проектирования гнутых труб; сейчас
развивается как CAD общего назначения по плану `docs/ROADMAP_GENERAL_CAD.md`. Лицензия — MIT.

Этот файл описывает только то, что есть в репозитории и проверяется тестами. Правила
разработки — `docs/PROJECT_PROCESS.md` (в том числе правило «никаких заглушек»);
что и почему удалено как заглушка — `docs/STUB_REMOVAL.md`; удалённые устаревшие отчёты —
`docs/DOCS_CLEANUP.md`.

Готовность к v1.0 по плану (что выполнено, что частично, что может проверить только
человек, что блокирует выпуск) — [`docs/V1_READINESS.md`](docs/V1_READINESS.md).

## Что есть

### Ядро (C++)

| Модуль | Каталог | Документация |
|---|---|---|
| Ядро геометрии на OCCT: формы, STEP, тесселяция, свойства, лечение, XCAF, анализ | `src/kernel`, `include/bendcore/kernel` | `docs/kernel/` |
| Документ детали: эскизы с ограничениями (PlaneGCS), дерево операций, топологическое именование, переменные и выражения | `src/modeling` | `docs/modeling/` |
| Документ сборки: компоненты, решатель сопряжений, пересечения, BOM, крепёж | `src/assembly_v2` | `docs/assembly/` |
| Документ чертежа: виды (HLR), разрезы, размеры, аннотации, спецификация, экспорт PDF/DXF/SVG, импорт DXF | `src/drawing_v2`, `src/dxf_reader.cpp` | `docs/drawing/` |
| Трубный модуль: путь трубы, гибка, развёртка, проверки, экземпляры, чертёж трубы | `src/tube_*.cpp`, `src/bend_*.cpp`, `src/core_api.cpp` и др. | `docs/ROADMAP_GENERAL_CAD.md` (раздел о трубном модуле) |

Исполняемые файлы: `bendcore_cli` — команды ядра «раздел.действие» по строкам JSON через
stdin/stdout (`src/cli/main.cpp`, `src/core_api.cpp`); `bendcore_server` (сборка с OCCT) —
HTTP-сервис тела трубы и распознавания трубы в STEP (`POST /v1/tube`, `POST /v1/analyze`,
`GET /v1/health`, порт по умолчанию 8080).

### Интерфейс в браузере (WebAssembly)

Каждая страница — React-приложение поверх своего WASM-модуля; вся геометрия приходит из ядра.

| Страница | Что умеет | Описание, сборка, запуск |
|---|---|---|
| Вьюпорт ядра (`web/kernel-viewer`) | открыть STEP, показать сетку ядра, выбрать грань, свойства | [`docs/ui/KERNEL_VIEWPORT.md`](docs/ui/KERNEL_VIEWPORT.md) |
| Моделирование детали (`web/part-modeler`) | дерево построения, операции с выражениями, выбор граней и рёбер, отмена/повтор, массовые свойства, файл | [`docs/ui/PART_MODELING_UI.md`](docs/ui/PART_MODELING_UI.md) |
| Редактор эскиза (на странице детали) | рисование с привязками, ограничения и размеры, перерешение, определённость | [`docs/ui/SKETCH_EDITOR.md`](docs/ui/SKETCH_EDITOR.md) |
| Сборка (`web/assembly`) | дерево сборки, сопряжения, перетаскивание с перерешением, пересечения, BOM, разнесённый вид, крепёж | [`docs/ui/ASSEMBLY_UI.md`](docs/ui/ASSEMBLY_UI.md) |
| Чертёж (`web/drawing`) | листы, виды, размеры, аннотации, обновление по модели, спецификация, экспорт PDF/DXF/SVG | [`docs/ui/DRAWING_UI.md`](docs/ui/DRAWING_UI.md) |

Руководство пользователя (быстрый старт, справка по эскизу, детали, сборке, чертежу, обмену
данными, ограничения версии) — [`docs/USER_GUIDE/`](docs/USER_GUIDE/README.md). Оболочка с
вкладками всех редакторов — `web/app` (`npm run dev:app`, `docs/ui/APP_SHELL.md`).

Исходники страниц — `src/ui/{kernel,modeling,assembly,drawing}`, JS API модулей — `src/wasm/`.
`web/index.html` и `web/view.html` — проверочные страницы модуля `bendcore_wasm`
(эскиз 2D, импорт и просмотр трубы, `src/wasm/bindings.cpp`).

### Окно трубного приложения (Electron)

`src/main.ts` (главный процесс) запускает `bendcore_cli` и передаёт ему команды ядра;
окно — `src/ui/index.tsx` (сборка `webpack.config.js`). Рабочие панели: проверка трубопровода
(`CheckPanel`, `check.run`), данные гибки (`BendDataDialog`, `bend.lra`/`bend.xyz`),
экземпляры (`InstancesPanel`, `instances.*`), чертёж трубы (`DrawingWorkspace`: `drawing.*`,
слои, комментарии, 2D-эскиз), импорт STEP через `bendcore_server` (`ImportDialog`). Кнопки
ленты без соответствующей команды ядра возвращают ошибку «Команда не поддерживается ядром».
Панели можно проверить без Electron стендом `tests/ui_preview` (настоящий `bendcore_cli` за
локальным HTTP-сервером).

## Сборка и тесты

Требования: CMake ≥ 3.20, Ninja, компилятор C++20, Eigen ≥ 3.3, Node.js ≥ 18. OCCT не
хранится в git: сборка — `docs/kernel/OCCT_BUILD.md`.

```bash
# Native без OCCT (трубный модуль, CLI)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build && ctest --test-dir build --output-on-failure

# Native с OCCT (ядро, деталь, сборка, чертёж, сервер)
cmake -S . -B build-occt -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBENDCORE_WITH_OCCT=ON -DBENDCORE_OCCT_DIR=$PWD/third_party/occt-install-checked
cmake --build build-occt && ctest --test-dir build-occt --output-on-failure

# WebAssembly-модули страниц: emsdk, OCCT, собранный для WASM, Eigen
source /path/to/emsdk/emsdk_env.sh
emcmake cmake -S . -B build-wasm -G Ninja -DCMAKE_BUILD_TYPE=Release -DBENDCORE_WITH_OCCT=ON \
  -DBENDCORE_OCCT_DIR=$PWD/third_party/occt-install-wasm-checked -DBENDCORE_EIGEN_DIR=/usr/include/eigen3
cmake --build build-wasm --target bendcore_kernel_wasm bendcore_modeling_wasm \
  bendcore_assembly_wasm bendcore_drawing_wasm
```

Модуль `bendcore_wasm` (трубы и эскиз 2D) — `docs/WASM_BUILD_GUIDE.md`,
`scripts/build_wasm.sh`.

```bash
npm ci
npm run typecheck                 # tsc
npx jest                          # логика интерфейса и трубного окна
# страница и её сквозной тест (Playwright), каталог модуля — из сборки выше:
BENDCORE_MODELING_WASM_DIR=build-wasm/wasm_modeling npm run dev:part-modeler
BENDCORE_MODELING_WASM_DIR=$PWD/build-wasm/wasm_modeling npm run test:e2e:modeling
# аналогично: dev:kernel-viewer / test:e2e:kernel (BENDCORE_KERNEL_WASM_DIR),
# dev:assembly / test:e2e:assembly (BENDCORE_ASSEMBLY_WASM_DIR),
# dev:drawing / test:e2e:drawing (BENDCORE_DRAWING_WASM_DIR)
npm run build                     # окно Electron: tsc + webpack → dist/
bash scripts/check_no_stubs.sh    # проверка правила «никаких заглушек»
```

Порты страниц, переменные и состав тестов — в соответствующем `docs/ui/*.md`.

## Ограничения

Ограничения каждого модуля перечислены в его документации (раздел «Ограничения»). Код вне
перечисленных модулей (`backend/`, часть `src/` верхнего уровня) — наследие ранних этапов; найденные в нём заглушки и что с ними сделано —
`docs/STUB_REMOVAL.md`.

## Участие

`CONTRIBUTING.md`, `docs/PROJECT_PROCESS.md`. Лицензия — `LICENSE` (MIT).
