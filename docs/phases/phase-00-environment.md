# Phase 0 — Environment Verification & Repo Scaffolding

| Field | Value |
|---|---|
| Goal | Confirm every required tool is installed and working; lay down the project skeleton. |
| Status | ✅ Complete |
| Artifacts | `scripts/check_env.{ps1,sh}`, `.gitignore`, `.editorconfig`, `.clang-format`, `LICENSE`, `README.md` |
| Tests added | 0 (no code yet) |

---

## 1. Why this phase exists

Every "build the same project on a fresh machine in 30 seconds" promise
breaks because of an undocumented dependency. Before writing one line of
C++, we **audit the toolchain** and write down what versions are present.
This becomes the contract that CI and the documentation enforce.

It also lets us surface the *missing* tools early so we can decide:
install them now, or defer to a later phase?

---

## 2. Required toolchain

| Tool | Why needed | Min version | Found on this machine |
|---|---|---|---|
| C++ compiler | Build the SDK | C++20 capable | MinGW-w64 GCC 15.2.0 ✅ |
| CMake | Build-system generator | 3.25 | 4.3.1 ✅ |
| Ninja | Build executor | 1.10 | 1.13.2 ✅ |
| Git | Version control + FetchContent | 2.30+ | 2.50.1 ✅ |
| Python | Bindings + pytest | 3.10+ | 3.13.13 (Microsoft Store) ✅ |
| numpy + pytest | Python tests | latest | numpy 2.3.2, pytest 7.4.3 ✅ |
| Docker + buildx | Multi-arch (Phase 8) | 24+ | Docker Desktop 28.3.2 ✅ |

### Optional / deferred

| Tool | Reason deferred | Picked up in |
|---|---|---|
| MSVC (cl.exe) | Not installed; would require VS Build Tools download | Phase 8 |
| Clang on Windows | Same reason | Phase 8 |
| WSL Ubuntu | Only `docker-desktop` distro present, no real Ubuntu | Use Docker instead (Phase 8) |
| NSIS | Installed in Phase 7 via `winget install NSIS.NSIS` | Phase 7 |
| WiX (`candle.exe`) | Adds MSI capability; deferred | Phase 8/9 |

---

## 3. The scripts

### [`scripts/check_env.ps1`](../../scripts/check_env.ps1) (Windows)

Probes each tool with `Get-Command` and prints a green ✅ / red ❌ row.
Reports its version. Exits non-zero if any **required** tool is missing.

### [`scripts/check_env.sh`](../../scripts/check_env.sh) (Linux/macOS)

POSIX `sh` equivalent. Same behaviour.

Run from the repo root:

```powershell
.\scripts\check_env.ps1
```

```bash
./scripts/check_env.sh
```

### Sample output

```
=== PixelForge environment audit ===
[OK] CMake     4.3.1
[OK] Ninja     1.13.2
[OK] Git       2.50.1
[OK] g++       (MinGW-w64) 15.2.0
[OK] python3   3.13.13
[OK] docker    28.3.2
[--] cl.exe    not found (MSVC) — deferred to Phase 8
[--] clang     not found (Windows) — deferred to Phase 8
```

---

## 4. Repo scaffolding

The following files were created with conservative, production-quality
defaults:

- **`.gitignore`** — ignores `build/`, `dist/`, `*.pyd`, `*.whl`,
  `__pycache__/`, `.vscode/settings.json`, etc.
- **`.editorconfig`** — UTF-8, LF, 4-space indent for C++/CMake/Python,
  trim trailing whitespace.
- **`.clang-format`** — based on Google style, 100-column line limit,
  pointer-attached-to-type. Run with `clang-format -i src/**/*.cpp`.
- **`LICENSE`** — MIT.
- **`README.md`** — project pitch + quick-start commands.

These are the files every serious C++ repo has on day one.

---

## 5. Lessons learned (the hard ones)

### 5.1 Trailing-space folder names break Win32

The original workspace was at `C:\Users\shsingh\Downloads\Software\` —
note the *trailing space*. PowerShell handles it fine, but every Win32
child process inherits a *stripped* current directory. CMake configure
worked; CMake build failed because `ninja.exe` got launched with the
trailing space silently removed and couldn't find `build.ninja`.

**Fix:** moved everything to `C:\Users\shsingh\pixelforge`. Documented in
[`progress.md`](../../) (session memory) so we don't repeat this mistake.

### 5.2 MSVC absence is OK for now

We chose MinGW-w64 GCC as the primary Windows compiler because it was
already installed via winget. The plan is to keep the code MSVC-clean (no
GNU-only extensions), then prove it builds with MSVC in Phase 8 — at
which point the multi-compiler matrix becomes real, not aspirational.

---

## 6. What this phase locked in

| Decision | Rationale |
|---|---|
| C++20 | `std::format`, `<filesystem>`, `<span>`, structured bindings — all stable across MSVC/GCC/Clang at this point. |
| CMake 3.25+ | Needed for `add_dependencies` on alias targets and modern preset features. |
| Ninja-only | Faster than Make, predictable across platforms, single-config matches one preset → one build dir. |
| Out-of-source builds **enforced** | Top-level `CMakeLists.txt` errors out if `CMAKE_BINARY_DIR == CMAKE_SOURCE_DIR`. |
| `CXX_VISIBILITY_PRESET hidden` globally | Forces every public symbol to opt in via `PIXELFORGE_API`. |

---

## 7. Next: [Phase 1 — Core static library](phase-01-core-static-lib.md)
