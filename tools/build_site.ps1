<#
  build_site.ps1

  Materializa a pagina da Forja: le site/catalogo.json, gera os blocos de HTML
  (navegacao, cards de linha, secoes por familia) e injeta entre os marcadores
  <!--FORJA:XXX--> / <!--/FORJA:XXX--> de site/index.html. O resultado e HTML
  estatico, indexavel, sem JavaScript: o Pages so entrega a pasta site/ como ela
  esta, entao o index.html gerado precisa estar commitado.

  Tambem copia o tema compartilhado (forja-ui-theme.css da raiz) para dentro de
  site/, porque o artefato do Pages e a pasta site/ e o @import do forja-site.css
  resolve relativo a ela.

  Uso (a partir de qualquer pasta):
    powershell -ExecutionPolicy Bypass -File tools/build_site.ps1
    powershell -ExecutionPolicy Bypass -File tools/build_site.ps1 -Checks
      # so compara o index.html com o catalogo, sem escrever (modo do CI)
#>
[CmdletBinding()]
param(
    [switch] $Checks,
    [string] $SiteDir = "",
    [string] $CatalogoPath = ""
)

Set-StrictMode -Version 2
$ErrorActionPreference = "Stop"

# Le um campo opcional do catalogo sem esbarar no StrictMode.
function Campo {
    param($Obj, [string] $Nome)
    if ($null -eq $Obj) { return "" }
    if ($Obj.PSObject.Properties.Name -contains $Nome) {
        $v = $Obj.$Nome
        if ($null -eq $v) { return "" }
        return [string] $v
    }
    return ""
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $SiteDir) { $SiteDir = Join-Path $repoRoot "site" }
if (-not $CatalogoPath) { $CatalogoPath = Join-Path $SiteDir "catalogo.json" }

$indexPath = Join-Path $SiteDir "index.html"
$themeSrc = Join-Path $repoRoot "forja-ui-theme.css"
$themeDst = Join-Path $SiteDir "forja-ui-theme.css"
$utf8 = New-Object System.Text.UTF8Encoding($false)

if (-not (Test-Path -LiteralPath $CatalogoPath)) {
    Write-Error "catalogo nao encontrado: $CatalogoPath"
    exit 1
}
if (-not (Test-Path -LiteralPath $indexPath)) {
    Write-Error "index.html nao encontrado: $indexPath"
    exit 1
}

$catalogo = Get-Content -LiteralPath $CatalogoPath -Raw -Encoding UTF8 | ConvertFrom-Json
$produtos = Get-Content -LiteralPath (Join-Path $repoRoot "ci/products.json") -Raw -Encoding UTF8 | ConvertFrom-Json

function Esc-Html([string] $s) {
    if ($null -eq $s) { return "" }
    $s = $s -replace '&', '&amp;'
    $s = $s -replace '<', '&lt;'
    $s = $s -replace '>', '&gt;'
    $s = $s -replace '"', '&quot;'
    return $s
}

# ------------------------------------------------------------------ URLs de release
$repo = "pablito331/forja-suite"
if ($catalogo.PSObject.Properties.Name -contains "repo" -and $catalogo.repo) { $repo = $catalogo.repo }
$base = "https://github.com/$repo"

function Get-TagMaisRecente {
    param([string] $Prefixo)

    if (-not (Get-Command git -ErrorAction SilentlyContinue)) { return "" }
    try {
        $tags = & git -C $repoRoot tag --list "$Prefixo*" 2>$null
    }
    catch { return "" }
    if (-not $tags) { return "" }

    # ordem de versao: mesmo truque do CI (sort -V) resolvido no PowerShell
    $ordenadas = $tags | ForEach-Object {
        $t = $_.Trim()
        $v = $t.Substring($Prefixo.Length)
        $pedacos = $v -split '[.-]'
        $chave = (@($pedacos | ForEach-Object {
            if ($_ -match '^\d+$') { '{0:d6}' -f [int]$_ } else { $_ }
        }) -join '.')
        [pscustomobject]@{ Tag = $t; Chave = $chave }
    } | Sort-Object Chave
    return (@($ordenadas | Select-Object -Last 1).Tag)
}

function Get-UrlRelease {
    param([string] $Slug)

    if ($Slug -eq "suite" -or -not $Slug) { return "$base/releases/latest" }

    $nome = $Slug
    if (-not $produtos.PSObject.Properties.Name -contains $nome) { return "$base/releases" }
    $prefixo = $produtos.$nome.tagPrefix
    if (-not $prefixo) { return "$base/releases" }
    if (-not $prefixo.EndsWith("-")) { $prefixo = $prefixo + "-" }

    $tag = Get-TagMaisRecente -Prefixo $prefixo
    if ($tag) { return "$base/releases/tag/$tag" }
    return "$base/releases"
}

# ----------------------------------------------------------------------- render
function Novo-Buf { return (New-Object System.Text.StringBuilder) }
function Add-Linha($buf, [string] $s) { [void] $buf.Append($s).Append("`n") }

