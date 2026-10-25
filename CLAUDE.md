# bendcore — рабочие заметки для агентов

Ядро C++20 для гнутых труб (CAD). Интерфейс — TypeScript, мост WASM (embind), веб-страницы в `web/`.
Тексты для пользователя — по-русски. Спецификации в репозитории не менять без согласования.

## Сборка и тесты

Linux (GCC), без OCCT:
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

Eigen: системный (`find_package(Eigen3)`) или каталог `-DBENDCORE_EIGEN_DIR=<путь, где папка Eigen/>`.
В репозитории Eigen нет.

Open CASCADE (только статическая сборка, V8.0.1, модули — как в `.github/workflows/wasm.yml`, задача `occt`):
    cmake -S . -B build-occt -G Ninja -DCMAKE_BUILD_TYPE=Release -DBENDCORE_WITH_OCCT=ON \
      -DBENDCORE_OCCT_DIR=<установка OCCT> -DBENDCORE_EIGEN_DIR=<Eigen>
Сервер `bendcore_server` собирается только с OCCT и только не на Windows (POSIX-сокеты).

WebAssembly: `scripts/build_wasm.sh` (emsdk 6.0.12). Проверка страниц в Chromium: `scripts/check_wasm_page.mjs`.

Docker-образ сервиса: `docker build --build-context occt=<установка OCCT> -f docker/Dockerfile -t bendcore-server .`

## CI

- `wasm.yml` — задачи `wasm` и `occt` (Linux). Запускается на push в `main`, на pull request и вручную.
- `windows.yml` — задачи `msvc` (ядро и тесты MSVC x64, без OCCT) и `msvc-occt` (OCCT под MSVC, кэш установки
  `occt-msvc-V8.0.1-static-v1`). Запускается вручную или на push в `feature/windows-build`.

Особенности:
- Логи шагов через API недоступны (403). Ключевые строки выводятся аннотациями `::error::` (см. шаги «Ошибки в аннотации»).
  Читать: `gh api repos/Do4ent/bendcore/check-runs/<job-id>/annotations`.
- `workflow_dispatch` работает только для файлов из ветки по умолчанию.
- `gh pr create` и `gh pr merge` через GraphQL в сессии недоступны; используйте REST:
  `gh api -X POST repos/Do4ent/bendcore/pulls` и `gh api -X PUT repos/Do4ent/bendcore/pulls/<n>/merge`.

## Решения (зафиксированы в переписке)

- Open CASCADE для тела трубы (не truck). Статическая сборка с DataExchange.
- Эскизы: PlaneGCS (LGPL-2.1, `third_party/planegcs`).
- Сервис тела трубы: HTTP без сторонних библиотек (POSIX-сокеты), CORS открыт, без авторизации; тело отдаётся
  JSON-массивами (вершины, индексы), `deflection` по умолчанию 0.01 мм в API, 0.05 мм в `view.html`.
- Стенка — отверстие булевым вычитанием двух протяжек. Спирали со стенкой при коротком шаге — отказ (см. ниже).
- Пути с самопересечением отклоняются ошибкой 400: шаг спирали < ∅, радиус гиба или витка < радиуса трубы.
- Платформа версии 1 — Windows x64 (основная); Windows-сборка пока в разработке (см. ниже).
- Слияние в `main` и новые коммиты — только по явной команде пользователя.
- Коммиты: трейлер `Co-Authored-By: Claude Haiku 5.5 <noreply@anthropic.com>` и `Claude-Session: <ссылка сессии>`.

## Открыто

- Windows: OCCT под MSVC (задача `msvc-occt`), затем сервер через Winsock или исключение сервера из Windows-сборки.
- Тесты WASM на файлах примеров (`--preload-file`); сейчас файловые тесты пропускаются под `__EMSCRIPTEN__`.
- Сравнение с солвером CADCraft (`storytold/cadcraft`): нужен доступ к crates.io из песочницы.
- Ветка `feature/windows-build` содержит много коммитов диагностики CI: перед слиянием свести к одному чистому коммиту.
