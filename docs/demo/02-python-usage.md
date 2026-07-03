# Python Usage Guide

Install and use PixelForge Python bindings.

## Installation

### Step 1: Install the Wheel

```powershell
pip install pixelforge-0.1.0-cp313-cp313-win_amd64.whl
```

### Step 2: Install NumPy (Optional)

NumPy is required for array interoperability:

```powershell
pip install numpy
```

### Step 3: Verify Installation

```python
import pixelforge as pf
print(pf.version_string)  # 0.1.0
```

---

## Basic Usage

### Load and Save Images

```python
import pixelforge as pf

# Load image
img = pf.load_image("input.jpg")
print(f"Size: {img.width}x{img.height}")
print(f"Channels: {img.channels}")

# Save image
pf.save_image(img, "output.png")
```

### Convert to Grayscale

```python
import pixelforge as pf

img = pf.load_image("input.jpg")
gray = pf.to_grayscale(img)
pf.save_image(gray, "grayscale.png")
```

---

## NumPy Integration

### Get NumPy Array (Zero-Copy)

```python
import pixelforge as pf
import numpy as np

img = pf.load_image("input.jpg")
arr = pf.as_numpy(img)

print(arr.shape)   # (height, width, channels)
print(arr.dtype)   # uint8
```

### Create Image from NumPy

```python
import pixelforge as pf
import numpy as np

# Create 256x256 red image
arr = np.zeros((256, 256, 3), dtype=np.uint8)
arr[:, :, 0] = 255  # Red channel

img = pf.from_numpy(arr)
pf.save_image(img, "red.png")
```

### Manipulate with NumPy

```python
import pixelforge as pf
import numpy as np

img = pf.load_image("input.jpg")
arr = pf.as_numpy(img).copy()

# Invert colors
inverted = 255 - arr
img_inv = pf.from_numpy(inverted)
pf.save_image(img_inv, "inverted.png")

# Brighten
brightened = np.clip(arr.astype(np.int16) + 50, 0, 255).astype(np.uint8)
img_bright = pf.from_numpy(brightened)
pf.save_image(img_bright, "bright.png")
```

---

## API Reference

### Functions

| Function | Description |
|----------|-------------|
| `pf.load_image(path)` | Load image from file |
| `pf.save_image(img, path)` | Save image to file |
| `pf.to_grayscale(img)` | Convert to grayscale |
| `pf.as_numpy(img)` | Get NumPy array view |
| `pf.from_numpy(arr)` | Create image from NumPy array |

### Image Properties

| Property | Type | Description |
|----------|------|-------------|
| `img.width` | int | Width in pixels |
| `img.height` | int | Height in pixels |
| `img.channels` | int | Number of channels |

### Module Attributes

| Attribute | Description |
|-----------|-------------|
| `pf.version_string` | Version string "0.1.0" |

---

## Complete Example

```python
import pixelforge as pf
import numpy as np

# Load image
img = pf.load_image("photo.jpg")
print(f"Loaded: {img.width}x{img.height}, {img.channels} channels")

# Get NumPy array
arr = pf.as_numpy(img)
print(f"Shape: {arr.shape}, dtype: {arr.dtype}")

# Image statistics
print(f"Mean brightness: {arr.mean():.1f}")
print(f"Min/Max: {arr.min()}/{arr.max()}")

# Convert to grayscale
gray = pf.to_grayscale(img)
pf.save_image(gray, "photo_gray.png")

# Create inverted version with NumPy
inverted = pf.from_numpy(255 - arr.copy())
pf.save_image(inverted, "photo_inverted.png")

print("Done!")
```

---

## Uninstall

```powershell
pip uninstall pixelforge
```
