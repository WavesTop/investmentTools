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

Write-Host "[1/4] Building InvestInsight Release..."
cmake --build $buildDir --config Release --target InvestInsight -- /m
if ($LASTEXITCODE -ne 0) {
    throw "InvestInsight build failed with exit code $LASTEXITCODE"
}

Write-Host "[2/4] Building UI smoke tests..."
cmake --build $buildDir --config Release --target InvestInsightUiSmoke -- /m
if ($LASTEXITCODE -ne 0) {
    throw "UI smoke build failed with exit code $LASTEXITCODE"
}

if (-not (Test-Path $smokeExe)) {
    throw "UI smoke executable not found: $smokeExe"
}

Write-Host "[3/4] Running UI smoke tests..."
& $smokeExe
if ($LASTEXITCODE -ne 0) {
    throw "UI smoke tests failed with exit code $LASTEXITCODE"
}

if (-not (Test-Path $appExe)) {
    throw "InvestInsight executable not found: $appExe"
}

Write-Host "[4/4] Running main window smoke..."
$uiSmoke = Start-Process -FilePath $appExe -ArgumentList "--ui-smoke" -PassThru -WindowStyle Hidden
if (-not $uiSmoke.WaitForExit(15000)) {
    Stop-Process -Id $uiSmoke.Id -Force
    throw "Main window smoke timed out."
}
if ($uiSmoke.ExitCode -ne 0) {
    throw "Main window smoke failed with exit code $($uiSmoke.ExitCode)"
}

if ($WithSectorDump) {
    Write-Host "[extra] Running sector change diagnostics..."
    & $appExe --dump-sector-changes
    if ($LASTEXITCODE -ne 0) {
        throw "Sector diagnostics failed with exit code $LASTEXITCODE"
    }
}

Write-Host "UI smoke verification passed."
