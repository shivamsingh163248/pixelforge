# Phase 2 — Shared Runtime DLL

| Field | Value |
|---|---|
| Goal | Wrap the static core in a **shared library** with controlled symbol visibility, plus image I/O and the pipeline engine. This is the artifact every consumer (CLI, Python, plugins indirectly) will load at run time. |
| Status | ✅ Complete |
| Artifacts | `libpixelforge.dll` (1.4 MB), `libpixelforge.dll.a` (import lib) |
| Tests added | 9 (3 pipeline + 6 I/O) — total 22/22 |

---

## 1. What got built

### New public headers
- [`include/pixelforge/export.h`](../../include/pixelforge/export.h) — the
  `PIXELFORGE_API` macro that tags exported symbols.
- [`include/pixelforge/io.h`](../../include/pixelforge/io.h) — `load_image`,
  `save_image`, `IoError`.
- [`include/pixelforge/pipeline.h`](../../include/pixelforge/pipeline.h) —
  `Pipeline`, `Filter` (= `std::function<Image(const Image&)>`).

### New sources (in [`src/runtime/`](../../src/runtime/))
- `pipeline.cpp` — `Pipeline::add()`, `Pipeline::run()`.
- `io_stb.cpp` — wraps the vendored `stb_image` / `stb_image_write`.

### Vendored third-party
- [`third_party/stb/stb_image.h`](../../third_party/stb/) +
  `stb_image_write.h` — single-header public-domain image codecs. Pinned
  to commit `f0569113`.

### Build target

```cmake
add_library(pixelforge SHARED pipeline.cpp io_stb.cpp)
add_library(pixelforge::pixelforge ALIAS pixelforge)
target_link_libraries(pixelforge
    PUBLIC  pixelforge::core
    PRIVATE pixelforge::warnings)
```

---

## 2. The export macro pattern

[`export.h`](../../include/pixelforge/export.h):

```cpp
#if defined(_WIN32)
  #if defined(PIXELFORGE_BUILDING_DLL)
    #define PIXELFORGE_API __declspec(dllexport)
  #else
    #define PIXELFORGE_API __declspec(dllimport)
  #endif
#else
  #define PIXELFORGE_API __attribute__((visibility("default")))
#endif
```

The build system defines `PIXELFORGE_BUILDING_DLL` *only* when compiling
the runtime sources, via:

```cmake
target_compile_definitions(pixelforge PRIVATE PIXELFORGE_BUILDING_DLL)
```

So the same header serves three roles:
1. **Compiling the DLL itself** → `dllexport` (this is mine, expose it).
2. **Compiling the CLI / .pyd that uses the DLL** → `dllimport` (this
   lives in another module, set up an import thunk).
3. **On Linux** → `visibility("default")` regardless (just opts the
   symbol back into the export table after we hid everything globally).

### Why hide everything by default?

In the top-level `CMakeLists.txt`:

```cmake
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_CXX_VISIBILITY_PRESET     hidden)
```

This makes GCC/Clang behave like MSVC — nothing is exported unless you
ask. Then `PIXELFORGE_API` *opts in* the eight symbols that form our
public ABI:

- `Image` constructors and methods
- `Pipeline::add`, `Pipeline::run`
- `load_image`, `save_image`
- `to_grayscale`
- `version_string`
- The two exception types (`IoError`, `PluginError` — added in Phase 3)

You can verify with `objdump`:

```powershell
objdump -p build\win-mingw\bin\libpixelforge.dll | grep "Export"
# (...)
# [Ordinal/Name Pointer] Table
#   8 entries
```

If a STL helper or stb function leaked here, that count would be in the
hundreds and our DLL would be a maintenance nightmare across compiler
upgrades.

---

## 3. Why a shared library, not just bigger static?

| | Static | Shared |
|---|---|---|
| Plugin loader needs **one** copy of process-wide state | ❌ each consumer gets its own | ✅ all consumers share |
| CLI exe + `.pyd` ship in the same install | wasteful (duplicate code) | one DLL, both link to it |
| Cross-language interop (Python) | impossible | works |
| ABI hygiene matters | irrelevant | mandatory |
| Easier to ship hotfixes | full reinstall | replace one .dll |

For PixelForge, the runtime layer **must** be shared. The core layer
beneath it can stay static because it has no global state.

---

## 4. The Pipeline + IO additions

