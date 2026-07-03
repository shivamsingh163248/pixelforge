# Phase 4 — CLI Executable

| Field | Value |
|---|---|
| Goal | Ship a user-facing `pixelforge.exe` that drives the runtime + plugins from the command line. End users get a working tool the moment they install. |
| Status | ✅ Complete |
| Artifacts | `pixelforge.exe` (3.7 MB) |
| Tests added | 3 — total 31/31 |

---

## 1. What the CLI does

```text
pixelforge — image processing tool

Usage:
  pixelforge <input> -f FILTER [-f FILTER ...] -o <output> [--plugin-dir DIR]
  pixelforge list-filters [--plugin-dir DIR]
  pixelforge --version
  pixelforge --help
```

Examples:

```powershell
pixelforge in.png -f grayscale -f invert -o out.png
pixelforge list-filters
pixelforge --version    # → 0.1.0
```

---

## 2. Source: [`src/cli/main.cpp`](../../src/cli/main.cpp)

A single ~250-line file. Responsibilities:

| Section | Lines | What it does |
|---|---|---|
| `exe_path()` | ~10 | Self-locate via `GetModuleFileNameW` / `/proc/self/exe`. |
| `default_plugin_dir()` | ~15 | Walk candidate dirs relative to the exe. |
| Argument parsing | ~80 | Hand-rolled; turns `argv` into an `Args` struct. |
| Subcommand dispatch | ~10 | `--version`, `--help`, `list-filters`, run pipeline. |
| `cmd_list_filters()` | ~25 | Pretty-print discovered plugins. |
| `cmd_run_pipeline()` | ~50 | Build pipeline, load image, run, save. |
| `main()` | ~10 | Catch-all. |

### Why hand-rolled args, not `argparse`/CLI11?
- Demonstrates "what the libraries are doing for you."
- Zero extra dependencies → trivial cross-platform builds.
- The total parser is ~80 lines; not worth a dependency for this scale.

---

## 3. Self-locating the executable

The CLI **must** find its plugins without the user setting any env var.
Pattern:

```cpp
fs::path exe_path() {
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::wstring(buf, n);
#elif defined(__APPLE__)
    char buf[PATH_MAX]; uint32_t sz = sizeof(buf);
    _NSGetExecutablePath(buf, &sz);
    return std::filesystem::canonical(buf);
#else
    return std::filesystem::canonical("/proc/self/exe");
#endif
}
```

Then `default_plugin_dir()` probes, in order:

1. `<exe_dir>/../lib/pixelforge/plugins`  ← installed layout
2. `<exe_dir>/../plugins`                  ← build-tree layout
3. `<exe_dir>/plugins`                     ← portable bundle

The first one that exists wins. The user can override with `--plugin-dir`.

This is what makes the same `pixelforge.exe` binary work in three layouts:
- Build tree: `build/win-mingw/bin/pixelforge.exe` finds
  `build/win-mingw/plugins/`.
- Installed: `<prefix>/bin/pixelforge.exe` finds
  `<prefix>/lib/pixelforge/plugins/`.
- Portable ZIP: `pixelforge/bin/pixelforge.exe` finds
  `pixelforge/plugins/`.

---

## 4. Pipeline construction with proper plugin lifetime

```cpp
// Inside cmd_run_pipeline:
auto plugins = pixelforge::load_plugins_from_directory(dir);
pixelforge::Pipeline pipe;
for (const auto& spec : a.filters) {
    auto it = std::find_if(plugins.begin(), plugins.end(),
        [&](const Plugin& p) { return p.name() == spec.name; });
    if (it == plugins.end()) die("filter not found: " + spec.name);

    auto holder = std::make_shared<pixelforge::Plugin>(std::move(*it));
    plugins.erase(it);

    pipe.add(spec.name,
             [holder, params = spec.params](const Image& img) {
                 return holder->apply(img, params);
             });
}
auto in  = pixelforge::load_image(a.input);
auto out = pipe.run(in);
pixelforge::save_image(out, a.output);
```

