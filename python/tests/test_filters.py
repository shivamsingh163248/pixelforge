"""End-to-end pytest suite for the PixelForge Python bindings.

The tests treat the `pixelforge` package as a black box and verify that
the data types, shapes, and pixel values round-trip correctly between
NumPy and the C++ runtime.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

import numpy as np
import pytest

# Allow running directly from the build tree: PIXELFORGE_PY_PATH points
# at the directory containing _pixelforge.<pyd|so> + libpixelforge.dll.
_extra = os.environ.get("PIXELFORGE_PY_PATH")
if _extra and _extra not in sys.path:
    sys.path.insert(0, _extra)
_pkg_root = os.environ.get("PIXELFORGE_PKG_ROOT")
if _pkg_root and _pkg_root not in sys.path:
    sys.path.insert(0, _pkg_root)

# On Windows, PYD's that depend on a sibling DLL need the directory
# added via os.add_dll_directory (Python 3.8+).
if sys.platform == "win32" and _extra and hasattr(os, "add_dll_directory"):
    os.add_dll_directory(_extra)

import pixelforge as pf  # noqa: E402


PLUGIN_DIR = Path(os.environ["PIXELFORGE_PLUGIN_DIR"])
TMP_DIR    = Path(os.environ["PIXELFORGE_TMP_DIR"])
TMP_DIR.mkdir(parents=True, exist_ok=True)


def _gradient(w: int, h: int, c: int) -> np.ndarray:
    arr = np.zeros((h, w, c), dtype=np.uint8)
    for y in range(h):
        for x in range(w):
            for k in range(c):
                arr[y, x, k] = (x * 7 + y * 11 + k * 31) & 0xFF
    return arr


def test_version_string_present():
    assert isinstance(pf.version_string, str)
    assert pf.version_string.count(".") == 2


def test_image_round_trip_via_numpy():
    arr = _gradient(8, 5, 3)
    img = pf.from_numpy(arr)
    assert img.width == 8 and img.height == 5 and img.channels == 3
    out = pf.as_numpy(img)
    assert out.shape == arr.shape
    assert np.array_equal(out, arr)


def test_to_grayscale_matches_numpy_formula():
    arr = _gradient(4, 4, 3)
    img = pf.from_numpy(arr)
    gray = pf.to_grayscale(img)
    assert gray.format == pf.PixelFormat.Gray
    expected = np.round(0.299 * arr[..., 0] + 0.587 * arr[..., 1]
                        + 0.114 * arr[..., 2]).astype(np.uint8)
    out = pf.as_numpy(gray)
    assert np.allclose(out.astype(int), expected.astype(int), atol=1)


def test_save_and_load_png_round_trip():
    arr = _gradient(6, 4, 4)
    img = pf.from_numpy(arr)
    path = TMP_DIR / "py_round.png"
    pf.save_image(img, str(path))
    loaded = pf.load_image(str(path))
    assert pf.as_numpy(loaded).shape == arr.shape
    assert np.array_equal(pf.as_numpy(loaded), arr)


def test_plugins_discovered():
    plugins = pf.load_plugins_from_directory(PLUGIN_DIR)
    names = sorted(p.name for p in plugins)
    assert {"blur", "edge", "grayscale", "invert", "sharpen"}.issubset(set(names))


def test_pipeline_grayscale_then_invert():
    pipe = pf.build_pipeline(["grayscale", "invert"], plugin_dir=PLUGIN_DIR)
    assert len(pipe) == 2

    arr = _gradient(8, 6, 3)
    img = pf.from_numpy(arr)
    out = pipe.run(img)
    out_np = pf.as_numpy(out)
    assert out_np.ndim == 2  # grayscale flattens the channel axis

    expected_gray = np.round(0.299 * arr[..., 0] + 0.587 * arr[..., 1]
                             + 0.114 * arr[..., 2]).astype(np.uint8)
    expected = (255 - expected_gray).astype(np.uint8)
    assert np.allclose(out_np.astype(int), expected.astype(int), atol=1)


def test_image_buffer_protocol_zero_copy_view():
    arr = _gradient(4, 3, 3)
    img = pf.from_numpy(arr)
    # `np.asarray(img)` uses the buffer protocol; should reflect the same bytes.
    view = np.asarray(img)
    assert view.shape == (3, 4, 3)
    assert np.array_equal(view, arr)


def test_io_error_on_missing_file():
    with pytest.raises(pf.IoError):
        pf.load_image(str(TMP_DIR / "does_not_exist.png"))


def test_plugin_error_on_non_plugin():
    # Try to load libpixelforge itself as a plugin -- must fail cleanly.
    if sys.platform == "win32":
        runtime = PLUGIN_DIR.parent / "bin" / "libpixelforge.dll"
    else:
        runtime = PLUGIN_DIR.parent / "lib" / "libpixelforge.so"
    if runtime.exists():
        with pytest.raises(pf.PluginError):
            pf.Plugin(str(runtime))
