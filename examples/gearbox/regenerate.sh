#!/usr/bin/env bash
# Пересборка закоммиченного комплекта редуктора РЦ-80 в examples/gearbox/:
# документы bendcore, STEP сборки, PNG-превью листов 100 dpi (PDF комплекта > 3 МБ в репозиторий
# не кладётся). Полный комплект (PDF, DXF каждого листа, STEP деталей) — в каталоге kit/ сборки.
#   bash examples/gearbox/regenerate.sh [каталог сборки, по умолчанию build-occt]
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
build="${1:-$here/../../build-occt}"
cmake --build "$build" --target gearbox_generate
full="$build/gearbox_kit"
rm -rf "$full"
"$build/gearbox_generate" "$full" >/dev/null
# В репозиторий: документы и STEP сборки.
cp "$full"/*.bcpart.json "$full"/*.bcasm.json "$full"/*.bcdrw.json "$full"/*.bcspec.json "$full"/gearbox.step "$here/"
rm -f "$here"/preview/*.png
mkdir -p "$here/preview"
pdftoppm -r 100 -png "$full/gearbox_kit.pdf" "$here/preview/sheet"
echo "kit: $full; committed files: $here"
