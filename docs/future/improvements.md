# Future Improvements

This document is the **roadmap** for what to add after Phase 9. Each item
is concrete enough that a contributor could pick it up and execute,
with rough effort estimates expressed as "phases" of similar weight to
the existing ones.

---

## A. Functionality

### A.1 More image formats
- **JPEG via libjpeg-turbo** — current stb_image JPEG decoder is slow.
  Effort: ~½ phase. Introduces vcpkg as a dependency manager.
- **WebP, AVIF** — modern formats. Each: ~½ phase via vcpkg.
- **TIFF** for scientific imagery: ~½ phase.

### A.2 Higher-bit-depth pixels
Today everything is `uint8_t`. Add:
- `PixelFormat::Gray16`, `RGBA16`, `RGBAf32`.
- Generic `Image<T>` template *or* a tagged-union runtime polymorphic
  type. The latter keeps the C ABI of plugins simple.
- Effort: ~1 phase (ripples through every layer).

### A.3 More filters
- Gaussian blur (separable kernel)
- Bilateral filter
- Histogram equalization
- Color-space conversions (RGB ↔ HSV ↔ Lab)
- Resize / crop / rotate (true geometric transforms)

Each: 1–2 days.

### A.4 GPU backend
- Add a `pixelforge::gpu::` namespace using either Vulkan compute or
  CUDA.
- Plugin ABI grows a `device` field; CPU-only plugins ignore it.
- Effort: ~2 phases. Big learning value (ICDs, dispatch dispatch dispatch).

---

## B. Build / packaging

### B.1 Conan and vcpkg
- Provide `conanfile.py` + `vcpkg.json` so PixelForge can be consumed by
  either ecosystem.
- Effort: ~½ phase each.

### B.2 Reproducible builds
- Pin every FetchContent commit (already partly done).
- Strip embedded timestamps from binaries (`SOURCE_DATE_EPOCH`,
  `-Wl,--no-insert-timestamp`).
- Hash the wheel contents in CI; assert byte-for-byte identical builds
  across reruns.

### B.3 Universal2 macOS wheels
- arm64 + x86_64 in one wheel via `lipo`.
- cibuildwheel handles this; just add `CIBW_ARCHS_MACOS = "universal2"`.

### B.4 Static / shared / static-with-PIC matrix
- Today everything is one configuration (shared runtime + static core).
- Add a `BUILD_SHARED_LIBS` toggle that turns the runtime into a static
  archive. Useful for embedding PixelForge into a single-binary tool.

---

## C. Quality

### C.1 Sanitizer coverage
- Already planned in Phase 9 nightly. Expand to:
  - ThreadSanitizer (relevant once we add parallel filters).
  - MemorySanitizer (Linux/Clang only; very strict).

### C.2 Fuzzing
- libFuzzer harness around `load_image()` — feed random bytes, assert no
  crash.
- Run continuously via OSS-Fuzz.

### C.3 Static analysis
- `clang-tidy`, `cppcheck`, MSVC `/analyze`.
- Add to CI as **advisory** first, then **blocking** once the warning
  count is zero.

### C.4 Property-based tests
- For `convolve`: kernel sum = 1 ⇒ output mean = input mean (roughly).
- For `to_grayscale`: a uniform-grey input maps to itself.
- Use Hypothesis on the Python side; rapidcheck on the C++ side.

---

## D. Documentation

### D.1 API reference site
- Doxygen for C++ API → publish to GitHub Pages.
- Sphinx + autodoc for the Python API.
- One unified site: `pixelforge.io` style.

### D.2 Tutorials
- "Write your first plugin in 30 lines" — with a screenshot of the
  before/after image.
- "Embed PixelForge in your CMake app" — the Phase 7 SDK consumer doc as
  a standalone tutorial.
- "Build PixelForge from source on every OS" — the per-OS commands.

### D.3 Architecture decision records (ADRs)
- One short markdown doc per major design choice (e.g.
  `0001-c-abi-for-plugins.md`, `0002-pybind11-vs-nanobind.md`).
- Forces us to record *why*, not just *what*.

---

## E. Distribution

### E.1 PyPI
- Phase 9 publishes wheels on tag.
- Set up a long-lived API token in repo secrets.
- Auto-bump `pyproject.toml` version on tag (`hatch-vcs` style).

### E.2 Linux distro packaging
- Ship `.deb` to a PPA (Launchpad) for Ubuntu users.
- Ship `.rpm` to COPR (Fedora).
- Both have CI integrations.

### E.3 Homebrew (macOS)
- Once macOS is in the matrix, add a Homebrew formula:
  `brew install pixelforge`.

### E.4 winget / Chocolatey (Windows)
- Submit the NSIS installer to the winget repo.
- Auto-PR via [`vedantmgoyal2009/winget-releaser`](https://github.com/vedantmgoyal2009/winget-releaser).

---

## F. Performance

### F.1 SIMD
- Vectorised inner loops for blur, sharpen, edge.
- Use `xsimd` for portable SSE2/AVX2/NEON.
- Expected speedup: 4–8× on filters dominated by per-pixel arithmetic.

### F.2 Parallel pipelines
- `Pipeline::run_parallel` that splits the image into tiles and processes
  them on a thread pool.
- Plugins must remain re-entrant (already are; they're pure functions).

### F.3 Pipeline fusion
- A "compiler" that detects e.g. `grayscale → invert` can be fused into
  a single pass over the image.
- This is the actual mini-compiler we deferred from the original choice.

---

## G. Stretch goals (educational)

### G.1 GUI
- Qt 6-based image viewer that loads a PixelForge pipeline live.
- Demonstrates linking against PixelForge from a C++ application.

### G.2 WASM build
- Use Emscripten to compile the runtime + a pinned set of plugins to
  WebAssembly.
- A 200-line static-page demo: drop an image, pick filters, see result.
- Demonstrates the same CMake codebase building for an exotic target.

### G.3 ABI compatibility report
- A CI job that compares the public symbols of the new `libpixelforge.so`
  against the previous tag using `abi-compliance-checker`.
- Fail the build if anything *removed* without a major version bump.

---

## How to pick what's next

If your goal is **interview prep / portfolio**: pick D (docs site) and
G (WASM demo) — they're visually impressive and easy to demo.

If your goal is **production-grade SDK**: pick C (sanitisers + fuzzing),
B.2 (reproducible builds), and E (distro packaging) — those are what
real shipping software needs.

If your goal is **deeper learning**: pick A.4 (GPU backend) and F.1
(SIMD) — they push you into hardware-aware programming.
