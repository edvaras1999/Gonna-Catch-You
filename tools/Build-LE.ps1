[CmdletBinding()]
param(
    [string]$SdkRoot = $env:SKSE_LE_SDK,
    [string]$SdkArchive,
    [string]$MSBuildPath
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$defaultSdkRoot = Join-Path $repoRoot 'build\deps\skse\skse_1_07_03'

if (-not $SdkRoot) {
    if (Test-Path -LiteralPath (Join-Path $defaultSdkRoot 'src\skse\skse\PluginAPI.h')) {
        $SdkRoot = $defaultSdkRoot
    } elseif ($SdkArchive) {
        $dependencyRoot = Join-Path $repoRoot 'build\deps\skse'
        New-Item -ItemType Directory -Force -Path $dependencyRoot | Out-Null
        $sevenZipCandidates = @(
            (Join-Path $env:ProgramFiles '7-Zip\7z.exe'),
            (Join-Path ${env:ProgramFiles(x86)} '7-Zip\7z.exe')
        )
        $sevenZipCommand = Get-Command 7z.exe -ErrorAction SilentlyContinue
        if ($sevenZipCommand) {
            $sevenZipCandidates = @($sevenZipCommand.Source) + $sevenZipCandidates
        }
        $sevenZip = $sevenZipCandidates | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1
        if (-not $sevenZip) {
            throw '7-Zip was not found; it is required to extract the official SKSE .7z archive.'
        }
        & $sevenZip x $SdkArchive "-o$dependencyRoot" -y | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "Could not extract SKSE LE source archive (exit $LASTEXITCODE)." }
        $SdkRoot = $defaultSdkRoot
    }
}

if (-not $SdkRoot -or -not (Test-Path -LiteralPath (Join-Path $SdkRoot 'src\skse\skse\PluginAPI.h'))) {
    throw 'SKSE LE 1.7.3 source not found. Set SKSE_LE_SDK or pass -SdkRoot / -SdkArchive.'
}
$SdkRoot = (Resolve-Path -LiteralPath $SdkRoot).Path
if (-not (Test-Path -LiteralPath (Join-Path $SdkRoot 'src\common\ITypes.h')) -or
    -not (Test-Path -LiteralPath (Join-Path $SdkRoot 'src\skse\skse\skse_version.h'))) {
    throw "The selected SDK is incomplete or has an unexpected layout: $SdkRoot"
}

if (-not $MSBuildPath) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $vsRoot = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath
        if ($vsRoot) {
            $candidate = Join-Path $vsRoot 'MSBuild\Current\Bin\MSBuild.exe'
            if (Test-Path -LiteralPath $candidate) { $MSBuildPath = $candidate }
        }
    }
}
if (-not $MSBuildPath -or -not (Test-Path -LiteralPath $MSBuildPath)) {
    throw 'MSBuild was not found. Install Visual Studio 2022 Build Tools with the C++ build tools workload, then rerun this script.'
}

$project = Join-Path $repoRoot 'src\GonnaCatchYou.vcxproj'
& $MSBuildPath $project /m /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 "/p:SKSE_SDK_ROOT=$SdkRoot" /v:minimal
if ($LASTEXITCODE -ne 0) { throw "GonnaCatchYou compilation failed (exit $LASTEXITCODE)." }

& (Join-Path $PSScriptRoot 'Package-Mod.ps1')
if ($LASTEXITCODE -ne 0) { throw "Install package assembly failed (exit $LASTEXITCODE)." }

