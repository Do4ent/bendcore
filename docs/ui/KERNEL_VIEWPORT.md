# Вьюпорт ядра: WASM-модуль `bendcore_kernel_wasm` и просмотр STEP в браузере

Фаза 0, п. 5 `docs/ROADMAP_GENERAL_CAD.md`, фронтенд-часть. Ядро (`bendcore_kernel`,
`docs/kernel/KERNEL_CORE.md`) собирается в WebAssembly и работает в браузере и в node:
чтение STEP, свойства формы, тесселяция. React-компонент показывает сетку ядра в three.js,
выбирает грани лучом и показывает их свойства, полученные от ядра.

![Вьюпорт ядра: скруглённый брусок, выбрана верхняя грань](img/kernel_viewport.png)

## Состав

| Файл | Назначение |
|---|---|
| `cmake/wasm_kernel.cmake` | цель `bendcore_kernel_wasm` (только Emscripten), тест `wasm_kernel_corpus` |
| `src/wasm/kernel_bindings.cpp` | embind: реестр форм по handle, функции ядра → `{ok, value}` / `{ok:false, code, message}` |
| `src/wasm/bendcore_kernel_api.mjs`, `.d.mts` | JS API: `loadKernel`, `Kernel`, `KernelShape`, `KernelError`, `faceOfTriangle` |
| `tests/wasm/kernel_corpus.test.mjs` | проверка модуля в node по корпусу `tests/corpus/generated/` |
| `src/ui/kernel/KernelViewport.tsx` | вьюпорт three.js: грани, рёбра, выбор и подсветка грани |
| `src/ui/kernel/KernelViewerApp.tsx` | страница: открыть STEP, параметры сетки, панели «Модель» и «Грань» |
| `src/ui/kernel/kernelLoader.ts` | загрузка модуля в браузере |
| `src/ui/kernel/edgeBuffers.ts` | упаковка полилиний рёбер в один буфер `LineSegments` (+ Jest-тест) |
| `web/kernel-viewer/` | webpack-конфигурация страницы (на основе `webpack.web.js`), шаблон, конфигурация Playwright |
| `tests/e2e-kernel/kernel_viewport.spec.ts` | сквозной тест Playwright |

## Сборка

```bash
export EMSDK=/root/emsdk PATH=/root/emsdk/upstream/emscripten:$PATH   # или source emsdk_env.sh
emcmake cmake -S . -B build-wasm-kernel -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBENDCORE_WITH_OCCT=ON \
  -DBENDCORE_OCCT_DIR=/home/user/bendcore/third_party/occt-install-wasm-checked \
  -DBENDCORE_EIGEN_DIR=/home/user/bendcore/third_party/eigen-5.0.0
cmake --build build-wasm-kernel --target bendcore_kernel_wasm
```

OCCT для WASM собирается `third_party/build_occt_wasm.sh` (`docs/kernel/OCCT_BUILD.md`,
раздел 3). Собирается только цель `bendcore_kernel_wasm` и её зависимость `bendcore_kernel`;
остальные цели корневого `CMakeLists.txt` при этом не нужны (предупреждение о заглушке
Eigen к ним и относится).

Результат — `build-wasm-kernel/wasm_kernel/`:

| Файл | Что это |
|---|---|
| `bendcore_kernel.mjs` | клей Emscripten: `-sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createBendcoreKernel -sENVIRONMENT=web,node` |
| `bendcore_kernel.wasm` | модуль; ищется рядом с клеем через `import.meta.url` |
| `bendcore_kernel_api.mjs`, `bendcore_kernel_api.d.mts` | копии JS API (POST_BUILD), чтобы node и браузер импортировали всё из одного каталога |

Параметры линковки: `-lembind --no-entry -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB
-sSTACK_SIZE=4MB`, исключения `-fexceptions` (как у OCCT для WASM). Уровень оптимизации —
кэш-переменная `BENDCORE_WASM_KERNEL_OPT` (по умолчанию `Oz`).

### Размер

Измерено 2026-11-07, emsdk 6.0.12, OCCT 8.0.1 (`occt-install-wasm`, 29 тулкитов, Release, без проверок):

