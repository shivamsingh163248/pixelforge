# Glossary — Every File Extension and Tool, Explained

This glossary explains every file type, tool, and acronym that appears
anywhere in PixelForge. It is written in the order you actually meet them
when you walk a `.cpp` file from source to a running installer.

---

## Source-level

### `.h` / `.hpp` — header file

Plain text. Declares types, function signatures, templates, and `inline`
or `constexpr` definitions. The compiler **`#include`s** these literally
into every `.cpp` that uses them — there is no separate "load header"
step. Headers exist because C and C++ split *declarations* (what a thing
looks like) from *definitions* (what it does), so multiple `.cpp` files
can call the same function without each having a copy of its body.

PixelForge: [`include/pixelforge/image.h`](../include/pixelforge/image.h)
declares the `pixelforge::Image` class. Twenty different `.cpp` files
across the SDK include it; only one (`image.cpp`) defines it.

### `.cpp` / `.cc` / `.cxx` — translation unit

A single C++ source file plus all the headers it `#include`s, after
preprocessing. The compiler treats each `.cpp` as one independent
**translation unit**: it produces exactly one object file from each one.
Two translation units never see each other directly — they communicate
through the linker.

### `.in` — CMake template

`@VARIABLE@` placeholders that `configure_file()` substitutes at configure
time. PixelForge uses [`include/pixelforge/version.h.in`](../include/pixelforge/version.h.in)
to bake `PROJECT_VERSION` into a real header at build time, so C++ code can
read `pixelforge::version_string`.

---

## Compiler output

### `.obj` (Windows) / `.o` (Linux/macOS) — object file

The compiler's output for one translation unit. Binary, not human-readable.
Contains:
- **Machine code** for every function defined in the `.cpp`.
- **Symbol table**: names of every function and global the `.cpp` defines
  or calls.
- **Relocations**: instructions for the linker on how to patch addresses
  once neighbouring object files are placed in memory.
- **Debug info** (DWARF or PDB-link), if `-g` / `/Z7`.

You almost never look at these directly. The linker does.

### `.pdb` — Microsoft debug database

Companion to `.obj`/`.exe`/`.dll` produced by MSVC and `clang-cl`. Holds
debug symbols separately from the binary so a release `.exe` stays small
but a debugger can still map addresses to source lines. (`.dwp` is the
GCC/Clang equivalent on Linux.)

---

## Linker output (the four shapes a program can take)

### `.lib` / `.a` — static library (archive)

A bundle of `.obj`/`.o` files glued into one file with a table of
contents. **No linking has happened yet.** When something links against
a `.a`, the linker pulls out only the object files whose symbols are
actually referenced; everything else is discarded. PixelForge: 
`libpixelforge_core.a` — the math layer.

### `.dll` (Windows) / `.so` (Linux) / `.dylib` (macOS) — shared library

A *fully linked* program **without** a `main()`, designed to be loaded
into another process at run time. Has its own symbol table that lists
which functions it makes visible to the outside world (its **exports**)
and which functions it needs from elsewhere (its **imports**).

PixelForge: `libpixelforge.dll` exports exactly 8 symbols (those tagged
`PIXELFORGE_API`); everything else stays internal.

### `.dll.a` (MinGW) / `.lib` (MSVC import library)

Confusingly, on Windows an "import library" has the same `.lib` extension
as a static library, but it contains *no* code — only stubs that tell the
linker "this symbol lives in `pixelforge.dll`; emit a delay-load thunk."
A program that links against `pixelforge.lib` (the import lib) is told at
load time "go find `pixelforge.dll` and resolve these names."

MinGW uses a slightly different convention (`libpixelforge.dll.a`) but
the role is identical. PixelForge ships this in the SDK component because
downstream developers need it to link against the runtime.

### `.exe` (Windows) / no extension (Linux/macOS) — executable

A linked binary with a `main()` (or `WinMain()`). The OS loader runs it.
PixelForge: `pixelforge.exe`.

### MODULE library (CMake term) — runtime-loadable plugin

