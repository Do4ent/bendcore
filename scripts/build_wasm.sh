#!/usr/bin/env bash
# Воспроизводимая сборка WebAssembly (Emscripten) и тестов в node.
# Использование: EMSDK=/path/to/emsdk [BENDCORE_EIGEN_DIR=/path/with/Eigen] ./scripts/build_wasm.sh
# Завершается с ненулевым кодом, если любой шаг или проверка выходных файлов не прошли.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if [[ -z "${EMSDK:-}" || ! -f "${EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" ]]; then
  echo "ОШИБКА: EMSDK не задан или не указывает на установку emsdk (нет Emscripten.cmake)." >&2
  echo "Установите emsdk и выполните: source \$EMSDK/emsdk_env.sh" >&2
  exit 2
fi

# shellcheck disable=SC1091
source "${EMSDK}/emsdk_env.sh" >/dev/null
command -v emcc >/dev/null || { echo "ОШИБКА: emcc не найден после emsdk_env.sh" >&2; exit 2; }
command -v node >/dev/null || { echo "ОШИБКА: node не найден (нужен для тестов WASM)" >&2; exit 2; }

cd "$ROOT"
# Eigen: BENDCORE_EIGEN_DIR — каталог, в котором лежит папка Eigen/ (например /usr/include/eigen3).
CMAKE_EXTRA=()
if [[ -n "${BENDCORE_EIGEN_DIR:-}" ]]; then
  CMAKE_EXTRA+=("-DBENDCORE_EIGEN_DIR=${BENDCORE_EIGEN_DIR}")
fi

cmake --preset wasm "${CMAKE_EXTRA[@]}"
cmake --build --preset wasm
ctest --preset wasm

OUT="$ROOT/build-wasm/web"
for f in bendcore.js bendcore.wasm bendcore.mjs index.html; do
  [[ -s "$OUT/$f" ]] || { echo "ОШИБКА: нет выходного файла $OUT/$f" >&2; exit 1; }
done
echo "OK: $OUT/bendcore.js, $OUT/bendcore.wasm ($(stat -c %s "$OUT/bendcore.wasm") байт)"