| `BENDCORE_WASM_KERNEL_OPT` | `.wasm`, байт | gzip -9, байт | клей `.mjs` (gzip) |
|---|---|---|---|
| `Oz` (по умолчанию) | 12 625 582 (12,0 МиБ) | 3 793 177 (3,6 МиБ) | 168 676 (29 264) |
| `O2` | 14 968 320 (14,3 МиБ) | 3 934 054 (3,8 МиБ) | 169 755 |

Основная часть — STEP (TKDESTEP + TKXSBase, см. `OCCT_BUILD.md`) и BRepMesh; булевых
операций в модуле нет, поэтому он меньше пробной программы из `OCCT_BUILD.md`.
Время в node 22 (`Oz`): инициализация модуля ~130 мс, чтение STEP скруглённого бруска
(64 КБ, 26 граней) ~430 мс, тесселяция (0,1 мм / 0,3 рад, 1380 треугольников) ~85 мс.

## API JS-модуля

```js
import createBendcoreKernel from './bendcore_kernel.mjs';
import { loadKernel, KernelError, faceOfTriangle } from './bendcore_kernel_api.mjs';

const kernel = await loadKernel(createBendcoreKernel);
const shape = kernel.readStep(new Uint8Array(bytes));   // KernelShape
try {
  shape.counts();             // { solids, shells, faces, edges, vertices }
  shape.volumeProperties();   // { volume, centerOfMass: [x,y,z] }, мм³
  shape.surfaceProperties();  // { area, centroid }, мм²
  shape.boundingBox();        // { min: [x,y,z], max: [x,y,z] }, мм
  shape.checkValidity();      // { valid, issues: string[] }
  shape.faceTypes();          // ['Plane', 'Cylinder', ...] по индексу грани
  shape.edgeTypes();          // ['Line', 'Circle', ...] по индексу ребра
  shape.face(3);              // { index, surfaceType, orientation, area, centroid, parameters }
  const mesh = shape.tessellate({ linearDeflection: 0.1, angularDeflection: 0.3 });
  // mesh.positions, mesh.normals: Float32Array (xyz на вершину)
  // mesh.indices: Uint32Array (тройки, против часовой стрелки снаружи)
  // mesh.faceSpans: Uint32Array троек (faceIndex, firstIndex, indexCount)
  // mesh.edges: [{ edgeIndex, points: Float32Array }]
  // mesh.missingFaces, mesh.missingEdges: Uint32Array
  faceOfTriangle(mesh.faceSpans, triangleIndex);  // индекс грани или -1
} finally {
  shape.dispose();
}
kernel.info();                // { occtVersion: '8.0.1', liveShapes }
```

Индексы граней и рёбер — `TopologyIndex` ядра: одинаковы в `faceTypes()`, `face(i)`,
`faceSpans` и `edges`. Значения и ошибки функций — как у C++ API (`KERNEL_CORE.md`); типы —
`bendcore_kernel_api.d.mts`.

`face(i).parameters` — параметры поверхности в глобальных координатах
(`BRepAdaptor_Surface`): плоскость — `origin`, `normal` (наружу из материала, у грани
`REVERSED` развёрнута); цилиндр — `radius`, `axisOrigin`, `axisDirection`; конус —
`referenceRadius`, `semiAngle` (рад), `apex`, ось; сфера — `radius`, `center`; тор —
`majorRadius`, `minorRadius`, ось; B-сплайн — `uDegree`, `vDegree`, `uPoles`, `vPoles`;
`Other` — пустой объект. `area` грани — `BRepGProp` по этой грани.

Параметры `tessellate`: `linearDeflection` (мм, по умолчанию 0,1), `angularDeflection`
(рад, 0,5), `relative` (false), `requireAllFaces` (true). Тесселяция однопоточная
(`inParallel` всегда false: модуль без `-pthread`).

### Время жизни форм

Формы хранятся в реестре внутри модуля; JS получает целый handle (`shape.handle`).
`dispose()` освобождает форму (вместе с её триангуляцией и индексом топологии), повторный
вызов ничего не делает. Сборщик мусора JS формы **не** освобождает: каждую `readStep`
нужно закрыть `dispose()`. `kernel.info().liveShapes` — число живых форм (тест проверяет,
что после всех проверок оно 0). Массивы сетки — копии (не ссылаются на память WASM) и
остаются действительными после `dispose()` и роста памяти.

### Ошибки

