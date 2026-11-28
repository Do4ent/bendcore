#!/usr/bin/env bash
# Самотест scripts/check_no_stubs.sh на фикстурах tests/qa/fixtures.
#   1. Полный режим на фикстурах нарушений: множество «файл:строка:правило» совпадает
#      с tests/qa/fixtures/expected_violations.txt, код выхода 1.
#   2. Полный режим на законном коде (fixtures/clean): нарушений нет, код выхода 0.
#   3. Режим --diff во временном git-репозитории: сообщаются только добавленные строки
#      (закоммиченные и незакоммиченные), старые нарушения и каталог фикстур не сообщаются;
#      полная проверка без --paths охватывает src, include, tests целиком.
#   4. Ошибки запуска: код выхода 2.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
checker="$repo/scripts/check_no_stubs.sh"
fixtures="$here/fixtures"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
failures=0

fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
pass() { echo "ok:   $*"; }

normalize() { sed -E -n 's/^([^:]+):([0-9]+): \[([a-z-]+)\].*/\1:\2:\3/p' | sort; }

# --- 1. нарушения -------------------------------------------------------------------
set +e
bash "$checker" --root "$fixtures/violations" --paths src tests >"$tmp/out1" 2>"$tmp/err1"
rc=$?
set -e
normalize <"$tmp/out1" >"$tmp/got1"
grep -v '^#' "$fixtures/expected_violations.txt" | sed '/^$/d' | sort >"$tmp/want1"
if [[ $rc -ne 1 ]]; then fail "фикстуры нарушений: код выхода $rc, ожидался 1"; cat "$tmp/err1" >&2; fi
if diff -u "$tmp/want1" "$tmp/got1" >"$tmp/d1"; then
  pass "фикстуры нарушений: $(wc -l <"$tmp/want1") ожидаемых нарушений найдены, лишних нет"
else
  fail "фикстуры нарушений: расхождение (- ожидалось, + получено)"; cat "$tmp/d1" >&2
fi
# Формат вывода: файл:строка: [правило] пояснение | фрагмент
if ! grep -qE '^src/kernel/bad_shape\.cpp:10: \[todo\] .+ \| // TODO: compute the real volume$' "$tmp/out1"; then
  fail "формат строки вывода не соответствует 'файл:строка: [правило] пояснение | фрагмент'"
else
  pass "формат вывода"
fi

# --- 2. законный код ------------------------------------------------------------------
set +e
bash "$checker" --root "$fixtures/clean" --paths src tests >"$tmp/out2" 2>"$tmp/err2"
rc=$?
set -e
if [[ $rc -eq 0 && ! -s "$tmp/out2" ]]; then
  pass "законный код: ложных срабатываний нет"
else
  fail "законный код: код выхода $rc, ложные срабатывания:"; cat "$tmp/out2" >&2
fi

# --- 3. режим --diff --------------------------------------------------------------------
g() { git -C "$tmp/repo" -c user.name=qa -c user.email=qa@localhost -c commit.gpgsign=false "$@"; }
mkdir -p "$tmp/repo/src/legacy" "$tmp/repo/tests/qa/fixtures" "$tmp/repo/docs"
g init -q
cat >"$tmp/repo/src/legacy/old.cpp" <<'EOF'
// TODO legacy marker that predates the base
int old_value(int x) { int y = x + 1; return y; }
EOF
echo '// TODO fixture marker' >"$tmp/repo/tests/qa/fixtures/sample.cpp"
g add -A && g commit -q -m base
g branch -q base-ref

cat >>"$tmp/repo/src/legacy/old.cpp" <<'EOF'
int added_value(int x) { int y = x * 2; return y; }
// FIXME new problem in a committed change
EOF
cat >"$tmp/repo/src/legacy/new_feature.cpp" <<'EOF'
// Placeholder for the new feature
int feature(int x) { int y = x - 1; return y; }
EOF
mkdir -p "$tmp/repo/src/ui/kernel"
cat >"$tmp/repo/src/ui/kernel/panel.tsx" <<'EOF2'
// Viewer panel
export function onPick(face: number): void {}
EOF2
echo '// TODO another fixture marker' >>"$tmp/repo/tests/qa/fixtures/sample.cpp"
echo 'TODO in documentation is not code' >"$tmp/repo/docs/notes.md"
g add -A && g commit -q -m change
# Незакоммиченное изменение рабочей копии тоже проверяется.
echo '// stub left in the working tree' >>"$tmp/repo/src/legacy/old.cpp"

set +e
bash "$checker" --root "$tmp/repo" --no-full --diff base-ref >"$tmp/out3" 2>"$tmp/err3"
rc=$?
set -e
normalize <"$tmp/out3" >"$tmp/got3"
printf '%s\n' \
  'src/legacy/new_feature.cpp:1:placeholder' \
  'src/legacy/old.cpp:4:todo' \
  'src/legacy/old.cpp:5:stub' \
  'src/ui/kernel/panel.tsx:2:empty-body' | sort >"$tmp/want3"
