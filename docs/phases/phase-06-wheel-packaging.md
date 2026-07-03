# Phase 6 — Wheel + sdist Packaging

| Field | Value |
|---|---|
| Goal | Turn the Python bindings into a `pip install`-able wheel that bundles every native dependency (the runtime DLL + plugin DLLs) — no env vars, no system installs needed by the user. |
| Status | ✅ Complete |
| Artifacts | `pixelforge-0.1.0-cp313-cp313-win_amd64.whl` (1.2 MB), `pixelforge-0.1.0.tar.gz` (184 KB) |
| Tests added | reuses Phase 5 pytest suite, run against the **installed wheel** |

---

## 1. The wheel layout

```
pixelforge-0.1.0-cp313-cp313-win_amd64.whl
├── pixelforge/
│   ├── __init__.py                              ← pure-Python facade
│   ├── _pixelforge.cp313-win_amd64.pyd          ← compiled extension
│   ├── libpixelforge.dll                        ← bundled runtime
│   └── plugins/
│       ├── pf_blur.dll
│       ├── pf_edge.dll
│       ├── pf_grayscale.dll
│       ├── pf_invert.dll
│       └── pf_sharpen.dll
└── pixelforge-0.1.0.dist-info/
    ├── METADATA          ← name, version, deps from pyproject.toml
    ├── WHEEL             ← wheel format version, generator, tags
    ├── RECORD            ← SHA-256 of every file
    └── licenses/LICENSE
```

Total: **13 files, 1.2 MB**. After `pip install`, everything lives at
`<venv>/Lib/site-packages/pixelforge/`. `import pixelforge` works
immediately — no PATH changes, no env vars, no missing-DLL pop-up.

---

## 2. The wheel filename, decoded

`pixelforge-0.1.0-cp313-cp313-win_amd64.whl`:

| Component | Value | Meaning |
|---|---|---|
| Distribution | `pixelforge` | Project name from `pyproject.toml` |
| Version | `0.1.0` | Project version |
| Python tag | `cp313` | CPython 3.13 only (no PyPy / Jython) |
| ABI tag | `cp313` | Bound to CPython 3.13's stable ABI |
| Platform tag | `win_amd64` | Windows x86-64 only |

The platform tag is the critical one: it tells `pip` "do not even consider
installing this on Linux or ARM." If we had built a "stable ABI" wheel
(`cp312-abi3-win_amd64`), it would install on 3.12, 3.13, 3.14, ... but
we'd lose access to a few APIs.

---

## 3. The toolchain: scikit-build-core

[`pyproject.toml`](../../pyproject.toml) in three sections:

### `[build-system]` — what builds the wheel
```toml
[build-system]
requires      = ["scikit-build-core>=0.10", "pybind11>=2.13"]
build-backend = "scikit_build_core.build"
```

`pip install` reads this *first* in an isolated venv. It pip-installs
exactly these requirements, then calls `scikit_build_core.build.build_wheel()`.

### `[project]` — PEP 621 metadata
Name, version, description, author, license, classifiers, dependencies.
Most of this propagates into the wheel's `dist-info/METADATA`.

### `[tool.scikit-build]` — how to drive CMake
```toml
[tool.scikit-build]
cmake.version    = ">=3.25"
ninja.version    = ">=1.10"
build-dir        = "build/wheel/{wheel_tag}"
wheel.packages   = ["python/pixelforge"]
wheel.install-dir = "."
wheel.exclude    = ["**/*.pdb", "**/*.lib", "**/*.exp"]
install.strip    = true

[tool.scikit-build.cmake.define]
PIXELFORGE_BUILD_TESTS    = "OFF"
PIXELFORGE_BUILD_PYTHON   = "ON"
PIXELFORGE_BUILD_NANOBIND = "OFF"
PIXELFORGE_BUILD_CLI      = "OFF"
```

The CMake defines turn off everything we don't want in the wheel:
- No tests → no GoogleTest fetch.
- No nanobind → faster build, smaller wheel.
- No CLI → end users use `pixelforge` from CLI install instead.

---

## 4. The bundling pattern

[`python/pybind11_module/CMakeLists.txt`](../../python/pybind11_module/CMakeLists.txt)
has a wheel-only block, gated on `SKBUILD_PROJECT_NAME` (which scikit-
build-core sets):

```cmake
if(DEFINED SKBUILD_PROJECT_NAME)
    install(TARGETS _pixelforge
        LIBRARY DESTINATION pixelforge
        RUNTIME DESTINATION pixelforge)
    install(TARGETS pixelforge       # the runtime DLL
        LIBRARY DESTINATION pixelforge
        RUNTIME DESTINATION pixelforge)
    foreach(_plugin IN ITEMS grayscale invert blur sharpen edge)
        if(TARGET pf_${_plugin})
            install(TARGETS pf_${_plugin}
                LIBRARY DESTINATION pixelforge/plugins
                RUNTIME DESTINATION pixelforge/plugins)
        endif()
    endforeach()
endif()
```

