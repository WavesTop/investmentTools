param(
    [string]$BuildDir = "build-package-windows",
    [string]$Configuration = "Release",
    [string]$ToolchainFile = $env:CMAKE_TOOLCHAIN_FILE,
    [switch]$SkipBuild,
    [switch]$NoZip
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildPath = Join-Path $Root $BuildDir
$PackageBase = Join-Path $Root "InvestInsight-Windows"
$ZipBase = Join-Path $Root "InvestInsight-Windows.zip"

function Get-CMakeCacheValue {
    param(
        [Parameter(Mandatory = $true)][string]$CachePath,
        [Parameter(Mandatory = $true)][string]$Name
    )

    if (-not (Test-Path -LiteralPath $CachePath)) { return $null }
    $line = Select-String -LiteralPath $CachePath -Pattern "^$([regex]::Escape($Name))(:[^=]+)?=" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $line) { return $null }
    return ($line.Line -replace "^[^=]+=", "").Trim()
}

if ([string]::IsNullOrWhiteSpace($ToolchainFile)) {
    $ToolchainFile = Get-CMakeCacheValue -CachePath (Join-Path $Root "build\CMakeCache.txt") -Name "CMAKE_TOOLCHAIN_FILE"
}

function New-CleanOutputPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (Test-Path -LiteralPath $Path) {
        try {
            Remove-Item -LiteralPath $Path -Recurse -Force -ErrorAction Stop
        } catch {
            $parent = Split-Path -Parent $Path
            $leaf = Split-Path -Leaf $Path
            $stem = [System.IO.Path]::GetFileNameWithoutExtension($leaf)
            $extension = [System.IO.Path]::GetExtension($leaf)
            if ([string]::IsNullOrWhiteSpace($extension)) {
                $stem = $leaf
            }

            for ($i = 1; $i -lt 1000; $i++) {
                $candidateLeaf = if ($extension) { "$stem-$i$extension" } else { "$stem-$i" }
                $candidate = Join-Path $parent $candidateLeaf
                if (-not (Test-Path -LiteralPath $candidate)) {
                    Write-Warning "Cannot remove $Path, using $candidate instead."
                    return $candidate
                }
            }

            throw "Cannot find an available incremental name for $Path."
        }
    }

    return $Path
}

