#requires -Version 5.1
<#
.SYNOPSIS
    Verifies the PixelForge build toolchain on Windows.
.DESCRIPTION
    Prints PASS/FAIL for each required tool. Exits non-zero if any
    REQUIRED tool is missing.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Continue'
$script:failures = 0

function Test-Tool {
    param(
        [Parameter(Mandatory)] [string] $Name,
        [Parameter(Mandatory)] [scriptblock] $VersionCmd,
        [switch] $Optional
    )
    try {
        $output = & $VersionCmd 2>&1 | Select-Object -First 1
        if ($LASTEXITCODE -ne 0 -and -not $output) { throw 'not found' }
        Write-Host ('  [PASS] {0,-20} {1}' -f $Name, $output) -ForegroundColor Green
    }
    catch {
        $tag = if ($Optional) { 'WARN' } else { 'FAIL' }
        $color = if ($Optional) { 'Yellow' } else { 'Red' }
        Write-Host ('  [{0}] {1,-20} not found' -f $tag, $Name) -ForegroundColor $color
        if (-not $Optional) { $script:failures++ }
    }
}

Write-Host 'PixelForge environment check' -ForegroundColor Cyan
Write-Host '----------------------------'

Write-Host 'Required:'
Test-Tool 'cmake'   { cmake --version }
Test-Tool 'ninja'   { ninja --version }
Test-Tool 'git'     { git --version }
Test-Tool 'python'  { python --version }

Write-Host ''
Write-Host 'Compilers (need at least one):'
Test-Tool 'gcc (MinGW)' { gcc --version }       -Optional
Test-Tool 'clang'       { clang --version }     -Optional
Test-Tool 'cl (MSVC)'   { cl 2>&1 | Out-String } -Optional

Write-Host ''
Write-Host 'Optional:'
Test-Tool 'docker'        { docker --version }        -Optional
Test-Tool 'docker buildx' { docker buildx version }   -Optional
Test-Tool 'wsl'           { wsl --status }            -Optional

Write-Host ''
if ($script:failures -gt 0) {
    Write-Host "$script:failures required tool(s) missing." -ForegroundColor Red
    exit 1
}
Write-Host 'All required tools present.' -ForegroundColor Green
exit 0
