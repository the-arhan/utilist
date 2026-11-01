param (
    [string]$Preset = "windows-msvc-debug"
)

$ErrorActionPreference = "Stop"

$ctestPath = "ctest"
if (-not (Get-Command ctest -ErrorAction SilentlyContinue)) {
    $vsCtest = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
    if (Test-Path $vsCtest) {
        $ctestPath = $vsCtest
    } else {
        Write-Error "CTest could not be found."
        exit 1
    }
}

Write-Host "Running tests with preset '$Preset'..."
& $ctestPath --preset $Preset --output-on-failure
