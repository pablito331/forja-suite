<#
  capture_site.ps1

  Enche site/assets com as capturas da pagina da Forja.

  -FromDocs (padrao): copia os PNGs que a suite ja tem em docs/images/ para
  site/assets/<slug>.png. Idempotente; -Force reescreve.

  -Build: para os itens que ainda sem captura, configura build-capture/ com
  -DFORJA_STANDALONE=ON, compila o alvo Standalone do plugin, abre o .exe,
  espera a janela e captura o conteudo da area util (PrintWindow com
  PW_RENDERFULLCONTENT e, se vier preto, CopyFromScreen da janela em primeiro
  plano). A captura exige sessao interativa: sem janela visivel o script avisa
  e mantem o card como "sem captura", sem bloquear o resto.

  A area util do Standalone traz a barra de titulo do JUCE, o botao Options e a
  faixa "Audio input is muted" acima do editor. O PNG gravado e o recorte do
  canvas: a largura logica do editor (1000 dos grandes, 560 dos compactos -- ver
  ForjaEditorBase::getPreferredWidth) vezes a escala da janela, ancorada embaixo
  e centrada na horizontal. Sai na mesma proporcao das capturas de docs/images/.

  Uso (a partir da raiz do repositorio):
    powershell -ExecutionPolicy Bypass -File tools/capture_site.ps1
    powershell -ExecutionPolicy Bypass -File tools/capture_site.ps1 -Build
    powershell -ExecutionPolicy Bypass -File tools/capture_site.ps1 -Build -Only forjarack,minisat
#>
[CmdletBinding()]
param(
    [string[]] $Only = @(),
    [switch] $Build,
    [switch] $Force,
    [string] $BuildRoot = "build-capture"
)

Set-StrictMode -Version 2
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$siteDir = Join-Path $repoRoot "site"
$assets = Join-Path $siteDir "assets"
$docsImages = Join-Path $repoRoot "docs/images"
$catalogo = Get-Content -LiteralPath (Join-Path $siteDir "catalogo.json") -Raw -Encoding UTF8 | ConvertFrom-Json

New-Item -ItemType Directory -Force -Path $assets | Out-Null

# Com -File o PowerShell entrega "a,b,c" como um unico elemento; separa por
# virgula aqui para o -Only funcionar nos dois modos de chamada.
$Only = @($Only | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim().ToLowerInvariant() } | Where-Object { $_ })

# Alguns itens reaproveitam uma captura com outro nome em docs/images/.
$alias = @{ drumforge = "drumforge-kits" }

function Test-Alvo {
    param([string] $Slug)
    if ($Only.Count -eq 0) { return $true }
    return [bool] ($Only -contains $Slug.ToLowerInvariant())
}

function Get-Arquivo {
    param($Item)
    if ($Item.PSObject.Properties.Name -notcontains "imagem") { return "" }
    $img = [string] $Item.imagem
    if (-not $img) { return "" }
    return ($img -split '/')[-1]
}

# ------------------------------------------------------------------------ copia
$faltam = @()
foreach ($item in $catalogo.itens) {
    $arquivo = Get-Arquivo $item
    if (-not $arquivo) { continue }
    if (-not (Test-Alvo $item.slug)) { continue }

    $dest = Join-Path $assets $arquivo
    if ((Test-Path -LiteralPath $dest) -and -not $Force) { continue }

    $origemBase = $item.slug
    if ($alias.ContainsKey($item.slug)) { $origemBase = $alias[$item.slug] }
    $origem = Join-Path $docsImages ($origemBase + ".png")

    if (Test-Path -LiteralPath $origem) {
        Copy-Item -LiteralPath $origem -Destination $dest -Force
        Write-Host ("copiado  {0}  <- docs/images/{1}.png" -f $arquivo, $origemBase)
    }
    else {
        $faltam += $item
    }
}

