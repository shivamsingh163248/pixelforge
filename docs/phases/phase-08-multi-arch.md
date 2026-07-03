# Phase 8 — Multi-arch (Linux x64/ARM64 + Windows multi-compiler) Matrix

| Field | Value |
|---|---|
| Status | ✅ Done — Linux x64 GCC, Linux x64 Clang, **and Linux ARM64** all 32/32 green locally; MSVC + clang-cl deferred to CI |

---

## 1. Goal

Prove the codebase builds and runs on the **full target matrix**:

| OS      | Arch    | Compiler | Status (local)                | Status (CI, Phase 9) |
|---------|---------|----------|-------------------------------|----------------------|
| Windows | x86-64  | MinGW    | ✅ 32/32 (since Phase 0)      | planned              |
| Windows | x86-64  | MSVC     | ⏭ deferred (no install here)  | planned              |
| Windows | x86-64  | clang-cl | ⏭ deferred (no install here)  | planned              |
| Linux   | x86-64  | GCC 13   | ✅ 32/32 (Docker, this phase) | planned              |
| Linux   | x86-64  | Clang 18 | ✅ 32/32 (Docker, this phase) | planned              |
| Linux   | aarch64 | GCC 12   | ✅ 32/32 (Docker + QEMU, this phase, ~2 min test time) | planned              |

For each cell: configure → build → run all 32 CTest tests.

---

## 2. Files added in this phase

| Path | Purpose |
|---|---|
| [`CMakePresets.json`](../../CMakePresets.json) | added `win-clang-cl` and `linux-gcc-arm64` presets |
| [`docker/linux-x64-gcc.Dockerfile`](../../docker/linux-x64-gcc.Dockerfile)     | Ubuntu 24.04 + GCC 13 builder |
| [`docker/linux-x64-clang.Dockerfile`](../../docker/linux-x64-clang.Dockerfile) | Ubuntu 24.04 + Clang 18 builder |
| [`docker/linux-arm64.Dockerfile`](../../docker/linux-arm64.Dockerfile)         | Debian 12 (bookworm-slim) ARM64 builder |
| [`scripts/run_matrix.ps1`](../../scripts/run_matrix.ps1) | one-shot runner: build all images, run all containers, print pass/fail grid |
| [`.dockerignore`](../../.dockerignore)                   | keeps build/ dist/ .git/ out of the build context |

---

## 3. The CMake preset matrix

`CMakePresets.json` now exposes 6 configurations:

```
win-mingw         GCC under MinGW-w64       Windows host
win-msvc          cl.exe                    Windows host
win-clang-cl      clang-cl.exe              Windows host
linux-gcc         system gcc                Linux host
linux-clang       system clang              Linux host
linux-gcc-arm64   gcc with SYSTEM_PROCESSOR=aarch64   Linux host
```

The `condition` block on each preset hides those that don't apply to
the host OS, so `cmake --list-presets` always shows a clean per-OS list.

---

## 4. Docker buildx + QEMU (the cross-arch trick)

To build and run an `aarch64` Linux image on an `x86_64` Windows host:

```powershell
# 1. Make sure buildx is the active builder.
docker buildx create --name pixelforge-builder --use

# 2. Register QEMU emulators system-wide.
docker run --privileged --rm tonistiigi/binfmt --install arm64

# 3. Build for the foreign platform.
docker buildx build --platform linux/arm64 `
    --file docker/linux-arm64.Dockerfile `
    --tag pixelforge:linux-arm64 `
    --load .

# 4. Run a foreign-arch container; QEMU transparently emulates each syscall.
docker run --rm --platform linux/arm64 pixelforge:linux-arm64
```

The `--load` flag is critical: by default `buildx` exports to an OCI
registry; we want the image in the local Docker engine so `docker run`
can find it.

`tonistiigi/binfmt` registers the right `qemu-aarch64-static` binary
into the kernel's `binfmt_misc` table; afterwards every Linux ARM
binary the kernel sees gets transparently dispatched to QEMU.

---

## 5. Why three different base images

| Dockerfile | Base | Reason |
|---|---|---|
| `linux-x64-gcc.Dockerfile`   | `ubuntu:24.04`         | Most common Linux baseline; ships GCC 13 directly |
| `linux-x64-clang.Dockerfile` | `ubuntu:24.04`         | Same baseline, swap compiler to Clang 18 |
| `linux-arm64.Dockerfile`     | `debian:bookworm-slim` (arm64) | Same Debian/Ubuntu apt ecosystem as the x64 cells; once the network proxy was unblocked, this is the simplest base. Final image is ~213 MB (apt-trimmed Debian + GCC 12 + Python tooling). |

> Earlier in this phase the ARM64 image used `arm64v8/alpine:3.20`
> as a workaround for a corporate TLS-intercepting proxy. After the
> network was unblocked, Alpine 3.20 still failed because
> `py3-numpy` / `py3-pytest` aren't in its default repos for arm64.
> Switching to `debian:bookworm-slim` (which uses the same
> `python3-pytest` / `python3-numpy` packages as the x64 Ubuntu
> images) makes the three Linux cells maximally consistent.

---

## 6. The `run_matrix.ps1` orchestrator

```powershell
pwsh -File scripts/run_matrix.ps1                  # build + run all 3
pwsh -File scripts/run_matrix.ps1 -Targets gcc     # only one cell
pwsh -File scripts/run_matrix.ps1 -Targets gcc,arm64
```

For each target it:
1. `docker buildx build --load`s the image,
2. `docker run`s it (the container's `CMD` does the configure + build + ctest),
3. records `PASS` / `BUILD-FAILED` / `TEST-FAILED`,
4. prints a colored summary table at the end and exits non-zero on
   any failure.

---

## 7. Concrete results from a local run

### Linux x64 GCC 13 → 32/32 PASS

```
[1/53] Building CXX object plugins/invert/CMakeFiles/pf_invert.dir/invert.cpp.o
…
[53/53] Linking CXX executable bin/pixelforge_runtime_tests
…
32/32 Test #32: Python.Pybind11 ........................ Passed   0.76 sec

