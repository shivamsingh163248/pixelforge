# Guide 07 — Troubleshooting

A checklist of every issue we hit while building PixelForge end-to-end,
with the exact fix that worked. Search this page (Ctrl-F) for the
keyword in your error message.

---

## CMake configure errors

### `In-source builds are not allowed`

You ran `cmake .` in the repo root. PixelForge enforces out-of-source
builds.

**Fix:** always use a preset, which puts the build dir under `build/<preset>/`:

```powershell
cmake --preset win-mingw
```

### `install(EXPORT "PixelForgeTargets") includes target … which requires target "pixelforge_warnings" that is not in any export set`

You added a new internal target that links `pixelforge::warnings` but
forgot to install the warnings interface in the same export set.

**Fix:** [`cmake/CompilerWarnings.cmake`](../../cmake/CompilerWarnings.cmake)
already does this when not building a wheel:

```cmake
if(NOT DEFINED SKBUILD_PROJECT_NAME)
    install(TARGETS pixelforge_warnings EXPORT PixelForgeTargets)
endif()
```

### `Could NOT find Python (missing: Development.Module)`

Python is on PATH but the **dev headers** aren't installed.

**Fix:**

```bash
sudo apt-get install -y python3-dev          # Ubuntu/Debian
sudo dnf install -y python3-devel            # Fedora/RHEL
```

On Windows the Microsoft Store Python comes with headers; the standalone
installer needs the "Development tools" optional feature ticked.

---

## Compile / link errors

### `error: 'shared_ptr' is not a member of 'std'`

Missing `#include <memory>`. We compile `-Wall -Wpedantic` so this
manifests as an error.

### `undefined reference to '__imp__Py_*'`  (Windows pybind11 link)

The Python module was linked against the wrong CPython. Most likely
two Pythons are installed and CMake picked the wrong one.

**Fix:** force the right interpreter:

```powershell
cmake --preset win-mingw -DPython_EXECUTABLE="$((Get-Command python).Source)"
```

### MinGW: `cannot find /usr/lib/llvm-18/lib/clang/18/lib/linux/libclang_rt.asan_static-x86_64.a`

You configured with `-DPIXELFORGE_SANITIZERS=address,…` on Linux but
didn't install the sanitizer runtime libs.

**Fix:**

```bash
sudo apt-get install -y libclang-rt-18-dev    # for clang-18
```

(GCC's sanitizers ship in `gcc-13`'s package directly, no extra install.)

---

## ctest / runtime errors

### Test 32 (`Python.Pybind11`) reports `No module named pytest`

The container or system Python doesn't have pytest.

**Fix:**

```bash
# Linux containers:
apt-get install -y python3-pytest python3-numpy

# Local Linux:
pip install pytest numpy

# Windows:
python -m pip install pytest numpy
```

### `pixelforge.exe` exits with `0xC0000135` / `code execution cannot proceed because libpixelforge.dll was not found`

The runtime DLL is not on `PATH`. This happens when you copy
`pixelforge.exe` away from `build\<preset>\bin\` without bringing
`libpixelforge.dll` along.

**Fix:** install via the NSIS installer
([Guide 04 §2](04-installing.md#2-windows--nsis-installer)) — that puts
both files in the right relative location automatically.

### Plugin discovery fails (`Plugins in <dir>: (none)`)

The CLI looked for `pf_*.dll` / `pf_*.so` in a directory that doesn't
contain any.

**Fix:** point it explicitly:

```powershell
pixelforge.exe list-filters --plugin-dir "C:\Program Files\PixelForge\lib\pixelforge\plugins"
```

### `LD_LIBRARY_PATH` for the portable Linux tarball

After extracting `pixelforge-…-Linux-x86_64.tar.gz` you may get
`error while loading shared libraries: libpixelforge.so.0`.

**Fix:**

```bash
export LD_LIBRARY_PATH="/opt/pixelforge-0.1.0-Linux-x86_64/lib:$LD_LIBRARY_PATH"
```

The CLI was built with an `RPATH` of `$ORIGIN/../lib`, so this only
matters if you've moved `pixelforge` out of `bin/`.

### ASan tests fail at `import _pixelforge`

ASan-instrumented `.so` loaded into a clean CPython needs
`LD_PRELOAD=$(clang -print-file-name=libclang_rt.asan-x86_64.so)`.

**Fix:** for sanitizer builds, disable the Python module:

```bash
cmake --preset linux-clang -DPIXELFORGE_SANITIZERS=address,undefined -DPIXELFORGE_BUILD_PYTHON=OFF
```

The C++ surface (31 tests) still runs fully under ASan/UBSan.

---

## Wheel / packaging errors

### `pip install pixelforge…whl` succeeds but `import pixelforge` fails with `ImportError: DLL load failed`

The wheel didn't bundle the runtime DLL. This means
`delvewheel`/`auditwheel` wasn't run during wheel construction.

**Fix:** rebuild via cibuildwheel, which has the repair step wired in
(`[tool.cibuildwheel].repair-wheel-command`). Or install the NSIS
runtime alongside.

### `cpack` says `CPack: Could not find a valid generator`

You ran `cpack` from the wrong directory.

**Fix:** always run from the *build directory*:

```powershell
cd build\win-mingw
cpack -B ..\..\dist
```

### NSIS not found at packaging time

`cpack` for `-G NSIS` shells out to `makensis.exe`.

**Fix:**

```powershell
winget install NSIS.NSIS
$env:PATH = "C:\Program Files (x86)\NSIS;$env:PATH"
```

### `pixelforge` not recognized after running the NSIS installer

Two possible causes:

1. **You're using the same shell session you opened *before* the
   install.** The PATH update only applies to **new** processes —
   close the PowerShell window and open a fresh one.
2. **You're running an old installer (≤ 0.1.0 first cut)** that used
   CPack's built-in `CPACK_NSIS_MODIFY_PATH`. That macro silently
   fails when the existing user PATH is > 1024 chars (very common on
   developer boxes), and showed a `Warning! PATH too long installer
   unable to modify PATH!` dialog. Rebuild the installer from current
   `main` — it now uses
   `[Environment]::SetEnvironmentVariable(...,'User')` which has no
   length limit.

Verify the registry was actually written:

```powershell
[Environment]::GetEnvironmentVariable('PATH','User') -split ';' |
    Select-String 'PixelForge'