Same file type as `.dll`/`.so`, but the build system treats it as
"this is a plugin loaded at runtime via `LoadLibrary`/`dlopen`; nobody
links to it at build time." CMake distinguishes it from a SHARED library
because (a) you should never put it in an import library, (b) on macOS
it gets a different file extension (`.so` vs `.dylib`).

PixelForge plugins (`pf_blur.dll`, etc.) are MODULE libraries.

---

## Python-flavoured outputs

### `.pyd` (Windows) / `.so` (Linux/macOS) — Python extension module

Just a regular `.dll`/`.so`, except that:
1. It exports exactly one symbol of the form `PyInit_<modulename>`.
2. CPython knows to look for files with this extension on its
   `sys.path` and to call `PyInit_` to bootstrap the module.
3. It links against `python3xx.lib` (the CPython import library) so it
   can call back into the interpreter.

PixelForge: `_pixelforge.cp313-win_amd64.pyd` — the leading `_` is the
convention for "compiled implementation"; the pure-Python `pixelforge`
package wraps it.

### `.whl` — wheel (PEP 427)

A ZIP file with a specific layout (`<package>/`, `<package>-<version>.dist-info/`).
The filename encodes the **wheel tag**: `pixelforge-0.1.0-cp313-cp313-win_amd64.whl`
means "PixelForge 0.1.0, CPython 3.13, ABI tag cp313, platform win_amd64."
`pip install` of a wheel just unzips it into `site-packages` — no
compilation, no `setup.py` execution.

### sdist (`.tar.gz`) — source distribution (PEP 517)

A source tarball with a `pyproject.toml`. When `pip install` sees a sdist,
it builds a wheel locally first by invoking the PEP 517 backend declared
in `pyproject.toml`'s `[build-system]` table.

PixelForge: backend is `scikit_build_core.build`, which drives our CMake
build to produce the wheel.

### `pyproject.toml`

The PEP 517 / 518 / 621 unified configuration file: declares the build
backend, project metadata (name, version, dependencies, classifiers),
and tool-specific configuration (in our case `[tool.scikit-build]` and
`[tool.pytest.ini_options]`).

---

## Build-system tools

### CMake

A meta-build generator. Reads `CMakeLists.txt` files (declarative) and
emits a build for some other tool (Ninja, Make, MSBuild, Xcode). The same
`CMakeLists.txt` produces a Visual Studio solution on one machine and a
Ninja graph on another.

### Ninja

A small, fast build tool that consumes `build.ninja` (typically generated
by CMake). Optimised for incremental builds — re-runs only what changed.
PixelForge uses Ninja exclusively (no Make, no MSBuild for our own builds).

### `CMakePresets.json`

Per-project file that names common CMake invocations: 
`win-mingw`, `linux-gcc`, etc. Replaces ad-hoc `build.sh` scripts. PixelForge
uses presets for both configure and CTest stages.

### CTest

The test runner that ships with CMake. Reads `CTestTestfile.cmake`
(generated from `add_test()` calls in your `CMakeLists.txt`), runs each
test, captures pass/fail, prints a summary. Parallelisable.

### CPack

The packager that ships with CMake. Reads the same `install(TARGETS …)`
rules CMake already uses and produces native installers (NSIS, WiX, DEB,
RPM, productbuild) plus archives (TGZ, ZIP).

### GoogleTest

C++ unit-testing framework. PixelForge fetches it via `FetchContent`
(no system install required).

### pybind11 / nanobind

Header-only C++ libraries that wrap the CPython C API in modern C++.
You write `py::class_<MyClass>(m, "MyClass").def(...)` and they generate
the boilerplate to expose `MyClass` as a Python type. nanobind is
pybind11's smaller, faster successor; PixelForge ships both for comparison.

### scikit-build-core

A PEP 517 backend that drives CMake. Replaces `setup.py` for projects
whose extension modules are built with CMake.

### NSIS (Nullsoft Scriptable Install System)

