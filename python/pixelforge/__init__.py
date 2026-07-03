"""Pythonic facade over the PixelForge C++ runtime.

Re-exports everything from the compiled `_pixelforge` extension and adds a
few helpers to make NumPy interop natural.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path
from typing import Iterable

import numpy as np

# When installed as a wheel the compiled extension and its sibling DLLs
# (libpixelforge.dll on Windows) live next to this file. On Python 3.8+
# Windows no longer searches PATH for dependent DLLs, so we must
# explicitly add this directory to the loader search path *before*
# importing the extension.
_PKG_DIR = Path(__file__).resolve().parent
if sys.platform == "win32" and hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(str(_PKG_DIR))
    except (OSError, FileNotFoundError):
        pass

# In an installed wheel the compiled extension lives inside this package
# (`pixelforge._pixelforge`). When running directly from the build tree we
# instead pre-pend the build's python_modules dir to sys.path, so the
# extension is importable as a top-level `_pixelforge` module. Try both.
try:
    from . import _pixelforge as _ext  # type: ignore[attr-defined]
except ImportError:
    import _pixelforge as _ext  # type: ignore[no-redef]

# Re-export the pybind11 surface unchanged.
Image                       = _ext.Image
Pipeline                    = _ext.Pipeline
Plugin                      = _ext.Plugin
PixelFormat                 = _ext.PixelFormat
IoError                     = _ext.IoError
PluginError                 = _ext.PluginError
load_image                  = _ext.load_image
save_image                  = _ext.save_image
load_plugins_from_directory = _ext.load_plugins_from_directory
to_grayscale                = _ext.to_grayscale

__version__     = _ext.__version__
version_string  = _ext.version_string

__all__ = [
    "Image", "Pipeline", "Plugin", "PixelFormat",
    "IoError", "PluginError",
    "load_image", "save_image", "load_plugins_from_directory", "to_grayscale",
    "as_numpy", "from_numpy",
    "default_plugin_dir", "build_pipeline",
    "__version__", "version_string",
]


def as_numpy(image: "Image") -> np.ndarray:
    """Return a NumPy view (copy) of the image's pixel buffer."""
    return image.to_numpy()


def from_numpy(arr: np.ndarray) -> "Image":
    """Build an Image from a (H, W) or (H, W, C) uint8 NumPy array."""
    if arr.dtype != np.uint8:
        arr = arr.astype(np.uint8)
    return Image.from_numpy(np.ascontiguousarray(arr))


def default_plugin_dir() -> Path | None:
    """Return the default plugin directory, or None if not found.

    Searches: <module_dir>/plugins, then <module_dir>/../plugins,
    then <sys.prefix>/share/pixelforge/plugins.
    """
    here = Path(__file__).resolve().parent
    import sys
    for cand in (here / "plugins",
                 here.parent / "plugins",
                 Path(sys.prefix) / "share" / "pixelforge" / "plugins"):
        if cand.is_dir():
            return cand
    return None


def build_pipeline(filter_names: Iterable[str], plugin_dir: Path | None = None) -> "Pipeline":
    """Convenience: build a Pipeline from a list of plugin names."""
    plugin_dir = plugin_dir or default_plugin_dir()
    if plugin_dir is None:
        raise RuntimeError("no plugin directory found; pass plugin_dir explicitly")
    plugins = {p.name: p for p in load_plugins_from_directory(plugin_dir)}
    pipe = Pipeline()
    for name in filter_names:
        if name not in plugins:
            raise KeyError(f"plugin not found: {name}")
        pipe.add_plugin(plugins[name])
    return pipe
