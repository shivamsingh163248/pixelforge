# Guide 03 — Running PixelForge

Three ways to use what you just built:

1. [Run the CLI](#1-run-the-cli)
2. [Use the Python module](#2-use-the-python-module)
3. [Link against the C++ SDK from your own project](#3-link-against-the-c-sdk)

---

## 1. Run the CLI

The CLI lives at `build\<preset>\bin\pixelforge.exe` (Windows) or
`build/<preset>/bin/pixelforge` (Linux/macOS).

### 1.1 Built-in subcommands

```powershell
PS> .\build\win-mingw\bin\pixelforge.exe --help
PixelForge 0.1.0 — image processing CLI
Usage:
  pixelforge --version
  pixelforge --help
  pixelforge list-filters [--plugin-dir DIR]
  pixelforge run --in IMG --out IMG --filters f1,f2,... [--plugin-dir DIR]
```

### 1.2 List the available filters (= discovered plugins)

```powershell
PS> .\build\win-mingw\bin\pixelforge.exe list-filters
Plugins in C:\Users\you\pixelforge\build\win-mingw\plugins:
  blur       1.0.0  3x3 box blur
  edge       1.0.0  3x3 Laplacian edge detector
  grayscale  1.0.0  Convert image to single-channel BT.601 luma
  invert     1.0.0  Invert color channels (alpha preserved)
  sharpen    1.0.0  3x3 unsharp mask
```

The CLI auto-discovers plugins by walking up from its own location and
finding the nearest `plugins/` directory. Override with `--plugin-dir`.

### 1.3 Run a real pipeline

```powershell
# Apply grayscale, then blur, then save:
PS> .\build\win-mingw\bin\pixelforge.exe run `
        --in  test.png `
        --out test_out.png `
        --filters grayscale,blur
Loaded 5 plugins from "C:\...\plugins"
Pipeline: grayscale -> blur
Read    : test.png  (640x480, 4ch)
Wrote   : test_out.png  (640x480, 1ch)
```

Supported input/output formats: PNG, JPG, BMP, TGA, GIF (read);
PNG, JPG, BMP, TGA (write). All via the vendored `stb_image` /
`stb_image_write` (Phase 2).

### 1.4 What if the DLL is missing?

If you copy `pixelforge.exe` somewhere else, Windows will say
`The code execution cannot proceed because libpixelforge.dll was not
found`. Either:

- Copy `libpixelforge.dll` next to it, or
- Add `build\win-mingw\bin\` to your `PATH`, or
- Install via the NSIS installer (Guide 04) which puts the DLL in
  the correct location automatically.

---

## 2. Use the Python module

There are two equivalent ways to make Python find the freshly-built
extension:

### 2.1 Option A — install the wheel (recommended)

See [Guide 05](05-generating-packages.md#wheel) to build the wheel,
then [Guide 04](04-installing.md#wheel) to `pip install` it.

After install:

```python
import pixelforge as pf
import numpy as np

print(pf.version_string)        # "0.1.0"
print(pf.list_filters())        # ['blur', 'edge', 'grayscale', 'invert', 'sharpen']

# Read → process → write
img = pf.imread("test.png")     # numpy.ndarray, shape (H, W, 4), dtype uint8
out = pf.apply(img, ["grayscale", "blur"])
pf.imwrite("test_out.png", out)

# Or work with NumPy arrays directly:
canvas = np.zeros((128, 128, 4), dtype=np.uint8)
canvas[:, :, 0] = 255           # solid red
gray = pf.apply(canvas, ["grayscale"])
print(gray.shape, gray.dtype)   # (128, 128, 1) uint8
```

### 2.2 Option B — point Python at the build directory

Without installing, you can run from the build tree directly:

```powershell
PS> $env:PYTHONPATH = "C:\Users\you\pixelforge\python;C:\Users\you\pixelforge\build\win-mingw\python_modules"
PS> python -c "import pixelforge as pf; print(pf.version_string)"
0.1.0
```

(On Windows, the `.pyd` also depends on `libpixelforge.dll`. The pure-
Python facade calls `os.add_dll_directory()` in `__init__.py` to make
sure the DLL is findable — see Phase 5/6 docs for details.)

### 2.3 Verify zero-copy NumPy

```python
import pixelforge as pf, numpy as np

a = np.zeros((4, 4, 4), dtype=np.uint8)
img = pf.from_numpy(a)
print(img.shape, img.dtype)            # (4, 4, 4) uint8
print(np.shares_memory(pf.to_numpy(img), a))   # True  (no copy)
```

---

## 3. Link against the C++ SDK

This is what an external C++ project does after installing PixelForge
(via the NSIS installer, the ZIP, or `cmake --install`).

### 3.1 Minimal `CMakeLists.txt` for a downstream consumer

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyApp CXX)
set(CMAKE_CXX_STANDARD 20)

find_package(PixelForge REQUIRED)

add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE pixelforge::pixelforge)
```

### 3.2 Minimal `main.cpp`

```cpp
#include <pixelforge/version.h>
#include <pixelforge/image.h>
#include <pixelforge/pipeline.h>
#include <iostream>

int main() {
    std::cout << "PixelForge SDK " << pixelforge::version_string()
              << "  pipeline_size=" << pixelforge::Pipeline{}.size() << '\n';
    pixelforge::Image img(8, 4, pixelforge::PixelFormat::RGBA8);
    std::cout << "image=" << img.width() << "x" << img.height() << '\n';
}
```

### 3.3 Configure & build the consumer

```powershell
cmake -B build -G Ninja -DCMAKE_PREFIX_PATH="C:\Program Files\PixelForge"
cmake --build build
.\build\myapp.exe
```

Expected output:

```
PixelForge SDK 0.1.0  pipeline_size=0
image=8x4
```

The `find_package(PixelForge)` call works because the install tree
contains `lib\cmake\PixelForge\PixelForgeConfig.cmake` (generated in
Phase 7). It exports a single namespaced target `pixelforge::pixelforge`
that brings the include paths and the runtime DLL link in one shot.

---

## 4. Where do I run from?

| You want to…              | Run from              | Notes |
|---------------------------|-----------------------|-------|
| Quick smoke test          | `build\<preset>\bin\` | DLL + plugins are in expected relative locations |
| Demo to someone           | Installed location    | Use the NSIS installer (Guide 04); end-user gets a clean Start Menu entry |
| Use from a script         | A venv with the wheel | `pip install pixelforge…whl` then `import pixelforge` |
| Embed in a C++ app        | Anywhere              | The consumer project pulls the SDK via `find_package` |

---

Next: [Guide 04 — Installing PixelForge](04-installing.md).
