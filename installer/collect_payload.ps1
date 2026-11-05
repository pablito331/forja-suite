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

  -OnlyPath e o filtro do trem de release por produto (ci/products.json, campo
  pathFilters): limita a coleta a um prefixo de caminho da base, sem mexer no
  agrupamento por familia. Vazio = suite inteira (comportamento historico).
  O forja.iss monta os componentes a partir do manifest escrito aqui
  (payload\forja-payload.txt: uma familia por linha, so as com conteudo), entao
  um produto so ganha o componente que foi de fato coletado e componente vazio e
  impossivel por construcao.

  Uso (a partir da raiz do repositorio, depois de compilar):
    powershell -ExecutionPolicy Bypass -File installer/collect_payload.ps1
    powershell -ExecutionPolicy Bypass -File installer/collect_payload.ps1 -BuildRoot build -Clean
    powershell -ExecutionPolicy Bypass -File installer/collect_payload.ps1 -Clean -OnlyPath Minis/Canivete -LeiamE Forjarack/Forjarack/README.md
#>
[CmdletBinding()]
param(
    [string] $BuildRoot = "build",
    [string] $PayloadDir = "installer/payload",
    [string[]] $OnlyPath = @(),
    [string] $LeiamE = "",
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
        "Pianos"     { return "Pianos" }
        "Traco"      { return "Traco" }
        default      { return "Outros" }
    }
}

# Nome do grupo no payload. Normalmente a familia (Get-ForjaFamily), mas quando
# o trem e de um plugin especifico (-OnlyPath tem tres segmentos, ex.:
# Minis/Canivete/Canivete) o grupo vira o ULTIMO segmento do filtro, que e o
# nome do plugin: a arvore do instalador diz CANIVETE em vez de MINIS.
function Get-ForjaGroup {
    param([string] $RelativePath)

    if ($OnlyPath -and $OnlyPath.Count -eq 1) {
        $bits = ($OnlyPath[0] -replace '\\', '/') -split '/'
        if ($bits.Count -ge 3) { return $bits[-1] }
    }

    return Get-ForjaFamily $RelativePath
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

# Filtro do trem de produto: o caminho relativo tem que comecar com um dos
# prefixos. Compara em barras normalizadas porque o Forjarack vive em
# "Forjarack/..." no repositorio e em "Forjarack/Forjarack/..." no build. A
# fronteira aceita '/' (pasta filha) e '_' (a pasta de artefatos do proprio
# plugin, Minis/Canivete/Canivete -> Canivete_artefacts), mas nao casa
# "Canivete" com uma irma tipo "CaniveteBackup".
function Test-ForjaSelected {
    param([string] $RelativePath)

    if (-not $OnlyPath -or $OnlyPath.Count -eq 0) { return $true }

    $normalized = ($RelativePath -replace '\\', '/')
    foreach ($filter in $OnlyPath) {
        $prefix = ($filter -replace '\\', '/').TrimStart('/', '\')
        if ($prefix -eq '' -or $prefix -eq '*') { return $true }
        if ($normalized -eq $prefix) { return $true }

        if ($normalized.Length -gt $prefix.Length -and
            $normalized.StartsWith($prefix, [System.StringComparison]::Ordinal)) {
            $next = $normalized[$prefix.Length]
            if ($next -eq '/' -or $next -eq '_') { return $true }
        }
    }
    return $false
}

$byFamily = [ordered]@{}
$copied = 0
$skipped = @()

foreach ($bundle in $bundles) {
    $relative = $bundle.FullName.Substring($BuildRoot.TrimEnd('\', '/').Length).TrimStart('\', '/')

    if (-not (Test-ForjaSelected $relative)) { continue }

    $family = Get-ForjaGroup $relative

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

if ($copied -eq 0) {
    Write-Error ("Nenhum .vst3 casou com o filtro -OnlyPath ({0}). A build daquele produto existe?" -f ($OnlyPath -join ", "))
    exit 1
}

# Copia a licenca MIT da suite para o payload: o instalador a mostra como pagina
# de licenca e a instala junto para referencia.
$repoLicense = Join-Path $repoRoot "LICENSE"
if (Test-Path -LiteralPath $repoLicense) {
    Copy-Item -LiteralPath $repoLicense -Destination (Join-Path $PayloadDir "LICENSE.txt") -Force
}

# LEIA-ME do pacote: a suite usa o guia de install da raiz; um produto pode
# embarcar o proprio README (campo readme do ci/products.json). O .iss instala
# este arquivo como LEIA-ME.txt ao lado da documentacao compartilhada.
$leiamSource = if ($LeiamE) { Join-Path $repoRoot $LeiamE } else { Join-Path $repoRoot "docs\instalar\INSTALAR-WINDOWS.txt" }
if (Test-Path -LiteralPath $leiamSource) {
    Copy-Item -LiteralPath $leiamSource -Destination (Join-Path $PayloadDir "LEIA-ME.txt") -Force
} else {
    Write-Warning ("LEIA-ME nao encontrado: {0}" -f $leiamSource)
}

# Manifest para o Inno Setup: uma familia por linha, somente as com bundle
# coletado. O .iss le isto e define em [Components]/[Files] no maximo o que
# esta aqui -- por isso um instalador de produto nunca sai com componente vazio.
Set-Content -LiteralPath (Join-Path $PayloadDir "forja-payload.txt") `
    -Value @($byFamily.Keys) -Encoding ASCII

# Espelho ISPP do manifest. O preprocessor do Inno NAO tem como iterar um
# arquivo de texto (FileRead so le a primeira linha, e #if nao chama macro com
# referencia de parametro), entao o lado facil do contrato e aqui: cada familia
# coletada vira um #define e o forja.iss so usa #IFDEF. Quem muda o catalogo de
# familias e o .iss; isto e gerado, nao mexido a mao.
# Nome de #define precisa ser identificador ascii: [A-Za-z0-9_]. Familia fora
# desse formato e descartada do include com aviso (o .iss entao nao a oferece).
$defines = @()
foreach ($family in $byFamily.Keys) {
    if ($family -match '^[A-Za-z0-9_]+$') {
        $defines += "#define ForjaColhido_$family"
    } else {
        Write-Warning ("Familia com nome nao-ascii ignorada no include ISPP: {0}" -f $family)
    }
}
Set-Content -LiteralPath (Join-Path $PayloadDir "forja-payload.isl") `
    -Value $defines -Encoding ASCII

Write-Host ""
Write-Host "Payload montado em $PayloadDir" -ForegroundColor Green
Write-Host ("Bundles copiados: {0}" -f $copied)
if ($OnlyPath -and $OnlyPath.Count -gt 0) {
    Write-Host ("  filtro: " + ($OnlyPath -join ", ")) -ForegroundColor Yellow
}
foreach ($family in $byFamily.Keys) {
    Write-Host ("  {0,-12} -> {1}" -f $family, ($byFamily[$family] -join ", ")) -ForegroundColor Cyan
}
if ($skipped.Count -gt 0) {
    Write-Host ("  IGNORADOS (travados): " + ($skipped -join ", ")) -ForegroundColor Yellow
}
Write-Host ""