# Dimensao intrinseca de cada PNG: entra no width/height do <img> para o
# navegador reservar o espaco certo (os Minis sao 560x260, o resto 1000x540).
$medidas = @{}
function Get-Medida {
    param([string] $Caminho)

    $pleno = Join-Path $SiteDir $Caminho
    if ($medidas.ContainsKey($pleno)) { return $medidas[$pleno] }

    $m = $null
    try {
        Add-Type -AssemblyName System.Drawing -ErrorAction Stop
        $img = [System.Drawing.Image]::FromFile($pleno)
        $m = [pscustomobject]@{ W = $img.Width; H = $img.Height }
        $img.Dispose()
    }
    catch { $m = $null }
    $medidas[$pleno] = $m
    return $m
}

function Render-Imagem {
    param($item, [string] $alt)

    $caminho = Campo $item "imagem"
    if (-not $caminho -or -not (Test-Path -LiteralPath (Join-Path $SiteDir $caminho))) {
        return '      <div class="card__fig card__fig--sem-imagem">' + (Esc-Html $catalogo.rotulos.semImagem) + '</div>'
    }
    $atributos = ""
    $m = Get-Medida $caminho
    if ($m) { $atributos = ' width="' + $m.W + '" height="' + $m.H + '"' }
    return '      <div class="card__fig"><img src="' + (Esc-Html $caminho) + '" alt="' + (Esc-Html $alt) + '"' + $atributos + ' loading="lazy"></div>'
}

function Render-Card {
    param($item, [bool] $linha = $false)

    $estado = Campo $item "estado"
    if (-not $estado) { $estado = "publicado" }
    if ($catalogo.estados.PSObject.Properties.Name -notcontains $estado) {
        throw "estado desconhecido no catalogo: $estado (slug $($item.slug))"
    }
    $infoEstado = $catalogo.estados.$estado

    $nome = Campo $item "nome"
    $classe = "card"
    if ($linha) { $classe = "card card--linha" }

    $buf = Novo-Buf
    # so os cards de plugin recebem id (os de linha compartem o nome da secao)
    $atributos = ' class="' + $classe + '"'
    $slug = Campo $item "slug"
    $familia = Campo $item "familia"
    $linhaSlug = Campo $item "linha"
    if (-not $linha) {
        $atributos += ' id="' + (Esc-Html $slug) + '"'
        if ($linhaSlug) { $atributos += ' data-linha="' + (Esc-Html $linhaSlug) + '"' }
        if ($familia) { $atributos += ' data-familia="' + (Esc-Html $familia) + '"' }
    }
    $atributos += ' data-estado="' + (Esc-Html $estado) + '"'
    Add-Linha $buf ('    <article' + $atributos + '>')
    $alt = "Interface do plugin " + $nome
    if ($linha) { $alt = "Interface de " + $nome }
    Add-Linha $buf (Render-Imagem $item $alt)
    Add-Linha $buf '      <div class="card__corpo">'
    Add-Linha $buf '        <div class="card__cabeca">'
    Add-Linha $buf ('          <h3 class="card__nome">' + (Esc-Html $nome) + '</h3>')
    Add-Linha $buf ('          <span class="selo">' + (Esc-Html (Campo $infoEstado "selo")) + '</span>')
    Add-Linha $buf '        </div>'

    $sub = Campo $item "sub"
    if ($sub) {
        Add-Linha $buf ('        <p class="card__sub">' + (Esc-Html $sub) + '</p>')
    }
    Add-Linha $buf ('        <p class="card__resumo">' + (Esc-Html (Campo $item "resumo")) + '</p>')

    $acoes = @()
    if ($infoEstado.download) {
        $acoes += ('<a class="botao botao--sm" href="' + (Esc-Html (Get-UrlRelease (Campo $item "release"))) + '">' + (Esc-Html $catalogo.rotulos.download) + '</a>')
    }
    $video = Campo $item "video"
    if ($video) {
        $acoes += ('<a class="card__link-video" href="' + (Esc-Html $video) + '">' + (Esc-Html $catalogo.rotulos.video) + '</a>')
    }
    if ($acoes.Count -gt 0) {
        Add-Linha $buf ('        <p class="card__acoes">' + ($acoes -join " ") + '</p>')
    }

    Add-Linha $buf '      </div>'
    Add-Linha $buf '    </article>'
    return $buf.ToString().TrimEnd("`r", "`n")
}

# navegacao: uma ancora por familia, na ordem do catalogo
$bufNav = Novo-Buf
foreach ($fam in $catalogo.familias) {
    Add-Linha $bufNav ('      <li><a href="#' + (Esc-Html $fam.slug.ToLowerInvariant()) + '">' + (Esc-Html $fam.nome) + '</a></li>')
}

$bufLinhas = Novo-Buf
for ($i = 0; $i -lt $catalogo.linhas.Count; $i++) {
    if ($i -gt 0) { Add-Linha $bufLinhas "" }
    Add-Linha $bufLinhas (Render-Card $catalogo.linhas[$i] $true)
}

