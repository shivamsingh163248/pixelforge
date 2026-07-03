# Phase 7 — Native Installers (NSIS + ZIP on Windows; DEB/RPM/TGZ on Linux)

| Field | Value |
|---|---|
| Goal | Produce native, system-level installers — the same way Photoshop, Postman, or Inkscape ship. Component-based: end users pick Runtime / SDK / Plugins. |
| Status | ✅ Complete |
| Artifacts | `pixelforge-0.1.0-Windows-AMD64.exe` (NSIS, 2.1 MB), `pixelforge-0.1.0-Windows-AMD64.zip` (5.5 MB) |
| Tests added | manual end-to-end (silent install + downstream `find_package`) |

---

## 1. The CPack model

CPack reads the **same** `install(TARGETS …)` rules CMake already uses
for `cmake --install`, plus a small per-generator config block, and
produces an installer.

We never write a `.wxs` file by hand or maintain a `debian/` directory —
CPack does it.

```
       cmake --install         (regular dev install to a prefix)
              │
              │ same install(TARGETS) rules
              ▼
         CPack
       ┌──┴──┬──────┬──────┬──────┬──────────┐
       ▼     ▼      ▼      ▼      ▼          ▼
      ZIP  NSIS    DEB    RPM    TGZ    productbuild
     (Win) (Win) (Linux)(Linux)(any)    (macOS)
```

---

## 2. The components

We split installs into three named **components** so end users can pick:

| Component | Contains | Required? |
|---|---|---|
| **Runtime** | `pixelforge.exe`, `libpixelforge.dll`, README, LICENSE | ✅ yes |
| **SDK** | Headers, import lib (`.dll.a` / `.lib`), CMake config | optional, depends Runtime |
| **Plugins** | All five `pf_*.dll` | optional, depends Runtime |

In each per-target `CMakeLists.txt`:

```cmake
install(TARGETS pixelforge
    EXPORT  PixelForgeTargets
    RUNTIME DESTINATION bin       COMPONENT Runtime  # .dll on Windows
    LIBRARY DESTINATION lib       COMPONENT Runtime  # .so on Linux
    ARCHIVE DESTINATION lib       COMPONENT SDK)     # .lib / .dll.a
```

```cmake
install(TARGETS pixelforge_cli
    RUNTIME DESTINATION bin       COMPONENT Runtime)
```

```cmake
install(TARGETS pf_${name}
    LIBRARY DESTINATION lib/pixelforge/plugins  COMPONENT Plugins
    RUNTIME DESTINATION lib/pixelforge/plugins  COMPONENT Plugins)
```

---

## 3. The SDK config — `find_package(PixelForge)` works

Top-level `CMakeLists.txt`:

```cmake
install(EXPORT PixelForgeTargets
    FILE        PixelForgeTargets.cmake
    NAMESPACE   pixelforge::
    DESTINATION lib/cmake/PixelForge
    COMPONENT   SDK)

configure_package_config_file(
    "${CMAKE_BINARY_DIR}/PixelForgeConfig.cmake.in"
    "${CMAKE_BINARY_DIR}/PixelForgeConfig.cmake"
    INSTALL_DESTINATION lib/cmake/PixelForge)

write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/PixelForgeConfigVersion.cmake"
    VERSION       ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    "${CMAKE_BINARY_DIR}/PixelForgeConfig.cmake"
    "${CMAKE_BINARY_DIR}/PixelForgeConfigVersion.cmake"
    DESTINATION lib/cmake/PixelForge
    COMPONENT   SDK)
```

After install, downstream consumers can:

```cmake
# Some completely unrelated project
cmake_minimum_required(VERSION 3.25)
project(my_app CXX)
find_package(PixelForge REQUIRED CONFIG)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE pixelforge::pixelforge)
```

This is the same API you use when you write
`find_package(OpenCV REQUIRED)`. PixelForge is now a real consumable SDK.

---

## 4. The CPack config