Каждая ошибка — `KernelError` (`extends Error`): `code` — код ядра (`InvalidInput`,
`InvalidShape`, `OperationFailed`, `ParseError`, `IoError`, `Unsupported`) или `Disposed`
(вызов на освобождённой форме); `detail` — текст ядра; `message` — `"<code>: <detail>"`.
Исключения C++ через границу WASM не проходят: привязки возвращают `{ok:false, code,
message}`, исключения OCCT (`Standard_Failure`) и `std::exception` перехватываются и
становятся `OperationFailed`. Примеры: не-STEP или пустой буфер → `ParseError`; не
`Uint8Array` → `InvalidInput`; `face(n)` вне диапазона → `InvalidInput`; прогиб ≤ 0 →
`InvalidInput`; форма без тел для `volumeProperties()` → `InvalidInput`.

## Вьюпорт (`KernelViewport`, `KernelViewerApp`)

- **Геометрия** — только из ядра: `positions`, `normals`, `indices` идут в один
  `BufferGeometry` как есть (без пересчёта нормалей). Для каждой тройки `faceSpans`
  создаётся группа отрисовки (`addGroup(firstIndex, indexCount)`), у сетки два материала:
  обычный и подсветка. Рёбра — один `LineSegments`: точки полилиний ядра подряд, индексы
  соединяют соседние точки одной полилинии (`edgeBuffers.ts`). Грани рисуются с
  `polygonOffset`, чтобы рёбра были видны поверх.
- **Выбор**: щелчок без перетаскивания (≤ 4 px) → `Raycaster.intersectObject` →
  `faceIndex` треугольника (three.js 0.187 для индексированной геометрии даёт номер
  треугольника) → `faceOfTriangle(faceSpans, t)`: двоичный поиск по `firstIndex` диапазонов,
  O(log n). Щелчок мимо модели снимает выбор.
- **Подсветка**: у группы выбранной грани `materialIndex = 1`, без перестроения буферов.
- **Панель «Грань»**: `shape.face(i)` — тип поверхности, площадь, ориентация, параметры
  поверхности, точка попадания. Панель «Модель»: число тел/граней/рёбер, объём (только при
  наличии тел), площадь, габарит, число треугольников.
- **Камера**: `OrbitControls` (`three/addons/controls/OrbitControls.js`), Z вверх, изометрический вид по ограничивающей
  сфере сетки при открытии файла; отрисовка по событию (без постоянного цикла).

### Автоматическое вписывание

`autoFrame = 'always'` (просмотрщик STEP) — вид вписывается в каждую новую сетку.
`autoFrame = 'first'` (редактор детали) — при первой непустой сетке после монтирования или
смены `frameKey` (открытие документа, F) и, кроме того, когда операция добавила тело вне кадра
(`src/ui/kernel/autoFrame.ts`, `shouldReframePart`): число тел показанной формы (`bodyCount`,
`properties(target).counts.solids` той же цели, что и сетка) выросло, габарит модели вышел за
прежний (новое тело не внутри старой модели) и хотя бы один угол габарита проецируется за
пределы изображения или за ближнюю/дальнюю плоскость (`boxInView`). Обычные правки — длина
выдавливания, скругление, удаление и объединение тел, разрезание тела внутри прежнего
габарита — камеру не меняют, даже если модель ушла из кадра (тогда F). Вьюпорт сборки
(`AssemblyViewport`) вписывает вид при открытии и вставке, а после остальных правок — если
хотя бы одно добавленное вхождение (массив, крепёж) не видно целиком
(`shouldReframeInstances`); перемещение и разнесение не добавляют вхождений и вид не меняют.
Число автоматических вписываний — атрибут `data-frames` контейнера вьюпорта (для сквозных
тестов). Тесты: `src/ui/kernel/__tests__/autoFrame.test.ts` (камера с известным полем зрения,
коробки внутри и за краем кадра, массив, правки без новых тел), `bodies.spec.ts`.
- **Открытие файла**: `<input type=file>` → `File.arrayBuffer()` → `kernel.readStep`.
  Прежняя форма освобождается после успешного чтения новой. «Перестроить сетку» вызывает
  `tessellate` с новыми прогибами для той же формы.
- **Состояние для тестов**: `window.bendcoreKernelViewer.getState()` (статус ядра, ошибка,
  сводка модели, выбор) и `worldToClient(x, y, z)` — экранные координаты точки модели в
  текущем виде. Тест проверяет выбор по состоянию, а не по пикселям.

