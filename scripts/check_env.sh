#!/usr/bin/env bash
# Verifies the PixelForge build toolchain on Linux/macOS.
set -u

failures=0

check() {
    local name="$1"; shift
    local optional="$1"; shift
    if out=$("$@" 2>&1 | head -n1); then
        printf '  [PASS] %-20s %s\n' "$name" "$out"
    else
        if [[ "$optional" == "1" ]]; then
            printf '  [WARN] %-20s not found\n' "$name"
        else
            printf '  [FAIL] %-20s not found\n' "$name"
            failures=$((failures + 1))
        fi
    fi
}

echo 'PixelForge environment check'
echo '----------------------------'
echo 'Required:'
check cmake   0 cmake --version
check ninja   0 ninja --version
check git     0 git --version
check python3 0 python3 --version

echo
echo 'Compilers (need at least one):'
check gcc   1 gcc --version
check g++   1 g++ --version
check clang 1 clang --version

echo
echo 'Optional:'
check docker     1 docker --version
check dpkg-deb   1 dpkg-deb --version

echo
if (( failures > 0 )); then
    echo "$failures required tool(s) missing." >&2
    exit 1
fi
echo 'All required tools present.'
