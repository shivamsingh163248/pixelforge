# Portable & Source Usage Guide

Use PixelForge without installation.

## ZIP Archive (Windows Portable)

### Step 1: Extract

Extract `pixelforge-0.1.0-Windows-AMD64.zip` to any folder:

```powershell
Expand-Archive pixelforge-0.1.0-Windows-AMD64.zip -DestinationPath C:\pixelforge
```

### Step 2: Use Directly

```powershell
# Run directly
C:\pixelforge\bin\pixelforge.exe --version

# Or add to PATH temporarily
$env:Path = "C:\pixelforge\bin;" + $env:Path
pixelforge --version
```

### Structure

```
pixelforge-0.1.0-Windows-AMD64/
├── bin/
│   └── pixelforge.exe      # CLI executable
├── lib/
│   ├── libpixelforge.dll   # Core library
│   └── pixelforge/
│       └── plugins/        # Filter plugins
│           ├── blur.dll
│           ├── edge.dll
│           ├── grayscale.dll
│           ├── invert.dll
│           └── sharpen.dll
└── include/                # C/C++ headers (SDK)
```

---

## Source Tarball

### Step 1: Extract

```bash
tar -xzf pixelforge-0.1.0.tar.gz
cd pixelforge-0.1.0
```

### Step 2: Build

**Prerequisites:**
- CMake 3.20+
- C++17 compiler (GCC, Clang, MSVC)
- Python 3.10+ (optional, for bindings)

**Build:**

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --parallel

# Test
ctest --test-dir build

# Install (optional)
cmake --install build --prefix /usr/local
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `PIXELFORGE_BUILD_TESTS` | ON | Build unit tests |
| `PIXELFORGE_BUILD_PYTHON` | ON | Build Python bindings |
| `PIXELFORGE_BUILD_PLUGINS` | ON | Build filter plugins |

---

## Docker Usage

### Using Pre-built Images

```bash
# Pull image
docker pull pixelforge/pixelforge:0.1.0

# Run CLI
docker run --rm -v $(pwd):/data pixelforge/pixelforge:0.1.0 \
    pixelforge /data/input.jpg -f grayscale -o /data/output.png
```

### Build from Source

```bash
cd pixelforge
docker build -f .docker/Dockerfile.linux-x64-gcc -t pixelforge:local .
```

---

## Linux Packages

### DEB (Debian/Ubuntu)

```bash
sudo dpkg -i pixelforge-0.1.0-Linux-x86_64.deb
```

### RPM (Fedora/RHEL)

```bash
sudo rpm -i pixelforge-0.1.0-Linux-x86_64.rpm
```

---

## Environment Variables

| Variable | Description |
|----------|-------------|
| `PIXELFORGE_PLUGIN_DIR` | Override plugin search directory |

Example:

```bash
export PIXELFORGE_PLUGIN_DIR=/custom/plugins
pixelforge input.jpg -f custom_filter -o output.png
```
