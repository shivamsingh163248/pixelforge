# PixelForge — Project Documentation

> A real, installable, cross-platform image-processing SDK built end-to-end
> in C++20 with Python bindings. Designed as a **teaching codebase** that
> exercises every layer of the modern native software pipeline:
> source → object files → static libs → shared libs → plugins → CLI exe →
> Python wheel → native installers → multi-arch builds → CI/CD.

---

## 1. What is PixelForge?

PixelForge is a small but **production-shaped** image-processing SDK. It does
the same kind of work as a tiny subset of Pillow / OpenCV, but the *value*
of the project is not the algorithms — it is the **build, packaging, and
distribution pipeline** wrapped around them.

It demonstrates, in working code, every concept on the modern C++/Python
toolchain checklist:

| Concept | Where it lives in PixelForge |
|---|---|
| `.h` header files | [`include/pixelforge/*.h`](../include/pixelforge/) |
| `.cpp` translation units | [`src/**/*.cpp`](../src/) |
| `.obj` object files | `build/<preset>/**/*.obj` (compiler output) |
| `.lib` / `.a` static archive | `libpixelforge_core.a` ([`src/core/`](../src/core/)) |
| `.dll` / `.so` shared library | `libpixelforge.dll` ([`src/runtime/`](../src/runtime/)) |
| `.dll.a` import library | `libpixelforge.dll.a` (auto from MinGW) |
| Plugin (MODULE) library | `pf_*.dll` ([`plugins/`](../plugins/)) |
| `.exe` executable | `pixelforge.exe` ([`src/cli/`](../src/cli/)) |
| `.pyd` Python extension | `_pixelforge.cp313-win_amd64.pyd` ([`python/pybind11_module/`](../python/pybind11_module/)) |
| `.whl` Python wheel | `dist/pixelforge-0.1.0-cp313-cp313-win_amd64.whl` |
| sdist tarball | `dist/pixelforge-0.1.0.tar.gz` |
| CMake build system | [`CMakeLists.txt`](../CMakeLists.txt), [`CMakePresets.json`](../CMakePresets.json) |
| Ninja generator | `build/<preset>/build.ninja` |
| CTest harness | `build/<preset>/CTestTestfile.cmake` |
| GoogleTest unit tests | [`tests/test_*.cpp`](../tests/) |
| pytest integration tests | [`python/tests/test_filters.py`](../python/tests/test_filters.py) |
| NSIS installer | `dist/pixelforge-0.1.0-Windows-AMD64.exe` |
| ZIP portable archive | `dist/pixelforge-0.1.0-Windows-AMD64.zip` |
| DEB / RPM (Linux) | wired in CPack, built when run on Linux |
| `pyproject.toml` / PEP 517 | [`pyproject.toml`](../pyproject.toml) |
| `find_package` SDK config | `lib/cmake/PixelForge/PixelForgeConfig.cmake` |

---

## 2. How to read these docs

```
docs/
├── README.md                ← you are here (overview + index)
├── architecture.md          ← deep dive: layering, ABI, lifetime, threading
├── glossary.md              ← every file extension and tool, explained
├── guides/                  ← HANDS-ON: copy-paste commands for every workflow
│   ├── README.md
│   ├── 01-building-from-source.md
│   ├── 02-build-artifacts.md           (what each .dll/.so/.pyd/.whl/.exe is, where stored)
│   ├── 03-running-pixelforge.md        (CLI + Python + C++ SDK consumer)
│   ├── 04-installing.md                (wheel / NSIS / ZIP / DEB / RPM / PKG)
│   ├── 05-generating-packages.md       (how to PRODUCE wheel/sdist/MSI/DEB/RPM)
│   ├── 06-docker-builds.md             (Linux x64 GCC + Clang + ARM64 from Windows)
│   └── 07-troubleshooting.md           (every error we hit + the fix)
├── phases/                  ← WHY: design rationale, one doc per phase
│   ├── phase-00-environment.md
│   ├── phase-01-core-static-lib.md
│   ├── phase-02-runtime-dll.md
│   ├── phase-03-plugins.md
│   ├── phase-04-cli.md
│   ├── phase-05-python-bindings.md
│   ├── phase-06-wheel-packaging.md
│   ├── phase-07-native-installers.md
│   ├── phase-08-multi-arch.md          (Docker buildx + multi-compiler matrix)
│   └── phase-09-ci-cd.md               (GitHub Actions: ci, release, nightly)
└── future/
    └── improvements.md      ← what to add next, and why
```