if (-not $Build) {
    Write-Host ""
    if ($faltam.Count -eq 0) {
        Write-Host "capture_site: site/assets completo para o catalogo atual"
    }
    else {
        Write-Host ("capture_site: {0} item(ns) sem captura em docs/images: {1}" -f `
            $faltam.Count, (($faltam | ForEach-Object { $_.slug }) -join ", "))
        Write-Host "               rode com -Build para capturar a UI de cada um."
    }
    exit 0
}

if ($faltam.Count -eq 0) {
    Write-Host "capture_site: nada a capturar, site/assets esta completo"
    exit 0
}

# ------------------------------------------------------------------- capturas
$native = @'
using System;
using System.Runtime.InteropServices;
public struct ForjaRect { public int Left; public int Top; public int Right; public int Bottom; }
public static class ForjaCaptura {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out ForjaRect r);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out ForjaRect r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref System.Drawing.Point p);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr h, int n);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr h);
}
'@
Add-Type -AssemblyName System.Drawing
if (-not ("ForjaCaptura" -as [type])) { Add-Type -TypeDefinition $native -ReferencedAssemblies "System.Drawing" }
[void][ForjaCaptura]::SetProcessDPIAware()

function Get-NomeAlvo {
    # slug -> nome do alvo CMake (primeira letra maiuscula do diretorio do plugin)
    param([string] $Slug)

    $alvos = Get-ChildItem -LiteralPath $repoRoot -Recurse -Filter CMakeLists.txt -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -notmatch '\\build|\\vendor|\\JUCE|\\templates' } |
        ForEach-Object {
            $txt = Get-Content -LiteralPath $_.FullName -Raw
            [regex]::Matches($txt, 'forja_add_(?:plugin|instrument)\s*\(\s*([A-Za-z0-9_]+)') |
                ForEach-Object { $_.Groups[1].Value }
        }
    $achado = @($alvos | Where-Object { $_.ToLowerInvariant() -eq $Slug.ToLowerInvariant() })
    if ($achado.Count -eq 0) { return "" }
    return $achado[0]
}

function Get-ExeStandalone {
    param([string] $Alvo, [string] $BuildRoot)

    $raiz = Join-Path $BuildRoot ($Alvo + "_artefacts")
    if (-not (Test-Path -LiteralPath $raiz)) { $raiz = $BuildRoot }
    $exe = Get-ChildItem -LiteralPath $raiz -Recurse -Filter "*.exe" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -replace '\\', '/' -match '/Release/Standalone/[^/]+\.exe$' -and
                       $_.BaseName -eq $Alvo } |
        Select-Object -First 1
    if ($exe) { return $exe.FullName }
    return ""
}

function Get-TelaCanvas {
    # Dimensoes logicas possiveis do editor (ForjaEditorBase: 1000x540 ou 560x260
    # compacto). Escolhe a maior que caiba na area util, na escala da janela.
    param([int] $Largura, [int] $Altura, [double] $Escala)

    $telas = @(
        [pscustomobject]@{ W = 1000; H = 540 },
        [pscustomobject]@{ W = 560;  H = 260 }
    )
    foreach ($tela in ($telas | Sort-Object W -Descending)) {
        $w = [int] [Math]::Round($tela.W * $Escala)
        $h = [int] [Math]::Round($tela.H * $Escala)
        if ($w -le $Largura -and $h -le $Altura) {
            return [pscustomobject]@{ W = $w; H = $h }
        }
    }
    return $null
}

function Get-MediaBrilho {
    param([System.Drawing.Bitmap] $Bmp)

    $soma = [double]0
    $amostras = 0
    for ($y = 0; $y -lt $Bmp.Height; $y += 8) {
        for ($x = 0; $x -lt $Bmp.Width; $x += 8) {
            $c = $Bmp.GetPixel($x, $y)
            $soma += ($c.R + $c.G + $c.B) / 3
            $amostras++
        }
    }
    if ($amostras -eq 0) { return 0 }
    return $soma / $amostras
}

function Save-Captura {
    param([System.Drawing.Bitmap] $Bmp, [string] $Destino)

    $tmp = [System.IO.Path]::ChangeExtension($Destino, ".tmp.png")
    $Bmp.Save($tmp, [System.Drawing.Imaging.ImageFormat]::Png)
    if (Test-Path -LiteralPath $Destino) { Remove-Item -LiteralPath $Destino -Force }
    Move-Item -LiteralPath $tmp -Destination $Destino
}

function Capture-Janela {
    param([System.Diagnostics.Process] $Proc, [string] $Destino)

    $hwnd = [IntPtr]::Zero
    for ($i = 0; $i -lt 60; $i++) {
        $Proc.Refresh()
        $h = $Proc.MainWindowHandle
        if ($h -ne [IntPtr]::Zero -and [ForjaCaptura]::IsWindowVisible($h)) { $hwnd = $h; break }
        Start-Sleep -Milliseconds 500
    }
    if ($hwnd -eq [IntPtr]::Zero) { return "janela nao apareceu" }

    # da tempo do JUCE desenhar o editor e das fontes carregarem
    Start-Sleep -Seconds 3
    [void][ForjaCaptura]::ShowWindowAsync($hwnd, 9)   # SW_RESTORE
    [void][ForjaCaptura]::SetForegroundWindow($hwnd)
    Start-Sleep -Milliseconds 800

    $cli = New-Object ForjaRect
    [void][ForjaCaptura]::GetClientRect($hwnd, [ref] $cli)
    $largura = $cli.Right
    $altura = $cli.Bottom
    if ($largura -lt 64 -or $altura -lt 48) { return "area util da janela pequena demais ($largura x $altura)" }

    # 1) PrintWindow com PW_RENDERFULLCONTENT (funciona sem a janela na tela)
    $bmp = New-Object System.Drawing.Bitmap($largura, $altura, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][ForjaCaptura]::PrintWindow($hwnd, $hdc, 2)
    $g.ReleaseHdc($hdc)
    $g.Dispose()

    $brilho = Get-MediaBrilho $bmp
    if ($brilho -lt 6) {
        # 2) fallback: copia o que esta na tela na posicao da janela
        $rect = New-Object ForjaRect
        [void][ForjaCaptura]::GetWindowRect($hwnd, [ref] $rect)
        $ponto = New-Object System.Drawing.Point(0, 0)
        [void][ForjaCaptura]::ClientToScreen($hwnd, [ref] $ponto)
        $bmp.Dispose()
        $bmp = New-Object System.Drawing.Bitmap($largura, $altura, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $g2 = [System.Drawing.Graphics]::FromImage($bmp)
        $g2.CopyFromScreen($ponto.X, $ponto.Y, 0, 0, (New-Object System.Drawing.Size($largura, $altura)))
        $g2.Dispose()
        $brilho = Get-MediaBrilho $bmp
    }

    if ($brilho -lt 6) {
        $bmp.Dispose()
        return "captura saiu preta (mediana $([math]::Round($brilho, 2)))"
    }

    $dpi = [ForjaCaptura]::GetDpiForWindow($hwnd)
    if ($dpi -eq 0) { $dpi = 96 }
    $escala = [double] $dpi / 96.0

    # Recorta so o canvas: o Standalone desenha a barra de titulo, o botao Options
    # e a faixa de aviso acima do editor. O canvas fica encostado na base da area
    # util, centrado na horizontal.
    $tela = Get-TelaCanvas -Largura $largura -Altura $altura -Escala $escala
    $final = $bmp
    if ($tela) {
        $x = [int] [Math]::Floor(($largura - $tela.W) / 2)
        $y = $altura - $tela.H
        $recorte = $bmp.Clone((New-Object System.Drawing.Rectangle($x, $y, $tela.W, $tela.H)), $bmp.PixelFormat)
        $bmp.Dispose()
        $final = $recorte
    }
    else {
        Write-Warning ("{0}: area util {1}x{2} menor que qualquer canvas da suite; salvo sem recorte" -f `
            (Split-Path $Destino -Leaf), $largura, $altura)
    }

    Save-Captura $final $Destino
    $largura = $final.Width
    $altura = $final.Height
    $final.Dispose()
    Write-Host ("capturado {0}  ({1}x{2}, brilho {3}, dpi {4})" -f (Split-Path $Destino -Leaf), $largura, $altura, [math]::Round($brilho, 1), $dpi)
    return ""
}