### Импорт STEP/IGES, экспорт, массы (деталь без истории)

Просмотрщик читает файл через `Kernel.importModel(bytes, format)` (STEP или IGES со структурой
XCAF; формат — по расширению): форма — единственная деталь или compound деталей в мировой СК.
Панель «Модель» показывает число деталей файла (`model-parts`), объём (`model-volume`),
массовые характеристики (`MassSection`, плотность по умолчанию 7850 кг/м³); предупреждения
импорта (сшивка IGES) — строкой `kernel-notice`. «Экспорт…» (`viewer-export`) пишет исходную
структуру файла в STEP/IGES/STL/3MF. В оболочке просмотрщик — вкладка «Деталь без истории»;
свойства `importFile`, `onAssembly`, `exportRequest` (`docs/ui/DATA_EXCHANGE.md`).

## Измерения и сечение

Измерения и сечение — функции ядра `bendcore_kernel_analysis` (`docs/kernel/ANALYSIS.md`, разделы
4, 5), выведенные в WASM-модули и в интерфейс. Геометрия не считается в JS: страница передаёт
индексы выбранных граней и рёбер, ядро возвращает значение и точки для показа.

**WASM-биндинги** (`src/wasm/analysis_common.hpp`, общий заголовок трёх модулей):

| Модуль | Вызов | Что |
|---|---|---|
| ядро | `KernelShape.measure(spec)`, `KernelShape.section(plane, deflection?)`, `Kernel.materials()` | элементы одной формы |
| деталь | `PartDocumentHandle.measure(spec, target?)`, `.section(plane, target?, deflection?)`, `ModelingKernel.materials()` | элементы цели документа (`final`, `before:<id>`…) |
| сборка | `AssemblyDocumentHandle.measure(spec)` | элементы компонентов в мировых координатах (форма детали сдвигается на положение вхождения, `worldPlacement`) |

`spec = { type, a, b }`, `type` — `distance` (`BRepExtrema`, любые элементы), `angle` (две плоские
грани или два прямых ребра), `length` (ребро), `area` (грань), `radius` (ребро-окружность,
цилиндр, сфера, конус, тор); элемент — `{ kind: 'face' | 'edge', index }`, в сборке ещё
`path` (цепочка вхождений). Результат `{ type, unit: 'mm' | 'mm2' | 'deg', value, points }`:
`points` — то, что рисовать (ближайшие точки; вершина угла и точки на элементах; начало, конец,
середина ребра). Радиус возвращает ещё `diameter`. Неверный запрос — `InvalidInput`
(индекс вне диапазона, угол между гранью и ребром, расстояние с одним элементом), элемент без
поддержки — `Unsupported`. Плоскость сечения — `{ origin, normal }`; ответ — `{ area, polylines }`
(контур, мм; площадь только для тел, 0 — плоскость не пересекает тело).

**Интерфейс** (`ViewToolsPanel.tsx`, `useViewTools.ts`, `viewTools.ts` — общие для страниц
детали, STEP-просмотра и сборки):

- Панель «Измерения»: «Измерять» включает инструмент — щелчки во вьюпорте принадлежат
  измерению, а не показу параметров грани. Что измерить: расстояние, угол, длина ребра, площадь
  грани, радиус. Для расстояния и угла выбираются два элемента (переключатель «Грани» /
  «Рёбра»; щелчок по выбранному снимает его, после полного набора следующий щелчок начинает
  новое измерение); для длины, площади и радиуса — один. Выбранные элементы подсвечены. Результат
  — в панели (`measure-value`) и на вьюпорте: линия между точками и подпись у её середины
  (`measure-label`, `data-label`). Расстояния и длины — мм (3 знака), площади — мм², углы — градусы
  (2 знака). Смена модели (перестроение) сбрасывает выбор.
- Панель «Сечение» (деталь и STEP-просмотр): плоскость нормали X, Y или Z, ползунок вдоль габарита
  модели (0…1000 шагов), «Оставить другую сторону». Модель отсекается плоскостью отсечения
  отрисовщика (`renderer.clippingPlanes`; сторона `(p − o)·n ≤ 0` остаётся, как в `clipByPlane`
  ядра), поэтому ползунок не вызывает ядро; после паузы 150 мс ядро строит контур сечения
  (`sectionByPlane`), он рисуется красной линией, площадь сечения показывается в панели. Щелчок
  не выбирает отсечённые грани. `data-clipped` и `data-section-lines` вьюпорта — для тестов.
