# Phase 3 — Plugin Architecture

| Field | Value |
|---|---|
| Goal | Make filters **hot-loadable** at run time. The runtime knows nothing about specific filter implementations; plugins are dropped into a directory and discovered. |
| Status | ✅ Complete |
| Artifacts | 5 plugin DLLs: `pf_grayscale`, `pf_invert`, `pf_blur`, `pf_sharpen`, `pf_edge` |
| Tests added | 6 — total 28/28 |

---

## 1. The contract: a pure-C ABI

If C++ types crossed the plugin boundary, every plugin would need to be
built with the *exact* same compiler + STL version + flags as the host —
making the whole point of plugins (mix-and-match builds) impossible.

Solution: define the boundary in **plain C**.

[`include/pixelforge/plugin_api.h`](../../include/pixelforge/plugin_api.h)
declares:

```c
#define PIXELFORGE_PLUGIN_ABI_VERSION 1
#define PIXELFORGE_PLUGIN_ENTRY_SYMBOL "pixelforge_plugin_entry"

typedef struct {
    uint32_t  width;
    uint32_t  height;
    uint32_t  channels;
    const uint8_t* pixels;     /* row-major, tightly packed */
} PixelforgeImageView;

typedef struct {
    uint32_t  width;
    uint32_t  height;
    uint32_t  channels;
    uint8_t*  pixels;          /* host-allocated via alloc_fn */
} PixelforgeImageBuffer;

typedef uint8_t* (*PixelforgeAllocFn)(size_t bytes, void* user_data);

typedef int (*PixelforgeApplyFn)(
    const PixelforgeImageView*  in,
    PixelforgeImageBuffer*      out,
    const char*                 params_json,
    PixelforgeAllocFn           alloc,
    void*                       alloc_user_data);

typedef struct {
    uint32_t            abi_version;     /* MUST equal PIXELFORGE_PLUGIN_ABI_VERSION */
    const char*         name;
    const char*         version;
    const char*         description;
    PixelforgeApplyFn   apply;
} PixelforgePlugin;

/* The single entry point every plugin must export. */
PIXELFORGE_PLUGIN_API const PixelforgePlugin* pixelforge_plugin_entry(void);
```

### Why these design choices?

- **POD structs only** — no constructors, no virtual tables.
- **`uint8_t*` pixel data** — the lowest-common-denominator pointer.
  Higher-level pixel formats are negotiated via `channels`.