```

You should see your install dir's `bin\` folder. If not, re-run the
installer (it's idempotent).

### Desktop shortcut not created

Same root cause as above — fixed in the current installer. The new
installer **unconditionally** creates `%USERPROFILE%\Desktop\PixelForge CLI.lnk`
in its `EXTRA_INSTALL_COMMANDS` block (no checkbox).

---

## Docker errors

### `ERROR: failed to copy: tls: failed to verify certificate: x509: certificate signed by unknown authority`

You're behind a corporate TLS-intercepting proxy and tried to use a
freshly-created `docker buildx create` builder, which doesn't have
the corp CA chain.

**Fix:** use the default `desktop-linux` builder instead:

```powershell
docker buildx use desktop-linux
```

The matrix runner now does this automatically.

### `403 authenticationrequired` from `ports.ubuntu.com` (during ARM64 build)

Same proxy issue, against Ubuntu's ARM mirror.

**Fix:** the ARM64 Dockerfile uses **Alpine** (HTTPS-only mirrors),
not Ubuntu, specifically to dodge this.

### `permission denied` removing `build/linux-gcc/_deps/...`

The container previously ran as root and left files owned by uid 0
in your bind-mounted source.

**Fix:** build into `/tmp/<name>` inside the container instead of
`/src/build/...`:

```powershell
docker run --rm -v C:\Users\you\pixelforge:/src pixelforge:linux-x64-gcc `
    bash -c "cd /src && cmake --preset linux-gcc -B /tmp/build && cmake --build /tmp/build && ctest --test-dir /tmp/build"
```

### Docker / PowerShell working-directory trap

`docker buildx build … .` resolves `.` against the **PowerShell host
process's** cwd, which may not be your prompt's cwd.

**Fix:** always pass absolute paths:

```powershell
docker buildx build `
    --file   C:\Users\you\pixelforge\docker\linux-x64-gcc.Dockerfile `
    --tag    pixelforge:linux-x64-gcc `
    --load `
    C:\Users\you\pixelforge
```

### `docker buildx build` succeeds but `docker run` says "image not found"

You forgot `--load`. By default buildx exports to a registry and
**throws away** the image locally.

**Fix:** add `--load` to every `docker buildx build` you intend to
run locally.

---

## CI / GitHub Actions errors

### "Resource not accessible by integration"

A workflow tried to write something it doesn't have permission for
(e.g. create a release, comment on a PR).

**Fix:** add the appropriate `permissions:` block at the top of the
workflow. `release.yml` already has:

```yaml
permissions:
  contents: write       # gh release create
  id-token: write       # PyPI trusted publisher
```

### PyPI publish step fails with "invalid-publisher"

You haven't registered the GitHub repo as a trusted publisher on PyPI.

**Fix:** go to https://pypi.org/manage/account/publishing/ and add:

```
Owner:        <your-org>
Repository:   pixelforge
Workflow:     release.yml
Environment:  pypi
```

### Cache miss on every CI run

The cache key includes the **runner OS**. A typo there forces a fresh
download each time.

**Fix:** include both `${{ matrix.name }}` (which encodes os+compiler)
and a content hash:

```yaml
key: deps-${{ matrix.name }}-${{ hashFiles('CMakeLists.txt', 'cmake/**') }}
```

---

## Checking your environment in one shot

```powershell
pwsh scripts\check_env.ps1     # Windows
```

```bash
bash scripts/check_env.sh      # Linux / macOS
```

These scripts print every required tool's version and flag missing
ones. Re-run after installing something to confirm it took.

---

## Asking for help

If none of the above matches, please include the following when filing
an issue:

1. Output of the env-check script.
2. The full `cmake --preset <name>` configure log
   (`build/<preset>/CMakeFiles/CMakeOutput.log`).
3. The full failing command and its output.
4. Your OS, compiler, and CMake versions.
