# Installation Guide

## System Requirements

- OS: Linux (Ubuntu 20.04+), Windows 10+, macOS 11+
- RAM: 4GB minimum, 8GB recommended
- Disk: 2GB free space
- CPU: x86-64 with SSE2 support

## Linux Installation

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y cmake g++ libocct-dev libocct-devel

git clone https://github.com/Do4ent/bendcore.git
cd bendcore
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install
```

## macOS Installation

```bash
brew install cmake opencascade
git clone https://github.com/Do4ent/bendcore.git
cd bendcore
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.ncpu)
sudo make install
```

## Windows Installation

1. Install Visual Studio 2019+ with C++20 support
2. Install CMake 3.20+
3. Install OpenCASCADE 7.6+
4. Clone the repository
5. Create a build directory and run CMake
6. Build with Visual Studio

## Verification

```bash
bendcore --version  # Should print v6.0.0
bendcore --test     # Run test suite
```

