# Spec do trem de release: espelha a logica do job resolve (release.yml) para
# conferir ci/products.json sem runner nem jq. Uso:
#   powershell -File ci/test_resolve.ps1            # espera tudo verde
$ErrorActionPreference = "Stop"
$products = Get-Content (Join-Path $PSScriptRoot "products.json") -Raw | ConvertFrom-Json

function Resolve-ForjaTag {
    param([string] $Tag, $Products)

    $candidates = @($Products.PSObject.Properties | Where-Object {
        $_.Name -notmatch '^_' -and $_.Value.kind -eq 'product' -and $Tag.StartsWith($_.Value.tagPrefix)
    }) | Sort-Object { $_.Value.tagPrefix.Length }

    if ($candidates.Count -eq 0) {
        return [pscustomobject]@{ Kind = 'suite'; Slug = 'suite'; Version = $Tag;
                                 ArtifactBase = $Products.suite.artifactBase;
                                 PathFilters = @() }
    }

    $win = $candidates[-1].Value
    [pscustomobject]@{
        Kind         = 'product'
        Slug         = $candidates[-1].Name
        Version      = $Tag.Substring($win.tagPrefix.Length)
        ArtifactBase = $win.artifactBase
        PathFilters  = @($win.pathFilters)
    }
}

$esperado = @(
    @{ Tag = 'v0.5.2';             Kind = 'suite';   Version = 'v0.5.2';  Slug = 'suite';      Base = 'Forja' },
    @{ Tag = 'vrack-1.0.0';        Kind = 'product'; Version = '1.0.0';   Slug = 'rack';       Base = 'ForjaRack' },
    @{ Tag = 'vcanivete-2.0.1';    Kind = 'product'; Version = '2.0.1';   Slug = 'canivete';   Base = 'ForjaCanivete' },
    @{ Tag = 'vminis-1.0.0';       Kind = 'product'; Version = '1.0.0';   Slug = 'minis';      Base = 'ForjaMinis' },
    @{ Tag = 'vminisat-1.3.0';     Kind = 'product'; Version = '1.3.0';   Slug = 'minisat';    Base = 'ForjaMinisat' },
    @{ Tag = 'vdrumforge-0.2.0';   Kind = 'product'; Version = '0.2.0';   Slug = 'drumforge';  Base = 'ForjaDrumforge' },
    @{ Tag = 'vairwindows-1.0.0';  Kind = 'product'; Version = '1.0.0';   Slug = 'airwindows'; Base = 'ForjaAirwindows' }
)

$fail = 0
foreach ($case in $esperado) {
    $got = Resolve-ForjaTag -Tag $case.Tag -Products $products
    $okKind  = $got.Kind -eq $case.Kind
    $okSlug  = $got.Slug -eq $case.Slug
    $okVer   = $got.Version -eq $case.Version
    $okBase  = $got.ArtifactBase -eq $case.Base
    if ($okKind -and $okSlug -and $okVer -and $okBase) {
        Write-Host ("ok   {0,-20} -> {1} {2} ({3})" -f $case.Tag, $got.Kind, $got.Slug, $got.ArtifactBase)
    } else {
        $fail++
        Write-Host ("FAIL {0}: kind={1} slug={2} version={3} base={4}" -f `
            $case.Tag, $got.Kind, $got.Slug, $got.Version, $got.ArtifactBase) -ForegroundColor Red
    }
}

# Toda linha de products.json precisa ser utilizavel pelo coletor e pelo .iss:
# pathFilters nao vazios para produto, readme existente, artifactBase sem espaco.
$iss = Get-Content (Join-Path $PSScriptRoot "..\installer\forja.iss") -Raw
foreach ($prop in $products.PSObject.Properties) {
    if ($prop.Name -match '^_') { continue }
    $p = $prop.Value
    if ($p.kind -eq 'product') {
        if (-not $p.pathFilters -or $p.pathFilters.Count -eq 0) {
            $fail++; Write-Host "FAIL $($prop.Name): pathFilters vazio" -ForegroundColor Red
        }
        foreach ($filter in $p.pathFilters) {
            $folder = ($filter -split '/')[0]
            if (-not (Test-Path (Join-Path (Join-Path $PSScriptRoot '..') $folder))) {
                $fail++; Write-Host "FAIL $($prop.Name): filtro aponta para pasta inexistente: $filter" -ForegroundColor Red
            }
        }
    }
    if ($p.readme -and -not (Test-Path (Join-Path (Join-Path $PSScriptRoot '..') ($p.readme -replace '/', '\')))) {
        $fail++; Write-Host "FAIL $($prop.Name): readme nao encontrado: $($p.readme)" -ForegroundColor Red
    }
    if ($p.artifactBase -match '\s') {
        $fail++; Write-Host "FAIL $($prop.Name): artifactBase com espaco: $($p.artifactBase)" -ForegroundColor Red
    }
    # Grupo de um plugin so (tres segmentos) precisa da linha no catalogo do .iss.
    foreach ($filter in @($p.pathFilters)) {
        $bits = $filter -split '/'
        if ($bits.Count -ge 3) {
            if ($iss -notmatch ('"' + $bits[-1] + '\|')) {
                $fail++; Write-Host "FAIL $($prop.Name): grupo '$($bits[-1])' sem linha no catalogo do forja.iss" -ForegroundColor Red
            }
        }
    }
}

if ($fail -gt 0) {
    Write-Host ""
    Write-Host "$fail falha(s) no trem de release" -ForegroundColor Red
    exit 1
}
Write-Host ""
Write-Host "PASS: resolucao de tags e registro de produtos coerentes"
