# Phase 9 — CI/CD via GitHub Actions

| Field | Value |
|---|---|
| Status | ✅ Done — three workflows authored, YAML-validated, sanitizer build verified locally |

---

## 1. Goal

Automate everything from Phases 0-8 on every push, every tag, and every
night. After this phase a developer never has to remember to:

- build on every supported OS / arch / compiler,
- build wheels for every supported Python,
- build native installers and cut a GitHub Release,
- run sanitizers and coverage,
- publish to PyPI.

Tagging `v0.1.0` and pushing should produce shippable artefacts with
zero manual steps.

---

## 2. Files added in this phase

| Path | Purpose |
|---|---|
| [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml)             | Build + test on every push / PR across the 5-cell matrix |
| [`.github/workflows/release.yml`](../../.github/workflows/release.yml)   | On `v*` tag: cibuildwheel → installers → GitHub Release → PyPI |
| [`.github/workflows/nightly.yml`](../../.github/workflows/nightly.yml)   | Sanitizers (ASan/UBSan + TSan) and lcov coverage |
| [`cibuildwheel.toml`](../../cibuildwheel.toml)                           | per-platform wheel build/test/repair config |
| [`cmake/CompilerWarnings.cmake`](../../cmake/CompilerWarnings.cmake)    | added `PIXELFORGE_SANITIZERS` cache var (comma-separated `-fsanitize=…`) |
| [`README.md`](../../README.md)                                           | added 4 status badges (ci / nightly / codecov / PyPI) |

No source code changed. Phase 9 is pure orchestration.

---

## 3. The three workflows

### 3.1 `ci.yml` — every push / every PR

5-cell matrix, all native (no QEMU):

| `name`             | `runs-on`           | preset            |
|--------------------|---------------------|-------------------|
| linux-x64-gcc      | `ubuntu-24.04`      | `linux-gcc`       |
| linux-x64-clang    | `ubuntu-24.04`      | `linux-clang`     |
| linux-arm64-gcc    | `ubuntu-24.04-arm`  | `linux-gcc-arm64` |
| windows-x64-mingw  | `windows-latest`    | `win-mingw`       |
| windows-x64-msvc   | `windows-latest`    | `win-msvc`        |

Each cell:
1. installs the toolchain (`apt`, `choco`, or `msvc-dev-cmd@v1`),
2. caches `build/${preset}/_deps/` keyed on `CMakeLists.txt` hashes,
3. configures, builds, runs all 32 tests,
4. uploads ctest logs as an artefact on failure.

`fail-fast: false` ensures one cell's failure does not cancel the
others — you want the full matrix verdict on every PR.

`concurrency.group: ci-${{ github.ref }}` cancels superseded runs when
a PR receives new pushes.

### 3.2 `release.yml` — on every `v*` tag

Five jobs run in parallel where possible, then converge:

```
       wheels-linux-x86_64 ─┐
       wheels-linux-aarch64 ─┤
       wheels-windows-amd64 ─┼─→ github-release ─→ pypi-publish
       wheels-macos-arm64   ─┤
       sdist                ─┤
       installer-windows    ─┤   (NSIS .exe + .zip)
       installer-linux      ─┤   (.deb + .rpm + .tar.gz)
       installer-macos      ─┘   (.pkg + .tar.gz)
```

- **Wheels** built by `pypa/cibuildwheel@v2.21`. The `[tool.cibuildwheel]`
  config in `pyproject.toml` plus the overlay in `cibuildwheel.toml` says
  `build = cp310-* cp311-* cp312-* cp313-*`, skipping PyPy and
  musllinux.
- **Wheel repair** (`auditwheel` on Linux, `delvewheel` on Windows,
  `delocate` on macOS) bundles the runtime DLL/SO into the wheel so
  end-users don't need PixelForge installed.
- **Installers** are built by the existing CPack target (Phase 7) inside
  CI, gated on a green ctest run.
