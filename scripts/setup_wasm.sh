#!/usr/bin/env bash
# Автоматическая установка зависимостей для WASM сборки
# Использование: ./scripts/setup_wasm.sh
set -euo pipefail

echo "=== BendCore WASM Setup ==="

# Detect OS
OS=$(uname -s)
DISTRO=""

if [[ "$OS" == "Linux" ]]; then
  if command -v lsb_release &>/dev/null; then
    DISTRO=$(lsb_release -si)
  fi
fi

echo "System: $OS $DISTRO"

# Step 1: Install system packages
echo ""
echo "[1/4] Installing system dependencies..."

if [[ "$OS" == "Linux" ]]; then
  if command -v apt-get &>/dev/null; then
    echo "  Using apt-get..."
    sudo apt-get update -qq
    sudo apt-get install -y -qq \
      build-essential cmake ninja-build git \
      python3 python3-pip \
      libeigen3-dev libx11-dev \
      wget curl pkg-config
  else
    echo "  ERROR: apt-get not found. Please install dependencies manually."
    exit 1
  fi
elif [[ "$OS" == "Darwin" ]]; then
  echo "  Using Homebrew..."
  if ! command -v brew &>/dev/null; then
    echo "  ERROR: Homebrew not found. Please install Homebrew first."
    exit 1
  fi
  brew install cmake ninja eigen
else
  echo "  ERROR: Unsupported OS: $OS"
  exit 1
fi

# Step 2: Install Node.js if needed
echo ""
echo "[2/4] Checking Node.js..."
if ! command -v node &>/dev/null; then
  echo "  Installing Node.js 20..."
  if [[ "$OS" == "Linux" ]] && command -v apt-get &>/dev/null; then
    curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
    sudo apt-get install -y -qq nodejs
  else
    echo "  ERROR: Please install Node.js 20 manually"
    exit 1
  fi
else
  NODE_VER=$(node -v)
  echo "  Node.js $NODE_VER found ✓"
fi

# Step 3: Install Emscripten
echo ""
echo "[3/4] Setting up Emscripten..."

EMSDK_DIR="${HOME}/emsdk"
if [[ -d "$EMSDK_DIR" ]]; then
  echo "  emsdk already exists at $EMSDK_DIR"
else
  echo "  Cloning emsdk..."
  git clone https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR" 2>/dev/null || true
fi

cd "$EMSDK_DIR"
echo "  Installing latest emscripten..."
./emsdk install latest >/dev/null 2>&1
./emsdk activate latest >/dev/null 2>&1

# Step 4: Setup environment
echo ""
echo "[4/4] Setting up environment..."

EMSDK_ENV="source ${EMSDK_DIR}/emsdk_env.sh >/dev/null"
BASHRC="${HOME}/.bashrc"

if ! grep -q "emsdk_env.sh" "$BASHRC"; then
  echo "  Adding emsdk to ~/.bashrc..."
  echo "" >> "$BASHRC"
  echo "# BendCore WASM - Emscripten SDK" >> "$BASHRC"
  echo "$EMSDK_ENV" >> "$BASHRC"
  echo "  Added to ~/.bashrc - reload with: source ~/.bashrc"
else
  echo "  emsdk already in ~/.bashrc ✓"
fi

# Activate for current session
eval "$EMSDK_ENV"

# Verify
echo ""
echo "=== Verification ==="
echo "Node.js: $(node -v)"
echo "npm: $(npm -v)"
echo "cmake: $(cmake --version | head -1)"
echo "ninja: $(ninja --version)"
echo "emcc: $(emcc --version | head -1)"

if command -v eigen3 &>/dev/null || [[ -d /usr/include/eigen3 ]]; then
  echo "Eigen3: found ✓"
else
  echo "Eigen3: NOT found (install with: apt-get install libeigen3-dev)"
fi

echo ""
echo "✓ Setup complete!"
echo ""
echo "Next steps:"
echo "  1. Reload shell: source ~/.bashrc"
echo "  2. Build WASM: ./scripts/build_wasm.sh"
echo "  3. Or: cmake --preset wasm && cmake --build --preset wasm"
