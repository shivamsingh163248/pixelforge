# PixelForge — Hands-On Guides

This folder is the **practical, copy-paste manual** for working with
PixelForge. The [`docs/phases/`](../phases/) docs explain *why* each
piece exists; the guides here explain *exactly which command to type*.

| # | Guide | Read this when… |
|---|---|---|
| 01 | [Building from source](01-building-from-source.md)    | …you cloned the repo and want to compile it |
| 02 | [Build artifacts — what & where](02-build-artifacts.md) | …you want to know which file is which (`.dll` vs `.so` vs `.pyd` vs `.whl` vs `.exe` vs `.lib`/`.a`) and where each one lands |
| 03 | [Running PixelForge](03-running-pixelforge.md)         | …you've built it and want to actually use the CLI, the Python module, or load a plugin |
| 04 | [Installing PixelForge](04-installing.md)              | …you have an artefact (`.whl`, `.exe`, `.zip`, `.deb`, `.rpm`) and want to install it on a machine |
| 05 | [Generating packages](05-generating-packages.md)       | …you want to **produce** a wheel, sdist, MSI/NSIS, ZIP, DEB, or RPM yourself |
| 06 | [Docker matrix builds](06-docker-builds.md)            | …you want to build/test on Linux x64 GCC, Linux x64 Clang, or Linux ARM64 from a Windows host |
| 07 | [Troubleshooting](07-troubleshooting.md)               | …something failed and you need a checklist of fixes |

Each guide is self-contained. You can jump straight to the one you need.

---

## Reading order if you've never built C++ before

1. [01 — Building from source](01-building-from-source.md)
2. [02 — Build artifacts](02-build-artifacts.md)  ← essential to understand what just got produced
3. [03 — Running PixelForge](03-running-pixelforge.md)
4. [05 — Generating packages](05-generating-packages.md) once you want to ship to others
5. [04 — Installing](04-installing.md) when an end-user takes over

## Reading order if you're a packager / release engineer

1. [05 — Generating packages](05-generating-packages.md)
2. [06 — Docker matrix builds](06-docker-builds.md)
3. [04 — Installing](04-installing.md) (verifies your packages on a clean box)
4. [`../phases/phase-09-ci-cd.md`](../phases/phase-09-ci-cd.md) for full automation
