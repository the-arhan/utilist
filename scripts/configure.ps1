param (
    [string]$Preset = "windows-msvc-debug"
)

$ErrorActionPreference = "Stop"

$cmakePath = "cmake"
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    $vsCmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $vsCmake) {
        $cmakePath = $vsCmake
    } else {
        Write-Error "CMake could not be found. Please ensure Visual Studio or CMake is installed."
        exit 1
    }
}

Write-Host "Configuring UTILIST with preset '$Preset' using $cmakePath..."
& $cmakePath --preset $Preset
