#Requires -Version 5.1
<#
  collect_payload.ps1

  Varre a arvore de build e junta cada bundle .vst3 dentro de uma pasta por
  familia em installer/payload/<Familia>/. O Inno Setup (forja.iss) monta um
  componente por familia a partir dessas pastas, entao o usuario escolhe quais
  series instalar.

  A familia e derivada do caminho do artefato, seguindo o layout do repositorio:
    Airwindows/<Familia>/<Plugin>/<Plugin>_artefacts/Release/VST3/<Plugin>.vst3
    drumforge/...   -> familia "Drumforge"
    Forjarack/...   -> familia "Rack"
    Minis/...       -> familia "Minis"

  Uso (a partir da raiz do repositorio, depois de compilar):
    powershell -ExecutionPolicy Bypass -File installer/collect_payload.ps1
    powershell -ExecutionPolicy Bypass -File installer/collect_payload.ps1 -BuildRoot build -Clean
#>
[CmdletBinding()]
param(
    [string] $BuildRoot = "build",
    [string] $PayloadDir = "installer/payload",
    [switch] $Clean,
    [switch] $SkipLocked
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not [System.IO.Path]::IsPathRooted($BuildRoot)) { $BuildRoot = Join-Path $repoRoot $BuildRoot }
if (-not [System.IO.Path]::IsPathRooted($PayloadDir)) { $PayloadDir = Join-Path $repoRoot $PayloadDir }

if (-not (Test-Path -LiteralPath $BuildRoot)) {
    Write-Error "Pasta de build nao encontrada: $BuildRoot. Compile a suite antes."
    exit 1
}

# Mapeia o caminho relativo do artefato para o nome da familia mostrada no
# instalador. Regra explicita (append-only): familias novas do Airwindows caem
# no ramo padrao; bases novas entram listadas aqui.
function Get-ForjaFamily {
    param([string] $RelativePath)

    $parts = $RelativePath -split '[\\/]'
    switch ($parts[0]) {
        "Airwindows" { if ($parts.Count -ge 2) { return $parts[1] } else { return "Outros" } }
        "drumforge"  { return "Drumforge" }
        "Forjarack"  { return "Rack" }
        "Minis"      { return "Minis" }
        default      { return "Outros" }
    }
}

if ($Clean -and (Test-Path -LiteralPath $PayloadDir)) {
    Remove-Item -LiteralPath $PayloadDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $PayloadDir | Out-Null

# Um bundle .vst3 e uma PASTA. Procuramos so os que moram num .../_artefacts/Release/VST3/.
$bundles = Get-ChildItem -LiteralPath $BuildRoot -Recurse -Directory -Filter "*.vst3" -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -replace '\\', '/' -match '_artefacts/Release/VST3/[^/]+\.vst3$' }

if (-not $bundles) {
    Write-Error "Nenhum .vst3 encontrado em $BuildRoot. A build produziu artefatos VST3?"
    exit 1
}

$byFamily = [ordered]@{}
$copied = 0
$skipped = @()

foreach ($bundle in $bundles) {
    $relative = $bundle.FullName.Substring($BuildRoot.TrimEnd('\', '/').Length).TrimStart('\', '/')
    $family = Get-ForjaFamily $relative

    $dest = Join-Path $PayloadDir (Join-Path $family $bundle.Name)
    New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null

    if (Test-Path -LiteralPath $dest) { Remove-Item -LiteralPath $dest -Recurse -Force }

    try {
        Copy-Item -LiteralPath $bundle.FullName -Destination $dest -Recurse -Force
    }
    catch {
        # Em dry-run local um .vst3 pode estar travado por um build/editor aberto.
        # Na CI build e coleta rodam sequenciais, entao nada cai aqui.
        if ($SkipLocked) {
            if (Test-Path -LiteralPath $dest) { Remove-Item -LiteralPath $dest -Recurse -Force }
            $skipped += $bundle.Name
            Write-Warning ("Bundle travado, ignorado (SkipLocked): " + $bundle.Name)
            continue
        }
        throw
    }

    if (-not $byFamily.Contains($family)) { $byFamily[$family] = @() }
    $byFamily[$family] += $bundle.Name
    $copied++
}

# Copia a licenca MIT da suite para o payload: o instalador a mostra como pagina
# de licenca e a instala junto para referencia.
$repoLicense = Join-Path $repoRoot "LICENSE"
if (Test-Path -LiteralPath $repoLicense) {
    Copy-Item -LiteralPath $repoLicense -Destination (Join-Path $PayloadDir "LICENSE.txt") -Force
}

Write-Host ""
Write-Host "Payload montado em $PayloadDir" -ForegroundColor Green
Write-Host ("Bundles copiados: {0}" -f $copied)
foreach ($family in $byFamily.Keys) {
    Write-Host ("  {0,-12} -> {1}" -f $family, ($byFamily[$family] -join ", ")) -ForegroundColor Cyan
}
if ($skipped.Count -gt 0) {
    Write-Host ("  IGNORADOS (travados): " + ($skipped -join ", ")) -ForegroundColor Yellow
}
Write-Host ""
