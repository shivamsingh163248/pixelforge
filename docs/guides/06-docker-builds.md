# Guide 06 — Docker matrix builds

You can reproduce every Linux build cell — even **Linux ARM64** —
without leaving Windows. This guide is the practical walk-through for
the Docker setup added in Phase 8.

---

## 1. Prerequisites

| Tool                          | Why                                                |
|-------------------------------|----------------------------------------------------|
| Docker Desktop ≥ 26           | provides `docker` and `docker buildx`              |
| (auto)  `desktop-linux` builder | Docker Desktop creates this; it has the corp-proxy CA chain pre-installed |
| (for ARM64) QEMU              | Registered via `tonistiigi/binfmt`; one-time setup |

Verify:

```powershell
docker --version
docker buildx version
docker buildx ls           # should list 'desktop-linux' as 'default'
```

---

## 2. The three Dockerfiles

All under [`docker/`](../../docker/):

| File | Base image                | Compiler   | Used by preset       |
|------|---------------------------|------------|----------------------|
| `linux-x64-gcc.Dockerfile`   | `ubuntu:24.04`        | GCC 13     | `linux-gcc`        |
| `linux-x64-clang.Dockerfile` | `ubuntu:24.04`        | Clang 18   | `linux-clang`      |
| `linux-arm64.Dockerfile`     | `arm64v8/alpine:3.20` | GCC (musl) | `linux-gcc-arm64`  |

Each Dockerfile installs build deps + Python test deps, copies the
source into `/src`, and `CMD`s the standard 3-command CMake pipeline
(configure → build → ctest).

---

## 3. Build a single cell, manually

Always pass **absolute paths** for `--file` and the build context — see
[Guide 07](07-troubleshooting.md#docker--powershell-working-directory-trap).

### 3.1 Linux x64 GCC

```powershell
docker buildx build `
    --platform linux/amd64 `
    --file C:\Users\you\pixelforge\docker\linux-x64-gcc.Dockerfile `
    --tag   pixelforge:linux-x64-gcc `
    --load `
    C:\Users\you\pixelforge

docker run --rm --platform linux/amd64 pixelforge:linux-x64-gcc
```

The container's `CMD` runs configure → build → 32 ctests, all inside
`/src/build/linux-gcc/`. Expected last line:

```
100% tests passed, 0 tests failed out of 32
```

### 3.2 Linux x64 Clang

```powershell
docker buildx build --platform linux/amd64 `
    --file C:\Users\you\pixelforge\docker\linux-x64-clang.Dockerfile `
    --tag   pixelforge:linux-x64-clang --load `
    C:\Users\you\pixelforge

docker run --rm --platform linux/amd64 pixelforge:linux-x64-clang
```

### 3.3 Linux ARM64 (cross-arch via QEMU)

One-time QEMU setup:

```powershell
docker run --privileged --rm tonistiigi/binfmt --install arm64
```

Then:

```powershell
docker buildx build --platform linux/arm64 `
    --file C:\Users\you\pixelforge\docker\linux-arm64.Dockerfile `
    --tag   pixelforge:linux-arm64 --load `
    C:\Users\you\pixelforge

docker run --rm --platform linux/arm64 pixelforge:linux-arm64
```

> **Why does this work on a Windows x86 host?** Docker buildx +
> `binfmt_misc` registers a QEMU ARM emulator with the kernel. Every
> ARM binary the kernel sees is transparently dispatched to QEMU.
> Builds are slower (~3-5× compile time) but functionally identical.

---

## 4. Build all cells with one command

The matrix runner [`scripts/run_matrix.ps1`](../../scripts/run_matrix.ps1)
wraps everything above:

```powershell
# All three cells:
powershell -File scripts\run_matrix.ps1

# Subset:
powershell -File scripts\run_matrix.ps1 -Targets gcc
powershell -File scripts\run_matrix.ps1 -Targets gcc,clang
powershell -File scripts\run_matrix.ps1 -Targets clang,arm64
```

Output ends with a colored summary table:

```
[matrix] === Results ===
gcc      : PASS
clang    : PASS
arm64    : PASS
```

Exit code is 0 only if every requested cell passed.

---

## 5. Iterating quickly with bind-mounts

The Dockerfiles `COPY` the source at image build time, so changing a
`.cpp` requires rebuilding the image. For tight iteration loops,
bind-mount the live source instead:

```powershell
docker run --rm --platform linux/amd64 `
    -v C:\Users\you\pixelforge:/src `
    pixelforge:linux-x64-clang `
    bash -c "cd /src && cmake --preset linux-clang -B /tmp/build && \
             cmake --build /tmp/build && ctest --test-dir /tmp/build --output-on-failure"
```

Notes:

- `/tmp/build` (not `/src/build/…`) avoids permission clashes with
  Windows-side build artefacts.
- The container reuses cached `_deps/` if you mount a persistent
  build dir; for a totally clean run, use a fresh `/tmp/<name>`.

---

## 6. Building installers inside a container

The CPack generators live inside the same image. To produce a `.deb`
without leaving Windows:

```powershell
docker run --rm --platform linux/amd64 `
    -v C:\Users\you\pixelforge:/src `
    pixelforge:linux-x64-gcc `
    bash -c "cd /src && cmake --preset linux-gcc -B /tmp/build && \
             cmake --build /tmp/build && \
             cd /tmp/build && cpack -B /src/dist"

dir dist\pixelforge*.deb
```

The `.deb` and `.rpm` show up under `dist\` on the Windows host,
courtesy of the bind-mount.

---

## 7. Producing a manylinux wheel locally

`cibuildwheel` does this in CI; the same recipe runs locally:

```powershell
pip install cibuildwheel
$env:CIBW_BUILD = "cp313-manylinux_x86_64"
cibuildwheel --platform linux --output-dir wheelhouse
```

cibuildwheel pulls the official `manylinux_2_28_x86_64` image
(GCC 12 toolchain) and runs the wheel build inside it. The output
wheel will be tagged `manylinux_2_28_x86_64`, suitable for upload to
PyPI.

---

## 8. Cleaning up

```powershell
# remove just our images:
docker rmi pixelforge:linux-x64-gcc pixelforge:linux-x64-clang pixelforge:linux-arm64

# remove dangling layers (anything that's not tagged):
docker system prune -f

# wipe the buildx cache (frees a lot of disk):
docker buildx prune -f
```

---

## 9. Common Docker build issues

See [Guide 07 — Troubleshooting](07-troubleshooting.md), specifically
the **Docker** section, for:

- "ERROR: failed to copy: tls: failed to verify certificate" (use the
  `desktop-linux` builder, not a custom one)
- 403 on `ports.ubuntu.com` for ARM64 (use the Alpine Dockerfile)
- "Permission denied removing build/" (build into `/tmp` instead)
- `pytest` missing causing test 32 to fail (Dockerfiles already include
  `python3-pytest python3-numpy`)

---

Next: [Guide 07 — Troubleshooting](07-troubleshooting.md).