**The critical move**: `auto holder = std::make_shared<Plugin>(std::move(*it));`.
The lambda captures `holder` by value, so the `shared_ptr` survives until
the pipeline does. Without this, the local `Plugin` would destruct at
end of loop iteration — `dlclose` runs — and the next call to that
filter crashes.

---

## 5. RPATH for the installed binary

[`src/cli/CMakeLists.txt`](../../src/cli/CMakeLists.txt):

```cmake
if(UNIX AND NOT APPLE)
    set_target_properties(pixelforge_cli PROPERTIES
        INSTALL_RPATH "$ORIGIN/../lib"
        BUILD_WITH_INSTALL_RPATH TRUE)
elseif(APPLE)
    set_target_properties(pixelforge_cli PROPERTIES
        INSTALL_RPATH "@loader_path/../lib"
        BUILD_WITH_INSTALL_RPATH TRUE)
endif()
```

`$ORIGIN/../lib` is a *literal* string baked into the ELF file. At load
time the dynamic linker (`ld-linux.so`) substitutes `$ORIGIN` with the
directory the executable lives in. So `<prefix>/bin/pixelforge` finds
`<prefix>/lib/libpixelforge.so` automatically, no `LD_LIBRARY_PATH`.

On Windows there's no equivalent of RPATH — Windows looks in:
1. The directory of the `.exe`.
2. `%PATH%`.

So our installer puts `pixelforge.exe` and `libpixelforge.dll` both in
`<prefix>\bin\`. Done.

---

## 6. Tests

### `Cli.HelpAndVersion`
```cmake
add_test(NAME Cli.HelpAndVersion
         COMMAND $<TARGET_FILE:pixelforge_cli> --version)
set_tests_properties(Cli.HelpAndVersion PROPERTIES
    PASS_REGULAR_EXPRESSION "^[0-9]+\\.[0-9]+\\.[0-9]+")
```

### `Cli.ListFilters`
```cmake
add_test(NAME Cli.ListFilters
         COMMAND $<TARGET_FILE:pixelforge_cli> list-filters
                 --plugin-dir "${CMAKE_BINARY_DIR}/plugins")
set_tests_properties(Cli.ListFilters PROPERTIES
    PASS_REGULAR_EXPRESSION "grayscale")
```

### `Cli.PipelineEndToEnd` — the interesting one
End-to-end test driven by a CMake script ([`tests/cli_smoke.cmake`](../../tests/cli_smoke.cmake)):

```cmake
add_test(NAME Cli.PipelineEndToEnd
         COMMAND ${CMAKE_COMMAND}
            -DPIXELFORGE_CLI=$<TARGET_FILE:pixelforge_cli>
            -DPLUGIN_DIR=${CMAKE_BINARY_DIR}/plugins
            -DTMP_DIR=${CMAKE_BINARY_DIR}/test_tmp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_smoke.cmake)
```

`cli_smoke.cmake` uses Python to write a 4×4 BMP, invokes
`pixelforge in.bmp -f grayscale -f invert -o out.png`, and asserts the
output exists and has expected dimensions.

This pattern — `cmake -P script.cmake` for end-to-end — is portable and
needs no extra runner.

---

## 7. Lessons

### 7.1 `for (const fs::path candidate : list)` triggers `-Wrange-loop-construct`
List initializer elements are temporaries. `const fs::path&` binds correctly:
```cpp
for (const fs::path& candidate : { ... }) { … }
```

### 7.2 CMake script-mode tests
`COMMAND ${CMAKE_COMMAND} -P script.cmake` is the cleanest portable
end-to-end harness. Pass context with `-D KEY=VALUE` *before* `-P`.

---

## 8. Verification

```powershell
cmake --build --preset win-mingw
ctest --preset win-mingw -R "^Cli\." --output-on-failure
# 3/3 tests pass; 31/31 total
```

---

## 9. Next: [Phase 5 — Python bindings](phase-05-python-bindings.md)