$bufSecoes = Novo-Buf
foreach ($fam in $catalogo.familias) {
    $itens = @($catalogo.itens | Where-Object { $_.familia -eq $fam.slug })
    if ($itens.Count -eq 0) { continue }

    if ($bufSecoes.Length -gt 0) { Add-Linha $bufSecoes "" }
    Add-Linha $bufSecoes ('<section class="secao" id="' + (Esc-Html $fam.slug.ToLowerInvariant()) + '">')
    Add-Linha $bufSecoes ('  <h2>' + (Esc-Html $fam.nome) + '</h2>')
    Add-Linha $bufSecoes '  <div class="grade">'
    for ($i = 0; $i -lt $itens.Count; $i++) {
        if ($i -gt 0) { Add-Linha $bufSecoes "" }
        Add-Linha $bufSecoes (Render-Card $itens[$i])
    }
    Add-Linha $bufSecoes '  </div>'
    Add-Linha $bufSecoes '</section>'
}

# --------------------------------------------------------------------- injecao
function Set-Regiao {
    param([string] $Texto, [string] $Nome, [string] $Conteudo)

    $abre = "<!--FORJA:$Nome-->"
    $fecha = "<!--/FORJA:$Nome-->"
    $i = $Texto.IndexOf($abre)
    $j = $Texto.IndexOf($fecha)
    if ($i -lt 0 -or $j -lt 0 -or $j -lt $i) {
        throw "marcador $abre / $fecha nao encontrado no index.html"
    }

    $inicio = $i + $abre.Length
    if ($Texto[$inicio] -eq "`r") { $inicio++ }
    if ($Texto[$inicio] -eq "`n") { $inicio++ }

    return $Texto.Substring(0, $inicio) + $Conteudo + "`n" + $Texto.Substring($j)
}

$index = [System.IO.File]::ReadAllText($indexPath, [System.Text.Encoding]::UTF8)
$index = $index -replace "`r`n", "`n"
$index = Set-Regiao $index "NAVEGACAO" $bufNav.ToString().TrimEnd("`r", "`n")
$index = Set-Regiao $index "LINHAS" $bufLinhas.ToString().TrimEnd("`r", "`n")
$index = Set-Regiao $index "SECOES" $bufSecoes.ToString().TrimEnd("`r", "`n")

# copia do tema compartilhado (so quando mudou)
$themeNovo = $null
if (Test-Path -LiteralPath $themeSrc) {
    $themeNovo = [System.IO.File]::ReadAllText($themeSrc, [System.Text.Encoding]::UTF8) -replace "`r`n", "`n"
}

if ($Checks) {
    $atual = [System.IO.File]::ReadAllText($indexPath, [System.Text.Encoding]::UTF8) -replace "`r`n", "`n"
    if ($atual -ne $index) {
        # Mostra a primeira linha que diverge: sem isso o erro empurra para
        # "rode o gerador" quando a causa real costuma ser outra - o link de
        # Download de um produto depende das tags do repositorio, entao um clone
        # raso (CI, shallow) gera outra pagina que o commit feito com tags.
        $linhasAtual = $atual -split "`n"
        $linhasNovas = $index -split "`n"
        $dif = -1
        for ($i = 0; $i -lt [Math]::Min($linhasAtual.Count, $linhasNovas.Count); $i++) {
            if ($linhasAtual[$i] -ne $linhasNovas[$i]) { $dif = $i; break }
        }
        if ($dif -ge 0) {
            Write-Output ("linha {0} commitada: {1}" -f ($dif + 1), $linhasAtual[$dif].Trim().Substring(0, [Math]::Min(140, $linhasAtual[$dif].Trim().Length)))
            Write-Output ("linha {0} gerada:    {1}" -f ($dif + 1), $linhasNovas[$dif].Trim().Substring(0, [Math]::Min(140, $linhasNovas[$dif].Trim().Length)))
        }
        if (-not (& git -C $repoRoot tag --list 2>$null)) {
            Write-Error "site/index.html nao bate: este clone nao tem tags, e o link de Download de um produto e derivado das tags. Precisa de fetch completo (git fetch --tags; na CI, fetch-depth: 0)."
            exit 1
        }
        Write-Error "site/index.html esta desatualizado em relacao a site/catalogo.json. Rode tools/build_site.ps1 e commithe o resultado."
        exit 1
    }
    if ($themeNovo -and ((Test-Path -LiteralPath $themeDst) -eq $false)) {
        Write-Error "site/forja-ui-theme.css nao existe. Rode tools/build_site.ps1."
        exit 1
    }
    if ($themeNovo) {
        $temaAtual = [System.IO.File]::ReadAllText($themeDst, [System.Text.Encoding]::UTF8) -replace "`r`n", "`n"
        if ($temaAtual -ne $themeNovo) {
            Write-Error "site/forja-ui-theme.css esta diferente de forja-ui-theme.css da raiz. Rode tools/build_site.ps1."
            exit 1
        }
    }
    Write-Host "build_site: index.html e o tema estao em sincronia com o catalogo"
    exit 0
}

[System.IO.File]::WriteAllText($indexPath, $index, $utf8)
if ($themeNovo) { [System.IO.File]::WriteAllText($themeDst, $themeNovo, $utf8) }

Write-Host ("build_site: " + $catalogo.itens.Count + " itens em " + $catalogo.familias.Count + " familias -> " + $indexPath)
