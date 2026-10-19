# bendcore C++ (ядро, этап 1)

Порт `bendcore/tubegeom.py` на C++20. Эталон — Python-реализация, значения в тестах взяты из неё.

Сборка и тесты (Windows x64 — основная платформа версии 1; здесь проверено на Linux):

    cmake -S . -B build
    cmake --build build
    ctest --test-dir build

Перенесено: `tangent_length`, `axis_length`, `normalize_rotation`, `wrap180`, `table_row`, `trace` (прямые, гибы, спираль),
`validate`, `self_intersections`, `manual_angle_tolerance`, `end_drift`, `path_from_table`; спираль — `spiral.hpp`.
Всё из `tubegeom.py` (этап 1, модуль tubegeom) перенесено; тесты сверяют с эталонами Python.
Текст сообщений `validate` сокращён: тесты сверяют вид и номер, не формулировку.

## Этап 1–2 в C++
- `tubegeom` (полностью), `spiral`, `blank`, `compensation`, `calib`, `ports`, `refs` — перенесены; тесты сверяют с эталонами Python или со сценариями тестов Python.
- `refs`: пересчёт вершин вынесен в решатель, передаваемый функцией (`PointSolver`). Выбор решателя (PlaneGCS) — отдельно.
- Не перенесено: `sketch`, `body3d`, `library`, `bendmap`, `keymap`, `commands`, `layout`, `ribbon`, `ui_model`, `settings`, `units`, `icons`.

## Eigen (зависимость решателя)
Для 3D- и 2D-эскизов нужна библиотека Eigen (MPL-2.0), в репозиторий она не входит. CMake ищет её в таком порядке:

1. Системная установка `Eigen3` (CMake-пакет `Eigen3`, версия 3.3 или новее).
2. Каталог из переменной `BENDCORE_EIGEN_DIR` (внутри него папка `Eigen/`), например:

    cmake -S . -B build -DBENDCORE_EIGEN_DIR=/path/to/eigen-5.0.0

3. Папка `third_party/eigen-5.0.0` внутри репозитория, если вы её туда положили.

Проверено с Eigen 5.0.0 (C++20, GCC 13 и Clang 18). Версия 3.4.90 из зеркала master не компилируется в C++20.

## WebAssembly (браузер и Capacitor)
Пресеты `CMakePresets.json`: `host` (нативно) и `wasm` (Emscripten). Для `wasm` нужен активированный emsdk
(переменная `EMSDK` задаётся `emsdk_env.sh` / `emsdk_env.bat`):

    source $EMSDK/emsdk_env.sh          # Windows: emsdk_env.bat
    cmake --preset wasm
    cmake --build --preset wasm
    ctest --preset wasm                 # тесты запускаются через node (нужен в PATH)

Результат: `build-wasm/web/bendcore.js` и `bendcore.wasm`, плюс `index.html` и `bendcore.mjs`.
В CI (`.github/workflows/wasm.yml`) после сборки страница `index.html` открывается в headless Chromium (Playwright):
проверка проходит, если страница показывает «Тест пройден» (квадрат на эскизе и `wrap180(190) = -170`).
Страница проверяет квадрат на 2D-эскизе; открывать через HTTP-сервер, не через `file://`:

    cd build-wasm/web && python3 -m http.server 8000     # затем http://localhost:8000

JS-интерфейс: `web/bendcore.mjs` — `loadBendcore()` и класс `Sketch2D` (методы в camelCase, `solve()` возвращает обычный объект).
Мост собран через embind (`src/wasm/bindings.cpp`): Sketch2D, `tangentLength`. Остальные функции ядра добавляются по мере нужды.

Особенности WASM-сборки:
- PlaneGCS собирается статически в тот же модуль (динамических библиотек в WASM нет). Вопрос LGPL для такой сборки и для iOS (Capacitor) — юристу.
- Исключения включены (`-fexceptions`): ядро бросает `std::invalid_argument` при неверных данных.
- Сборка, тесты в node и проверка страницы в браузере проверяются в CI (GitHub Actions, emsdk 6.0.12); локально Emscripten не запускался.
- Если сборка в CI падает, полный лог — в артефакте `bendcore-wasm-logs`, строки ошибок — в аннотациях задания.

## Open CASCADE: тело трубы (проверка)
Тело трубы (`src/occt/tube_body_occt.cpp`) собирается только при `BENDCORE_WITH_OCCT=ON`. Осевая линия по кускам — `axis_pieces` (`axis.hpp`, без OCCT, проверяется `test_axis`): прямые, дуги и спираль.
- `geom::make_tube(path, od, wall)` — труба сплошная (wall = 0) или со стенкой (наружное тело минус внутреннее булевой разностью). Объём: π·r²·L и π·(r² − r_в²)·L. Тест `test_tube_occt`: пять путей (плоский гиб, поворот A, пространственный, две спирали) и стенка 2 мм.
- `geom::end_ports(path, od, wall)` — порты по кромкам концов (окружности на плоских торцах), направление наружу от центра масс. У трубы со стенкой на торце два круга (наружный и внутренний): порт берётся по наружному.
- `geom::face_port(path, od, pick, name, wall)` — порт на грани тела (цилиндр, тор или торец): ближайшая точка поверхности к `pick`, нормаль наружу в ней. ∅ = od.
- `manual_port(pos, direction, od, name)` (`ports.hpp`, без OCCT) — порт по точке, направлению и ∅; направление нормируется.
- Автоматического создания портов при импорте пути нет: импорт в репозитории не реализован, а `end_ports` вызывается явно.
Нужна статическая сборка OCCT (тег V8.0.1) с модулями Foundation, ModelingData, ModelingAlgorithms, установленная в каталог:

    cmake -S /path/to/OCCT -B occt-build -G Ninja -DBUILD_LIBRARY_TYPE=Static -DBUILD_MODULE_Visualization=OFF \
      -DBUILD_MODULE_ApplicationFramework=OFF -DBUILD_MODULE_DataExchange=OFF -DBUILD_MODULE_Draw=OFF \
      -DBUILD_SAMPLES=OFF -DUSE_TBB=OFF -DUSE_FREETYPE=OFF -DUSE_FREEIMAGE=OFF -DUSE_RAPIDJSON=OFF \
      -DUSE_VTK=OFF -DUSE_OPENGL=OFF -DBUILD_Inspector=OFF
    cmake --build occt-build && cmake --install occt-build --prefix /path/to/occt-install

    cmake -S . -B build-occt -DBENDCORE_WITH_OCCT=ON -DBENDCORE_OCCT_DIR=/path/to/occt-install
    cmake --build build-occt && ctest --test-dir build-occt -R tube_occt

Особенности:
- Конфиг OCCT из `cmake --install` для статической сборки неполный, поэтому библиотеки подключаются напрямую из `lib/libTK*.a`.
- Заголовки в `include/opencascade` при установке копируются не все: при сборке нужно скопировать недостающие из `occt-build/include/opencascade`.
- Тело трубы: `BRepOffsetAPI_MakePipe` даёт только боковую поверхность, торцы добавляются вручную, затем сшивка в solid (допуск сшивки 1e-3 мм).
- Спираль: отсчёты центральной линии (256 на виток) аппроксимируются B-сплайном (степень до 8, допуск 1e-4 мм). Сплайн с допуском сшивки 1e-4 давал незамкнутое тело, поэтому допуск 1e-3.
- Проверено на Linux (GCC 13). Сборка на Windows (MSVC, `.lib`) этим блоком не покрыта.
