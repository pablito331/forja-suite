<#
  validate_site.ps1

  Guarda-do-site: confere se o catalogo reflete o repositorio de verdade e se a
  pagina gerada esta em dia. Roda na CI do Pages e antes de commitar.

  Checa:
   1. todo plugin declarado no CMake (forja_add_plugin / forja_add_instrument)
      tem um item no catalogo, e todo item aponta para um plugin que existe;
   2. o nome do card e o info.title do ForjaInfoText.h daquele plugin;
   3. familia e linha existem nos vocabularios do proprio catalogo;
   4. estado existe em catalogo.estados;
   5. release existe em ci/products.json (e a URL de release nao e 404 por
      construcao: produto sem tag vira pagina de releases, nao tag inventada);
   6. imagem, quando declarada, existe em site/;
   7. index.html esta identico ao que build_site.ps1 geraria (-Checks), o tema
      copiado esta em dia e os marcadores continuam la.

  Uso (a partir da raiz do repositorio):
    powershell -ExecutionPolicy Bypass -File tools/validate_site.ps1
#>
[CmdletBinding()]
param()

Set-StrictMode -Version 2
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$siteDir = Join-Path $repoRoot "site"
$failhas = 0

function Fail([string] $msg) {
    $script:failhas++
    Write-Host "FAIL $msg" -ForegroundColor Red
}

$catalogoPath = Join-Path $siteDir "catalogo.json"
if (-not (Test-Path -LiteralPath $catalogoPath)) {
    Fail "site/catalogo.json nao existe"
    exit 1
}
$catalogo = Get-Content -LiteralPath $catalogoPath -Raw -Encoding UTF8 | ConvertFrom-Json
$produtos = Get-Content -LiteralPath (Join-Path $repoRoot "ci/products.json") -Raw -Encoding UTF8 | ConvertFrom-Json

# ------------------------------------------------- plugins reais do repositorio
$alvos = @{}
$cmakes = Get-ChildItem -LiteralPath $repoRoot -Recurse -Filter CMakeLists.txt -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notmatch '\\build|\\vendor|\\JUCE|\\templates' }

foreach ($c in $cmakes) {
    $txt = Get-Content -LiteralPath $c.FullName -Raw
    foreach ($m in [regex]::Matches($txt, 'forja_add_(?:plugin|instrument)\s*\(\s*([A-Za-z0-9_]+)')) {
        $alvo = $m.Groups[1].Value
        $alvos[$alvo.ToLowerInvariant()] = [pscustomobject]@{
            Alvo    = $alvo
            Pasta   = (Split-Path $c.FullName -Parent)
        }
    }
}
if ($alvos.Count -eq 0) {
    Fail "nenhum forja_add_plugin/forja_add_instrument encontrado: o validador nao conseguiu ler o CMake"
    exit 1
}

# ------------------------------------------------------------------ vocabularios
$familias = @($catalogo.familias | ForEach-Object { $_.slug })
$linhas = @($catalogo.linhas | ForEach-Object { $_.slug })
$estados = @($catalogo.estados.PSObject.Properties.Name)
$produtosSlugs = @($produtos.PSObject.Properties.Name | Where-Object { $_ -notmatch '^_' })
$itens = @($catalogo.itens)

