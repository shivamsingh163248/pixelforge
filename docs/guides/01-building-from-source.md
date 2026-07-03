# Guide 01 — Building PixelForge from source

The repo uses **CMake presets** so you never have to memorise compiler
flags. Pick the preset that matches your toolchain and the same three
commands work everywhere.

---

## 1. Prerequisites

| Tool         | Minimum version | How to check                  |
|--------------|----------------:|-------------------------------|
| CMake        | 3.25            | `cmake --version`             |
| Ninja        | 1.10            | `ninja --version`             |
| Python       | 3.10            | `python --version`            |
| A C++20 compiler | GCC 11 / Clang 14 / MSVC 19.30 / MinGW-w64 GCC 12 | `g++ --version` |

Optional but recommended:

| Tool   | What it's for                          |
|--------|----------------------------------------|
| `pytest` + `numpy`            | Python integration tests          |
| `pybind11`, `nanobind` (auto) | Pulled by FetchContent at configure time, no manual install |
| NSIS                          | Building the Windows installer (Phase 7) |
| Docker Desktop + buildx       | Cross-platform / cross-arch builds (Phase 8) |

The repo ships [`scripts/check_env.ps1`](../../scripts/check_env.ps1) and
[`scripts/check_env.sh`](../../scripts/check_env.sh) which audit your
toolchain in one shot:

```powershell
# Windows
pwsh scripts\check_env.ps1
```

```bash
# Linux / macOS
bash scripts/check_env.sh
```

---

## 2. The 3-command build (any platform, any preset)

```text
cmake --preset <name>           # configure (run once per build dir)
cmake --build --preset <name>   # compile everything
ctest --preset <name>           # run all 32 tests
```

That's it. Every preset listed in
[`CMakePresets.json`](../../CMakePresets.json) follows the same pattern.

### Available presets

| Preset            | OS      | Compiler          | Notes |
|-------------------|---------|-------------------|-------|
| `win-mingw`       | Windows | MinGW-w64 GCC     | The default if you installed via [winlibs](https://winlibs.com) |
| `win-msvc`        | Windows | MSVC `cl.exe`     | Requires "Developer PowerShell for VS 2022" or `msvc-dev-cmd` |
| `win-clang-cl`    | Windows | LLVM `clang-cl`   | Requires LLVM-for-Windows installed |
| `linux-gcc`       | Linux   | system `gcc`/`g++`| Default on Ubuntu / Debian / Fedora |
| `linux-clang`     | Linux   | system `clang`    | Requires `apt install clang` or equivalent |
| `linux-gcc-arm64` | Linux   | `gcc` on aarch64  | Use only inside an arm64 container or on real ARM hardware |

`cmake --list-presets` only shows the ones valid for your current OS.

---

## 3. Worked examples

### 3.1 Windows / MinGW-w64 GCC (the canonical local build)

```powershell
cd C:\Users\you\pixelforge
cmake --preset win-mingw
cmake --build --preset win-mingw
ctest --preset win-mingw --output-on-failure
```

Expected last line:

```
100% tests passed, 0 tests failed out of 32
```

Build directory: `build\win-mingw\` (~140 MB).

### 3.2 Windows / MSVC

Open **"Developer PowerShell for VS 2022"** (so `cl.exe` is on `PATH`),
then:

```powershell
cmake --preset win-msvc
cmake --build --preset win-msvc
ctest --preset win-msvc --output-on-failure
```

### 3.3 Linux / GCC

```bash
sudo apt-get install -y build-essential cmake ninja-build python3-dev python3-pytest python3-numpy
cmake --preset linux-gcc
cmake --build --preset linux-gcc
ctest --preset linux-gcc --output-on-failure
```

### 3.4 Linux / Clang

```bash
sudo apt-get install -y clang libclang-rt-dev cmake ninja-build python3-dev python3-pytest python3-numpy
cmake --preset linux-clang
cmake --build --preset linux-clang
ctest --preset linux-clang --output-on-failure
```

---

## 4. Common configuration knobs

All optional, all `OFF` or empty by default unless noted.

| Cache var (`-D…=…`)           | Default | Effect |
|--------------------------------|---------|--------|
| `PIXELFORGE_BUILD_TESTS`       | `ON`    | Build the GoogleTest + pytest harness |
| `PIXELFORGE_BUILD_PYTHON`      | `ON`    | Build the `_pixelforge.pyd` / `.so` Python module |
| `PIXELFORGE_BUILD_CLI`         | `ON`    | Build the `pixelforge` CLI executable |
| `PIXELFORGE_BUILD_NANOBIND`    | `ON`    | Build the parallel nanobind module |
| `PIXELFORGE_SANITIZERS`        | (empty) | Comma list: `address,undefined,thread,leak`. GCC/Clang only |
| `CMAKE_BUILD_TYPE`             | `RelWithDebInfo` | `Debug`, `Release`, `MinSizeRel` also valid |
| `CMAKE_INSTALL_PREFIX`         | OS default | Where `cmake --install` puts files |

Example: a small, fast wheel-style build with no Python and no tests:

```powershell
cmake --preset win-mingw -DPIXELFORGE_BUILD_TESTS=OFF -DPIXELFORGE_BUILD_PYTHON=OFF
```

Example: a sanitizer-instrumented debug build:

```bash
cmake --preset linux-clang -DCMAKE_BUILD_TYPE=Debug -DPIXELFORGE_SANITIZERS=address,undefined
```

---

## 5. Building only one target

After configuring, you can build **just one** thing:

```powershell
cmake --build build\win-mingw --target pixelforge          # the runtime DLL only
cmake --build build\win-mingw --target pixelforge_cli      # the CLI exe only
cmake --build build\win-mingw --target pf_blur             # one plugin only
cmake --build build\win-mingw --target _pixelforge         # the pybind11 module
cmake --build build\win-mingw --target pixelforge_runtime_tests   # one test exe
```

Useful when iterating on a single file — finishes in seconds instead of minutes.

---

## 6. Cleaning

```powershell
# clean intermediate object files but keep the configuration:
cmake --build build\win-mingw --target clean

# nuke everything and start over:
Remove-Item -Recurse -Force build\win-mingw
```

```bash
# Linux equivalents:
cmake --build build/linux-gcc --target clean
rm -rf build/linux-gcc
```

---

## 7. After a successful build, what do I have?

See [Guide 02 — Build artifacts](02-build-artifacts.md) for the full
catalogue of every file the build produces and where it lives.

For running what you just built, jump to
[Guide 03 — Running PixelForge](03-running-pixelforge.md).
