#requires -Version 5.1
<#
.SYNOPSIS
    Copia os binarios Release da suite para a pasta de prototipos.

.DESCRIPTION
    Para cada plugin copia a pasta .vst3 inteira e o Standalone renomeado para
    <Nome>-standalone.exe. Os instaladores (.txt) da pasta de destino nao sao
    tocados. Rode depois de um build Release completo.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools/sync_prototipos.ps1
#>
param(
    [string]$Dest = "",
    [string]$BuildDir = ""
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
if (-not $Dest) { $Dest = Join-Path $HOME "Documents/prototipos" }
if (-not $BuildDir) { $BuildDir = Join-Path $Root "build" }

if (-not (Test-Path $BuildDir)) { throw "build folder not found: $BuildDir" }
New-Item -ItemType Directory -Force $Dest | Out-Null

$vst3Dirs = Get-ChildItem -Path $BuildDir -Recurse -Directory -Filter "*.vst3" |
    Where-Object { $_.FullName -match "_artefacts.Release.VST3" -and
                   (Test-Path (Join-Path $_.FullName "Contents")) }

$copied = 0
foreach ($bundle in $vst3Dirs) {
    $target = Join-Path $Dest $bundle.Name
    if (Test-Path $target) { Remove-Item $target -Recurse -Force }
    Copy-Item $bundle.FullName $Dest -Recurse -Force
    $copied++
}

$standalones = Get-ChildItem -Path $BuildDir -Recurse -Directory -Filter "Standalone" |
    Where-Object { $_.FullName -match "_artefacts.Release" }

foreach ($dir in $standalones) {
    Get-ChildItem $dir.FullName -Filter *.exe | ForEach-Object {
        $target = Join-Path $Dest ("{0}-standalone.exe" -f $_.BaseName)
        Copy-Item $_.FullName $target -Force
        $copied++
    }
}

Write-Output ("{0} binarios copiados para {1}" -f $copied, $Dest)
