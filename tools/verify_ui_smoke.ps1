param(
    [switch]$WithSectorDump
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build"
$smokeExe = Join-Path $buildDir "Release\InvestInsightUiSmoke.exe"
$appExe = Join-Path $buildDir "Release\InvestInsight.exe"

if (-not (Test-Path $buildDir)) {
    throw "Build directory not found: $buildDir"
}

Write-Host "[1/3] Building InvestInsight Release..."
cmake --build $buildDir --config Release --target InvestInsight -- /m
if ($LASTEXITCODE -ne 0) {
    throw "InvestInsight build failed with exit code $LASTEXITCODE"
}

Write-Host "[2/3] Building UI smoke tests..."
cmake --build $buildDir --config Release --target InvestInsightUiSmoke -- /m
if ($LASTEXITCODE -ne 0) {
    throw "UI smoke build failed with exit code $LASTEXITCODE"
}

if (-not (Test-Path $smokeExe)) {
    throw "UI smoke executable not found: $smokeExe"
}

Write-Host "[3/3] Running UI smoke tests..."
& $smokeExe
if ($LASTEXITCODE -ne 0) {
    throw "UI smoke tests failed with exit code $LASTEXITCODE"
}

if ($WithSectorDump) {
    if (-not (Test-Path $appExe)) {
        throw "InvestInsight executable not found: $appExe"
    }
    Write-Host "[extra] Running sector change diagnostics..."
    & $appExe --dump-sector-changes
    if ($LASTEXITCODE -ne 0) {
        throw "Sector diagnostics failed with exit code $LASTEXITCODE"
    }
}

Write-Host "UI smoke verification passed."