- **`alloc_fn` callback** instead of having the plugin `malloc` directly.
  Why? Because a plugin built with MSVC's CRT can't `malloc` memory that
  the host (built with MinGW's CRT) will then `free`. The host supplies
  the allocator, the plugin uses it, the host owns and frees the result.
  This is the **single most common cross-CRT crash on Windows.**
- **`params_json`** — extensibility without ABI changes. Plugins parse
  their own params (e.g. `{"radius": 5}` for blur) however they like.
- **`abi_version` first field** — host checks this *before* trusting any
  other field, so a future ABI change breaks gracefully.

---

## 2. The host-side wrapper

### [`include/pixelforge/plugin.h`](../../include/pixelforge/plugin.h)

C++ RAII wrapper around an OS module handle:

```cpp
class PIXELFORGE_API Plugin {
public:
    explicit Plugin(const std::filesystem::path& path);
    ~Plugin();

    Plugin(Plugin&&) noexcept;
    Plugin& operator=(Plugin&&) noexcept;
    Plugin(const Plugin&)            = delete;
    Plugin& operator=(const Plugin&) = delete;

    std::string_view name()        const noexcept;
    std::string_view version()     const noexcept;
    std::string_view description() const noexcept;
    std::filesystem::path path()   const noexcept;

    Image apply(const Image& in, const std::string& params_json = {}) const;
};

PIXELFORGE_API std::vector<Plugin>
load_plugins_from_directory(const std::filesystem::path& dir);
```

### [`src/runtime/plugin_loader.cpp`](../../src/runtime/plugin_loader.cpp)

Uses `LoadLibraryW` on Windows, `dlopen` on POSIX. After load:

1. `GetProcAddress(handle, "pixelforge_plugin_entry")` — find the entry.
2. Call it; receive a `const PixelforgePlugin*`.
3. Validate `abi_version`, all four name fields non-NULL, `apply` non-NULL.
4. If anything fails, throw `PluginError`; destructor unloads.

`load_plugins_from_directory` walks the directory, attempts to load every
`.dll`/`.so`/`.dylib`, and *silently skips* files that aren't valid
plugins (a non-plugin DLL won't have the entry symbol; that's fine).

### Move-only, never copy
Copying would call `dlopen` on the same path twice; the OS reuses the
handle but increments a refcount. When both copies destruct, you'd have
a dangling handle in one and a use-after-free in the other. Move-only
makes the rule unambiguous.

---

## 3. Writing a plugin

A typical plugin (e.g. [`plugins/grayscale/grayscale.cpp`](../../plugins/grayscale/grayscale.cpp))
is **40 lines**:

```cpp
#include <pixelforge/plugin_api.h>
#include <cstring>

extern "C" {

static int apply(const PixelforgeImageView* in,
                 PixelforgeImageBuffer*    out,
                 const char* /*params*/,
                 PixelforgeAllocFn alloc,
                 void* alloc_user) {
    if (!in || !out || in->channels < 3) return -1;
    const size_t n = in->width * in->height;
    out->width    = in->width;
    out->height   = in->height;
    out->channels = 1;
    out->pixels   = alloc(n, alloc_user);
    if (!out->pixels) return -2;
    for (size_t i = 0; i < n; ++i) {
        const uint8_t* p = in->pixels + i * in->channels;
        out->pixels[i]   = static_cast<uint8_t>(0.299 * p[0]
                                              + 0.587 * p[1]
                                              + 0.114 * p[2]);
    }
    return 0;
}

static const PixelforgePlugin kPlugin = {
    PIXELFORGE_PLUGIN_ABI_VERSION,
    "grayscale", "1.0.0",
    "Convert image to single-channel BT.601 luma",
    apply,
};

PIXELFORGE_PLUGIN_API const PixelforgePlugin* pixelforge_plugin_entry(void) {
    return &kPlugin;
}

}  // extern "C"
```

The plugin **does not link** against `libpixelforge.dll`. Its only
include is `<pixelforge/plugin_api.h>` — pure C, header-only.

---

## 4. CMake setup

### [`plugins/CMakeLists.txt`](../../plugins/CMakeLists.txt)

A helper function:

```cmake
function(pixelforge_add_plugin name)
    add_library(pf_${name} MODULE ${ARGN})
    add_library(pixelforge::plugin_${name} ALIAS pf_${name})
    set_target_properties(pf_${name} PROPERTIES
        PREFIX ""
        DEBUG_POSTFIX ""
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins")
    target_include_directories(pf_${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/plugins/common)
    target_compile_features(pf_${name} PRIVATE cxx_std_20)
    target_link_libraries(pf_${name} PRIVATE pixelforge::warnings)
endfunction()
```

Key choices:
- **`MODULE`** not `SHARED` — CMake idiom for `dlopen`-only consumers.
- **`PREFIX ""`** — without this you'd get `libpf_blur.so` on Linux but
  `pf_blur.dll` on Windows. Stripping the prefix gives `pf_blur.so` on
  Linux too, so the discovery code can use the same basenames.
- **No link to runtime** — only the plugin_api header.
- **Output to `<build>/plugins/`** — one directory the loader scans.

Then five one-liners:

```cmake
pixelforge_add_plugin(grayscale grayscale.cpp)
pixelforge_add_plugin(invert    invert.cpp)
pixelforge_add_plugin(blur      blur.cpp)
pixelforge_add_plugin(sharpen   sharpen.cpp)
pixelforge_add_plugin(edge      edge.cpp)
```

---

## 5. The 5 reference plugins

| Plugin | Filter | Notes |
|---|---|---|
| `pf_grayscale` | RGB→Gray (BT.601 luma) | Output channels = 1 |
| `pf_invert` | `out = 255 - in` | Alpha preserved |
| `pf_blur` | 3×3 box blur | Edge-clamp |
| `pf_sharpen` | 3×3 unsharp mask | Center kernel weight = 5 |
| `pf_edge` | 3×3 Laplacian | Output is RGB; classic edge detector |

Each is a single `.cpp`. None of them know about the others or about the
host's pipeline engine.

---

## 6. Tests ([`tests/test_plugin_loader.cpp`](../../tests/test_plugin_loader.cpp))

6 cases:

1. `load_plugins_from_directory` finds all 5 plugins.
2. Each plugin reports a non-empty `name` / `version` / `description`.
3. `Plugin::apply` on a tiny RGB image succeeds for every plugin.
4. Loading a non-existent path throws `PluginError`.
5. Loading the runtime DLL itself (which is not a plugin) throws.
6. Move-construction transfers the handle; old object becomes a no-op.

Build dependency wiring ensures the runtime test sees freshly built plugins:

```cmake
add_dependencies(pixelforge_runtime_tests
    pixelforge::plugin_grayscale
    pixelforge::plugin_invert
    pixelforge::plugin_blur
    pixelforge::plugin_sharpen
    pixelforge::plugin_edge)
```

---

## 7. Lessons learned

### 7.1 `EXPECT_THROW` and the most-vexing-parse
```cpp
EXPECT_THROW(Plugin(path), PluginError);   // BAD — declares a function
```
GCC parses `Plugin(path)` as a function declaration named `path`. Wrap in
a block:
```cpp
EXPECT_THROW({ Plugin p(path); (void)p; }, PluginError);
```

### 7.2 NOMINMAX redefinition
MinGW already `#define`s `NOMINMAX` in its windows.h. Our explicit
`#define NOMINMAX` triggered `-Wmacro-redefined`. Fix: guard with
`#ifndef NOMINMAX`.

### 7.3 Sequential edits to the same CMakeLists.txt
Two `replace_string_in_file` calls with overlapping context corrupted the
file (lines `PYTHON)\nendif()\n\nif(PIXELFORGE_BUILD_\nif(PIXELFORGE_BUILD_TESTS)`).
**Rule for the future**: re-read the file between edits, or do one bigger
combined edit.

---

## 8. Verification

```powershell
cmake --build --preset win-mingw
ctest --preset win-mingw -R "Plugin" --output-on-failure
# 6/6 tests pass; 28/28 total
```

---

## 9. Next: [Phase 4 — CLI executable](phase-04-cli.md)