### `pixelforge::Pipeline`
A vector of `(name, std::function<Image(const Image&)>)` pairs. `run()`
folds them: `out = filter_n(... filter_1(filter_0(input)))`. Empty
pipeline returns the input unchanged. Order matters; Phase 3 adds
plugin-as-filter support.

### `pixelforge::load_image` / `save_image`
Wraps `stbi_load_from_filename` / `stbi_write_png`. Format is auto-detected
from extension. `force_format` parameter lets you request a channel count.
Throws `pf::IoError` on any failure.

#### Why vendor stb instead of using libpng + libjpeg?
- Zero external dependencies = first-build cleanness.
- Bit-perfect across platforms (bug-for-bug compatible).
- We can later add libjpeg-turbo as an optional backend — the
  `pixelforge::IO` API stays the same.

---

## 5. Quirks of stb in a strict-warnings build

stb is C90 single-header code. It triggers many warnings under our
flags. **Surgical fix** — silence just for the one `.cpp` that
instantiates the implementations:

```cmake
if(MSVC)
    set_source_files_properties(io_stb.cpp PROPERTIES COMPILE_OPTIONS "/wd4244;/wd4267;/wd4456")
else()
    set_source_files_properties(io_stb.cpp PROPERTIES COMPILE_OPTIONS
        "-Wno-conversion;-Wno-sign-conversion;-Wno-old-style-cast;-Wno-shadow;-Wno-cast-align;-Wno-double-promotion;-Wno-missing-field-initializers")
endif()
```

The rest of our code stays under `-Wall -Wextra -Wpedantic`.

---

## 6. SOVERSION + symlinks

```cmake
set_target_properties(pixelforge PROPERTIES
    VERSION   ${PROJECT_VERSION}        # 0.1.0
    SOVERSION ${PROJECT_VERSION_MAJOR}  # 0
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)
```

On Linux this gives you `libpixelforge.so.0.1.0` plus symlinks
`libpixelforge.so.0` and `libpixelforge.so`. SOVERSION is the binary-
compatible part of the version: bump it whenever you make an ABI-breaking
change. Consumers linked against `libpixelforge.so.0` won't suddenly
break when `0.1.1` is installed; they would break if `1.0.0` arrives.

`WINDOWS_EXPORT_ALL_SYMBOLS OFF` makes us reject the cheap "auto-export
everything" mode and forces the explicit `PIXELFORGE_API` discipline.

---

## 7. Tests

### `tests/test_pipeline.cpp` — 3 tests
- Empty pipeline returns input unchanged.
- `add()` runs filters in insertion order.
- A core filter (grayscale) composes with a custom inline filter.

### `tests/test_io.cpp` — 6 tests
- PNG round-trip preserves RGB pixels exactly.
- PNG round-trip preserves RGBA pixels exactly.
- BMP round-trip works.
- `force_format=Gray` collapses RGB.
- Loading a missing file throws `IoError`.
- Saving with an unknown extension throws.

The runtime test exe needs `libpixelforge.dll` next to it; CMake
post-build copies the DLL into the bin directory:

```cmake
add_custom_command(TARGET pixelforge_runtime_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:pixelforge>
        $<TARGET_FILE_DIR:pixelforge_runtime_tests>)
```

---

## 8. Lessons learned

### 8.1 `add_link_options` silently dropped (again)
Same pattern as Phase 1 — re-confirmed. Use `set(CMAKE_*_LINKER_FLAGS …)`.

### 8.2 RAII for stb output buffers
`stbi_image_free()` not `delete[]` — stb mallocs via its own allocator.
Wrapped in a `std::unique_ptr<unsigned char, decltype(&stbi_image_free)>`.

### 8.3 The DLL count is a discipline check
After Phase 2, run:

```powershell
objdump -p build\win-mingw\bin\libpixelforge.dll | Select-String "\[\s*\d+\]"
```

You should see exactly the count of `PIXELFORGE_API`-tagged symbols. If
that number creeps up, somebody forgot a `static` or a `namespace { }`
on a helper. This is the single most useful "code quality alarm" the
build can give you.

---

## 9. Verification

```powershell
cmake --build --preset win-mingw --target pixelforge_runtime_tests
ctest --preset win-mingw -R "Pipeline\.|Io\." --output-on-failure
# 9/9 tests pass
```

---

## 10. Next: [Phase 3 — Plugin architecture](phase-03-plugins.md)
