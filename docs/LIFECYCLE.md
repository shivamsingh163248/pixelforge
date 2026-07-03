# PixelForge: Source Code → Running Pixels

**The Complete Software Lifecycle — From Developer's Keyboard to User's Pixels**

This document traces exactly what happens at every stage when building, packaging, and running PixelForge. Every command, every file transformation, every memory operation.

---

## Table of Contents

1. [Stage 1: Developer Writes Code](#stage-1-developer-writes-code)
2. [Stage 2: Configure (CMake)](#stage-2-configure-cmake-reads-the-build-graph)
3. [Stage 3: Build (Ninja)](#stage-3-build-ninja-runs-the-graph)
4. [Stage 4: Test (CTest)](#stage-4-test-ctest)
5. [Stage 5: Package (CPack + Wheel)](#stage-5-package-cpack--scikit-build-core)
6. [Stage 6: Cross-Platform (Docker)](#stage-6-cross-platform-via-docker)
7. [Stage 7: User Installs](#stage-7-user-installs)
8. [Stage 8: User Runs the Program](#stage-8-user-runs-pixelforge)
9. [Stage 9: CI/CD](#stage-9-cicd-github-actions)

---

## Stage 1: Developer Writes Code

### Source File Organization

| Layer | Folder | Language | Purpose |
|-------|--------|----------|---------|
| Public API headers | `include/pixelforge/` | C++ + pure C | What consumers `#include` |
| Core implementation | `src/core/` | C++20 | Image, Kernel, Colorspace (no I/O) |
| Runtime implementation | `src/runtime/` | C++20 | Pipeline, IO, plugin loader |
| CLI | `src/cli/` | C++20 | Command-line frontend |
| Plugins | `plugins/{blur,sharpen,edge,grayscale,invert}/` | C++ (pure-C ABI) | Hot-loadable filters |
| Python bindings | `python/pybind11_module/` | C++ | Glue to CPython |

### Example: Adding a New Filter "threshold"

**Step 1:** Create the plugin source file.

```cpp
// plugins/threshold/threshold.cpp
#include "pixelforge/plugin_api.h"
#include <cstdint>
#include <cstring>

static int threshold_apply(
    const PixelforgeImageView* input,
    PixelforgeImageView* output,
    const char* params_json,
    PixelforgeAllocator allocator,
    void* allocator_ctx
) {
    // Parse threshold value from JSON (default 128)
    int thresh = 128;
    if (params_json && std::strstr(params_json, "\"value\""))
        thresh = std::atoi(std::strstr(params_json, ":") + 1);

    // Allocate output buffer via host callback (avoids cross-CRT issues)
    const size_t size = input->width * input->height * input->channels;
    output->data = static_cast<uint8_t*>(allocator(size, allocator_ctx));
    output->width = input->width;
    output->height = input->height;
    output->channels = input->channels;
    output->stride = input->width * input->channels;

    // Apply threshold: pixel > thresh ? 255 : 0
    for (size_t i = 0; i < size; ++i) {
        output->data[i] = (input->data[i] > thresh) ? 255 : 0;
    }
    return 0;  // success
}

// The ONE entry point the runtime looks for
extern "C" PIXELFORGE_PLUGIN_EXPORT const PixelforgePlugin* pixelforge_plugin_entry() {
    static const PixelforgePlugin plugin = {
        .abi_version = PIXELFORGE_PLUGIN_ABI_VERSION,
        .name        = "threshold",
        .version     = "1.0.0",
        .description = "Binary threshold filter",
        .apply       = threshold_apply,
    };
    return &plugin;
}
```

**Step 2:** Create the CMake file.

```cmake
# plugins/threshold/CMakeLists.txt
pixelforge_add_plugin(threshold threshold.cpp)
```

**Step 3:** Register in parent CMakeLists.

```cmake
# plugins/CMakeLists.txt (add one line)
add_subdirectory(threshold)
```

**That's it.** No edits to runtime, CLI, or Python — they discover plugins at runtime via `LoadLibrary`/`dlopen`.

---

## Stage 2: Configure (CMake Reads the Build Graph)

### Command

```powershell
cmake --preset win-mingw
```

### What Happens Step-by-Step

#### 2.1 Preset Resolution

CMake reads `CMakePresets.json`:

```json
{
  "name": "win-mingw",
  "generator": "Ninja",
  "binaryDir": "${sourceDir}/build/win-mingw",
  "cacheVariables": {
    "CMAKE_C_COMPILER": "gcc",
    "CMAKE_CXX_COMPILER": "g++",
    "CMAKE_BUILD_TYPE": "RelWithDebInfo"
  }
}
```

Result:
- **Generator** = Ninja (not Make, not Visual Studio — Ninja is fastest)
- **Compiler** = MinGW g++
- **Build directory** = `build/win-mingw/`
- **Build type** = RelWithDebInfo (optimized + debug symbols)

#### 2.2 Top-Level CMakeLists.txt Processing

```cmake
cmake_minimum_required(VERSION 3.25)
project(PixelForge VERSION 0.1.0 LANGUAGES CXX)

# C++20, no extensions, position-independent code
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Hide all symbols by default (only PIXELFORGE_API ones exported)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
```

#### 2.3 Version Header Generation

```cmake
configure_file(
    "include/pixelforge/version.h.in"
    "${CMAKE_BINARY_DIR}/generated/pixelforge/version.h"
    @ONLY
)
```

**Input** (`version.h.in`):
```cpp
#define PIXELFORGE_VERSION "@PROJECT_VERSION@"
```

**Output** (`generated/pixelforge/version.h`):
```cpp
#define PIXELFORGE_VERSION "0.1.0"
```

#### 2.4 Target Declaration (Each Subdirectory)

| CMakeLists.txt | Target Name | CMake Command | Output |
|----------------|-------------|---------------|--------|
| `src/core/` | `pixelforge_core` | `add_library(... STATIC)` | `libpixelforge_core.a` |
| `src/runtime/` | `pixelforge` | `add_library(... SHARED)` | `libpixelforge.dll` + `.dll.a` |
| `src/cli/` | `pixelforge` (exe) | `add_executable(...)` | `pixelforge.exe` |
| `plugins/blur/` | `pf_blur` | `add_library(... MODULE)` | `pf_blur.dll` |
| `python/pybind11_module/` | `_pixelforge` | `pybind11_add_module(...)` | `_pixelforge.pyd` |
| `tests/` | `pixelforge_core_tests` | `add_executable(...)` | `pixelforge_core_tests.exe` |

#### 2.5 Output Files

CMake generates:

| File | Purpose |
|------|---------|
| `build/win-mingw/build.ninja` | All compile/link commands + dependency graph |
| `build/win-mingw/compile_commands.json` | Compiler flags per file (for IDE/clangd) |
| `build/win-mingw/CMakeCache.txt` | Cached configuration variables |
| `build/win-mingw/generated/pixelforge/version.h` | Generated header |

---

## Stage 3: Build (Ninja Runs the Graph)

### Command

```powershell
cmake --build --preset win-mingw
```

Ninja reads `build.ninja` and executes the dependency graph in parallel.

### 3a. Compile Phase (`.cpp` → `.o`)

For each source file, Ninja runs:

```bash
g++ -std=c++20 -O2 -g -DNDEBUG \
    -fvisibility=hidden -fPIC \
    -DPIXELFORGE_BUILDING_DLL \
    -I../../include \
    -I../generated \
    -c ../../src/runtime/pipeline.cpp \
    -o CMakeFiles/pixelforge.dir/pipeline.cpp.o
```

**What the compiler does:**

| Phase | Action |
|-------|--------|
| **Preprocess** | Expands `#include`, `#define`. Handles `PIXELFORGE_API` macro → `__declspec(dllexport)` |
| **Parse** | Builds Abstract Syntax Tree (AST). Checks syntax, types. |
| **Optimize** | Dead code elimination, inlining, loop unrolling |
| **Code gen** | Emits machine instructions + symbol table |
| **Output** | Writes `.o` file (COFF format on Windows, ELF on Linux) |

**Parallelism:** Ninja spawns one compile job per CPU core. On a 16-core machine, 16 `.cpp` files compile simultaneously.

### 3b. Archive Phase (`.o` → static lib)

```bash
ar rcs lib/libpixelforge_core.a \
    image.cpp.o kernel.cpp.o colorspace.cpp.o
```

A static library is literally a **tarball of object files**. No linking, no symbol resolution — just concatenation with an index.

### 3c. Link Shared Runtime (`.o` + `.a` → `.dll`)

```bash
g++ -shared \
    -o bin/libpixelforge.dll \
    pipeline.cpp.o io_stb.cpp.o plugin_loader.cpp.o \
    lib/libpixelforge_core.a \
    -static-libgcc -static-libstdc++ \
    -Wl,--out-implib,lib/libpixelforge.dll.a
```

**What the linker does:**

| Step | Action |
|------|--------|
| **Symbol resolution** | Finds definition for every `extern` symbol (e.g., `pixelforge::Image::Image()`) |
| **Export table** | Only symbols marked `PIXELFORGE_API` go in the DLL export table |
| **Import library** | Writes `libpixelforge.dll.a` — a stub for consumers to link against |
| **Static CRT** | Bakes in libstdc++/libgcc so users don't need them on PATH |
| **Relocation** | Patches addresses for position-independent code |

### 3d. Link Plugins (`.o` → MODULE `.dll`)

```bash
g++ -shared -o plugins/pf_blur.dll blur.cpp.o
```

**Key point:** Plugins link to **nothing** from PixelForge. They only `#include <pixelforge/plugin_api.h>` which contains zero library code — just struct definitions and function typedefs.

### 3e. Link CLI Executable

```bash
g++ -o bin/pixelforge.exe main.cpp.o \
    -Llib -lpixelforge
```

The linker records: "At runtime, load `libpixelforge.dll` and resolve these symbols."

### 3f. Link Python Module

```bash
g++ -shared -o python_modules/_pixelforge.pyd \
    bindings.cpp.o \
    -Llib -lpixelforge \
    -LC:/Python313/libs -lpython313
```

### Final Build Layout

```
build/win-mingw/
├── bin/
│   ├── pixelforge.exe              # CLI executable
│   ├── libpixelforge.dll           # Runtime shared library
│   ├── pixelforge_core_tests.exe   # Unit tests
│   └── pixelforge_runtime_tests.exe
├── lib/
│   ├── libpixelforge_core.a        # Static library (for other projects)
│   └── libpixelforge.dll.a         # Import library (for linking)
├── plugins/
│   ├── pf_blur.dll
│   ├── pf_edge.dll
│   ├── pf_grayscale.dll
│   ├── pf_invert.dll
│   └── pf_sharpen.dll
├── python_modules/
│   └── _pixelforge.pyd             # Python extension
└── generated/
    └── pixelforge/
        └── version.h               # Generated header
```

---

## Stage 4: Test (CTest)

### Command

```powershell
ctest --preset win-mingw
```

### What Runs

| Test | Binary | Framework | What It Tests |
|------|--------|-----------|---------------|
| `Core.Image` | `pixelforge_core_tests.exe` | GoogleTest | Image allocation, copy, move |
| `Core.Kernel` | `pixelforge_core_tests.exe` | GoogleTest | Convolution kernel operations |
| `Core.Colorspace` | `pixelforge_core_tests.exe` | GoogleTest | RGB↔grayscale conversion |
| `Runtime.Pipeline` | `pixelforge_runtime_tests.exe` | GoogleTest | Filter chaining |
| `Runtime.PluginLoader` | `pixelforge_runtime_tests.exe` | GoogleTest | dlopen/LoadLibrary |
| `Cli.Smoke` | `pixelforge.exe` | CTest script | End-to-end CLI run |
| `Python.Pybind11.Smoke` | pytest | pytest | Python bindings |

### Example GoogleTest Output

```
[==========] Running 12 tests from 3 test suites.
[----------] 4 tests from ImageTest
[ RUN      ] ImageTest.ConstructEmpty
[       OK ] ImageTest.ConstructEmpty (0 ms)
[ RUN      ] ImageTest.ConstructWithSize
[       OK ] ImageTest.ConstructWithSize (0 ms)
[ RUN      ] ImageTest.MoveSemantics
[       OK ] ImageTest.MoveSemantics (0 ms)
[ RUN      ] ImageTest.PixelAccess
[       OK ] ImageTest.PixelAccess (0 ms)
[----------] 4 tests from ImageTest (1 ms total)
...
[==========] 12 tests from 3 test suites ran. (5 ms total)
[  PASSED  ] 12 tests.
```

---

## Stage 5: Package (CPack + scikit-build-core)

### Native Installers (CPack)

```powershell
cd build/win-mingw
cpack -G "ZIP;NSIS"
```

**What CPack does:**

1. Reads `install(...)` rules from CMakeLists.txt
2. Stages files by component:
   ```
   _CPack_Packages/win64/NSIS/
   ├── Runtime/
   │   └── bin/
   │       ├── pixelforge.exe
   │       └── libpixelforge.dll
   ├── SDK/
   │   ├── include/pixelforge/*.h
   │   └── lib/libpixelforge.dll.a
   └── Plugins/
       └── plugins/pf_*.dll
   ```
3. Runs generator:
   - **ZIP** → Plain archive
   - **NSIS** → Runs `makensis` → Windows installer with "Add to PATH" option
   - **DEB** → Creates `.deb` for Debian/Ubuntu
   - **RPM** → Creates `.rpm` for Fedora/RHEL

**Output:**

```
dist/
├── pixelforge-0.1.0-Windows-AMD64.exe   # NSIS installer
├── pixelforge-0.1.0-Windows-AMD64.zip   # Portable archive
└── pixelforge-0.1.0.tar.gz              # Source tarball
```

### Python Wheel (scikit-build-core)

```powershell
python -m build --wheel
```

**What happens:**

1. pip reads `pyproject.toml`:
   ```toml
   [build-system]
   requires = ["scikit-build-core", "pybind11"]
   build-backend = "scikit_build_core.build"
   ```

2. scikit-build-core spawns CMake:
   ```bash
   cmake -DSKBUILD_PROJECT_NAME=pixelforge ...
   ```

3. Builds only: `_pixelforge.pyd` + `libpixelforge.dll` + plugins

4. Stages into wheel layout:
   ```
   pixelforge/
   ├── __init__.py
   └── _native/
       ├── _pixelforge.pyd
       ├── libpixelforge.dll
       └── plugins/
           └── pf_*.dll
   ```

5. Zips into wheel:
   ```
   pixelforge-0.1.0-cp313-cp313-win_amd64.whl
   ```

---

## Stage 6: Cross-Platform via Docker

### Dockerfiles

```dockerfile
# docker/linux-x64-gcc.Dockerfile
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y \
    build-essential cmake ninja-build python3-dev
WORKDIR /src
```

### Build Command

```bash
# Build for both x64 and ARM64 from an x64 host
docker buildx build \
    --platform linux/amd64,linux/arm64 \
    -f docker/linux-x64-gcc.Dockerfile \
    --target builder \
    .
```

**How ARM64 works on x64:**
- Docker uses **QEMU** user-mode emulation
- The `linux/arm64` image runs ARM instructions translated to x64
- ~10x slower than native, but produces real ARM64 binaries

---

## Stage 7: User Installs

### Option A: Windows Installer (.exe)

User downloads `pixelforge-0.1.0-Windows-AMD64.exe`, double-clicks:

1. NSIS extracts to `C:\Program Files\PixelForge 0.1\`:
   ```
   C:\Program Files\PixelForge 0.1\
   ├── bin\
   │   ├── pixelforge.exe
   │   └── libpixelforge.dll
   ├── plugins\
   │   ├── pf_blur.dll
   │   ├── pf_edge.dll
   │   ├── pf_grayscale.dll
   │   ├── pf_invert.dll
   │   └── pf_sharpen.dll
   ├── include\pixelforge\    # (if SDK selected)
   │   ├── image.h
   │   ├── pipeline.h
   │   └── ...
   └── lib\                    # (if SDK selected)
       ├── libpixelforge.dll.a
       └── cmake\PixelForge\
           └── PixelForgeConfig.cmake
   ```

2. Optionally adds `bin\` to PATH

### Option B: Python Wheel

```powershell
pip install pixelforge-0.1.0-cp313-cp313-win_amd64.whl
```

pip unzips into:
```
C:\Python313\Lib\site-packages\pixelforge\
├── __init__.py
└── _native\
    ├── _pixelforge.pyd
    ├── libpixelforge.dll
    └── plugins\pf_*.dll
```

### Option C: Portable ZIP

Unzip anywhere, run directly:
```powershell
.\pixelforge-0.1.0-Windows-AMD64\bin\pixelforge.exe --version
```

---

## Stage 8: User Runs PixelForge

### Command

```powershell
pixelforge sample.png -f blur -f sharpen -o result.png
```

**Eight things happen in ~200 milliseconds:**

### 8.1 Process Creation (OS Loader)

Windows reads `pixelforge.exe`'s PE header:

```
Import Table:
  libpixelforge.dll
    - pixelforge::load_image
    - pixelforge::Pipeline::run
    - pixelforge::save_image
    - ...
```

1. **DLL search order:** Same directory → System32 → PATH
2. **Maps** `libpixelforge.dll` into process address space
3. **Runs** DLL's `DllMain()` entry point
4. **Patches** import table: `call load_image` → `call 0x7FF812345678`

### 8.2 Argument Parsing

`main()` in `src/cli/main.cpp` parses argv:

```cpp
Args args;
args.input = "sample.png";
args.output = "result.png";
args.filters = {{"blur", ""}, {"sharpen", ""}};
```

### 8.3 Plugin Discovery

```cpp
auto plugins = pixelforge::load_plugins_from_directory("../plugins");
```

**What happens for each `.dll`:**

| Step | Win32 Call | Result |
|------|------------|--------|
| 1 | `LoadLibraryW(L"pf_blur.dll")` | OS maps DLL into memory |
| 2 | `GetProcAddress(handle, "pixelforge_plugin_entry")` | Returns function pointer |
| 3 | `entry()` | Returns `PixelforgePlugin*` descriptor |
| 4 | Check `plugin->abi_version == 1` | Reject incompatible plugins |
| 5 | Store `shared_ptr<Plugin>` | Prevents premature unload |

### 8.4 Image Load

```cpp
auto img = pixelforge::load_image("sample.png");
```

1. Opens file, reads bytes into memory
2. `stbi_load_from_memory()` — vendored stb_image decodes PNG
3. Returns raw `uint8_t*` buffer + dimensions (64×64×3)
4. Wraps in `pixelforge::Image` (RAII owns the buffer)

### 8.5 Pipeline Construction

```cpp
Pipeline pipe;
pipe.add(plugins["blur"]);    // captures shared_ptr
pipe.add(plugins["sharpen"]); // keeps DLL loaded
```

**Critical:** The `shared_ptr<Plugin>` capture keeps the DLL mapped for the pipeline's lifetime. Without this, the DLL would unload and the function pointer would dangle.

### 8.6 Pipeline Execution (The Pixel Work)

```cpp
auto result = pipe.run(img);
```

**For each filter in sequence:**

```cpp
// Build input view (zero-copy reference to image data)
PixelforgeImageView input = {
    .width = 64,
    .height = 64,
    .channels = 3,
    .stride = 192,  // 64 * 3
    .data = img.data()
};

// Allocator callback — host provides memory
auto allocator = [](size_t size, void* ctx) -> void* {
    return static_cast<Pipeline*>(ctx)->allocate(size);
};

// Call plugin's apply function
PixelforgeImageView output;
plugin->apply(&input, &output, "{}", allocator, this);

// Wrap output in new Image for next filter
img = Image(output.data, output.width, output.height, output.channels);
```

**Why the allocator callback?**

Plugins may be compiled with a different CRT (C runtime). If the plugin `malloc()`'d and the host `free()`'d, you'd get heap corruption. The callback ensures the host allocates AND frees.

### 8.7 Image Save

```cpp
pixelforge::save_image(result, "result.png");
```

1. Detects `.png` extension
2. Calls `stbi_write_png()` — vendored stb_image_write
3. Writes file to disk

### 8.8 Cleanup (Destructors Run)

| Order | Destructor | Action |
|-------|------------|--------|
| 1 | `Pipeline::~Pipeline()` | Drops `shared_ptr<Plugin>` references |
| 2 | `Plugin::~Plugin()` (if refcount→0) | `FreeLibrary(handle)` — OS unmaps DLL |
| 3 | `Image::~Image()` | Frees pixel buffer |
| 4 | `main()` returns | Process exits, OS unmaps `libpixelforge.dll` |

---

## Same Flow from Python

```python
import pixelforge as pf

img = pf.load_image("sample.png")
pipe = pf.Pipeline()
pipe.add(pf.filters.blur())
result = pipe.run(img)
pf.save_image(result, "result.png")
```

### Import Sequence

1. Python finds `site-packages/pixelforge/__init__.py`
2. `__init__.py` does `from ._native import _pixelforge`
3. Python's importer calls `LoadLibrary("_pixelforge.pyd")`
4. `_pixelforge.pyd`'s import table requires `libpixelforge.dll`
5. OS loads `libpixelforge.dll` (sitting next to the `.pyd`)
6. pybind11's init function (`PyInit__pixelforge`) registers:
   - `pf.Image` → wraps `pixelforge::Image`
   - `pf.Pipeline` → wraps `pixelforge::Pipeline`
   - `pf.load_image()` → wraps `pixelforge::load_image()`

### Zero-Copy NumPy Interop

```python
import numpy as np
arr = np.asarray(img)  # (64, 64, 3) uint8 view — NO COPY
```

pybind11's `def_buffer()` exports the Image's memory as a Python buffer. NumPy wraps it directly.

**Caveat:** If `img` is garbage-collected while `arr` is alive, `arr` becomes a dangling pointer. Documented behavior.

---

## Stage 9: CI/CD (GitHub Actions)

### ci.yml — Runs on Every Push

```yaml
jobs:
  build:
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
        compiler: [gcc, clang, msvc]
    steps:
      - uses: actions/checkout@v4
      - name: Configure
        run: cmake --preset ${{ matrix.preset }}
      - name: Build
        run: cmake --build --preset ${{ matrix.preset }}
      - name: Test
        run: ctest --preset ${{ matrix.preset }}
```

### release.yml — Runs on Tags

```yaml
on:
  push:
    tags: ['v*']

jobs:
  wheels:
    runs-on: ${{ matrix.os }}
    steps:
      - uses: pypa/cibuildwheel@v2.16
      - uses: actions/upload-artifact@v4

  installers:
    steps:
      - run: cpack -G "ZIP;NSIS"      # Windows
      - run: cpack -G "TGZ;DEB"       # Linux
      - uses: softprops/action-gh-release@v1
        with:
          files: dist/*
```

### nightly.yml — Sanitizer Builds

```yaml
schedule:
  - cron: '0 3 * * *'  # 3 AM UTC daily

jobs:
  sanitizers:
    steps:
      - run: cmake --preset linux-asan
      - run: cmake --build --preset linux-asan
      - run: ctest --preset linux-asan
```

Runs with AddressSanitizer + UndefinedBehaviorSanitizer to catch memory bugs.

---

## TL;DR — The Mental Model

```
┌─────────────────────────────────────────────────────────────────┐
│                     DEVELOPMENT TIME                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  .cpp files ──► CMake ──► Ninja ──► .o ──► .a/.dll/.exe/.pyd   │
│                   │                                             │
│                   └──► build.ninja (dependency graph)           │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│                      PACKAGE TIME                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  binaries ──► CPack ──► .exe installer / .deb / .zip           │
│           ──► scikit-build-core ──► .whl (Python wheel)        │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│                       RUN TIME                                  │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  User runs pixelforge.exe                                       │
│       │                                                         │
│       ├──► OS loads libpixelforge.dll (import table)           │
│       ├──► main() parses args                                   │
│       ├──► LoadLibrary() loads pf_blur.dll (plugins)           │
│       ├──► stb_image decodes PNG                                │
│       ├──► Pipeline calls plugin->apply() per filter            │
│       ├──► stb_image_write encodes output                       │
│       └──► destructors unload DLLs, free memory                 │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Appendix: File Extension Glossary

| Extension | Full Name | Created By | Consumed By |
|-----------|-----------|------------|-------------|
| `.h` | C/C++ header | Developer | Compiler (preprocessor) |
| `.cpp` | C++ source | Developer | Compiler |
| `.o` / `.obj` | Object file | Compiler | Linker |
| `.a` / `.lib` | Static library | `ar` / `lib.exe` | Linker |
| `.dll` / `.so` | Shared library | Linker | OS loader |
| `.dll.a` / `.lib` | Import library | Linker | Linker (for other binaries) |
| `.exe` | Executable | Linker | OS |
| `.pyd` | Python extension | Linker | Python import |
| `.whl` | Python wheel | pip/build | pip install |
| `.deb` | Debian package | CPack | apt/dpkg |
| `.msi` / `.exe` | Windows installer | CPack/WiX/NSIS | Windows |

---

## Further Reading

- [architecture.md](architecture.md) — Why the layers exist
- [glossary.md](glossary.md) — Every file type explained
- [phases/](phases/) — Step-by-step phase documentation
- [guides/](guides/) — How-to guides for specific tasks
