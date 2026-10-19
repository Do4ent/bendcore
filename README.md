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

## WebAssembly (браузер и Capacitor)
Пресеты `CMakePresets.json`: `host` (нативно) и `wasm` (Emscripten). Для `wasm` нужен активированный emsdk
(переменная `EMSDK` задаётся `emsdk_env.sh` / `emsdk_env.bat`):

    source $EMSDK/emsdk_env.sh          # Windows: emsdk_env.bat
    cmake --preset wasm
    cmake --build --preset wasm
    ctest --preset wasm                 # тесты запускаются через node (нужен в PATH)

Результат: `build-wasm/web/bendcore.js` и `bendcore.wasm`, плюс `index.html` и `bendcore.mjs`.
Страница проверяет квадрат на 2D-эскизе; открывать через HTTP-сервер, не через `file://`:

    cd build-wasm/web && python3 -m http.server 8000     # затем http://localhost:8000

JS-интерфейс: `web/bendcore.mjs` — `loadBendcore()` и класс `Sketch2D` (методы в camelCase, `solve()` возвращает обычный объект).
Мост собран через embind (`src/wasm/bindings.cpp`): Sketch2D, `tangentLength`. Остальные функции ядра добавляются по мере нужды.

Особенности WASM-сборки:
- PlaneGCS собирается статически в тот же модуль (динамических библиотек в WASM нет). Вопрос LGPL для такой сборки и для iOS (Capacitor) — юристу.
- Исключения включены (`-fexceptions`): ядро бросает `std::invalid_argument` при неверных данных.
- Сборка и тесты WASM ещё не проверены: эта часть написана без доступа к Emscripten. Первый запуск может потребовать правок флагов.