$slugsVistos = @{}
foreach ($item in $itens) {
    $slug = [string] $item.slug
    if (-not $slug) { Fail "item sem slug no catalogo"; continue }
    if ($slugsVistos.ContainsKey($slug)) { Fail "slug duplicado no catalogo: $slug" }
    $slugsVistos[$slug] = $item

    foreach ($campo in "nome", "familia", "linha", "resumo", "estado", "release") {
        if (-not ([string] $item.$campo)) { Fail "${slug}: campo obrigatorio '$campo' vazio" }
    }

    # 1 e 5: plugin existe, release existe
    if (-not $alvos.ContainsKey($slug)) {
        Fail "${slug}: nao ha alvo CMake forja_add_* com esse nome (plugin removido ou slug errado)"
    }
    if ($produtosSlugs -notcontains [string] $item.release) {
        Fail "${slug}: release '$($item.release)' nao esta em ci/products.json"
    }

    # 2 e 3: vocabulario
    if ($familias -notcontains [string] $item.familia) {
        Fail "${slug}: familia '$($item.familia)' nao esta em catalogo.familias"
    }
    if ($linhas -notcontains [string] $item.linha) {
        Fail "${slug}: linha '$($item.linha)' nao esta em catalogo.linhas"
    }
    if ($estados -notcontains [string] $item.estado) {
        Fail "${slug}: estado '$($item.estado)' nao existe em catalogo.estados"
    }

    # 6: imagem
    $img = [string] $item.imagem
    if ($img) {
        if (-not (Test-Path -LiteralPath (Join-Path $siteDir ($img -replace '/', '\')))) {
            Fail "${slug}: imagem declarada nao existe: site/$img"
        }
    }
    else {
        Write-Host "aviso $slug sem captura (card sai como 'sem captura')" -ForegroundColor Yellow
    }
}

# 1 (direcao contraria): todo plugin do repositorio tem card
foreach ($slug in ($alvos.Keys | Sort-Object)) {
    if (-not $slugsVistos.ContainsKey($slug)) {
        Fail "plugin $($alvos[$slug].Alvo) nao esta em site/catalogo.json (registre o card)"
    }
}

# 2: nome do card == info.title do plugin
foreach ($item in $itens) {
    $slug = [string] $item.slug
    if (-not $alvos.ContainsKey($slug)) { continue }

    $infoPath = Join-Path $alvos[$slug].Pasta "Source\ForjaInfoText.h"
    if (-not (Test-Path -LiteralPath $infoPath)) {
        Fail "${slug}: sem Source/ForjaInfoText.h"
        continue
    }
    $txt = Get-Content -LiteralPath $infoPath -Raw -Encoding UTF8
    $m = [regex]::Match($txt, 'info\.title\s*=\s*(?:u\s*)?\(?\s*"([^"]*)"')
    if (-not $m.Success) {
        Fail "${slug}: nao achei info.title em Source/ForjaInfoText.h"
        continue
    }
    $title = $m.Groups[1].Value
    $title = [regex]::Replace($title, '\\u([0-9a-fA-F]{4})', {
        param($mm) [char]::ConvertFromUtf32([Convert]::ToInt32($mm.Groups[1].Value, 16))
    })
    if ($title.ToLowerInvariant() -ne ([string] $item.nome).ToLowerInvariant()) {
        Fail "${slug}: card diz '$($item.nome)' mas o plugin se apresenta como '$title'"
    }

    # 5: o resumo do card deve vir do tagline do proprio plugin
    $tm = [regex]::Match($txt, '(?s)info\.tagline\s*=\s*u\s*\(([^;]*?)\)\s*;')
    if ($tm.Success) {
        $pedacos = [regex]::Matches($tm.Groups[1].Value, '"((?:\\.|[^"\\])*)"')
        $bruto = ""
        foreach ($p in $pedacos) { $bruto += $p.Groups[1].Value }
        $bruto = [regex]::Replace($bruto, '\\u([0-9a-fA-F]{4})', {
            param($mm) [char]::ConvertFromUtf32([Convert]::ToInt32($mm.Groups[1].Value, 16))
        })
        $esperado = ($bruto -replace '\s+', ' ').Trim()
        $actual = (([string] $item.resumo) -replace '\s+', ' ').Trim()
        if ($esperado -ne $actual) {
            Fail "${slug}: resumo diferente do info.tagline do plugin"
        }
    }
}

# 7: pagina gerada esta em dia (inclui a copia do tema e os marcadores)
$build = Join-Path $PSScriptRoot "build_site.ps1"
& $build -Checks
if ($LASTEXITCODE -ne 0) { Fail "build_site.ps1 -Checks respondeu $LASTEXITCODE" }

Write-Host ""
if ($failhas -gt 0) {
    Write-Host "$failhas problema(s) no site" -ForegroundColor Red
    exit 1
}
Write-Host ("PASS: site coerente com {0} plugins em {1} familias" -f $alvos.Count, $familias.Count)
