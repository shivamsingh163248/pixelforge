# Architecture — How the Layers Fit Together

This document is the single source of truth for **why PixelForge is shaped
the way it is.** Every design choice traces back to one of three goals:

1. **Demonstrate every concept** in the modern native software pipeline.
2. **Keep clean ABI boundaries** so each layer can evolve independently.
3. **Stay reproducible** — same `cmake --preset … && cmake --build` works
   on a fresh machine.

If you ever find yourself thinking "why isn't this just one big binary?",
read this doc.

---

## 1. Layer diagram

```
                 ┌────────────────────────────────────────────┐
                 │            User-facing surfaces            │
                 │                                            │
                 │   pixelforge.exe        Python: import     │
                 │   (CLI, Phase 4)        pixelforge as pf   │
                 │                         (Phase 5/6)        │
                 └────────────┬───────────────┬───────────────┘
                              │               │
                              ▼               ▼
                 ┌────────────────────────────────────────────┐
                 │       libpixelforge.dll / .so   (Phase 2)  │
                 │   ── 8 PIXELFORGE_API symbols, hidden rest │
                 │   ── owns: Pipeline, IO, plugin loader     │
                 └────────────┬───────────────┬───────────────┘
                              │               │
        loads via dlopen      │               │  links statically
              ▼               │               ▼
 ┌──────────────────────┐    │     ┌────────────────────────────┐
 │   pf_*.dll plugins   │    │     │  libpixelforge_core.a      │
 │   (Phase 3)          │    │     │  (Phase 1)                 │
 │   pure-C ABI         │    │     │  Image, Colorspace, Kernel │
 │   no link to runtime │    │     │  no external deps          │
 └──────────────────────┘    │     └────────────────────────────┘
                              │
                              ▼
                       <pixelforge/plugin_api.h>
                       (the only header plugins #include)
```

### Why this layering, specifically?

- **`pixelforge_core` is static** because it has zero state, zero global
  resources, no I/O, no allocations beyond what the caller requests. It
  belongs *inside* every consumer (the runtime DLL, eventually a future
  GPU runtime, etc.). Static linking gives the inliner everything it needs.

- **`pixelforge` (the runtime) is shared** because it owns the *single*
  copy of the plugin loader, the I/O subsystem, and the pipeline scheduler.
  Multiple consumers (the CLI and the Python module) need to share that
  single copy or they would each have their own plugin registries.

- **Plugins are MODULE libraries** (CMake distinguishes MODULE from SHARED:
  MODULE is for things loaded only via `dlopen`, never linked at build time).
  They never link against the runtime DLL. Their *only* dependency is
  [`include/pixelforge/plugin_api.h`](../include/pixelforge/plugin_api.h),
  which is **pure C, no C++ types crossing the boundary**.

- **The CLI is its own `.exe`** so end users get a usable command-line
  tool out of the box. It's a thin argv-parser + pipeline driver — no
  business logic lives there.

- **Two Python modules** (pybind11 + nanobind) ship in the dev build but
  only pybind11 ships in the wheel. nanobind is for direct
  side-by-side comparison.

---

## 2. The four ABI surfaces

PixelForge has exactly **four** ABI boundaries, and each one is treated
with different rules:

### ABI #1: `libpixelforge_core.a` ↔ `libpixelforge.dll` (intra-package C++)

- Same compiler, same C++ runtime, same flags.
- Full C++ types are fair game: `std::vector`, `std::string`, exceptions, RTTI.
- No special markers needed; the linker glues object files together.

### ABI #2: `libpixelforge.dll` ↔ CLI / Python `.pyd` (intra-package, dynamic)

- Same compiler, same CRT — enforced because we ship them together.
- Only symbols tagged `PIXELFORGE_API` (defined in
  [`include/pixelforge/export.h`](../include/pixelforge/export.h)) are
  exported. Everything else is hidden via `CXX_VISIBILITY_PRESET hidden`.
- C++ types crossing the boundary are owned consistently — the side that
  allocates also frees.

### ABI #3: `libpixelforge.dll` ↔ plugins (cross-compiler stable C ABI)

- **Pure C only.** Defined in
  [`include/pixelforge/plugin_api.h`](../include/pixelforge/plugin_api.h).
- One entry point: `pixelforge_plugin_entry()` returning a `PixelforgePlugin*`
  descriptor.
- An ABI version constant (`PIXELFORGE_PLUGIN_ABI_VERSION`) — bump it
  when the descriptor struct changes; the loader rejects mismatched
  plugins.
- Image data crosses as `PixelforgeImageView`/`PixelforgeImageBuffer`
  POD structs — width/height/channels/`uint8_t*`. The plugin allocates an
  output buffer with a callback supplied by the host, so cross-CRT
  `malloc/free` mismatches are impossible.

### ABI #4: `_pixelforge.pyd` ↔ CPython (CPython stable ABI consumer)

- Goes through pybind11, which goes through the CPython C API.
- We link against `python313.lib` (the import library that ships with
  CPython), so our `.pyd` is bound to CPython 3.13 specifically.
