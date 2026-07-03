# Phase 8 — Multi-arch & multi-compiler matrix runner (PowerShell).
#
# Builds + tests PixelForge inside three Docker containers:
#   - Linux x64   GCC 13
#   - Linux x64   Clang 18
#   - Linux arm64 GCC 13   (executed under QEMU on x86 hosts)
#
# Usage:
#   pwsh -File scripts\run_matrix.ps1                 # all three
#   pwsh -File scripts\run_matrix.ps1 -Targets gcc    # subset
#   pwsh -File scripts\run_matrix.ps1 -Targets gcc,arm64
#
# Requires Docker Desktop with buildx, and (for arm64 on x86) QEMU
# binfmt_misc registered. The script registers QEMU automatically.

param(
    [string[]]$Targets = @('gcc', 'clang', 'arm64')
)

$ErrorActionPreference = 'Continue'
$repo = (Resolve-Path "$PSScriptRoot\..").Path
Set-Location $repo

$matrix = @{
    'gcc'   = @{ dockerfile = (Join-Path $repo 'docker/linux-x64-gcc.Dockerfile');   platform = 'linux/amd64'; image = 'pixelforge:linux-x64-gcc'   }
    'clang' = @{ dockerfile = (Join-Path $repo 'docker/linux-x64-clang.Dockerfile'); platform = 'linux/amd64'; image = 'pixelforge:linux-x64-clang' }
    'arm64' = @{ dockerfile = (Join-Path $repo 'docker/linux-arm64.Dockerfile');     platform = 'linux/arm64'; image = 'pixelforge:linux-arm64'     }
}

# Ensure buildx is available. Use Docker Desktop's default builder
# ("desktop-linux") rather than creating a custom one — on Windows hosts
# behind a TLS-intercepting corporate proxy the desktop builder is
# pre-configured with the right CA chain, while a fresh `docker buildx
# create` builder is not.
$builderInfo = docker buildx inspect 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Error "docker buildx not available. Is Docker Desktop running?"
    exit 1
}
Write-Host "[matrix] Using default buildx builder." -ForegroundColor Cyan

# Register QEMU emulators for arm64 if we'll need it.
if ($Targets -contains 'arm64') {
    Write-Host "[matrix] Registering QEMU binfmt for cross-arch emulation..." -ForegroundColor Cyan
    docker run --privileged --rm tonistiigi/binfmt --install arm64 | Out-Null
}

$results = @{}
foreach ($t in $Targets) {
    if (-not $matrix.ContainsKey($t)) {
        Write-Warning "Unknown target '$t', skipping."
        continue
    }
    $cfg = $matrix[$t]
    Write-Host "`n[matrix] === $t  ($($cfg.platform)) ===" -ForegroundColor Yellow

    # Build the image. --load forces it into the local docker engine
    # (buildx defaults to OCI registry export otherwise).
    docker buildx build `
        --platform $cfg.platform `
        --file $cfg.dockerfile `
        --tag   $cfg.image `
        --load `
        $repo
    if ($LASTEXITCODE -ne 0) {
        $results[$t] = "BUILD-FAILED"
        continue
    }

    # Run the container; the Dockerfile CMD does configure+build+test.
    docker run --rm --platform $cfg.platform $cfg.image
    $results[$t] = if ($LASTEXITCODE -eq 0) { 'PASS' } else { "TEST-FAILED ($LASTEXITCODE)" }
}
Write-Host "`n[matrix] === Results ===" -ForegroundColor Green
$results.GetEnumerator() | Sort-Object Name | ForEach-Object {
    $color = if ($_.Value -eq 'PASS') { 'Green' } else { 'Red' }
    Write-Host ("{0,-8} : {1}" -f $_.Key, $_.Value) -ForegroundColor $color
}

if ($results.Count -gt 0 -and -not (@($results.Values) | Where-Object { $_ -ne 'PASS' })) {
    exit 0
} else {
    exit 1
}
