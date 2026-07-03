# Guide 04 — Installing PixelForge on a clean machine

This guide assumes you already **have** the artefacts (someone built
them, or you downloaded them from a release). To **build** them
yourself, see [Guide 05](05-generating-packages.md).

| You want to install via… | Read section | Result |
|--------------------------|--------------|--------|
| `pip` (Python wheel)     | [§1 Wheel](#1-python-wheel) | Python `import pixelforge` works |
| Windows installer (`.exe`) | [§2 NSIS](#2-windows--nsis-installer) | CLI on Start Menu, DLL/SDK in Program Files |
| Portable Windows ZIP     | [§3 ZIP](#3-windows--portable-zip) | Unpack and run, no admin rights needed |
| Linux `.deb` (Ubuntu/Debian) | [§4 DEB](#4-linux--deb) | `apt install`, files in `/usr` |
| Linux `.rpm` (Fedora/RHEL/openSUSE) | [§5 RPM](#5-linux--rpm) | `dnf install`, files in `/usr` |
| Linux portable tarball    | [§6 TGZ](#6-linux--tarball) | Extract anywhere |
| macOS `.pkg`              | [§7 PKG](#7-macos--pkg) | GUI installer to `/usr/local` |

---

## 1. Python wheel

The wheel is the simplest distribution: it bundles the Python facade,
the C-extension `.pyd`/`.so`, the runtime `libpixelforge.dll`/`.so`,
and all five plugins into one self-contained file.

### 1.1 Install (recommended: into a virtual environment)

```powershell
# Windows
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install pixelforge-0.1.0-cp313-cp313-win_amd64.whl
```

```bash
# Linux / macOS
python -m venv .venv
source .venv/bin/activate
pip install pixelforge-0.1.0-cp313-cp313-manylinux_2_28_x86_64.whl
```

### 1.2 Verify

```powershell
python -c "import pixelforge as pf; print(pf.version_string, pf.list_filters())"
# 0.1.0 ['blur', 'edge', 'grayscale', 'invert', 'sharpen']
```

### 1.3 Uninstall

```powershell
pip uninstall -y pixelforge
```

### 1.4 What just happened on disk?

```
.venv\Lib\site-packages\pixelforge\
├── __init__.py                                ← pure-Python facade
├── _pixelforge.cp313-win_amd64.pyd            ← pybind11 binding
├── libpixelforge.dll                          ← runtime, bundled by delvewheel/auditwheel
└── plugins\
    ├── pf_blur.dll
    ├── pf_edge.dll
    ├── pf_grayscale.dll
    ├── pf_invert.dll
    └── pf_sharpen.dll
```

The wheel is **standalone** — no separate DLL install needed. The
`__init__.py` calls `os.add_dll_directory()` to make the bundled DLL
findable on Windows (see Phase 6 docs).

---

## 2. Windows — NSIS installer

The file `pixelforge-0.1.0-Windows-AMD64.exe` (≈2 MB) is a real
NSIS installer with a wizard, uninstaller, Start Menu folder, and
Desktop shortcut.

### 2.1 GUI install

Double-click the `.exe`. The wizard offers three components:

| Component | What goes in |
|-----------|--------------|
| **Runtime** (required) | `bin\pixelforge.exe`, `bin\libpixelforge.dll` |
| **SDK**               | `include\pixelforge\*.h`, `lib\libpixelforge.dll.a`, `lib\cmake\PixelForge\*` |
| **Plugins**           | `lib\pixelforge\plugins\pf_*.dll` |

Default install path: `C:\Program Files\PixelForge 0.1\`.

The installer **also** does the following automatically:

- Adds `<install-dir>\bin` to the **current user's** `PATH` (via
  `[Environment]::SetEnvironmentVariable` — no length limit, no
  reboot required).
- Creates a Desktop shortcut **`PixelForge CLI`** that opens a
  console with `pixelforge --help` already run.
- Creates a Start Menu folder **`PixelForge`** containing the same
  CLI shortcut plus an `Uninstall` shortcut.
- Registers a clean entry in **Settings → Apps → Installed apps**
  (and the legacy *Add or Remove Programs* control panel) named
  **PixelForge 0.1.0** with publisher, version, and uninstall command.

> **The `PATH` change only takes effect in *new* shells.** Close
> your existing PowerShell / cmd window and open a fresh one before
> running `pixelforge --version`.

### 2.2 Silent install (CI / scripted)

```powershell
# Default location, all components:
.\pixelforge-0.1.0-Windows-AMD64.exe /S

# Custom install dir:
.\pixelforge-0.1.0-Windows-AMD64.exe /S /D=C:\Tools\PixelForge
```

`/D=` must be the **last** argument and **must not** be quoted (NSIS
quirk).

### 2.3 Verify

```powershell
& "C:\Program Files\PixelForge\bin\pixelforge.exe" --version
# 0.1.0

& "C:\Program Files\PixelForge\bin\pixelforge.exe" list-filters
# Plugins in C:\Program Files\PixelForge\lib\pixelforge\plugins:
#   blur 1.0.0  3x3 box blur
#   …
```

### 2.4 Uninstall

Use **Settings → Apps → PixelForge → Uninstall**, or run
`C:\Program Files\PixelForge\Uninstall.exe`.

---

## 3. Windows — portable ZIP

`pixelforge-0.1.0-Windows-AMD64.zip` (≈5.5 MB) is the same payload as
the NSIS installer but with no installer wrapper. Just unzip and run.

```powershell
Expand-Archive .\pixelforge-0.1.0-Windows-AMD64.zip -DestinationPath C:\Tools
& "C:\Tools\pixelforge-0.1.0-Windows-AMD64\bin\pixelforge.exe" --version
```

Use this when you don't have admin rights on the target machine.

---

## 4. Linux — DEB

For Ubuntu / Debian / derivatives.

```bash
sudo apt install ./pixelforge_0.1.0_amd64.deb
# or, on older systems:
sudo dpkg -i pixelforge_0.1.0_amd64.deb && sudo apt-get install -f
```

Installed layout (Filesystem Hierarchy Standard):

```
/usr/bin/pixelforge
/usr/lib/x86_64-linux-gnu/libpixelforge.so.0.1.0
/usr/lib/x86_64-linux-gnu/libpixelforge.so.0      → libpixelforge.so.0.1.0
/usr/include/pixelforge/*.h
/usr/lib/x86_64-linux-gnu/cmake/PixelForge/*.cmake
/usr/lib/x86_64-linux-gnu/pixelforge/plugins/pf_*.so
/usr/share/doc/pixelforge/{LICENSE,README.md}
```

Verify:

```bash
pixelforge --version            # 0.1.0
pixelforge list-filters
dpkg -L pixelforge | head       # see every file
```

Uninstall:

```bash
sudo apt remove pixelforge
```

---

## 5. Linux — RPM

For Fedora / RHEL / CentOS / openSUSE.

```bash
sudo dnf install ./pixelforge-0.1.0-1.x86_64.rpm
# or:
sudo rpm -ivh pixelforge-0.1.0-1.x86_64.rpm
```

Layout matches the DEB (FHS). Uninstall:

```bash
sudo dnf remove pixelforge
```

---

## 6. Linux — Tarball

`pixelforge-0.1.0-Linux-x86_64.tar.gz` is the relocatable archive — it
unpacks anywhere and never touches `/usr`.

```bash
tar -xzf pixelforge-0.1.0-Linux-x86_64.tar.gz -C /opt
export PATH="/opt/pixelforge-0.1.0-Linux-x86_64/bin:$PATH"
export LD_LIBRARY_PATH="/opt/pixelforge-0.1.0-Linux-x86_64/lib:$LD_LIBRARY_PATH"
pixelforge --version
```

Use this on systems where you don't have root (HPC clusters, shared
boxes), or for side-by-side version testing.

---

## 7. macOS — PKG

```bash
sudo installer -pkg pixelforge-0.1.0-Darwin.pkg -target /
```

Installed under `/usr/local/{bin,lib,include}` by default.

Uninstall (no native uninstaller — macOS pkg system has none):

```bash
# List what was installed:
pkgutil --files com.pixelforge.pixelforge
# Then remove those files manually, plus the package receipt:
sudo pkgutil --forget com.pixelforge.pixelforge
```

---

## 8. End-user verification checklist

After installing by *any* of the above, confirm:

```bash
# 1. CLI is on PATH and reports the version:
pixelforge --version

# 2. Plugins are discoverable:
pixelforge list-filters       # should print 5 filters

# 3. End-to-end pipeline works on a real image:
pixelforge run --in some.png --out out.png --filters grayscale,blur
```

If step 2 fails ("Plugins in <dir>: (none)") your install is missing
the **Plugins** component. Re-run the installer and tick that
component, or copy the `pf_*.dll`/`.so` files into the directory
shown in the message.

---

Next: [Guide 05 — Generating packages](05-generating-packages.md).