100% tests passed, 0 tests failed out of 32
Total Test time (real) =   1.20 sec
```

### Linux x64 Clang 18 → 32/32 PASS

```
…
32/32 Test #32: Python.Pybind11 ........................ Passed   0.96 sec

100% tests passed, 0 tests failed out of 32
Total Test time (real) =   1.42 sec
```

### Windows x64 MinGW → 32/32 PASS (re-verified at start of this phase)

```
32/32 Test #32: Python.Pybind11 ........................ Passed  29.97 sec
100% tests passed, 0 tests failed out of 32
Total Test time (real) = 220.10 sec
```

The Linux container runs the same 32 tests in **1–2 seconds** vs Windows
MinGW's **220 seconds** — almost entirely because the heavy Python
binding test (`Python.Pybind11`) takes 30 s on Windows (cold-start of
the Microsoft Store Python plus DLL search) versus <1 s on Linux.

---

## 8. What this phase actually surfaced

### 8.1 The codebase is portable
Three independent C++ compilers (MinGW GCC 15, Linux GCC 13, Linux
Clang 18) — across two libc implementations (MSVCRT/UCRT, glibc) —
all produced **identical** test results on first try after Phase 5's
`shared_ptr`-holder fix. The discipline of "no compiler-specific
extensions, no exceptions in core, explicit export macros" paid off.

### 8.2 The `python3 python3-dev` Dockerfile gotcha
First container run produced `31/32`; only `Python.Pybind11` failed
with `No module named pytest`. Fix: add `python3-pytest python3-numpy`
to every Dockerfile. **Lesson**: ctest registers Python tests with
`COMMAND ${Python_EXECUTABLE} -m pytest …`; if the runtime image is
missing test deps, ctest reports it as a real test failure even though
the C++ side is fine. Always install runtime test deps in the matrix
image, not just build deps.

### 8.3 ARM64 base-image churn (proxy → Alpine → Debian)
The ARM64 cell went through three iterations on this host:

1. First attempt: `arm64v8/ubuntu:24.04` — failed with `403
   authenticationrequired` from `ports.ubuntu.com` (corporate TLS
   proxy blocked the Ubuntu ARM mirror).
2. Workaround: `arm64v8/alpine:3.20` (HTTPS-only, smaller). Worked
   for the build until we tried to install `py3-pytest`/`py3-numpy`,
   which aren't published in Alpine's main repo for arm64.
3. Final: `debian:bookworm-slim` with `--platform=linux/arm64`. Once
   the proxy was unblocked, this gave us the same package layout as
   the x64 cells (`python3-pytest`, `python3-numpy`, GCC 12 from
   `build-essential`) and produced a green 32/32 result under QEMU
   in ~2 minutes of test time.

**Lesson**: when network is the variable, don't rewrite the build —
rewrite the wait. The codebase compiled cleanly on every base image
tried; the only thing that ever differed was the package source.

### 8.4 `--load` vs registry export
`docker buildx build` defaults to "no output" or "registry push"
depending on driver. Without `--load`, the image is built but
**discarded**; subsequent `docker run` says "image not found".
The matrix script always passes `--load`.

### 8.5 PowerShell + Docker working-directory trap
`docker buildx build … .` resolves `.` against the *parent process*'s
cwd, not the PowerShell-side `Set-Location`. If you're piping through
`Tee-Object` the docker process inherits its cwd from VS Code's
shell-host, not your prompt. Always pass an **absolute path** for the
context dir and `--file` argument when scripting.

---

## 9. Output artifacts

Local images after Phase 8:

```
REPOSITORY                  TAG               SIZE
pixelforge:linux-x64-gcc    (latest)          914 MB
pixelforge:linux-x64-clang  (latest)         1.07 GB
pixelforge:linux-arm64      (latest)          213 MB   (Debian arm64 + GCC 12)
```

Per-container output (inside `/src/build/<preset>/`) mirrors what the
host MinGW build produces: `lib/`, `bin/`, `python_modules/`,
`plugins/`. CPack inside the container produces `.tar.gz` / `.deb` /
`.rpm` natively.

---

## 10. What remains for full coverage (handed to Phase 9)

- **MSVC + clang-cl on Windows**: requires installing
  `Microsoft.VisualStudio.2022.BuildTools` plus LLVM. The presets are
  already in `CMakePresets.json`; CI just runs them.
- **Linux ARM64 native runner**: now also covered locally via QEMU,
  but GitHub's `ubuntu-24.04-arm` hosted runners build natively in
  CI (no QEMU overhead) — see Phase 9.
- **macOS**: planned for Phase 10. Adds Apple Clang + universal2 wheels.

These are matrix expansions, not architectural changes — the codebase
itself is **already proven cross-platform** by the three cells that
passed in this phase.

---

## 11. Reading material

- [`docker/`](../../docker/) — three Dockerfiles
- [`scripts/run_matrix.ps1`](../../scripts/run_matrix.ps1) — orchestrator
- [`CMakePresets.json`](../../CMakePresets.json) — full preset list
- [CMake Presets v6 reference](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
- [Docker buildx multi-platform docs](https://docs.docker.com/build/building/multi-platform/)
- [tonistiigi/binfmt](https://github.com/tonistiigi/binfmt) — QEMU registration container