# build standalone so dos produtos que precisam de captura
# -BuildRoot aponta para uma arvore ja configurada (reaproveitar build/ evita
# baixar e compilar o JUCE de novo); o padrao e uma arvore propria build-capture/.
if (-not [System.IO.Path]::IsPathRooted($BuildRoot)) { $BuildRoot = Join-Path $repoRoot $BuildRoot }
$alvos = @()
foreach ($item in $faltam) {
    $alvo = Get-NomeAlvo -Slug $item.slug
    if (-not $alvo) {
        Write-Warning ("sem alvo CMake para '{0}': capture a UI manualmente" -f $item.slug)
        continue
    }
    $alvos += [pscustomobject]@{ Slug = $item.slug; Alvo = $alvo; Arquivo = (Get-Arquivo $item) }
}
if ($alvos.Count -eq 0) { exit 0 }

Write-Host "== configure Standalone ($BuildRoot)"
& cmake -S $repoRoot -B $BuildRoot -DFORJA_STANDALONE=ON
if ($LASTEXITCODE -ne 0) { Write-Error "cmake configure falhou"; exit 1 }

foreach ($a in $alvos) {
    Write-Host ("== build {0}_Standalone" -f $a.Alvo)
    & cmake --build $BuildRoot --config Release --target ($a.Alvo + "_Standalone")
    if ($LASTEXITCODE -ne 0) {
        Write-Warning ("build falhou para {0}; card fica sem captura" -f $a.Slug)
        continue
    }

    $exe = Get-ExeStandalone -Alvo $a.Alvo -BuildRoot $BuildRoot
    if (-not $exe) {
        Write-Warning ("nenhum .exe Standalone encontrado para {0}" -f $a.Alvo)
        continue
    }

    $destino = Join-Path $assets $a.Arquivo
    $proc = Start-Process -FilePath $exe -PassThru
    try {
        $erro = Capture-Janela -Proc $proc -Destino $destino
    }
    finally {
        if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
    }
    if ($erro) {
        Write-Warning ("{0}: {1}; card fica sem captura" -f $a.Slug, $erro)
    }
}

Write-Host ""
Write-Host "capture_site: rode tools/build_site.ps1 para embutir as capturas novas"
