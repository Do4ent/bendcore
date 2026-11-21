#!/usr/bin/env bash
# Приёмка ветки после слияния (docs/PROJECT_PROCESS.md, разделы 6–7): полный прогон, который
# ролевые агенты не делают сами. Требует настроенных build-occt (native, OCCT) и
# build-wasm-all (Emscripten, docs/ui/APP_SHELL.md) в корне репозитория.
#
#   bash scripts/accept_branch.sh            # всё
#   BENDCORE_ACCEPT_SKIP_E2E=1 bash ...      # без Playwright
#
# Код возврата — число упавших шагов (0 — ветка принимается).
set -u
cd "$(dirname "$0")/.."

failures=0
step() {
  local name="$1"; shift
  echo "== $name"
  if "$@"; then echo "   ok"; else echo "   FAILED: $name"; failures=$((failures + 1)); fi
}

native_build() { cmake --build build-occt -j2 >/dev/null; }
native_tests() { ctest --test-dir build-occt -E 'wasm|performance|stress|fuzz' -j2 --output-on-failure | tail -3; }
stress_tests() { ctest --test-dir build-occt -j1 -R 'performance|stress|fuzz' --output-on-failure | tail -3; }
no_stubs() { bash scripts/check_no_stubs.sh | tail -1; }

wasm_build() {
  # shellcheck disable=SC1091
  source /root/emsdk/emsdk_env.sh >/dev/null 2>&1
  cmake --build build-wasm-all -j2 --target bendcore_kernel_wasm bendcore_modeling_wasm \
    bendcore_assembly_wasm bendcore_drawing_wasm >/dev/null
}

# Node-тесты WASM: каталог модуля передаётся аргументом --module. Тест, который ничего не
# проверил (pass 0), считается упавшим — так ловится неверный каталог.
node_test() {
  local out pass fail
  out=$(node "$@" 2>&1)
  pass=$(printf '%s\n' "$out" | sed -n 's/^# pass \([0-9]*\)$/\1/p' | tail -1)
  fail=$(printf '%s\n' "$out" | sed -n 's/^# fail \([0-9]*\)$/\1/p' | tail -1)
  echo "   pass ${pass:-?} fail ${fail:-?}"
  [ "${fail:-1}" = "0" ] && [ "${pass:-0}" -gt 0 ]
}

W=build-wasm-all
step "native build" native_build
step "native ctest" native_tests
step "stress/fuzz -j1" stress_tests
step "check_no_stubs" no_stubs
step "wasm build" wasm_build
step "node kernel" node_test tests/wasm/kernel_corpus.test.mjs --module $W/wasm_kernel
step "node modeling" node_test tests/wasm/modeling_document.test.mjs --module $W/wasm_modeling
step "node sketch" node_test tests/wasm/sketch_editor.test.mjs --module $W/wasm_modeling
step "node assembly" node_test tests/wasm/assembly_document.test.mjs --module $W/wasm_assembly \
  --modeling-module $W/wasm_modeling
step "node exchange" node_test tests/wasm/exchange.test.mjs --module $W/wasm_kernel
step "node drawing" node_test tests/wasm/drawing_document.test.mjs --module $W/wasm_drawing \
  --assembly-module $W/wasm_assembly
step "node viewport tools" node_test tests/wasm/viewport_tools.test.mjs --modeling $W/wasm_modeling \
  --kernel $W/wasm_kernel --assembly $W/wasm_assembly
step "tsc" npx tsc --noEmit -p .
step "jest" bash -c 'npx jest 2>&1 | grep -E "^Tests:"; exit ${PIPESTATUS[0]}'

if [ "${BENDCORE_ACCEPT_SKIP_E2E:-0}" != "1" ]; then
  export PLAYWRIGHT_CHROMIUM_EXECUTABLE="${PLAYWRIGHT_CHROMIUM_EXECUTABLE:-/opt/pw-browsers/chromium-1243/chrome-linux64/chrome}"
  export BENDCORE_KERNEL_WASM_DIR=$W/wasm_kernel BENDCORE_MODELING_WASM_DIR=$W/wasm_modeling \
    BENDCORE_ASSEMBLY_WASM_DIR=$W/wasm_assembly BENDCORE_DRAWING_WASM_DIR=$W/wasm_drawing
  for suite in app drawing assembly modeling kernel; do
    step "e2e $suite" bash -c "npm run -s test:e2e:$suite -- --trace off 2>&1 | grep -E ' passed| failed| flaky'; exit \${PIPESTATUS[0]}"
  done
fi

echo "== failed steps: $failures"
exit "$failures"