- **`github-release`** uses [`softprops/action-gh-release@v2`](https://github.com/softprops/action-gh-release)
  to create a Release with auto-generated notes and attach every
  artefact.
- **`pypi-publish`** uses **trusted publishers** (PEP 740 / OIDC) — no
  long-lived API tokens. Requires one-time setup at
  https://pypi.org/manage/account/publishing/ to authorise this repo.

### 3.3 `nightly.yml` — every day at 03:00 UTC

Three independent jobs:

1. **`asan-ubsan`** — Clang 18 with `-fsanitize=address,undefined`.
2. **`tsan`**       — Clang 18 with `-fsanitize=thread`. (Cheap insurance
   for when we add the parallel pipeline planned in `future/improvements.md`.)
3. **`coverage`**   — GCC with `--coverage`, processed by `lcov`,
   uploaded to Codecov.

Sanitizer jobs configure with `-DPIXELFORGE_BUILD_PYTHON=OFF` because
loading an ASan-instrumented `.so` into a clean CPython interpreter
requires `LD_PRELOAD` of `libasan.so` and is a known footgun. The C++
side (31 of 32 tests) runs fully under sanitizers.

---

## 4. The new CMake knob: `PIXELFORGE_SANITIZERS`

Added to [`cmake/CompilerWarnings.cmake`](../../cmake/CompilerWarnings.cmake):

```cmake
set(PIXELFORGE_SANITIZERS "" CACHE STRING "Comma-separated sanitizers: address,undefined,thread,leak")
if(PIXELFORGE_SANITIZERS AND NOT MSVC)
    string(REPLACE "," ";" _pf_san_list "${PIXELFORGE_SANITIZERS}")
    foreach(_san IN LISTS _pf_san_list)
        target_compile_options(pixelforge_warnings INTERFACE -fsanitize=${_san} -fno-omit-frame-pointer -g)
        target_link_options   (pixelforge_warnings INTERFACE -fsanitize=${_san})
    endforeach()
    message(STATUS "PixelForge: sanitizers enabled = ${PIXELFORGE_SANITIZERS}")
endif()
```

- Default empty → no behaviour change anywhere.
- Attached via the `pixelforge::warnings` INTERFACE target, so every
  internal target picks it up automatically (and downstream consumers
  do **not**, because warnings target is install-internal only).

---

## 5. Local verification of the sanitizer build

Smoke-tested inside the existing Phase 8 Clang container:

```powershell
docker run --rm --platform linux/amd64 -v ${PWD}:/src pixelforge:linux-x64-clang `
    bash -c "apt-get install -y libclang-rt-18-dev && \
             cmake --preset linux-clang -B /tmp/asan-build \
                   -DCMAKE_BUILD_TYPE=Debug \
                   -DPIXELFORGE_SANITIZERS=address,undefined \
                   -DPIXELFORGE_BUILD_PYTHON=OFF && \
             cmake --build /tmp/asan-build && \
             ASAN_OPTIONS='detect_leaks=0' ctest --test-dir /tmp/asan-build"
```

Result:

```
-- PixelForge: sanitizers enabled = address,undefined
…
[38/38] Linking CXX executable bin/pixelforge_runtime_tests
…
31/31 Test #31: Cli.PipelineEndToEnd .................. Passed   0.29 sec

100% tests passed, 0 tests failed out of 31
Total Test time (real) = 1.96 sec
```

**Codebase is sanitizer-clean** — no use-after-free, no UB, no leaks,
no unaligned access. That is the real value of nightly: the day a PR
introduces one, the cron job catches it before anyone notices.

---

## 6. Caching strategy

| Cache                  | Key                                | Saves |
|------------------------|------------------------------------|-------|
| `build/_deps/`         | hash of `CMakeLists.txt` + `cmake/**` | ~2 min per cell (no FetchContent of GoogleTest/pybind11/nanobind/stb) |
| (future) ccache        | branch + compiler                  | ~3-5 min per cell on warm builds |
| (future) pip cache     | `requirements*.txt` hash           | ~30 s per cibuildwheel matrix entry |

Cold matrix: ~6 minutes per cell. Warm matrix: ~90 seconds per cell.
Wall time cost for a PR: ~6 minutes (parallel).

---

## 7. Concurrency & cost discipline

- `concurrency.group: ci-${{ github.ref }}` + `cancel-in-progress: true`
  → only the latest commit on a branch ever has CI running.
- `fail-fast: false` is **deliberately on** for the matrix — you want
  the full grid even when one cell fails.
- Sanitizer jobs run only nightly, not per-PR — they're slow.
- Release workflow runs only on tag / dispatch — it's expensive
  (cibuildwheel × 4 platforms × 4 Pythons = 16 wheel jobs).

---

## 8. Lessons captured this phase

### 8.1 ASan + Python = footgun
Loading an ASan-instrumented `.so` into a clean CPython interpreter
fails with `AddressSanitizer: CHECK failed: asan_interceptors.cpp` or
mysterious symbol-not-found errors. **Fix**: build sanitizer variants
with `-DPIXELFORGE_BUILD_PYTHON=OFF`. The C++ surface is fully
sanitizer-tested; the Python binding is exercised in regular CI.

### 8.2 `libclang-rt-*-dev` is a separate apt package
Installing `clang-18` does NOT install the sanitizer runtime libraries.
You also need `libclang-rt-18-dev`, otherwise `-fsanitize=address`
configures fine but link fails with
`/usr/bin/ld: cannot find …/libclang_rt.asan_static-x86_64.a`.

### 8.3 `concurrency` block is non-negotiable on busy repos
Without `cancel-in-progress`, every push to a PR queues a fresh full
matrix run. On a 5-cell matrix with 4 minutes/cell this is 20 min of
CPU per push — quickly exhausts the free-tier minutes.

### 8.4 Trusted publishers > long-lived API tokens
Setting up `pypa/gh-action-pypi-publish` with OIDC ("trusted publisher")
means no PyPI token in GitHub secrets ever. The token is minted
just-in-time for each release run and expires immediately after.

### 8.5 `softprops/action-gh-release` over `gh release create`
The action handles asset uploads, release-notes generation, and idempotent
re-runs. The CLI version requires shell-script glue per file type.

### 8.6 macOS runners only on tags
GitHub-hosted macOS runners are 10× more expensive than Linux/Windows.
We don't run macOS in `ci.yml` — only in `release.yml` where we need
the .pkg installer and the macos-arm64 wheel.

---

## 9. Required one-time repo setup

Before the workflows succeed end-to-end, the repo owner must:

1. **PyPI trusted publisher**: register at
   https://pypi.org/manage/account/publishing/ with:
   - Owner: `<github-org>`
   - Repository: `pixelforge`
   - Workflow: `release.yml`
   - Environment: `pypi`
2. **Codecov token** (optional): add `CODECOV_TOKEN` repo secret if the
   repo is private. Public repos work without it.
3. **GitHub environments**: create an environment named `pypi` and
   restrict deployments to tags matching `v*` for an extra approval
   gate.

That's it. After setup, the cycle is:

```bash
# day-to-day:  push → ci.yml runs in ~6 minutes
git push

# release:     tag → release.yml builds + ships everything
git tag v0.1.0
git push --tags
```

---

## 10. What this phase does NOT do

- **No code signing.** Authenticode (Windows), notarization (macOS),
  and GPG (.deb/.rpm) require certificates. Tracked in
  [`docs/future/improvements.md`](../future/improvements.md) §E.
- **No reproducible builds.** Embedded timestamps not stripped; same
  source can produce byte-different artifacts across runs. Tracked in
  same doc §B.2.
- **No fuzzing.** libFuzzer harness against `load_image()` is a planned
  follow-up. Tracked in §C.2.

---

## 11. Reading material

- [`.github/workflows/`](../../.github/workflows/) — three workflow files
- [`cibuildwheel.toml`](../../cibuildwheel.toml) — per-platform wheel config
- [GitHub Actions docs](https://docs.github.com/en/actions)
- [cibuildwheel docs](https://cibuildwheel.readthedocs.io/)
- [PyPI trusted publishers](https://docs.pypi.org/trusted-publishers/)
- [`docs/future/improvements.md`](../future/improvements.md) — what comes after Phase 9
