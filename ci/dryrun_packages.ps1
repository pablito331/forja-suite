# Dry-run local do caminho "packages" do release.yml (Windows).
# Reproduz passo a passo o que a CI fara no job windows quando um trem
# declara packages[]. Nao commite nada: e so para provar o desenho.
# Uso:
#   powershell -ExecutionPolicy Bypass -File ci/dryrun_packages.ps1 -Trem minis -Versao 1.0.0
[CmdletBinding()]
param(
    [string] $Trem = "minis",
    [string] $Versao = "1.0.0"
)
$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $repoRoot

$produtos = Get-Content ci/products.json -Raw -Encoding UTF8 | ConvertFrom-Json
if (-not ($produtos.$Trem.PSObject.Properties.Name -contains "packages")) {
    Write-Error "trem '$Trem' nao declara packages[] em ci/products.json"
    exit 1
}
$packages = @($produtos.$Trem.packages)
$tag = "$($produtos.$Trem.tagPrefix)$Versao"
Write-Host ("trem: {0} ({1}) versao {2} -> {3} packages" -f $Trem, $tag, $Versao, $packages.Count) -ForegroundColor Cyan

$iscc = @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
          "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe") |
    Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) {
    Write-Warning "ISCC nao encontrado; pulando passo de installer (.exe)"
}

# staging limpo
$staging = "installer/staging"
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory -Force $staging | Out-Null

foreach ($pkg in $packages) {
    Write-Host ("==> collect: " + $pkg.slug) -ForegroundColor Yellow
    & ./installer/collect_payload.ps1 -BuildRoot build -Clean `
        -OnlyPath @($pkg.pathFilter) -LeiamE $pkg.readme
    if (-not $?) { throw "collect[$($pkg.slug)] falhou" }
    $dst = Join-Path $staging $pkg.slug
    New-Item -ItemType Directory -Force $dst | Out-Null
    Copy-Item installer/payload (Join-Path $dst "payload") -Recurse -Force
}

foreach ($pkg in $packages) {
    if ($iscc) {
        Write-Host ("==> iscc: " + $pkg.slug) -ForegroundColor Yellow
        if (Test-Path installer/payload) { Remove-Item installer/payload -Recurse -Force }
        Copy-Item "installer/staging/$($pkg.slug)/payload" installer/payload -Recurse -Force
        & $iscc "/DAppVersion=$tag" "/DProductName=$($pkg.display)" "/DProductVersion=$Versao" installer/forja.iss
        if (-not $?) { throw "ISCC[$($pkg.slug)] falhou" }
        $exe = Get-ChildItem installer/dist -Filter *.exe |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1
        Move-Item $exe.FullName "./$($pkg.artifactBase)-$Versao-windows-x64.exe" -Force
    }

    Write-Host ("==> zip: " + $pkg.slug) -ForegroundColor Yellow
    $src = "installer/staging/$($pkg.slug)/payload"
    if (Test-Path package) { Remove-Item package -Recurse -Force }
    New-Item -ItemType Directory -Force package/VST3 | Out-Null
    Get-ChildItem $src -Recurse -Directory -Filter *.vst3 | ForEach-Object {
        Copy-Item $_.FullName package/VST3/ -Recurse -Force
    }
    Copy-Item (Join-Path $src "LEIA-ME.txt") package/LEIA-ME.txt -Force
    Compress-Archive -Path package/* -DestinationPath "$($pkg.artifactBase)-$Versao-windows-x64.zip" -Force
}

Write-Host ""
Write-Host "ARTEFATOS GERADOS:" -ForegroundColor Green
Get-ChildItem -Filter "Forja*-$Versao-windows-x64.*" | Select-Object Name, @{n='KB';e={[int]($_.Length/1KB)}} | Format-Table -AutoSize
Write-Host "OK - a CI geraria exatamente esses arquivos em cada SO."
