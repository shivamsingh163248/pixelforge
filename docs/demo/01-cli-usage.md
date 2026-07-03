# CLI Usage Guide

Install and use PixelForge from the command line.

## Installation

### Step 1: Download the Installer

Get `pixelforge-0.1.0-Windows-AMD64.exe` from the `dist/` folder.

### Step 2: Run the Installer

```powershell
# GUI install (with options)
.\pixelforge-0.1.0-Windows-AMD64.exe

# Silent install
.\pixelforge-0.1.0-Windows-AMD64.exe /S
```

**Install Options:**
- ✅ Add PixelForge to PATH (current user) — checked by default
- ⬜ Add PixelForge to PATH (all users) — requires admin
- ✅ Create Desktop shortcut — checked by default

### Step 3: Open New Terminal

**Important:** Open a NEW terminal window to pick up PATH changes.

### Step 4: Verify Installation

```powershell
pixelforge --version
# Output: 0.1.0
```

---

## Usage

### Syntax

```
pixelforge <input> -f <filter> [-f <filter> ...] -o <output>
```

**Note:** Input file is positional (NOT `-i`).

### Basic Commands

```powershell
# Show help
pixelforge --help

# Show version
pixelforge --version

# List available filters
pixelforge list-filters
```

### Apply Single Filter

```powershell
pixelforge input.jpg -f grayscale -o output.png
pixelforge input.jpg -f blur -o blurred.png
pixelforge input.jpg -f edge -o edges.png
pixelforge input.jpg -f invert -o inverted.png
pixelforge input.jpg -f sharpen -o sharp.png
```

### Apply Multiple Filters (Pipeline)

Filters are applied in order:

```powershell
# Grayscale, then blur
pixelforge input.jpg -f grayscale -f blur -o output.png

# Blur, then sharpen
pixelforge input.jpg -f blur -f sharpen -o enhanced.png
```

### JPEG Quality

```powershell
pixelforge input.jpg -f sharpen -o output.jpg --jpeg-quality 95
```

---

## Available Filters

| Filter | Description |
|--------|-------------|
| `blur` | 3x3 box blur |
| `edge` | 3x3 Laplacian edge detector |
| `grayscale` | Convert to BT.601 luma |
| `invert` | Invert color channels |
| `sharpen` | 3x3 unsharp mask |

---

## Supported Formats

| Format | Read | Write |
|--------|------|-------|
| PNG | ✅ | ✅ |
| JPEG | ✅ | ✅ |
| BMP | ✅ | ✅ |
| TGA | ✅ | ✅ |

---

## Examples

```powershell
# Convert photo to grayscale
pixelforge photo.jpg -f grayscale -o photo_gray.png

# Enhance image (blur + sharpen)
pixelforge photo.jpg -f blur -f sharpen -o photo_enhanced.jpg

# Detect edges
pixelforge photo.jpg -f grayscale -f edge -o photo_edges.png
```

---

## Uninstall

```powershell
# GUI
& "C:\Program Files\PixelForge 0.1\Uninstall.exe"

# Silent
& "C:\Program Files\PixelForge 0.1\Uninstall.exe" /S
```
