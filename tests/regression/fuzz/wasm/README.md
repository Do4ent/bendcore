# Искажённые входы, вызывающие ловушку WebAssembly

В native такие входы обрабатывает защита ядра (`src/kernel/occt_fault_guard.*`, OCCT с
проверками в Release): `kernel_malformed_input`, `fuzz_loaders` — 0 падений. В WASM сигналов нет:
разыменование нулевого дескриптора читает линейную память с адреса 0, а выход за её пределы —
ловушка (`WebAssembly.RuntimeError`), после которой состояние модуля не определено.
Интерфейс переводит её в ошибку `ModuleFault`, закрывает воркер и предлагает перезапуск с
восстановлением из черновика (`docs/ui/APP_SHELL.md`, «Сбой модуля»).

| Файл | Как получен | Результат в WASM |
|---|---|---|
| `assembly_brep_trap.bcasm.json` | документ сборки с деталью `tests/assembly-fixtures/block.json` (`save({ embedGeometry: true })`), во встроенном двоичном BRep байт со смещением 4384 заменён 0 → 235; найден перебором однобайтовых и четырёхбайтовых мутаций встроенного BRep (400 мутаций, 3 ловушки, каждая сведена к одному байту) | `loadDocument` → `RuntimeError: memory access out of bounds` |

Сквозной тест: `tests/e2e-app/app_shell.spec.ts`, «a module fault saves a draft and the module
restarts from it».
