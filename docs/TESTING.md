# Сборка и проверки

## Ядро C++ и тесты

Нужны CMake ≥ 3.20, компилятор C++20 и Eigen (заголовки). nlohmann/json лежит в `third_party/nlohmann`.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBENDCORE_EIGEN_DIR=<папка с Eigen/>
cmake --build build -j
ctest --test-dir build --output-on-failure
```

`test_performance` собирается, только если найден GoogleTest.

## Ядро для интерфейса

`bendcore_cli` — ядро отдельным процессом: одна строка JSON на запрос и ответ
(`{"id", "command", "args"}` → `{"id", "ok", "data" | "error"}`). Electron запускает его через
`src/core_bridge.ts`; путь — переменная `BENDCORE_CLI`, ресурсы сборки или `build/bendcore_cli(.exe)`.
Команды: `bend.lra`, `bend.xyz`, `bend.table`, `check.run`, `instances.*`, `drawing.part`,
`drawing.release_check` (см. `src/core_api.cpp`). В WebAssembly тот же диспетчер — функция `dispatch`.

## Тесты интерфейса

```bash
# логика панелей и мост к ядру (Node 22, tsx)
BENDCORE_CLI=build/bendcore_cli npx tsx --test tests/node/*.test.ts

# стенд панелей в браузере против настоящего ядра
npx esbuild tests/ui_preview/main.tsx --bundle --outfile=tests/ui_preview/dist/main.js \
  --loader:.css=css --jsx=automatic --define:process.env.NODE_ENV='"production"'
BENDCORE_CLI=build/bendcore_cli node tests/ui_preview/server.mjs &   # http://localhost:5199/?view=check|bend|instances|drawing|sketch
node tests/ui_preview/shots.mjs                                       # Playwright: действия и снимки, «UI: OK»
```
