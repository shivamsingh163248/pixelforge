# Phase 5 — Python Bindings (pybind11 + nanobind)

| Field | Value |
|---|---|
| Goal | Expose the entire C++ surface to Python: `Image`, `Pipeline`, `Plugin`, `load_image`/`save_image`, plugin discovery — with **NumPy zero-copy interop**. |
| Status | ✅ Complete |
| Artifacts | `_pixelforge.cp313-win_amd64.pyd` (pybind11), `_pixelforge_nb.cp313-win_amd64.pyd` (nanobind) |
| Tests added | 9 (pytest, run via CTest) — total 32/32 |

---

## 1. Two parallel implementations, why?

We build the **same Python surface** twice:
- **pybind11 v2.13.6** — the de-facto standard since ~2017.
- **nanobind v2.4.0** — pybind11's smaller, faster successor (same author).

This is a teaching project, so showing both side-by-side lets you compare
ergonomics, compile time, and binary size with a fair-fight benchmark.

In production you would pick one. We default to pybind11 because the
ecosystem (numpy/torch/etc.) skews that way.

---

## 2. The compiled extension (`.pyd`)

Both modules are **plain DLLs** with one extra discipline: they export a
single symbol named `PyInit_<modulename>`, which CPython calls during
import.

### pybind11 module: [`python/pybind11_module/bindings.cpp`](../../python/pybind11_module/bindings.cpp)

```cpp
PYBIND11_MODULE(_pixelforge, m) {
    m.doc() = "PixelForge Python bindings (pybind11)";
    m.attr("__version__")    = pf::version_string;
    m.attr("version_string") = pf::version_string;

    py::enum_<pf::PixelFormat>(m, "PixelFormat")
        .value("Gray", pf::PixelFormat::Gray)
        .value("RGB",  pf::PixelFormat::RGB)
        .value("RGBA", pf::PixelFormat::RGBA);

    py::class_<pf::Image>(m, "Image", py::buffer_protocol())
        .def_property_readonly("width",  &pf::Image::width)
        .def_property_readonly("height", &pf::Image::height)
        .def_buffer([](pf::Image& img) -> py::buffer_info {
            // Zero-copy NumPy view via memoryview/np.asarray
            return py::buffer_info(img.data(), sizeof(uint8_t),
                py::format_descriptor<uint8_t>::format(),
                /* ndim   */ img.channels() == 1 ? 2 : 3,
                /* shape  */ ...,
                /* stride */ ...);
        });

    py::class_<pf::Plugin, std::shared_ptr<pf::Plugin>>(m, "Plugin")
        .def(py::init<const std::filesystem::path&>())
        ... ;

    py::class_<pf::Pipeline>(m, "Pipeline")
        .def("add_plugin", [](pf::Pipeline& self,
                              std::shared_ptr<pf::Plugin> plugin,
                              std::string params_json) -> pf::Pipeline& {
                 // Capture shared_ptr to keep DLL loaded
                 return self.add(std::string(plugin->name()),
                                 [plugin, params_json](const pf::Image& img) {
                                     return plugin->apply(img, params_json);
                                 });
             });

    py::register_exception<pf::IoError>     (m, "IoError");
    py::register_exception<pf::PluginError> (m, "PluginError");
}
```

### nanobind module: [`python/nanobind_module/bindings.cpp`](../../python/nanobind_module/bindings.cpp)

Same surface, slightly different syntax. Notable differences:
- `nanobind_add_module(... NB_STATIC ...)` to statically link nanobind
  itself for a smaller wheel.