- Numpy buffer protocol crosses this boundary; we use the `def_buffer`
  hook so `np.asarray(image)` is zero-copy.

---

## 3. Build graph

```
                    cmake --preset win-mingw
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
        ▼                   ▼                   ▼
 src/core/CMakeLists  src/runtime/CMake…  plugins/{*}/CMake…
 ┌───────────────┐    ┌───────────────┐    ┌─────────────────┐
 │ pixelforge_   │◄───┤ pixelforge    │    │ pf_grayscale    │
 │   _core.a     │    │   .dll + .a   │    │ pf_invert       │
 └───────────────┘    └───────┬───────┘    │ pf_blur         │
                              │            │ pf_sharpen      │
              ┌───────────────┼────────┐   │ pf_edge         │
              │               │        │   └─────────────────┘
              ▼               ▼        ▼
       src/cli/CMake…  python/pybind11_  python/nanobind_
       ┌────────────┐  ┌──────────────┐  ┌──────────────────┐
       │ pixelforge │  │ _pixelforge  │  │ _pixelforge_nb   │
       │  .exe      │  │  .pyd        │  │  .pyd            │
       └────────────┘  └──────────────┘  └──────────────────┘
                              │
                              ▼
                    tests/CMakeLists.txt
                    ┌──────────────────────┐
                    │ pixelforge_core_     │
                    │   tests.exe          │
                    │ pixelforge_runtime_  │
                    │   tests.exe          │
                    │ Cli.* tests          │
                    │ Python.Pybind11      │
                    └──────────────────────┘
```

### What can change in parallel?

- Plugin sources: change → only the touched plugin and the runtime tests
  rebuild.
- A core file (e.g. `image.cpp`): change → core lib + runtime DLL + CLI
  + .pyd all rebuild because they all transitively depend on the core lib.
- A header in `include/pixelforge/`: change → likely a full rebuild
  because every translation unit that includes it is invalidated.

This is exactly the trade-off Ninja optimises for — **only rebuild what's
downstream of what changed**.

---

## 4. Object lifetime + ownership rules

### `pixelforge::Image`
- Owns its pixel buffer (a `std::vector<uint8_t>`).
- Move-only on the C++ side (cheap), but **bindings copy** when crossing
  to Python (we do `def("to_numpy", &image_to_numpy)` which `memcpy`s).
- The NumPy buffer-protocol view (`np.asarray(img)`) is **zero-copy** but
  becomes dangling if the underlying `Image` is destroyed while the view
  is in use. Documented; not enforced by code.

### `pixelforge::Plugin`
- Owns the OS handle returned by `LoadLibrary`/`dlopen`.
- Move-only — copying would call `dlopen` on the same path twice and the
  loader would reuse the same handle, then both `Plugin` instances would
  call `dlclose` and the second would crash.
- **Critical:** when a `Plugin` is added to a `Pipeline` via
  `pipe.add_plugin(plugin)`, we capture a `std::shared_ptr<Plugin>` in
  the lambda, so the DLL stays loaded for the lifetime of the pipeline.
  Without this, you'd get a "DLL unloaded mid-call" crash.

### Python `Plugin` holder
- pybind11 must be told `py::class_<pf::Plugin, std::shared_ptr<pf::Plugin>>`
  — otherwise pybind11 picks `unique_ptr` as the holder and our
  `load_plugins_from_directory` (which returns `vector<shared_ptr<Plugin>>`)
  causes silent heap corruption.
- This bug took half a session to find. See Phase 5 doc.

---

## 5. Threading model (current and future)

### Current (Phase 0–7)
- **Single-threaded.** Pipeline runs filters sequentially.
- No thread-safety claims on shared state.
- The CLI and Python module are independently single-threaded.

### Planned (Phase 8/9 stretch goal)
- Per-tile parallelism inside individual filters using `std::for_each(par_unseq, …)`.
- Pipeline stays sequential.
- Plugin API stays single-threaded — it's the host's job to schedule.

---

## 6. Versioning

- One project version: `0.1.0`, defined in
  [`cmake/Version.cmake`](../cmake/Version.cmake).
- Propagated to:
  - `pixelforge::version_string` (C++) via generated `version.h`.
  - `pixelforge.__version__` (Python) via the pybind11 module.
  - DLL `SOVERSION` — the major version forms the `.so.0` symlink on Linux.
  - Wheel filename via `pyproject.toml` `[project] version`.
  - Installer filename via `CPACK_PACKAGE_VERSION`.
- **Single source of truth** — bumping `Version.cmake` changes everywhere.
  Until you do that, there will be a mismatch between `pyproject.toml`
  (hand-coded version) and `Version.cmake`. Today they happen to match
  (`0.1.0`); in CI we will assert that.

---

## 7. Where to go next

- For "what runs when I type `cmake --build`?", read each
  [`phases/phase-0X-*.md`](phases/) in order.
- For "what file extension is that and why does it exist?", read
  [`glossary.md`](glossary.md).
- For "what's missing and what I'd add next?", read
  [`future/improvements.md`](future/improvements.md).