Windows installer generator. CPack's `NSIS` generator produces a
graphical installer with a wizard, optional PATH modification, and
component selection. PixelForge: `pixelforge-0.1.0-Windows-AMD64.exe`
is built this way.

### WiX

The Windows Installer XML toolset — generates `.msi` files (the format
that "Add/Remove Programs" and Group Policy understand). MSI is what
Microsoft ships their own products as. PixelForge has CPack `WIX` config
ready but isn't building MSIs yet (WiX isn't installed on this machine).

### dpkg / rpmbuild

The Debian and RPM packaging tools. CPack generates `.deb` and `.rpm`
files when running on Linux without you having to write `debian/control`
or a `.spec` file by hand.

---

## OS / loader concepts

### Symbol visibility (`dllexport` / `visibility("default")`)

By default, MSVC hides nothing in a DLL (you have to opt in via a `.def`
file or `dllexport`). GCC/Clang on Linux export *everything* by default.
PixelForge inverts both: `CXX_VISIBILITY_PRESET hidden` makes the GCC/Clang
default match MSVC, and the `PIXELFORGE_API` macro in
[`include/pixelforge/export.h`](../include/pixelforge/export.h) explicitly
tags the small set of symbols that should be visible across the DLL
boundary. This is the **only** way to get a stable, minimal ABI.

### `LoadLibrary` (Windows) / `dlopen` (POSIX)

OS calls that load a `.dll`/`.so` into the current process at run time
and return a handle. `GetProcAddress` / `dlsym` then looks up a single
named symbol in that handle. This is how plugins work.

PixelForge wraps both behind the cross-platform 
[`pixelforge::Plugin`](../include/pixelforge/plugin.h) RAII class.

### RPATH / `@loader_path`

A field in a Linux/macOS executable header that tells the loader "in
addition to the system library paths, also look in *this* directory for
my dependencies." `$ORIGIN/../lib` means "relative to the executable's
own location." This is what lets PixelForge install to
`<prefix>/{bin/pixelforge, lib/libpixelforge.so}` and Just Work without
the user having to set `LD_LIBRARY_PATH`.

### `os.add_dll_directory` (Python 3.8+, Windows)

On Windows since Python 3.8, dependent DLLs of a `.pyd` are no longer
resolved by walking `%PATH%`. The package must call
`os.add_dll_directory(path_to_my_dlls)` *before* importing the `.pyd`.
PixelForge does this in
[`python/pixelforge/__init__.py`](../python/pixelforge/__init__.py)
so the bundled `libpixelforge.dll` next to the `.pyd` gets found.

### CRT (C Runtime)

The standard C library implementation. Windows has many: MSVC ships
`vcruntime140.dll` + `ucrtbase.dll`; MinGW uses `msvcrt.dll` or UCRT;
each version is mutually incompatible at the heap-allocator level.
**Memory `malloc`'d in one CRT cannot be `free`'d in another.** This is
why you'll see notes throughout PixelForge insisting that ownership stays
on one side of an FFI boundary.

---

## CI / packaging concepts

### `manylinux` / `auditwheel`

Linux wheels can't depend on a specific glibc version, or they won't
install on older distros. The `manylinux` images (Docker containers
based on very old CentOS) provide a glibc baseline; `auditwheel` rewrites
a wheel to bundle any non-baseline `.so`s the extension needs. PixelForge
will use these in Phase 8/9.

### `delvewheel`

Windows analogue of `auditwheel`: scans a `.whl`, finds dependent DLLs,
and copies them into the wheel so they ship together. Not strictly
necessary for PixelForge today (we manually `install(TARGETS pixelforge …
DESTINATION pixelforge)`) but useful when bringing in third-party DLLs.

### Docker buildx + QEMU

`buildx` is the modern Docker build front-end that supports multi-platform
images. Combined with QEMU user-mode emulation registered in the kernel
(`binfmt_misc`), it can build ARM64 images on an x86-64 host. PixelForge
will use this in Phase 8 to produce a Linux ARM64 build without owning
ARM hardware.
