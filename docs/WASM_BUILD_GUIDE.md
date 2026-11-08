# BendCore WebAssembly Build Guide

## Overview

BendCore C++ core is compiled to WebAssembly (WASM) using **Emscripten**.

- **Build tool:** CMake + Ninja + Emscripten
- **Output:** `build-wasm/web/bendcore.{js,wasm,mjs}`
- **CI:** Automated via `.github/workflows/wasm.yml`
- **Local:** Manual setup required (see below)

## Prerequisites

### System Requirements
- Ubuntu 20.04+ or similar Linux
- 4+ GB RAM
- 20+ GB free disk space (for emsdk + OCCT)

### Required Software

```bash
# 1. Install system dependencies
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build git \
  python3 python3-pip \
  libeigen3-dev libx11-dev \
  wget curl

# 2. Install Node.js 18+ (for WASM tests)
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt-get install -y nodejs

# 3. Install Emscripten (emsdk)
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
cd ~/emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh  # Add to ~/.bashrc for permanent setup
```

## Building WASM Locally

### Quick Build

```bash
cd /home/user/bendcore

# Activate emsdk
source ~/emsdk/emsdk_env.sh

# Build
./scripts/build_wasm.sh

# Output location
ls -lh build-wasm/web/
```

## Eigen (обязателен, только настоящий)

Eigen 3.3+ (MPL-2.0, только заголовки) нужен `sketch.cpp`, PlaneGCS, решателям CFD и
теплопроводности. Заглушки Eigen в репозитории больше нет (прежние `third_party/eigen-5.0.0`,
`third_party/eigen-stub`, `third_party_stubs/include/Eigen` — «Minimal Eigen Mock» — удалены,
см. `docs/STUB_REMOVAL.md`, раздел 6). `CMakeLists.txt` ищет Eigen так:

1. `BENDCORE_EIGEN_DIR` — переменная CMake или окружения (каталог, внутри которого `Eigen/`);
   если указан каталог без `Eigen/Dense` и заголовка версии или с версией < 3.3 — ошибка;
2. пакет `Eigen3` (`find_package`); при кросс-компиляции Emscripten системный пакет не
   виден (`CMAKE_FIND_ROOT_PATH`), поэтому для WASM работает п. 3;
3. `/usr/include/eigen3`, `/usr/local/include/eigen3`, `/opt/homebrew/include/eigen3` —
   заголовки не зависят от целевой платформы и подходят для WASM;
4. `-DBENDCORE_FETCH_EIGEN=ON` — загрузка Eigen 3.4.0 с gitlab.com по тегу с проверкой
   SHA-256 (`8586084f…1c72`), только заголовки (CMake-проект Eigen не подключается).

Если Eigen не найден — конфигурация останавливается с ошибкой и подсказкой (тихого перехода
на заглушку нет). В журнале конфигурации: `-- Eigen 3.4.0: headers in /usr/include/eigen3`.

### Результаты тестов WASM (2026-11-08, emsdk 6.0.12, `cmake --preset wasm` без параметров)

| Состояние | Eigen | Сборка | ctest |
|---|---|---|---|
| До изменений, preset без `BENDCORE_EIGEN_DIR` | заглушка `third_party/eigen-5.0.0` | 8 объектов не компилируются (`sketch.cpp`, `cfd_solver.cpp`, `thermal_solver.cpp`, `sketch2d.cpp`, PlaneGCS ×3, `test_planegcs.cpp`) | 2 из 60 (`formula_comprehensive`, `phase_i_main` — не зависят от `bendcore_core`; остальные 58 не собраны) |
| До изменений, `BENDCORE_EIGEN_DIR=/usr/include/eigen3` (как `scripts/build_wasm.sh` и CI) | 3.4.0 | OK | 60/60 |
| После | 3.4.0 (найден автоматически) | OK | 59/59 (удалён `dxf_reader_comprehensive` — тест без проверок) |

Прежнее заявление «59 WASM-тестов проходят» (`BUILD_STATUS_2026-11-06.md`) не могло
относиться к заглушке: с ней не собирается даже библиотека ядра. Оно соответствует сборке
через `scripts/build_wasm.sh`, который передаёт настоящий Eigen.

### Advanced: Step-by-Step

```bash
cd /home/user/bendcore

# 1. Activate emsdk
source ~/emsdk/emsdk_env.sh

# 2. Configure CMake
cmake --preset wasm   # Eigen ищется автоматически; иначе -DBENDCORE_EIGEN_DIR=<каталог>

# 3. Build
cmake --build --preset wasm

# 4. Run tests
ctest --preset wasm --output-on-failure

# 5. Verify output
file build-wasm/web/bendcore.{js,wasm,mjs}
```

## GitHub Actions CI

### Workflow: `.github/workflows/wasm.yml`

Runs on:
- Every push to `main`
- Every PR
- Manual trigger (`workflow_dispatch`)

### Jobs

1. **wasm**: Build BendCore as WebAssembly
   - Install emsdk
   - Run `./scripts/build_wasm.sh`
   - Run Playwright tests on generated page
   - Upload artifacts

2. **occt**: Build OpenCASCADE (separate OCCT tests)
   - Cached separately
   - Runs independently

### Debugging Failures

If WASM workflow fails on GitHub:

```bash
# 1. Check logs in GitHub Actions UI
# Look for: CMake errors, link errors, undefined references

# 2. Reproduce locally
source ~/emsdk/emsdk_env.sh
./scripts/build_wasm.sh

# 3. Check CMakeOutput.log
cat build-wasm/CMakeFiles/CMakeOutput.log | grep -i error

# 4. Common issues:
# - EMSDK not activated: source ~/emsdk/emsdk_env.sh
# - Eigen not found: apt install libeigen3-dev, или BENDCORE_EIGEN_DIR=<dir>, или -DBENDCORE_FETCH_EIGEN=ON
# - Ninja not installed: sudo apt-get install ninja-build
# - Old cmake: sudo apt-get install cmake
```

## Output Files

### Generated Files

After successful build:

```
build-wasm/web/
├── bendcore.js      # JavaScript glue code
├── bendcore.wasm    # WebAssembly binary (~3-5 MB)
├── bendcore.mjs     # ES module wrapper
└── index.html       # Test/demo page
```

### Usage in Browser

```html
<script async type="module">
  import init from './bendcore.mjs';
  const bendcore = await init();
  // Use bendcore API
</script>
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `EMSDK not found` | Run `source ~/emsdk/emsdk_env.sh` |
| `emcc: command not found` | Activate emsdk, check PATH |
| `Eigen >= 3.3 not found` | `apt install libeigen3-dev`, или `BENDCORE_EIGEN_DIR=<каталог с Eigen/>`, или `-DBENDCORE_FETCH_EIGEN=ON` |
| `cmake: version too old` | Update: `sudo apt-get install cmake` |
| `Out of memory` | Increase swap, use Release build |
| `Link errors` | Check CMakeFiles/CMakeError.log |

## CI Status

- **JavaScript/TypeScript:** `.github/workflows/build-test.yml` ✅ (Always tested)
- **WebAssembly:** `.github/workflows/wasm.yml` ✅ (Tested on push to main)
- **OCCT:** Part of wasm.yml ✅ (Cached separately)

## Related

- `./scripts/build_wasm.sh` - Build script
- `CMakePresets.json` - CMake configuration
- `.github/workflows/wasm.yml` - CI workflow
- `docs/ARCHITECTURE.md` - System architecture