```cmake
set(CPACK_PACKAGE_NAME           "PixelForge")
set(CPACK_PACKAGE_VENDOR         "PixelForge Authors")
set(CPACK_PACKAGE_VERSION         ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE  "${CMAKE_SOURCE_DIR}/LICENSE")

set(CPACK_COMPONENTS_ALL                Runtime SDK Plugins)
set(CPACK_COMPONENT_RUNTIME_REQUIRED    TRUE)
set(CPACK_COMPONENT_SDK_DEPENDS         Runtime)
set(CPACK_COMPONENT_PLUGINS_DEPENDS     Runtime)

if(WIN32)
    set(CPACK_GENERATOR                 "ZIP;NSIS")
    set(CPACK_NSIS_MODIFY_PATH          ON)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
elseif(APPLE)
    set(CPACK_GENERATOR                 "TGZ;productbuild")
else()
    set(CPACK_GENERATOR                 "TGZ;DEB;RPM")
    set(CPACK_DEB_COMPONENT_INSTALL     ON)        # one .deb per component
    set(CPACK_RPM_COMPONENT_INSTALL     ON)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS  ON)        # auto-detect .so deps
endif()

include(CPack)
```

### Per-OS choices

| Platform | Generators | Why |
|---|---|---|
| Windows | `ZIP` + `NSIS` | ZIP for the "I just want to extract it" crowd; NSIS for the GUI installer with optional PATH and uninstaller. |
| Linux | `TGZ` + `DEB` + `RPM` | TGZ for any-distro; `.deb` for Debian/Ubuntu users; `.rpm` for Fedora/RHEL. `CPACK_DEB_COMPONENT_INSTALL` makes one `.deb` per component (`pixelforge-runtime_0.1.0_amd64.deb`, `pixelforge-sdk_…`, `pixelforge-plugins_…`). |
| macOS | `TGZ` + `productbuild` | productbuild → `.pkg` (Apple's native installer format). |

---

## 5. The NSIS installer in action

```powershell
# In build/win-mingw/, after a successful build:
$env:PATH = "C:\Program Files (x86)\NSIS;$env:PATH"
cpack -B ..\..\dist
# CPack: Create package using ZIP
# CPack: - package: dist/pixelforge-0.1.0-Windows-AMD64.zip generated.
# CPack: Create package using NSIS
# CPack:   - Install component: Runtime
# CPack:   - Install component: SDK
# CPack:   - Install component: Plugins
# CPack: - package: dist/pixelforge-0.1.0-Windows-AMD64.exe generated.
```

### Silent install for testing
```powershell
.\dist\pixelforge-0.1.0-Windows-AMD64.exe /S /D=C:\test\prefix
```

`/S` = silent, `/D=` = destination dir (must be the *last* arg, no quotes).

### Resulting layout
```
C:\test\prefix\
├── bin\
│   ├── pixelforge.exe              ← CLI
│   └── libpixelforge.dll           ← runtime
├── include\pixelforge\
│   ├── colorspace.h, image.h, io.h, kernel.h, pipeline.h,
│   ├── plugin.h, plugin_api.h, export.h, version.h
├── lib\
│   ├── libpixelforge_core.a        ← static core (SDK)
│   ├── libpixelforge.dll.a         ← import lib (SDK)
│   ├── cmake\PixelForge\
│   │   ├── PixelForgeConfig.cmake
│   │   ├── PixelForgeConfigVersion.cmake
│   │   ├── PixelForgeTargets.cmake
│   │   └── PixelForgeTargets-relwithdebinfo.cmake
│   └── pixelforge\plugins\
│       ├── pf_blur.dll, pf_edge.dll, pf_grayscale.dll,
│       ├── pf_invert.dll, pf_sharpen.dll
├── share\pixelforge\
│   ├── LICENSE
│   └── README.md
└── Uninstall.exe
```

This is **standard FHS** (`bin/`, `lib/`, `include/`, `share/`). The only
Windows-specific piece is `Uninstall.exe`.

### Verify it works
```powershell
& 'C:\test\prefix\bin\pixelforge.exe' --version
# 0.1.0
& 'C:\test\prefix\bin\pixelforge.exe' list-filters
# Plugins in C:\test\prefix\lib\pixelforge\plugins:
#   blur       1.0.0  3x3 box blur
#   edge       1.0.0  3x3 Laplacian edge detector
#   grayscale  1.0.0  Convert image to single-channel BT.601 luma
#   invert     1.0.0  Invert color channels (alpha preserved)
#   sharpen    1.0.0  3x3 unsharp mask
```

The CLI's `default_plugin_dir()` (Phase 4) was extended to probe
`<exe_dir>/../lib/pixelforge/plugins` first, which is exactly the
installer's layout.

---

## 6. SKBUILD-aware gating

Every `install(TARGETS ...)` block is wrapped in:

```cmake
if(NOT DEFINED SKBUILD_PROJECT_NAME)
    install(TARGETS … EXPORT PixelForgeTargets …)
endif()
```

`SKBUILD_PROJECT_NAME` is set only when scikit-build-core drives the
build. So:
- **Wheel build** (Phase 6): only the wheel-specific install rules
  (Python facade + bundled DLLs) run.
- **Native install** (Phase 7): only the SDK/Runtime/Plugins install
  rules run.
- **Dev build** (`cmake --build`): nothing installs (no `cmake --install`
  invoked).

---

## 7. The exported-warnings trap

`install(EXPORT)` requires every target an exported target *transitively*
depends on (even `PRIVATE`-linked) to also be in some export set, or it
errors out:

```
install(EXPORT "PixelForgeTargets" ...) includes target "pixelforge_core"
which requires target "pixelforge_warnings" that is not in any export set.
```

Fix in [`cmake/CompilerWarnings.cmake`](../../cmake/CompilerWarnings.cmake):

```cmake
add_library(pixelforge_warnings INTERFACE)
add_library(pixelforge::warnings ALIAS pixelforge_warnings)

if(NOT DEFINED SKBUILD_PROJECT_NAME)
    install(TARGETS pixelforge_warnings EXPORT PixelForgeTargets)
endif()
```

The flags don't propagate to downstream consumers (they don't link
`pixelforge::warnings` directly), but the export set is satisfied.