- Окно `window.bendcorePartModeler.getState().tools`, `bendcoreKernelViewer.getState().tools`,
  `bendcoreAssembly.getState().tools`: активность, тип, выбор, результат, сечение.

**Проверка.** `tests/wasm/viewport_tools.test.mjs` (`node tests/wasm/viewport_tools.test.mjs
--modeling <каталог> --kernel <каталог> --assembly <каталог>`, 18 проверок): расстояния 20 и 60
(мм), углы 90°, 180° и 0°, длина 100, площадь 6000, радиус цилиндра 5, сечения 6000, 1200 и
наклонное `60·20·√2`, плоскость мимо тела, ошибки, мировые координаты в сборке (расстояние 30 при
сдвиге 50, угол 90° при повороте). Jest: `src/ui/kernel/__tests__/viewTools.test.ts` (набор
выбора, формат, плоскость, уравнение отсечения). Playwright: `tests/e2e-kernel/viewport_tools.spec.ts`
(угол 90°, площадь 6000, радиус 20, диаметр 40, сечение цилиндра π·20² и 2000).

Ограничения: углы — только плоские грани и прямые рёбра (`Unsupported` иначе); расстояние до
изогнутой поверхности точно по значению (~1e-7 мм), точки — ~1e-3 мм вдоль поверхности;
измерение в сборке — только элементы деталей верхнего уровня и подсборок, видимых во вьюпорте;
сечение сборки в интерфейсе не выведено; вершины не выбираются; результат не сохраняется в файле
и не превращается в размер чертежа.

## Запуск и тесты

```bash
npm ci --ignore-scripts                     # один раз (electron и прочие postinstall не нужны)

# Страница: http://127.0.0.1:3100/ (модуль отдаётся из build-wasm-kernel/wasm_kernel под /kernel/)
BENDCORE_KERNEL_WASM_DIR=build-wasm-kernel/wasm_kernel npm run dev:kernel-viewer

# Модуль в node (корпус): ctest или npm
ctest --test-dir build-wasm-kernel -R wasm_kernel_corpus --output-on-failure
npm run test:kernel-wasm -- --module build-wasm-kernel/wasm_kernel

# Логика UI (Jest)
npx jest src/ui/kernel

# Сквозной тест (Playwright поднимает webpack dev server сам)
PLAYWRIGHT_CHROMIUM_EXECUTABLE=/opt/pw-browsers/chromium-1243/chrome-linux64/chrome \
  npm run test:e2e:kernel
# снимок экрана для этой страницы:
BENDCORE_DOC_SCREENSHOT=$PWD/docs/ui/img/kernel_viewport.png npm run test:e2e:kernel
```

Тест модуля зарегистрирован в **ctest** (`wasm_kernel_corpus`, `cmake/wasm_kernel.cmake`):
он зависит от результата сборки CMake, а `--module`/`--corpus` подставляются из путей
сборки; `npm run test:kernel-wasm` — тот же файл для запуска без ctest (по умолчанию
`build-wasm-kernel/wasm_kernel`). Используется встроенный `node:test`, внешних пакетов нет.

`PLAYWRIGHT_CHROMIUM_EXECUTABLE` нужен, когда браузер для версии `@playwright/test` из
`package-lock.json` не скачан (в облачной среде предустановлен Chromium 1243). WebGL в
headless-режиме — через SwiftShader (`--use-angle=swiftshader`).

### Что проверяется

`tests/wasm/kernel_corpus.test.mjs` (21 тест, ~4 с) — для каждой из 13 моделей корпуса
против `manifest.json` (значения по формулам из размеров, не измерения OCCT): объём,
площадь (относительно 1e-6), габарит (1e-4 мм), число граней и тел; типы граней по
построению модели (коробка — 6 плоскостей, цилиндр — 2 плоскости и цилиндр, тор, сфера,
усечённый конус и т. д.), радиусы цилиндров и сферы; сумма площадей граней = площадь
формы; сетка: одна тройка `faceSpans` на грань, диапазоны подряд и покрывают все индексы,
индексы в пределах вершин, нет `missingFaces`, объём сетки по теореме Гаусса — Остроградского
= объём по формуле с точностью 0,5 % (замкнутость и наружный обход); `faceOfTriangle` для
первого и последнего треугольника каждой грани. Отдельно: нормали граней коробки наружу и
их площади (1200/2000/6000), полуугол конуса `atan((R−r)/h)`, ошибки (`ParseError`,
`InvalidInput`, `Disposed`), `liveShapes = 0` в конце.

