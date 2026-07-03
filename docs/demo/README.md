# PixelForge Usage Demos

Quick start guides for using PixelForge.

## Available Guides

| Guide | Description |
|-------|-------------|
| [CLI Usage](01-cli-usage.md) | Install the Windows EXE and use the command line |
| [Python Usage](02-python-usage.md) | Install the wheel package and use Python API |
| [Portable Usage](03-portable-usage.md) | Use the ZIP archive or source tarball |

## Available Packages

| Package | Platform | Description |
|---------|----------|-------------|
| `pixelforge-0.1.0-Windows-AMD64.exe` | Windows | Installer with PATH setup |
| `pixelforge-0.1.0-cp313-cp313-win_amd64.whl` | Windows/Python 3.13 | Python wheel package |
| `pixelforge-0.1.0-Windows-AMD64.zip` | Windows | Portable ZIP archive |
| `pixelforge-0.1.0.tar.gz` | All | Source tarball |

## Quick Examples

### CLI
```bash
pixelforge input.jpg -f grayscale -o output.png
```

### Python
```python
import pixelforge as pf
img = pf.load_image("input.jpg")
gray = pf.to_grayscale(img)
pf.save_image(gray, "output.png")
```
