# Phase 1 — Core Static Library

| Field | Value |
|---|---|
| Goal | Produce `libpixelforge_core.a` — a self-contained C++20 static library with the math primitives that every other PixelForge component will use. |
| Status | ✅ Complete |
| Artifacts | `libpixelforge_core.a`, GoogleTest binary `pixelforge_core_tests.exe` |
| Tests added | 13 |

---

## 1. What got built

### Public headers (in [`include/pixelforge/`](../../include/pixelforge/))

| Header | Declares |
|---|---|
| [`image.h`](../../include/pixelforge/image.h) | `Image` class, `PixelFormat` enum |
| [`colorspace.h`](../../include/pixelforge/colorspace.h) | `to_grayscale(const Image&)` |
| [`kernel.h`](../../include/pixelforge/kernel.h) | `Kernel`, `convolve()` |
| `version.h.in` (template) | `version_string` constant; populated at configure time |

### Source files (in [`src/core/`](../../src/core/))

| File | Defines |
|---|---|
| `image.cpp` | `Image` constructors, accessors, `byte_size()` |
| `colorspace.cpp` | RGB/RGBA → Gray (BT.601 luma) |
| `kernel.cpp` | 3×3 convolution with edge clamp |

### Build target

```cmake
add_library(pixelforge_core STATIC image.cpp colorspace.cpp kernel.cpp)
add_library(pixelforge::core ALIAS pixelforge_core)
```

The `pixelforge::` namespace alias is the one downstream code uses — it
makes target lookups uniform whether PixelForge was found via
`add_subdirectory()` or `find_package()`.

---

## 2. Build flow (what happens when you run `cmake --build`)

```
       image.cpp       colorspace.cpp       kernel.cpp
           │                  │                 │
   g++ -c (compile)    g++ -c             g++ -c
           │                  │                 │
           ▼                  ▼                 ▼
       image.obj      colorspace.obj      kernel.obj
                              │
                       ar rcs (archiver)
                              ▼
                  libpixelforge_core.a   (static archive)
```

A `.a` is just `image.obj + colorspace.obj + kernel.obj` glued together
with a table of contents. **No linking has happened yet.** When the
runtime DLL (Phase 2) links against `pixelforge_core`, the linker pulls
out only the object files whose symbols it actually needs.

You can inspect the archive yourself:

```powershell
ar t build\win-mingw\lib\libpixelforge_core.a
# image.cpp.obj
# colorspace.cpp.obj
# kernel.cpp.obj
```

---

## 3. Key design choices

### Why `STATIC`?
- Zero ABI surface — gets baked entirely into whatever links it.
- Perfect for header-heavy template-light code with cheap operators.
- The `Image` constructor inlines into the runtime DLL across TUs.

### Why no exceptions in core?
- Actually we *do* use `throw std::invalid_argument` for bad arguments.
  The rule is just: don't catch in core; bubble up to the caller.
- Will revisit if we ever support `-fno-exceptions` builds.

### Why `std::vector<uint8_t>` for pixels (not raw `new uint8_t[]`)?
- Move semantics for free.
- Bounds-checked iterators in debug.
- Crystal-clear ownership: the `Image` owns the bytes; nobody else does.

### `include/pixelforge/` vs `src/core/include/`?
- **Public** headers live at the top level (`include/pixelforge/image.h`)
  so consumers do `#include <pixelforge/image.h>`.
- The `src/core/` directory holds only `.cpp` files — no private headers
  (the layer is small enough that none are needed yet).

---

## 4. The CMake target, line by line

```cmake
add_library(pixelforge_core STATIC
    image.cpp
    colorspace.cpp
    kernel.cpp
)
add_library(pixelforge::core ALIAS pixelforge_core)

target_include_directories(pixelforge_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include>
)
```

The `$<BUILD_INTERFACE:…>` / `$<INSTALL_INTERFACE:…>` generator
expressions split the include path between "what to use when this target
is consumed inside this build tree" and "what to use when consumed via
`find_package(PixelForge)`". Without this split, the installed
`PixelForgeConfig.cmake` would try to point at our build directory and
fail on the consumer's machine.

```cmake
target_compile_features(pixelforge_core PUBLIC cxx_std_20)
```

`PUBLIC` because anything that links us also needs C++20.

```cmake
target_link_libraries(pixelforge_core PRIVATE pixelforge::warnings)
```

`pixelforge::warnings` is an `INTERFACE`-only target defined in
[`cmake/CompilerWarnings.cmake`](../../cmake/CompilerWarnings.cmake)
that carries `-Wall -Wextra -Wpedantic -Wshadow -Wconversion …` (or `/W4`
under MSVC). Linking it `PRIVATE` means *we* get the warnings; downstream
consumers don't inherit them.

---

## 5. The 13 tests

`tests/test_image.cpp`, `test_colorspace.cpp`, `test_kernel.cpp`,
`test_version.cpp` — built into one binary `pixelforge_core_tests.exe`
linked against `pixelforge::core` and `GTest::gtest_main`.

GoogleTest is fetched via `FetchContent_Declare` so contributors don't
need a system install:

```cmake
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(googletest)
```

Test discovery uses `gtest_discover_tests(... DISCOVERY_MODE PRE_TEST)`
which runs the test binary at *configure* time to enumerate every TEST
macro and register them individually with CTest. Result: each TEST shows
up as its own row in CTest output, parallelisable, individually runnable
with `ctest -R Image\.SubsetCopy`.

### Sample output
```
Test #1: Image.DefaultIsEmpty .................. Passed
Test #2: Image.ConstructWithDimensions ......... Passed
…
13/13 Test #13: Version.MatchesProjectVersion .. Passed
100% tests passed, 0 tests failed out of 13
```

---

## 6. Lessons learned

### 6.1 MinGW static-runtime linking
Without `-static-libgcc -static-libstdc++ -static`, every test exe needed
`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll` on PATH at
run time. Solution applied at the top-level `CMakeLists.txt` so it covers
exes, shared libs, and modules:

```cmake
if(MINGW)
    set(_pf_mingw_static "-static-libgcc -static-libstdc++ -static")
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS}    ${_pf_mingw_static}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${_pf_mingw_static}")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${_pf_mingw_static}")
endif()
```

**Trap I hit:** I first tried `add_link_options(-static …)` — it was
silently dropped on some targets by CMake 4.3.1 + Ninja. Setting
`CMAKE_*_LINKER_FLAGS` directly works reliably.

### 6.2 Generated header path
`configure_file(include/pixelforge/version.h.in
${CMAKE_BINARY_DIR}/generated/pixelforge/version.h)` puts the generated
header outside the source tree (so `git status` stays clean) but inside
a path layout that mirrors the public include layout
(`generated/pixelforge/version.h` vs `include/pixelforge/image.h`). The
core lib then adds *both* dirs to its `PUBLIC` include path.

---

## 7. Verification commands

```powershell
cmake --preset win-mingw
cmake --build --preset win-mingw --target pixelforge_core_tests
ctest --preset win-mingw -L core --output-on-failure
```

---

## 8. Next: [Phase 2 — Shared runtime DLL](phase-02-runtime-dll.md)