> The **`guides/`** folder is the practical "how do I…?" manual.
> The **`phases/`** folder is the architectural "why is it built this way?" reference.
> Start with [`guides/README.md`](guides/README.md) if you just want to build, install, and run.

**Recommended reading order:**

1. This file (project overview).
2. [`glossary.md`](glossary.md) — if any file extension or tool is unfamiliar.
3. [`architecture.md`](architecture.md) — the big picture: how the layers fit.
4. [`phases/phase-00-environment.md`](phases/phase-00-environment.md) and
   onward, in order. Each phase doc is self-contained: it explains
   *what* was built, *why* it was built that way, *how* it works,
   *what failed and how it was fixed*, and what to read next.

---

## 3. Project status

| Phase | Status | Artifact(s) |
|---|---|---|
| 0 — Environment | ✅ | `scripts/check_env.{ps1,sh}` |
| 1 — Core static lib | ✅ | `libpixelforge_core.a` (13 tests) |
| 2 — Shared runtime | ✅ | `libpixelforge.dll` (+9 tests) |
| 3 — Plugin architecture | ✅ | 5 plugins (+6 tests) |
| 4 — CLI exe | ✅ | `pixelforge.exe` (+3 tests) |
| 5 — Python bindings | ✅ | `_pixelforge.pyd` + `_pixelforge_nb.pyd` (+9 tests) |
| 6 — Wheel + sdist | ✅ | `pixelforge-0.1.0-cp313-cp313-win_amd64.whl` (1.2 MB) |
| 7 — Native installers | ✅ | NSIS `.exe` (2.1 MB), portable `.zip` (5.5 MB) |
| 8 — Multi-arch matrix | ✅ | Linux x64 GCC + Clang via Docker buildx; ARM64/MSVC deferred to CI |
| 9 — CI/CD | ✅ | GitHub Actions: ci.yml (5-cell matrix) + release.yml (wheels + installers + PyPI) + nightly.yml (ASan/UBSan/TSan + coverage) |

**Total tests passing:**
- Windows MinGW: 32 / 32
- Linux x64 GCC 13 (Docker): 32 / 32
- Linux x64 Clang 18 (Docker): 32 / 32

---

## 4. Quick start

### Build & test (Windows, MinGW)

```powershell
cmake --preset win-mingw
cmake --build --preset win-mingw
ctest --preset win-mingw --output-on-failure
```

### Build the wheel

```powershell
python -m build --wheel --outdir dist
pip install dist\pixelforge-0.1.0-cp313-cp313-win_amd64.whl
python -c "import pixelforge as pf; print(pf.version_string)"
```

### Build the native installer

```powershell
$env:PATH = "C:\Program Files (x86)\NSIS;$env:PATH"
cd build\win-mingw
cpack -B ..\..\dist
# → dist/pixelforge-0.1.0-Windows-AMD64.exe   (NSIS installer)
# → dist/pixelforge-0.1.0-Windows-AMD64.zip   (portable archive)
```

---

## 5. What you should learn from this codebase

If you read every phase doc and trace the corresponding code, you will have
hands-on understanding of:

1. **Translation-unit / linker model.** Why `.cpp` → `.obj` → `.lib`/`.a`
   /`.dll`/`.so`/`.exe` exist as separate steps and what each is for.
2. **Symbol visibility.** Why `__declspec(dllexport)` /
   `__attribute__((visibility("default")))` matter, why a clean DLL exports
   *only* the `PIXELFORGE_API`-tagged symbols, and what goes wrong when it
   doesn't.
3. **C ABI for plugins.** Why the plugin entry point is pure C, not C++ —
   and how that lets a plugin built with one compiler work with a host
   built by a different compiler.
4. **Python C extensions.** What a `.pyd` actually is (it's a regular DLL
   with one `PyInit_<name>` symbol), why CPython 3.13 built with MSVC can
   load a `.pyd` built with MinGW, and where the CRT-mismatch landmines are.
5. **Wheel / PEP 517 / scikit-build-core.** How a CMake-driven C++
   extension becomes a `pip install`-able artifact that bundles its
   transitive native dependencies.
6. **Native installers.** How CPack drives WiX / NSIS / DEB / RPM / TGZ
   from a *single* set of `install(TARGETS …)` rules, and how components
   let you ship Runtime, SDK, and Plugins as separate selectable parts.
7. **`find_package(PixelForge)`** for downstream consumers — the same API
   you use when you write `find_package(OpenCV REQUIRED)`.
8. **CMake presets** — the modern replacement for ad-hoc shell scripts and
   per-developer `build.sh`s.

If you can defend every line of every CMakeLists.txt in this repo, you can
walk into a senior C++ build-engineer interview tomorrow.
