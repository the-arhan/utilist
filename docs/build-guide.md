# UTILIST Build Guide

This document details the environment requirements and procedures to configure and build UTILIST.

## Requirements

1. **Host Operating System**: Windows 10 or Windows 11 (64-bit).
2. **Visual Studio 2022**:
   - Workload: "Desktop development with C++"
   - Components: MSVC v143 (or v144) x64/x86 build tools, Windows 10/11 SDK.
   - Bundled CMake & Ninja (or system-installed CMake 3.20+).
3. **Qt 6**:
   - Version: Qt 6.5 or newer (MSVC 2022 64-bit).
   - Set environment variable `CMAKE_PREFIX_PATH` pointing to the Qt installation root, for example: `C:\Qt\6.8.0\msvc2022_64`.

## Build Steps

### Option A: Using Helper Scripts
Open PowerShell:
```powershell
# 1. Configure
.\scripts\configure.ps1 -Preset windows-msvc-debug

# 2. Build
.\scripts\build.ps1 -Preset windows-msvc-debug

# 3. Test
.\scripts\test.ps1 -Preset windows-msvc-debug
```

### Option B: Command-Line CMake Presets
```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug --output-on-failure
```

### Release Build
```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release --output-on-failure
```