**Crucial detail**: `wheel.install-dir = "."` in `pyproject.toml` plus
`DESTINATION pixelforge` in CMake puts files at exactly
`site-packages/pixelforge/...`. If you set both to nonzero values, you get
nested directories (`site-packages/pixelforge/pixelforge/...`).

---

## 5. The Windows DLL search trick

Windows since Python 3.8 no longer searches `%PATH%` for DLLs that a
`.pyd` depends on. Without intervention, importing the bundled `.pyd`
would fail with "ImportError: DLL load failed while importing
_pixelforge: The specified module could not be found." even though
`libpixelforge.dll` sits right next to it.

The fix is in [`python/pixelforge/__init__.py`](../../python/pixelforge/__init__.py),
**before** the extension import:

```python
_PKG_DIR = Path(__file__).resolve().parent
if sys.platform == "win32" and hasattr(os, "add_dll_directory"):
    try:    os.add_dll_directory(str(_PKG_DIR))
    except (OSError, FileNotFoundError): pass

from . import _pixelforge as _ext
```

Now Windows checks `_PKG_DIR` for dependent DLLs and finds
`libpixelforge.dll`.

---

## 6. End-to-end build

```powershell
python -m pip install --upgrade build scikit-build-core
python -m build --wheel --outdir dist
# Dispatches:
#   1. Creates an isolated venv
#   2. pip-installs scikit-build-core + pybind11 into it
#   3. Calls scikit_build_core.build.build_wheel()
#      a. Creates build/wheel/cp313-cp313-win_amd64/
#      b. cmake -DPIXELFORGE_BUILD_TESTS=OFF ... -G Ninja
#      c. ninja
#      d. cmake --install ... --prefix <wheel_root>
#      e. Wraps wheel_root into a .whl
#   4. Drops the .whl in dist/
```

```powershell
python -m build --sdist --outdir dist
# Just packages source files listed in [tool.scikit-build]'s sdist.include.
# Result: pixelforge-0.1.0.tar.gz
```

---

## 7. Verifying the wheel is self-contained

The "fresh venv" test:

```powershell
python -m venv build\wheel-venv
build\wheel-venv\Scripts\python.exe -m pip install --disable-pip-version-check `
    dist\pixelforge-0.1.0-cp313-cp313-win_amd64.whl pytest numpy

# No PIXELFORGE_* env vars set
build\wheel-venv\Scripts\python.exe -c "import pixelforge as pf; print(pf.version_string)"
# 0.1.0

build\wheel-venv\Scripts\python.exe -c "import pixelforge as pf; print(pf.default_plugin_dir())"
# C:\...\wheel-venv\Lib\site-packages\pixelforge\plugins

build\wheel-venv\Scripts\python.exe -m pytest python/tests
# 9 passed
```

---

## 8. sdist vs wheel — when does each get used?

`pip install pixelforge`:
1. `pip` queries PyPI for the project.
2. If a wheel matching the user's `cp313-win_amd64` tag exists → download it,
   unzip it. Done.
3. Otherwise → download the sdist, run `pip wheel` on it (which calls
   `scikit-build-core` which calls CMake which needs a working compiler
   on the user's machine).

So **wheels = ready-to-use binaries; sdists = source that builds itself**.
You always ship both.

For real distribution we'd build wheels for {win_amd64, linux_x86_64,
manylinux2014_aarch64} × {cp310, cp311, cp312, cp313}. That's Phase 8 + 9
territory.

---

## 9. Lessons learned

### 9.1 Pip's self-update can corrupt itself
`pip install --upgrade pip` *inside* a venv-creation step left
`pip._internal.cli` missing for a few seconds. Use
`--disable-pip-version-check` and skip the upgrade.

### 9.2 `wheel.install-dir = "."` is the safe default
Combined with `install(... DESTINATION <pkgname>)` in CMake. Anything else
gives nested paths.

### 9.3 The MinGW + MSVC-Python combo works
We feared CRT mismatches. In practice CPython 3.13 + MinGW UCRT both use
the universal CRT and play nicely. The only real cross-CRT trap is
`malloc`/`free` symmetry, which we already handle via the plugin
allocator-callback pattern.

### 9.4 `sdist.include` matters
By default scikit-build-core only includes files git-tracked. We added
explicit includes for `cmake/`, `third_party/`, and other folders that
might not be tracked yet.

---

## 10. Verification

```powershell
python -m build --wheel --sdist --outdir dist
# Created pixelforge-0.1.0-cp313-cp313-win_amd64.whl
# Created pixelforge-0.1.0.tar.gz
```

---

## 11. Next: [Phase 7 — Native installers](phase-07-native-installers.md)