if [[ $rc -ne 1 ]]; then fail "--diff: код выхода $rc, ожидался 1"; cat "$tmp/err3" >&2; fi
if diff -u "$tmp/want3" "$tmp/got3" >"$tmp/d3"; then
  pass "--diff: только добавленные строки, старые нарушения и фикстуры пропущены"
else
  fail "--diff: расхождение (- ожидалось, + получено)"; cat "$tmp/d3" >&2
fi

# Чистое изменение относительно базы: код выхода 0.
g checkout -q -- src/legacy/old.cpp
g reset -q --hard base-ref
echo 'int clean_change(int x) { int y = x * 3; return y; }' >>"$tmp/repo/src/legacy/old.cpp"
set +e
bash "$checker" --root "$tmp/repo" --no-full --diff base-ref >"$tmp/out4" 2>"$tmp/err4"
rc=$?
set -e
if [[ $rc -eq 0 && ! -s "$tmp/out4" ]]; then
  pass "--diff: чистое изменение проходит (старый TODO в базе не сообщается)"
else
  fail "--diff: чистое изменение: код выхода $rc"; cat "$tmp/out4" "$tmp/err4" >&2
fi

# --- 3б. каталоги полной проверки и типы файлов ---------------------------------------
# Полная проверка охватывает весь src, include, tests (новые модули и старый код: трубный
# модуль, сервер, CLI, Electron) и страницы web/<модуль>; .mts и .tsx — файлы кода;
# каталог вне списка (examples/) и фикстуры самотеста не сканируются.
full="$tmp/full"
for f in src/wasm/kernel_bindings.cpp src/wasm/bendcore_kernel_api.mjs src/wasm/api.d.mts \
         src/ui/kernel/KernelViewport.tsx src/modeling/features/extrude.cpp \
         src/assembly_v2/solver.cpp src/drawing_v2/model.cpp src/kernel/shape.cpp \
         tests/wasm/kernel_corpus.test.mjs tests/e2e-kernel/kernel_viewport.spec.ts \
         web/kernel-viewer/playwright.config.ts src/legacy/old_code.cpp src/server/main.cpp \
         src/ui/RibbonBar.tsx include/bendcore/tubegeom.hpp tests/test_tubegeom.cpp \
         examples/old_demo.cpp tests/qa/fixtures/marker.cpp; do
  mkdir -p "$full/$(dirname "$f")"
  printf '%s\n' '// TODO marker for the directory check' >"$full/$f"
done
set +e
bash "$checker" --root "$full" >"$tmp/out5" 2>"$tmp/err5"
rc=$?
set -e
normalize <"$tmp/out5" >"$tmp/got5"
printf '%s:1:todo\n' src/wasm/kernel_bindings.cpp src/wasm/bendcore_kernel_api.mjs src/wasm/api.d.mts \
  src/ui/kernel/KernelViewport.tsx src/modeling/features/extrude.cpp src/assembly_v2/solver.cpp \
  src/drawing_v2/model.cpp src/kernel/shape.cpp tests/wasm/kernel_corpus.test.mjs \
  tests/e2e-kernel/kernel_viewport.spec.ts web/kernel-viewer/playwright.config.ts \
  src/legacy/old_code.cpp src/server/main.cpp src/ui/RibbonBar.tsx include/bendcore/tubegeom.hpp \
  tests/test_tubegeom.cpp | sort >"$tmp/want5"
if [[ $rc -eq 1 ]] && diff -u "$tmp/want5" "$tmp/got5" >"$tmp/d5"; then
  pass "полная проверка: src, include, tests целиком (новый и старый код), web/<модуль>; examples/ и фикстуры — нет"
else
  fail "полная проверка каталогов: код $rc, расхождение (- ожидалось, + получено)"; cat "$tmp/d5" >&2 || true
fi

# --- 4. ошибки запуска -------------------------------------------------------------------
set +e
bash "$checker" --root "$tmp/repo" --diff no-such-ref >/dev/null 2>&1; rc_ref=$?
bash "$checker" --bogus-option >/dev/null 2>&1; rc_arg=$?
set -e
[[ $rc_ref -eq 2 ]] && pass "несуществующая ссылка: код 2" || fail "несуществующая ссылка: код $rc_ref, ожидался 2"
[[ $rc_arg -eq 2 ]] && pass "неизвестный аргумент: код 2" || fail "неизвестный аргумент: код $rc_arg, ожидался 2"

if [[ $failures -ne 0 ]]; then
  echo "test_check_no_stubs: провалено проверок: $failures" >&2
  exit 1
fi
echo "test_check_no_stubs: все проверки пройдены"