- `using namespace nb::literals;` is **mandatory** in every TU using `_a`
  (pybind11's `_a` is in the `pybind11` namespace by default).
- `nb::ndarray<numpy, uint8_t>` for typed array exchange (cleaner than
  pybind11's `py::array_t<uint8_t>`).

---

## 3. NumPy buffer protocol — zero-copy view

The `def_buffer` lambda lets `np.asarray(image)` produce a NumPy array
that shares memory with the `Image`'s underlying buffer.

```python
import numpy as np
import pixelforge as pf

img  = pf.from_numpy(np.zeros((100, 200, 3), dtype=np.uint8))
view = np.asarray(img)              # zero-copy
view[:] = 255                        # mutates the C++ Image in place
assert pf.as_numpy(img).sum() > 0    # confirms shared memory
```

`from_numpy` *copies* (safer for ownership), `np.asarray()` is a *view*.
The trade-off is documented in the [`__init__.py`](../../python/pixelforge/__init__.py)
docstrings.

---

## 4. The pure-Python facade

[`python/pixelforge/__init__.py`](../../python/pixelforge/__init__.py):

```python
from __future__ import annotations
import os, sys
from pathlib import Path
import numpy as np

# Critical on Windows Py 3.8+: PATH is no longer searched for dependent
# DLLs of an extension module. Add this package's directory to the
# loader search path BEFORE importing the extension.
_PKG_DIR = Path(__file__).resolve().parent
if sys.platform == "win32" and hasattr(os, "add_dll_directory"):
    try:    os.add_dll_directory(str(_PKG_DIR))
    except (OSError, FileNotFoundError): pass

# Wheel layout: pixelforge._pixelforge
# Build tree:   _pixelforge (top-level on sys.path)
try:
    from . import _pixelforge as _ext
except ImportError:
    import _pixelforge as _ext

Image, Pipeline, Plugin, PixelFormat = _ext.Image, _ext.Pipeline, _ext.Plugin, _ext.PixelFormat
load_image, save_image = _ext.load_image, _ext.save_image
load_plugins_from_directory = _ext.load_plugins_from_directory
to_grayscale = _ext.to_grayscale
IoError, PluginError = _ext.IoError, _ext.PluginError

def as_numpy(image): return image.to_numpy()
def from_numpy(arr): return Image.from_numpy(np.ascontiguousarray(arr.astype(np.uint8)))

def default_plugin_dir() -> Path | None:
    here = Path(__file__).resolve().parent
    for cand in (here / "plugins",
                 here.parent / "plugins",
                 Path(sys.prefix) / "share" / "pixelforge" / "plugins"):
        if cand.is_dir(): return cand
    return None

def build_pipeline(filter_names, plugin_dir=None):
    pd = plugin_dir or default_plugin_dir()
    plugins = {p.name: p for p in load_plugins_from_directory(pd)}
    pipe = Pipeline()
    for n in filter_names: pipe.add_plugin(plugins[n])
    return pipe
```

### Why a facade at all?
- **Ergonomics**: `pf.as_numpy(img)` reads better than `img.to_numpy()`.
- **Future flexibility**: we can swap pybind11 for nanobind without
  changing user code.
- **`os.add_dll_directory` lives here** — must run before the .pyd loads.

---

## 5. CMake setup

### [`python/CMakeLists.txt`](../../python/CMakeLists.txt) — interpreter pin

```cmake
# Two Pythons trap on Windows: prefer the one that actually has pip+numpy.
if(WIN32 AND NOT DEFINED CACHE{Python_EXECUTABLE})
    set(_pf_py_candidates
        "$ENV{LOCALAPPDATA}/Microsoft/WindowsApps/PythonSoftwareFoundation.Python.3.13_qbz5n2kfra8p0/python.exe"
        "$ENV{LOCALAPPDATA}/Microsoft/WindowsApps/python.exe")
    foreach(_cand IN LISTS _pf_py_candidates)
        if(EXISTS "${_cand}")
            execute_process(COMMAND "${_cand}" -c
                "import sys, sysconfig; sys.exit(0 if sysconfig.get_paths().get('stdlib') else 1)"
                RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
            if(_rc EQUAL 0)
                set(Python_EXECUTABLE "${_cand}" CACHE FILEPATH "Python interpreter")
                break()
            endif()
        endif()
    endforeach()
endif()

find_package(Python 3.10 REQUIRED COMPONENTS Interpreter Development.Module)
set(PYBIND11_FINDPYTHON ON CACHE BOOL "" FORCE)
```

### Per-module CMakeLists

`python/pybind11_module/CMakeLists.txt`:

```cmake
pybind11_add_module(_pixelforge MODULE bindings.cpp)
target_link_libraries(_pixelforge PRIVATE pixelforge::pixelforge)
target_compile_features(_pixelforge PRIVATE cxx_std_20)
set_target_properties(_pixelforge PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python_modules"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python_modules")

# On Windows the runtime DLL must sit next to the .pyd at import time.
if(WIN32)
    add_custom_command(TARGET _pixelforge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:pixelforge>
            $<TARGET_FILE_DIR:_pixelforge>)
endif()
```

---

## 6. Tests ([`python/tests/test_filters.py`](../../python/tests/test_filters.py))

9 pytest cases:
1. `version_string` is a non-empty `MAJOR.MINOR.PATCH` string.
2. Round-trip a NumPy array through `Image` and back.
3. `to_grayscale` matches the BT.601 formula computed in NumPy.
4. PNG save → load round-trip.
5. `load_plugins_from_directory` finds all 5 reference plugins.
6. `pipe = build_pipeline(["grayscale", "invert"])` produces expected output.
7. Buffer protocol: `np.asarray(img)` is zero-copy.
8. `IoError` raised for missing file.
9. `PluginError` raised for non-plugin DLL.

The CTest case wires it together:

```cmake
if(PIXELFORGE_BUILD_PYTHON AND TARGET _pixelforge)
    find_package(Python 3.10 REQUIRED COMPONENTS Interpreter)
    add_test(NAME Python.Pybind11
        COMMAND ${Python_EXECUTABLE} -m pytest -v
                ${CMAKE_SOURCE_DIR}/python/tests)
    set_tests_properties(Python.Pybind11 PROPERTIES
        ENVIRONMENT
        "PIXELFORGE_PY_PATH=${CMAKE_BINARY_DIR}/python_modules;PIXELFORGE_PKG_ROOT=${CMAKE_SOURCE_DIR}/python;PIXELFORGE_PLUGIN_DIR=${CMAKE_BINARY_DIR}/plugins;PIXELFORGE_TMP_DIR=${CMAKE_BINARY_DIR}/test_tmp")
endif()
```

The harness uses env vars (not hard-coded paths) so the same test
file works against both the build tree *and* an installed wheel.

---

## 7. Lessons learned (the painful ones)

### 7.1 The `shared_ptr` holder bug — heap corruption (0xc0000374)

```cpp
py::class_<pf::Plugin>(m, "Plugin")               // BUG
    .def(py::init<...>());
```

By default pybind11 picks `unique_ptr<T>` as the holder. But our
`load_plugins_from_directory` returns `vector<shared_ptr<Plugin>>`. When
that `shared_ptr` arrives in Python and pybind11 tries to manage it as a
`unique_ptr`, it eventually `delete`s the same object twice. The crash
manifests as `STATUS_HEAP_CORRUPTION` somewhere deep inside pytest, with
no useful stack trace.

**Fix**: always declare the holder when you intend to use shared_ptr:

```cpp
py::class_<pf::Plugin, std::shared_ptr<pf::Plugin>>(m, "Plugin")
```

### 7.2 Two-Pythons trap

`C:\Python313` is a stripped/headless install (no pip, broken
`Lib/`). The Microsoft Store Python at
`%LOCALAPPDATA%/Microsoft/WindowsApps/python3.exe` is the real one.

CMake's `find_package(Python)` happily picks the broken one. Solution:
the probe loop above pins `Python_EXECUTABLE` *before* `find_package`.

### 7.3 nanobind needs `Python::Module`, not `Python3::Module`

The legacy `find_package(Python3 …)` defines `Python3::Module`. nanobind
links against `Python::Module`. If you mix the two, configure fails with
"Target nanobind-static links to Python::Module but the target was not
found". Use modern `find_package(Python …)` (no "3").

### 7.4 nanobind `_a` literal

```cpp
.def("apply", &Plugin::apply, "image"_a, "params"_a = "");
```

This needs `using namespace nb::literals;` in every TU. Otherwise GCC
emits cryptic "no string literal operator `_a`".

### 7.5 CRT mismatch is not the bogeyman it's made out to be

CPython 3.13 is built with MSVC's UCRT. Our `.pyd` is built with MinGW
(also UCRT). They share the same allocator, so heap-allocated objects
can flow across the boundary safely. The crash above (#7.1) was *not* a
CRT mismatch — it was a refcount/ownership mismatch. Modern Windows is
much friendlier than the legacy advice suggests.

---

## 8. Verification

```powershell
cmake --build --preset win-mingw
ctest --preset win-mingw -R Python --output-on-failure
# Python.Pybind11 ........ Passed (9 tests inside)
```

---

## 9. Next: [Phase 6 — Wheel + sdist packaging](phase-06-wheel-packaging.md)
