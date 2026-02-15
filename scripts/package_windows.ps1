param(
    [string]$BuildDir = "build-mingw",
    [string]$Config = "Release",
    [string]$StageDir = "dist/stage",
    [string]$DistDir = "dist",
    [string]$Version = "",
    [switch]$AutoVersion,
    [switch]$CreateInstaller
)

$ErrorActionPreference = "Stop"

function Resolve-ExistingPath {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
        throw "Path not found: $Path"
    }
    return (Resolve-Path $Path).Path
}

function Find-WindeployQt {
    param(
        [string]$RepoRoot,
        [string]$BuildPath
    )

    $cmd = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
    if ($cmd -and (Test-Path $cmd.Source)) {
        return $cmd.Source
    }

    $cachePath = Join-Path $BuildPath "CMakeCache.txt"
    if (Test-Path $cachePath) {
        $line = Select-String -Path $cachePath -Pattern "^WINDEPLOYQT_EXECUTABLE:FILEPATH=(.+)$" | Select-Object -First 1
        if ($line) {
            $candidate = $line.Matches[0].Groups[1].Value.Trim()
            if (Test-Path $candidate) {
                return $candidate
            }
        }
    }

    $qtCandidates = @(
        "C:\Qt\6.10.2\mingw_64\bin\windeployqt.exe",
        "C:\Qt\6.10.1\mingw_64\bin\windeployqt.exe",
        "C:\Qt\6.9.0\mingw_64\bin\windeployqt.exe"
    )
    foreach ($candidate in $qtCandidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    throw "windeployqt.exe not found. Add Qt bin to PATH or configure once with CMake (build dir cache)."
}

function Find-Iscc {
    $cmd = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($cmd -and (Test-Path $cmd.Source)) {
        return $cmd.Source
    }

    $candidates = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles}\Inno Setup 6\ISCC.exe"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }
    return $null
}

function Resolve-PackageVersion {
    param(
        [string]$RepoRoot,
        [string]$ManualVersion,
        [bool]$UseAutoVersion
    )

    if (-not [string]::IsNullOrWhiteSpace($ManualVersion)) {
        return $ManualVersion
    }

    if (-not $UseAutoVersion) {
        return "0.1.0"
    }

    $datePart = Get-Date -Format "yyyy.MM.dd"
    $commitPart = "nogit"

    $gitCmd = Get-Command git -ErrorAction SilentlyContinue
    if ($gitCmd) {
        try {
            $commit = (& git -C $RepoRoot rev-parse --short HEAD 2>$null)
            if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($commit)) {
                $commitPart = $commit.Trim()
            }
        } catch {
            $commitPart = "nogit"
        }
    }

    return "$datePart+$commitPart"
}

$repoRoot = Resolve-ExistingPath (Join-Path $PSScriptRoot "..")
$effectiveVersion = Resolve-PackageVersion -RepoRoot $repoRoot -ManualVersion $Version -UseAutoVersion $AutoVersion
$buildPath = Resolve-ExistingPath (Join-Path $repoRoot $BuildDir)
$exePath = Join-Path $buildPath "app\ngpc_sound_creator.exe"
if (-not (Test-Path $exePath)) {
    throw "Missing executable: $exePath. Build the app first."
}

$stageRoot = Join-Path $repoRoot $StageDir
$stageAppRoot = Join-Path $stageRoot "NGPC Sound Creator"
$distRoot = Join-Path $repoRoot $DistDir

if (Test-Path $stageAppRoot) {
    Remove-Item -Recurse -Force $stageAppRoot
}
New-Item -ItemType Directory -Path $stageAppRoot -Force | Out-Null
New-Item -ItemType Directory -Path $distRoot -Force | Out-Null

Copy-Item -Path $exePath -Destination (Join-Path $stageAppRoot "ngpc_sound_creator.exe") -Force

$docFiles = @(
    "README.md",
    "TOOLCHAIN.md",
    "THIRD_PARTY.md",
    "driver_custom_latest\README.md",
    "driver_custom_latest\INTEGRATION_QUICKSTART.md"
)
foreach ($relativePath in $docFiles) {
    $sourcePath = Join-Path $repoRoot $relativePath
    if (Test-Path $sourcePath) {
        $targetName = Split-Path $relativePath -Leaf
        Copy-Item -Path $sourcePath -Destination (Join-Path $stageAppRoot $targetName) -Force
    }
}

$driverDir = Join-Path $repoRoot "driver_custom_latest"
if (Test-Path $driverDir) {
    Copy-Item -Path $driverDir -Destination (Join-Path $stageAppRoot "driver_custom_latest") -Recurse -Force
}

$windeployqt = Find-WindeployQt -RepoRoot $repoRoot -BuildPath $buildPath
$deployMode = if ($Config -match "Debug") { "--debug" } else { "--release" }
$deployArgs = @(
    $deployMode,
    "--compiler-runtime",
    "--no-translations",
    (Join-Path $stageAppRoot "ngpc_sound_creator.exe")
)
Write-Host "Running windeployqt: $windeployqt $($deployArgs -join ' ')"
& $windeployqt @deployArgs
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with code $LASTEXITCODE"
}

$zipPath = Join-Path $distRoot ("ngpc_sound_creator-windows-" + $effectiveVersion + ".zip")
if (Test-Path $zipPath) {
    Remove-Item -Force $zipPath
}
Compress-Archive -Path $stageAppRoot -DestinationPath $zipPath

Write-Host ""
Write-Host "Package version:"
Write-Host "  $effectiveVersion"
Write-Host ""
Write-Host "Zip package created:"
Write-Host "  $zipPath"

if ($CreateInstaller) {
    $iscc = Find-Iscc
    if (-not $iscc) {
        Write-Warning "ISCC.exe (Inno Setup 6) not found. Zip is ready, installer skipped."
        exit 0
    }

    $issPath = Join-Path $repoRoot "packaging\windows\ngpc_sound_creator.iss"
    if (-not (Test-Path $issPath)) {
        throw "Inno script missing: $issPath"
    }

    $isccArgs = @(
        "/DAppVersion=$effectiveVersion",
        "/DSourceRoot=$stageAppRoot",
        "/DOutputRoot=$distRoot",
        $issPath
    )
    Write-Host "Running ISCC: $iscc $($isccArgs -join ' ')"
    & $iscc @isccArgs
    if ($LASTEXITCODE -ne 0) {
        throw "ISCC failed with code $LASTEXITCODE"
    }

    Write-Host ""
    Write-Host "Installer created in:"
    Write-Host "  $distRoot"
}