`tests/e2e-kernel/kernel_viewport.spec.ts` (4 теста, ~14 с): загрузка ядра; открытие
`box_100x60x20.step` через поле файла, щелчок в центр верхней грани (50, 30, 20) → плоскость
с нормалью (0, 0, 1) и площадью 6000; в центр передней (50, 0, 10) → нормаль (0, −1, 0),
2000; щелчок по фону снимает выбор; `cylinder_r20_h50.step` — щелчок по боковой стороне →
`Cylinder`, радиус 20, площадь 2π·20·50; `box_80x60x40_all_fillets_r5.step` — верхняя грань
площадью 70·50 и снимок экрана (1280×800, `deviceScaleFactor: 1`); файл не STEP → на
странице `ParseError`.

## Выполнение в Web Worker (фаза 5)

Страница вызывает модуль не напрямую, а через воркер (`docs/ui/APP_SHELL.md`): загрузчик
возвращает `WasmClient`, методы API на странице асинхронны (`Promise`), ошибки — тот же
`KernelError`. Долгие вызовы показывают индикатор с кнопкой «Прервать»; после отмены документ
восстанавливается из снимка последнего завершённого изменения. Тестовый хук страницы получил
`idle()` — сквозные тесты читают состояние после него. Синхронный API `src/wasm/*_api.mjs`
не изменился и используется в node-тестах и внутри воркера.

## Ограничения

- Только STEP и только геометрия (как `readStep` ядра): без имён, цветов и структуры
  сборки (XCAF — отдельная задача); сборка показывается как один компаунд, грани всех
  вхождений нумеруются подряд.
- Однопоточный модуль: без `-pthread` (для него нужны заголовки COOP/COEP), BRepMesh без
  параллельного режима. Чтение и тесселяция выполняются в главном потоке и блокируют
  страницу на время вызова (сотни миллисекунд для корпуса); перенос модуля в Web Worker —
  отдельный шаг.
- Размер модуля (3,6 МиБ gzip) задаёт время первой загрузки; сервер должен отдавать `.wasm`
  с `Content-Type: application/wasm` (для потоковой компиляции) и сжатием.
- На этой странице выбираются только грани. Вьюпорт умеет и выбор рёбер (`pickMode="edge"`,
  ближайший видимый отрезок полилинии ребра в пределах 6 px, `edgePick.ts`), подсветку
  нескольких граней и рёбер и наложенную полупрозрачную сетку — их использует страница
  моделирования детали (`docs/ui/PART_MODELING_UI.md`). Вершины не выбираются.
- Формы не освобождаются сборщиком мусора — только `dispose()` (страница освобождает
  прежнюю форму при открытии новой и при размонтировании).
- Сетка в `float`: при координатах ~1e3 мм погрешность вершин ~1e-4 мм (`KERNEL_CORE.md`).
- Типы three.js в проекте — собственные объявления `src/declarations.d.ts` (пакета
  `@types/three` нет); в них добавлены используемые вьюпортом члены (`addGroup`, `groups`,
  `boundingSphere`, `intersectObject`, массив материалов `Mesh`, модуль `OrbitControls`).
- **three.js 0.187** (было 0.128). Внешний вид вьюпортов сохранён: `src/ui/legacyLook.ts`
  выключает управление цветом (`ColorManagement.enabled = false`), ставит
  `renderer.outputColorSpace = LinearSRGBColorSpace` (цвета палитры идут в кадр как записаны,
  как до r152), а интенсивности `AmbientLight`/`DirectionalLight` умножает на π (физические
  единицы без «старого» деления на π; значения 0,45 и 0,7 остались прежними). Снимки
  `docs/ui/img/three-upgrade/before_*.png` и `after_*.png` совпадают попиксельно. `Geometry`
  в коде не использовался (только `BufferGeometry`); `OrbitControls` — из `three/addons`.
  Jest компилирует ES-модули `three` в CommonJS (`jest.config.js`).
- Сквозной тест есть только для Chromium.