---

## 8. Verifying the SDK end-to-end

We built a tiny consumer project at `build/sdk-test/`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(use_pixelforge CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(PixelForge REQUIRED CONFIG)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE pixelforge::pixelforge)
```

```cpp
#include <pixelforge/version.h>
#include <pixelforge/image.h>
#include <iostream>

int main() {
    pixelforge::Image img(8, 4, pixelforge::PixelFormat::RGB);
    std::cout << "PixelForge SDK consumer v" << pixelforge::version_string
              << "  image=" << img.width() << "x" << img.height() << "\n";
}
```

```powershell
cmake -G Ninja -B build -DCMAKE_PREFIX_PATH=C:\test\prefix
cmake --build build
.\build\consumer.exe
# PixelForge SDK consumer v0.1.0  image=8x4
```

This is the same flow OpenCV / Boost / Qt users go through. PixelForge is
now a peer of those libraries from a consumability standpoint.

---

## 9. Lessons

### 9.1 NSIS is not on PATH after `winget install`
`makensis.exe` lives in `C:\Program Files (x86)\NSIS`. Prepend before
`cpack`.

### 9.2 The CLI must search the install layout
Adding `<exe_dir>/../lib/pixelforge/plugins` to `default_plugin_dir()`'s
candidate list was a one-line change that made the installer "just work."

### 9.3 Downstream MinGW consumers still need MinGW DLLs
Our consumer above needs `libgcc_s_seh-1.dll` + `libstdc++-6.dll` on PATH
unless it builds with `-static-libgcc -static-libstdc++` itself. That's a
*downstream* concern; it's not our installer's problem.

### 9.4 CPack's `_CPack_Packages/` working dir
CPack creates a working dir in the output dir. If you ship `dist/`
in CI artifacts, exclude it.

---

## 10. Verification

```powershell
ctest --preset win-mingw            # 32/32
cd build\win-mingw
cpack -B ..\..\dist                 # builds .exe + .zip
..\..\dist\pixelforge-0.1.0-Windows-AMD64.exe /S /D=C:\test\prefix
& C:\test\prefix\bin\pixelforge.exe list-filters
# 5 filters listed; Phase 7 done.
```

---

## 11. Next: [Phase 8 — Multi-arch + multi-compiler](phase-08-multi-arch.md) (planned)
