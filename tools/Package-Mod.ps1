[CmdletBinding()]
param(
    [string]$BuildRoot = (Join-Path $PSScriptRoot '..\build'),
    [string]$OutputRoot = (Join-Path $PSScriptRoot '..\out')
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$dllPath = Join-Path $BuildRoot 'Release\GonnaCatchYou.dll'
if (-not (Test-Path -LiteralPath $dllPath -PathType Leaf)) {
    throw "Compiled plugin not found: $dllPath"
}

$packageName = 'GonnaCatchYou-LE-install'
$packageRoot = Join-Path $OutputRoot $packageName
$pluginDir = Join-Path $packageRoot 'Data\SKSE\Plugins'
$zipPath = Join-Path $OutputRoot ($packageName + '.zip')

if (Test-Path -LiteralPath $packageRoot) {
    $resolvedPackage = [IO.Path]::GetFullPath($packageRoot)
    $resolvedOutput = [IO.Path]::GetFullPath($OutputRoot).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $outputPrefix = $resolvedOutput + [IO.Path]::DirectorySeparatorChar
    if (-not $resolvedPackage.StartsWith($outputPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clear package directory outside output root: $resolvedPackage"
    }
    Remove-Item -LiteralPath $resolvedPackage -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $pluginDir | Out-Null
Copy-Item -LiteralPath $dllPath -Destination $pluginDir
Copy-Item -LiteralPath (Join-Path $repoRoot 'dist\Data\SKSE\Plugins\GonnaCatchYou.ini') -Destination $pluginDir
Copy-Item -LiteralPath (Join-Path $repoRoot 'README.md') -Destination (Join-Path $packageRoot 'README.txt')

if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
Compress-Archive -Path (Join-Path $packageRoot '*') -DestinationPath $zipPath -CompressionLevel Optimal
Write-Host "Install package created: $zipPath"
Get-ChildItem -LiteralPath $packageRoot -Recurse -File | ForEach-Object {
    Write-Host ($_.FullName.Substring($packageRoot.Length + 1) + '  ' + $_.Length + ' bytes')
}
