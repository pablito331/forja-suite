#requires -Version 5.1
<#
.SYNOPSIS
    Cria o scaffold de um plugin Forja a partir de tools/templates/plugin.

.DESCRIPTION
    Equivalente Windows ao tools/new_plugin.py. O resultado compila como um
    processador neutro de um knob, com bypass, medidores, FUNDAMENTOS
    provisorio e README esqueleto. O TODO de DSP no adapter deve ser trocado
    pela transcricao literal do Airwindows antes do plugin ser considerado
    pronto (ver docs/NOVO-PLUGIN.md).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools/new_plugin.ps1 `
        -Family OneKnob -Product Areia -Code Area -Slug areia -Model Areia `
        -Airwindows PurestAir -FamilyLabel "DE MIX" -Subtitle "AMACIADOR" `
        -Status "Amaciador de picos" -Categories Fx -Company "Forja"
#>
param(
    [Parameter(Mandatory = $true)][string]$Family,
    [Parameter(Mandatory = $true)][string]$Product,
    [Parameter(Mandatory = $true)][string]$Code,
    [string]$Slug = "",
    [string]$Model = "",
    [string]$Bundle = "",
    [string]$Categories = "Fx",
    [string]$Company = "Forja",
    [string]$FamilyLabel = "",
    [string]$Subtitle = "PROCESSADOR",
    [string]$Status = "Processador Forja",
    [string]$Footer = "DSP Airwindows - MIT",
    [string]$Airwindows = "",
    [string]$ParamName = "Quantidade",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path (Join-Path $Root "Forja"))) {
    $Root = (Get-Location).Path
}
$Templates = Join-Path $Root "tools/templates/plugin"
$AirwindowsDir = Join-Path $Root "Airwindows"

function Exit-With([string]$Message) {
    Write-Error $Message
    exit 1
}

function Write-Utf8([string]$Path, [string]$Text) {
    $dir = Split-Path -Parent $Path
    if ($dir -and -not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
    [System.IO.File]::WriteAllText($Path, $Text, (New-Object System.Text.UTF8Encoding($false)))
}

function Identity([string]$Value, [string]$Label) {
    if ($Value -notmatch '^[A-Za-z][A-Za-z0-9]*$') {
        Exit-With "$Label must be an ASCII identifier (letters and digits): '$Value'"
    }
    return $Value
}

$Product = Identity $Product "-Product"
if ([string]::IsNullOrWhiteSpace($Slug)) { $Slug = $Product.ToLowerInvariant() }
if ($Slug -notmatch '^[a-z][a-z0-9]*$') {
    Exit-With "-Slug must be lowercase ASCII: '$Slug'"
}
if ([string]::IsNullOrWhiteSpace($Model)) { $Model = $Product }
$Model = Identity $Model "-Model"
if ($Code -notmatch '^[A-Za-z0-9]{4}$') {
    Exit-With "-Code must have exactly 4 letters/digits (VST3 plugin code)"
}

if ($Family -notmatch '^[A-Za-z][A-Za-z0-9]*$') {
    Exit-With "-Family must be an ASCII identifier: '$Family'"
}
$FamilyDir = Join-Path $AirwindowsDir $Family
if (-not (Test-Path $FamilyDir -PathType Container)) {
    Exit-With "family folder not found: $FamilyDir"
}

if ([string]::IsNullOrWhiteSpace($Airwindows)) { $Airwindows = $Model }
if ([string]::IsNullOrWhiteSpace($Bundle)) { $Bundle = "com.forja.$Slug" }
if ([string]::IsNullOrWhiteSpace($FamilyLabel)) { $FamilyLabel = $Family.ToUpperInvariant() }

$Values = @{
    PRODUCT         = $Product
    PREFIX          = $Product
    SLUG            = $Slug
    MODEL           = $Model
    CODE            = $Code
    BUNDLE          = $Bundle
    CATEGORIES      = $Categories
    COMPANY         = $Company
    FAMILY          = $Family
    FAMILY_LABEL    = $FamilyLabel
    SUBTITLE        = $Subtitle
    STATUS_SUBTITLE = $Status
    FOOTER          = $Footer
    AIRWINDOWS      = $Airwindows
    PARAM_NAME      = $ParamName
}

$PluginDir = Join-Path $FamilyDir $Product
if (Test-Path $PluginDir) {
    Exit-With "refusing to overwrite existing plugin folder: $PluginDir"
}

$AdapterPath = Join-Path $Root "Forja/Source/forja/dsp/Forja$Model.h"
if (Test-Path $AdapterPath) {
    Exit-With "refusing to overwrite existing adapter: $AdapterPath"
}

function Render([string]$Text) {
    return [regex]::Replace($Text, '@@([A-Z_]+)@@', {
        param($Match)
        $key = $Match.Groups[1].Value
        if (-not $Values.ContainsKey($key)) {
            Exit-With "unknown placeholder @@$key@@ in template"
        }
        return [string]$Values[$key]
    })
}

$Writes = New-Object System.Collections.Generic.List[object]
foreach ($Source in Get-ChildItem -Path $Templates -Recurse -File) {
    $Relative = $Source.FullName.Substring($Templates.Length).TrimStart('\', '/')
    if ($Source.Name -eq 'ForjaDsp.h') {
        $Destination = $AdapterPath
    } else {
        $Destination = Join-Path $PluginDir $Relative
    }
    $Writes.Add([pscustomobject]@{
        Path    = $Destination
        Content = (Render (Get-Content -LiteralPath $Source.FullName -Raw -Encoding UTF8))
    })
}

foreach ($Write in $Writes) {
    $Relative = $Write.Path.Substring($Root.Length).TrimStart('\', '/')
    if ($DryRun) {
        Write-Output "would write $Relative"
    } else {
        Write-Utf8 $Write.Path $Write.Content
        Write-Output "write $Relative"
    }
}

$FamilyCmake = Join-Path $FamilyDir "CMakeLists.txt"
$AddLine = "add_subdirectory($Product)"

if (-not $DryRun) {
    $Text = if (Test-Path $FamilyCmake) { Get-Content -LiteralPath $FamilyCmake -Raw -Encoding UTF8 } else { "" }
    if ($Text -notmatch [regex]::Escape($AddLine)) {
        if ($Text -and -not $Text.EndsWith("`n")) { $Text += "`n" }
        Write-Utf8 $FamilyCmake ($Text + $AddLine + "`n")
        Write-Output "write $($FamilyCmake.Substring($Root.Length).TrimStart('\', '/')) (+ $AddLine)"
    } else {
        Write-Output "kept family CMakeLists.txt (already lists $Product)"
    }

    $GroupCmake = Join-Path $AirwindowsDir "CMakeLists.txt"
    $FamilyLine = "add_subdirectory($Family)"
    $GroupText = Get-Content -LiteralPath $GroupCmake -Raw -Encoding UTF8
    if ($GroupText -notmatch ("(?m)^" + [regex]::Escape($FamilyLine) + "$")) {
        if ($GroupText -and -not $GroupText.EndsWith("`n")) { $GroupText += "`n" }
        Write-Utf8 $GroupCmake ($GroupText + $FamilyLine + "`n")
        Write-Output "write Airwindows/CMakeLists.txt (+ $FamilyLine)"
    } else {
        Write-Output "kept Airwindows/CMakeLists.txt (already lists $Family)"
    }
}

$AdapterRelative = $AdapterPath.Substring($Root.Length).TrimStart('\', '/')
Write-Output ""
Write-Output "Next steps (docs/NOVO-PLUGIN.md):"
Write-Output "  1. Vendorize Airwindows $Airwindows into Forja/Source/forja/vendor/airwindows/$Model/"
Write-Output "  2. Port processDoubleReplacing into $AdapterRelative"
Write-Output "  3. Replace the parameter/UI placeholders in Airwindows/$Family/$Product/Source"
Write-Output "  4. Add Tests/Source/parity_$Slug.cpp and wire it in Tests/CMakeLists.txt"
Write-Output "  5. Add the adapter to Tests/Source/alloc_guard.cpp"
Write-Output "  6. cmake configure + build ${Product}_VST3 ${Product}_Standalone and run the tests"
Write-Output "  7. Fill README.md and the FUNDAMENTOS, update the catalog and the suite README"
Write-Output ""
Write-Output "Then: cmake -S . -B build && cmake --build build --config Release --target ${Product}_VST3 ${Product}_Standalone"
