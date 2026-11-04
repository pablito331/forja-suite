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

# ------------------------------------------------------------- FUNDAMENTOS (.h)
# As paginas de produto saem do mesmo ForjaInfoText.h que o painel FUNDAMENTOS
# do plugin: fonte unica. O parser le o padrao fixo dos headers (info.title /
# subtitle / tagline, sections.push_back, controls.push_back, provenance.x) e
# decodifica os escapes \uXXXX obrigatorios pela regra de ASCII do C++.

function Decode-Esc([string] $s) {
    $s = [regex]::Replace($s, '\\u([0-9a-fA-F]{4})', {
        param($mm) [char]::ConvertFromUtf32([Convert]::ToInt32($mm.Groups[1].Value, 16))
    })
    $s = $s -replace '\\"', '"'
    $s = $s -replace '\\\\', '\'
    return $s
}

# Varre o trecho caractere a caractere com maquina de estados de literal C++:
# dentro de "..." (com \) tudo e texto; fora, virgula separa campo. Regex nao
# serve: ';' e ')' aparecem dentro das frases e uma regex "quebra corda"
# engole os separadores (closing quote de um campo vira opening do proximo).
function Get-Cadeias {
    param([string] $Corpo)

    $partes = @()
    $emLit = $false
    $escapa = $false
    $novo = ""
    for ($i = 0; $i -lt $Corpo.Length; $i++) {
        $c = $Corpo[$i]
        if ($emLit) {
            if ($escapa) { $novo += $c; $escapa = $false }
            elseif ($c -eq '\') { $novo += $c; $escapa = $true }
            elseif ($c -eq '"') { $emLit = $false }
            else { $novo += $c }
        }
        else {
            if ($c -eq '"') { $emLit = $true }
            elseif ($c -eq ',') { if ($novo) { $partes += (Decode-Esc $novo) }; $novo = "" }
        }
    }
    if ($novo) { $partes += (Decode-Esc $novo) }
    return $partes
}

# junta todas as literais de um trecho (as partes adjacentes do C++ se
# concatenam na virgula zero): um campo sem virgula e uma frase so
function Get-Literals([string] $corpo) {
    return (@(Get-Cadeias $corpo) -join "")
}

# divide em campos pelas virgulas que estao FORA de literal
function Split-Partes([string] $corpo) {
    return @(Get-Cadeias $corpo)
}

# indice do ponto e virgula que fecha a instrucao comecando em $Desde (o
# primeiro ';' fora das aspas; os de dentro das frases nao contam)
function Get-PosFimInstrucao([string] $Texto, [int] $Desde) {
    $emLit = $false
    $escapa = $false
    for ($i = $Desde; $i -lt $Texto.Length; $i++) {
        $c = $Texto[$i]
        if ($emLit) {
            if ($escapa) { $escapa = $false }
            elseif ($c -eq '\') { $escapa = $true }
            elseif ($c -eq '"') { $emLit = $false }
        }
        else {
            if ($c -eq '"') { $emLit = $true }
            elseif ($c -eq ';') { return $i }
        }
    }
    return $Texto.Length
}

function Get-ForjaInfo([string] $Caminho) {
    $txt = [System.IO.File]::ReadAllText($Caminho, [System.Text.Encoding]::UTF8)

    $info = [pscustomobject]@{
        Title = ""; Subtitle = ""; Tagline = ""
        Sections = @(); Controls = @(); Provenance = @{}
    }

    foreach ($campo in "title", "subtitle", "tagline") {
        $m = [regex]::Match($txt, "(?s)info\.$campo\s*=\s*")
        if ($m.Success) {
            $fim = Get-PosFimInstrucao $txt ($m.Index + $m.Length)
            $corpo = $txt.Substring($m.Index + $m.Length, $fim - $m.Index - $m.Length)
            switch ($campo) {
                "title"    { $info.Title = Get-Literals $corpo }
                "subtitle" { $info.Subtitle = Get-Literals $corpo }
                "tagline"  { $info.Tagline = Get-Literals $corpo }
            }
        }
    }

    foreach ($m in [regex]::Matches($txt, "(?s)info\.sections\.push_back\s*\(\s*\{(.+?)\}\s*\)\s*;")) {
        $sm = [regex]::Match($m.Groups[1].Value, "(?s)^(?<h>.*?),\s*\{(?<b>.*)\}\s*$")
        if (-not $sm.Success) { continue }
        $info.Sections += [pscustomobject]@{
            Heading = (Get-Literals $sm.Groups["h"].Value)
            Paragraphs = @(Split-Partes $sm.Groups["b"].Value)
        }
    }

    foreach ($m in [regex]::Matches($txt, "(?s)info\.controls\.push_back\s*\(\s*\{([^{}]*)\}\s*\)\s*;")) {
        $p = @(Split-Partes $m.Groups[1].Value)
        while ($p.Count -lt 3) { $p += "" }
        $info.Controls += [pscustomobject]@{
            Name = $p[0]; Range = $p[1]; Description = $p[2]
        }
    }

    foreach ($m in [regex]::Matches($txt, "(?s)info\.provenance\.(\w+)\s*=\s*")) {
        $fim = Get-PosFimInstrucao $txt ($m.Index + $m.Length)
        $corpo = $txt.Substring($m.Index + $m.Length, $fim - $m.Index - $m.Length)
        $info.Provenance[$m.Groups[1].Value] = Get-Literals $corpo
    }

    return $info
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

# ------------------------------------------------------------------ paginas de produto
# As linhas Forja ganham pagina propria (plugins/<linha>.html) montada a partir
# do FUNDAMENTOS de cada plugin; a linha airwindows nao tem pagina: o card rola
# para a primeira secao de familia dela, onde os proprios cards tem Download.
# O catalogo declara a linha com "pagina": true; o mapa abaixo so liga o slug do
# item ao ForjaInfoText.h de origem (mesmo caminho que o validador usa).
$infoPorSlug = @{
    forjarack  = "Forjarack/Forjarack/Source/ForjaInfoText.h"
    canivete   = "Minis/Canivete/Source/ForjaInfoText.h"
    minidrum   = "Minis/Minidrum/Source/ForjaInfoText.h"
    minisat    = "Minis/Minisat/Source/ForjaInfoText.h"
    drumforge  = "drumforge/Drumforge/Source/ForjaInfoText.h"
    pianoforge = "Pianos/Pianoforge/Source/ForjaInfoText.h"
}

function Test-LinhaPagina($linha) {
    $v = Campo $linha "pagina"
    return ($v -and $v -ne "False")
}

$linhasComPagina = @($catalogo.linhas | Where-Object { Test-LinhaPagina $_ } | ForEach-Object { $_.slug })

# primeira familia que abriga itens da linha sem pagina (ancora do card airwindows)
function Get-AncoraLinha([string] $linhaSlug) {
    foreach ($fam in $catalogo.familias) {
        $itensFam = @($catalogo.itens | Where-Object { $_.familia -eq $fam.slug -and (Campo $_ "linha") -eq $linhaSlug })
        if ($itensFam.Count -gt 0) { return $fam.slug.ToLowerInvariant() }
    }
    return ""
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
    $atributos = ' class="' + $classe + '"'
    $slug = Campo $item "slug"
    $familia = Campo $item "familia"
    $linhaSlug = Campo $item "linha"
    if ($linha) {
        # card de linha e ancora de destino ("Saiba mais" dos itens sem pagina)
        $atributos += ' id="' + (Esc-Html ("linha-" + $slug)) + '"'
    }
    else {
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
    # "Saiba mais" substitui o Download onde existe destino melhor:
    #  - linha com pagina: plugins/<linha>.html (o Download mora la dentro)
    #  - item de linha com pagina: ancora na pagina do produto
    #  - item de linha sem pagina (airwindows): rola para a familia de origem,
    #    onde os proprios cards guardam o Download
    $linhaDoItem = if ($linha) { $slug } else { $linhaSlug }
    $temPagina = $linhasComPagina -contains $linhaDoItem
    if ($temPagina) {
        $destino = "plugins/" + $linhaDoItem + ".html"
        if (-not $linha) { $destino += "#" + $slug }
        $acoes += ('<a class="botao botao--sm" href="' + (Esc-Html $destino) + '">' + (Esc-Html $catalogo.rotulos.saibaMais) + '</a>')
    }
    elseif ($infoEstado.download -and -not $linha) {
        $acoes += ('<a class="botao botao--sm" href="' + (Esc-Html (Get-UrlRelease (Campo $item "release"))) + '">' + (Esc-Html $catalogo.rotulos.download) + '</a>')
    }
    elseif ($linha -and (Campo $item "release")) {
        $ancora = Get-AncoraLinha $slug
        if ($ancora) {
            $acoes += ('<a class="botao botao--sm" href="#' + (Esc-Html $ancora) + '">' + (Esc-Html $catalogo.rotulos.saibaMais) + '</a>')
        }
        else {
            $acoes += ('<a class="botao botao--sm" href="' + (Esc-Html (Get-UrlRelease (Campo $item "release"))) + '">' + (Esc-Html $catalogo.rotulos.download) + '</a>')
        }
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

# ------------------------------------------------------------------ paginas geradas
# Uma pagina por linha Forja: site/plugins/<linha>.html. Conteudo 100% derivado
# do FUNDAMENTOS do plugin (ForjaInfoText.h) + catalogo; nada e escrito a mao
# aqui, entao a pagina enriquece sozinha conforme o painel do plugin melhora.
function Render-Pagina {
    param($linha)

    $buf = Novo-Buf
    $nomeLinha = Campo $linha "nome"
    $urlDownload = Get-UrlRelease (Campo $linha "release")
    $itensLinha = @($catalogo.itens | Where-Object { (Campo $_ "linha") -eq $linha.slug })

    Add-Linha $buf '<!DOCTYPE html>'
    Add-Linha $buf '<html lang="pt-BR">'
    Add-Linha $buf '<head>'
    Add-Linha $buf '<meta charset="utf-8">'
    Add-Linha $buf '<meta name="viewport" content="width=device-width, initial-scale=1">'
    Add-Linha $buf ('<title>' + (Esc-Html $nomeLinha) + ' &#183; Forja</title>')
    Add-Linha $buf ('<meta name="description" content="' + (Esc-Html (Campo $linha "resumo")) + '">')
    Add-Linha $buf '<meta name="color-scheme" content="dark">'
    Add-Linha $buf '<link rel="stylesheet" href="../forja-site.css?v=2">'
    Add-Linha $buf '</head>'
    Add-Linha $buf '<body>'
    Add-Linha $buf ''
    Add-Linha $buf '<header class="pagina__topo">'
    Add-Linha $buf '  <a class="pagina__voltar" href="../index.html">&#8592; Cat&#225;logo Forja</a>'
    Add-Linha $buf ('  <p class="pagina__marca">' + (Esc-Html $nomeLinha) + '</p>')
    Add-Linha $buf '</header>'
    Add-Linha $buf ''
    Add-Linha $buf '<main class="conteudo pagina">'
    Add-Linha $buf '  <div class="pagina__hero">'
    Add-Linha $buf ('    <h1>' + (Esc-Html $nomeLinha) + '</h1>')
    Add-Linha $buf ('    <p>' + (Esc-Html (Campo $linha "resumo")) + '</p>')
    Add-Linha $buf ('    <p class="card__acoes"><a class="botao" href="' + (Esc-Html $urlDownload) + '">' + (Esc-Html $catalogo.rotulos.download) + '</a></p>')
    Add-Linha $buf '  </div>'

    foreach ($item in $itensLinha) {
        $slug = Campo $item "slug"
        $caminhoInfo = Join-Path $repoRoot ($infoPorSlug[$slug] -replace '/', '\')
        if (-not (Test-Path -LiteralPath $caminhoInfo)) {
            throw "ForjaInfoText.h nao encontrado para '$slug': $caminhoInfo"
        }
        $info = Get-ForjaInfo $caminhoInfo

        Add-Linha $buf ''
        Add-Linha $buf ('  <section class="produto" id="' + (Esc-Html $slug) + '">')
        Add-Linha $buf ('    <h2>' + (Esc-Html $info.Title) + '</h2>')
        if ($info.Subtitle) {
            Add-Linha $buf ('    <p class="produto__sub">' + (Esc-Html $info.Subtitle) + '</p>')
        }

        $caminhoImg = Campo $item "imagem"
        if ($caminhoImg -and (Test-Path -LiteralPath (Join-Path $SiteDir $caminhoImg))) {
            $m = Get-Medida $caminhoImg
            $attr = ""
            if ($m) { $attr = ' width="' + $m.W + '" height="' + $m.H + '"' }
            Add-Linha $buf ('    <figure class="produto__fig"><img src="../' + (Esc-Html $caminhoImg) + '" alt="' + (Esc-Html ("Interface do plugin " + $info.Title)) + '"' + $attr + ' loading="lazy"></figure>')
        }

        if ($info.Tagline) {
            Add-Linha $buf ('    <p class="produto__tagline">' + (Esc-Html $info.Tagline) + '</p>')
        }

        foreach ($sec in $info.Sections) {
            Add-Linha $buf ('    <h3 class="produto__secao">' + (Esc-Html $sec.Heading) + '</h3>')
            foreach ($par in $sec.Paragraphs) {
                Add-Linha $buf ('    <p>' + (Esc-Html $par) + '</p>')
            }
        }

        if (@($info.Controls).Count -gt 0) {
            Add-Linha $buf '    <h3 class="produto__secao">Controles</h3>'
            Add-Linha $buf '    <table class="produto__tabela">'
            Add-Linha $buf '      <thead><tr><th>Controle</th><th>Faixa</th><th>O que faz</th></tr></thead>'
            Add-Linha $buf '      <tbody>'
            foreach ($ctl in $info.Controls) {
                Add-Linha $buf ('        <tr><td>' + (Esc-Html $ctl.Name) + '</td><td>' + (Esc-Html $ctl.Range) + '</td><td>' + (Esc-Html $ctl.Description) + '</td></tr>')
            }
            Add-Linha $buf '      </tbody>'
            Add-Linha $buf '    </table>'
        }

        if ($info.Provenance.Count -gt 0) {
            Add-Linha $buf '    <h3 class="produto__secao">Proven&#238;ncia</h3>'
            Add-Linha $buf '    <dl class="produto__prov">'
            $rotulosProv = @{ source = "Fonte"; author = "Autor"; license = "Licen&#231;a"; commit = "Commit"; notes = "Notas" }
            foreach ($chave in @("source", "author", "license", "commit", "notes")) {
                $val = $info.Provenance[$chave]
                if ($val) {
                    Add-Linha $buf ('      <dt>' + $rotulosProv[$chave] + '</dt><dd>' + (Esc-Html $val) + '</dd>')
                }
            }
            Add-Linha $buf '    </dl>'
        }

        Add-Linha $buf ('    <p class="card__acoes"><a class="botao botao--sm" href="' + (Esc-Html $urlDownload) + '">' + (Esc-Html $catalogo.rotulos.download) + '</a></p>')
        Add-Linha $buf '  </section>'
    }

    Add-Linha $buf '</main>'
    Add-Linha $buf ''
    Add-Linha $buf '<footer class="rodape">'
    Add-Linha $buf ('  <p>' + (Esc-Html $nomeLinha) + ' &#183; Forja &#8212; <a href="../index.html">voltar ao cat&#225;logo</a></p>')
    Add-Linha $buf '</footer>'
    Add-Linha $buf '</body>'
    Add-Linha $buf '</html>'

    return $buf.ToString().TrimEnd("`r", "`n") + "`n"
}

$paginas = @{}
foreach ($linha in $catalogo.linhas) {
    if (-not (Test-LinhaPagina $linha)) { continue }
    $paginas[(Join-Path $SiteDir ("plugins\" + $linha.slug + ".html"))] = Render-Pagina $linha
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
    foreach ($paginaPath in $paginas.Keys) {
        if (-not (Test-Path -LiteralPath $paginaPath)) {
            Write-Error ("pagina de produto nao existe: " + $paginaPath + ". Rode tools/build_site.ps1.")
            exit 1
        }
        $paginaAtual = [System.IO.File]::ReadAllText($paginaPath, [System.Text.Encoding]::UTF8) -replace "`r`n", "`n"
        if ($paginaAtual -ne $paginas[$paginaPath]) {
            Write-Error ("pagina desatualizada: " + $paginaPath + ". Rode tools/build_site.ps1 e commithe o resultado.")
            exit 1
        }
    }
    Write-Host "build_site: index.html, tema e paginas de produto estao em sincronia com o catalogo"
    exit 0
}

[System.IO.File]::WriteAllText($indexPath, $index, $utf8)
if ($themeNovo) { [System.IO.File]::WriteAllText($themeDst, $themeNovo, $utf8) }
if ($paginas.Count -gt 0) {
    $dirPaginas = Join-Path $SiteDir "plugins"
    if (-not (Test-Path -LiteralPath $dirPaginas)) { [void] (New-Item -ItemType Directory -Path $dirPaginas) }
    foreach ($paginaPath in $paginas.Keys) {
        [System.IO.File]::WriteAllText($paginaPath, $paginas[$paginaPath], $utf8)
    }
}

Write-Host ("build_site: " + $catalogo.itens.Count + " itens em " + $catalogo.familias.Count + " familias e " + $paginas.Count + " paginas de produto -> " + $indexPath)