function Find-InvestInsightExe {
    $candidates = @(
        (Join-Path $BuildPath "$Configuration\InvestInsight.exe"),
        (Join-Path $BuildPath "InvestInsight.exe")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    $found = Get-ChildItem -LiteralPath $BuildPath -Filter "InvestInsight.exe" -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($found) { return $found.FullName }

    throw "InvestInsight.exe was not found. Build the Release target first."
}

function Find-WinDeployQt {
    $cmd = Get-Command "windeployqt.exe" -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $knownPaths = @()
    if ($env:QTDIR) { $knownPaths += (Join-Path $env:QTDIR "bin\windeployqt.exe") }

    $cachePaths = @(
        (Join-Path $BuildPath "CMakeCache.txt"),
        (Join-Path $Root "build\CMakeCache.txt")
    )
    foreach ($cache in $cachePaths) {
        foreach ($qtVar in @("Qt5_DIR", "Qt6_DIR")) {
            $qtDir = Get-CMakeCacheValue -CachePath $cache -Name $qtVar
            if (-not [string]::IsNullOrWhiteSpace($qtDir)) {
                $shareDir = Split-Path -Parent $qtDir
                $installedRoot = Split-Path -Parent $shareDir
                $knownPaths += (Join-Path $installedRoot "tools\Qt5\bin\windeployqt.exe")
                $knownPaths += (Join-Path $installedRoot "tools\Qt6\bin\windeployqt.exe")
                $knownPaths += (Join-Path $installedRoot "bin\windeployqt.exe")
            }
        }
    }

    $knownPaths += @(
        "C:\vcpkg\installed\x64-windows\tools\Qt5\bin\windeployqt.exe",
        "C:\vcpkg\installed\x64-windows\tools\Qt6\bin\windeployqt.exe",
        "C:\Qt\5.15.2\msvc2019_64\bin\windeployqt.exe",
        "C:\Qt\6.7.0\msvc2019_64\bin\windeployqt.exe",
        "C:\Qt\6.8.0\msvc2022_64\bin\windeployqt.exe"
    )

    foreach ($path in $knownPaths) {
        if (Test-Path -LiteralPath $path) { return $path }
    }

    return $null
}

function Find-VcpkgInstalledRoot {
    $cachePaths = @(
        (Join-Path $BuildPath "CMakeCache.txt"),
        (Join-Path $Root "build\CMakeCache.txt")
    )

    foreach ($cache in $cachePaths) {
        foreach ($qtVar in @("Qt5_DIR", "Qt6_DIR")) {
            $qtDir = Get-CMakeCacheValue -CachePath $cache -Name $qtVar
            if (-not [string]::IsNullOrWhiteSpace($qtDir)) {
                $parent = Split-Path -Parent $qtDir
                while ($parent -and (Split-Path -Leaf $parent) -ne "x64-windows") {
                    $next = Split-Path -Parent $parent
                    if ($next -eq $parent) { break }
                    $parent = $next
                }
                if ($parent -and (Test-Path -LiteralPath (Join-Path $parent "bin"))) {
                    return $parent
                }
            }
        }
    }

    $fallback = "C:\vcpkg\installed\x64-windows"
    if (Test-Path -LiteralPath (Join-Path $fallback "bin")) { return $fallback }
    return $null
}

function Copy-VcpkgRuntimeFallback {
    param([Parameter(Mandatory = $true)][string]$Destination)

    $installedRoot = Find-VcpkgInstalledRoot
    if (-not $installedRoot) {
        Write-Warning "vcpkg installed root was not found. The package may miss Qt DLLs."
        return
    }

    $binDir = Join-Path $installedRoot "bin"
    $pluginsDir = Join-Path $installedRoot "plugins"

    if (Test-Path -LiteralPath $binDir) {
        Copy-Item -Path (Join-Path $binDir "*.dll") -Destination $Destination -Force
    }

    foreach ($pluginName in @("platforms", "imageformats", "styles", "tls", "iconengines")) {
        $source = Join-Path $pluginsDir $pluginName
        if (Test-Path -LiteralPath $source) {
            $pluginDestination = Join-Path $Destination $pluginName
            New-Item -ItemType Directory -Force -Path $pluginDestination | Out-Null
            Copy-Item -Path (Join-Path $source "*.dll") -Destination $pluginDestination -Force
        }
    }

    @"
[Paths]
Plugins=.
"@ | Set-Content -LiteralPath (Join-Path $Destination "qt.conf") -Encoding ASCII

    Write-Warning "windeployqt was not found. Copied vcpkg runtime DLLs and Qt plugins from $installedRoot as a fallback."
}

if (-not $SkipBuild) {
    $configureArgs = @(
        "-S", $Root,
        "-B", $BuildPath,
        "-DCMAKE_BUILD_TYPE=$Configuration",
        "-DINVESTINSIGHT_WIN32_SUBSYSTEM=ON"
    )
    if (-not [string]::IsNullOrWhiteSpace($ToolchainFile)) {
        $configureArgs += "-DCMAKE_TOOLCHAIN_FILE=$ToolchainFile"
    }

    Write-Host "[package] Configure CMake..."
    & cmake @configureArgs

    Write-Host "[package] Build $Configuration..."
    & cmake --build $BuildPath --config $Configuration -- /m
}

$exe = Find-InvestInsightExe
$packageDir = New-CleanOutputPath $PackageBase
New-Item -ItemType Directory -Force -Path $packageDir | Out-Null

Copy-Item -LiteralPath $exe -Destination (Join-Path $packageDir "InvestInsight.exe") -Force

$deployQt = Find-WinDeployQt
if ($deployQt) {
    Write-Host "[package] Running windeployqt: $deployQt"
    & $deployQt (Join-Path $packageDir "InvestInsight.exe") --release
} else {
    Copy-VcpkgRuntimeFallback -Destination $packageDir
}

$readme = Join-Path $Root "README.md"
if (Test-Path -LiteralPath $readme) {
    Copy-Item -LiteralPath $readme -Destination (Join-Path $packageDir "README.md") -Force
}

if (-not $NoZip) {
    $zipPath = New-CleanOutputPath $ZipBase
    Compress-Archive -Path $packageDir -DestinationPath $zipPath -CompressionLevel Optimal
    Write-Host "[package] ZIP: $zipPath"
}

Write-Host "[package] Windows package: $packageDir"
