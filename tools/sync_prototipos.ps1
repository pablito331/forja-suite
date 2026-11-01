<#
.SYNOPSIS
    Copia os binarios Release para a pasta de prototipos e regera os guias.

.DESCRIPTION
    Para cada plugin copia a pasta .vst3 inteira para a pasta de prototipos.
    O teste e feito no VST3 dentro da DAW, entao o Standalone nao e copiado.
    Em seguida gera INSTALAR-FORJA.txt, um INSTALAR-<FAMILIA>.txt por familia e
    INSTALAR-BATERIA.txt do DRUMFORGE.

    A lista de plugins vem dos CMakeLists (forja_add_plugin / forja_add_instrument)
    de todas as bases (Airwindows, drumforge, Forjarack e Minis) e o texto de cada
    um vem do README do plugin. Assim o guia acompanha a suite sozinho: plugin novo
    entra na conta e na lista sem edicao manual.

.PARAMETER GuidesOnly
    Nao copia binarios; apenas regera os guias a partir do que ja existe em Dest.

.PARAMETER Verify
    Nao escreve nada: compara as contagens do README/AGENTS com a verdade do
    codigo e lista o que estiver divergente.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools/sync_prototipos.ps1

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools/sync_prototipos.ps1 -GuidesOnly -Verify
#>
param(
    [string]$Dest = "",
    [string]$BuildDir = "",
    [switch]$GuidesOnly,
    [switch]$Verify
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
if (-not $Dest) { $Dest = Join-Path $HOME "Documents/prototipos" }
if (-not $BuildDir) { $BuildDir = Join-Path $Root "build" }

$familyKeyMap = @{
    'Saturacao'    = 'SATURACAO'
    'EQ'           = 'EQ'
    'Dinamica'     = 'DINAMICA'
    'Channel Strip'= 'STRIPS'
    'Fita'         = 'FITA'
    'Espaco'       = 'ESPACO'
    'Lo-Fi'        = 'LOFI'
    'Mix'          = 'MIX'
    'Amp'          = 'AMP'
    'Bateria'      = 'BATERIA'
    'Minis'        = 'MINIS'
}
$familyOrder = @('Saturacao', 'EQ', 'Dinamica', 'Channel Strip', 'Fita', 'Espaco',
                 'Lo-Fi', 'Mix', 'Amp', 'Bateria', 'Minis')

function ConvertTo-AsciiText {
    param([string]$Text)
    if (-not $Text) { return "" }
    $decomposed = $Text.Normalize([System.Text.NormalizationForm]::FormD)
    $builder = New-Object System.Text.StringBuilder
    foreach ($ch in $decomposed.ToCharArray()) {
        if ([System.Globalization.CharUnicodeInfo]::GetUnicodeCategory($ch) -ne
            [System.Globalization.UnicodeCategory]::NonSpacingMark) {
            [void]$builder.Append($ch)
        }
    }
    return $builder.ToString().Replace([char]0x2014, '-').Replace([char]0x2013, '-')
}

function Get-WrappedText {
    param([string]$Text, [int]$Width = 76, [string]$FirstPrefix = "", [string]$NextPrefix = "")
    $words = @($Text -split '\s+' | Where-Object { $_ })
    $lines = @()
    $prefix = $FirstPrefix
    $current = $prefix
    foreach ($word in $words) {
        if ($current.Trim().Length -eq 0) {
            $candidate = $prefix + $word
        } else {
            $candidate = $current + " " + $word
        }
        if ($candidate.Length -gt $Width -and $current.Trim().Length -gt 0) {
            $lines += $current.TrimEnd()
            $prefix = $NextPrefix
            $current = $prefix + $word
        } else {
            $current = $candidate
        }
    }
    if ($current.Trim().Length -gt 0) { $lines += $current.TrimEnd() }
    return $lines
}

function Get-SuitePlugins {
    $manifests = @()
    $manifests += Get-ChildItem -Path (Join-Path $Root "Airwindows\*\*\CMakeLists.txt") -ErrorAction SilentlyContinue
    $manifests += Get-ChildItem -Path (Join-Path $Root "drumforge\*\CMakeLists.txt") -ErrorAction SilentlyContinue
    $manifests += Get-ChildItem -Path (Join-Path $Root "Forjarack\*\CMakeLists.txt") -ErrorAction SilentlyContinue
    $manifests += Get-ChildItem -Path (Join-Path $Root "Minis\*\CMakeLists.txt") -ErrorAction SilentlyContinue

    $found = @()
    foreach ($manifest in $manifests) {
        $text = Get-Content -Raw -Encoding UTF8 $manifest.FullName
        $call = [regex]::Match($text, 'forja_add_(?<kind>plugin|instrument)\s*\(')
        if (-not $call.Success) { continue }

        $open = $text.IndexOf('(', $call.Index)
        $close = $text.IndexOf(')', $open)
        if ($close -lt 0) { continue }
        $callArgs = $text.Substring($open + 1, $close - $open - 1)

        $head = [regex]::Match($callArgs, '^\s*([A-Za-z0-9_]+)\s+"([^"]+)"')
        if (-not $head.Success) { continue }
        $kind = $call.Groups['kind'].Value
        $product = $head.Groups[2].Value

        $quoted = @([regex]::Matches($callArgs, '"([^"]*)"') | ForEach-Object { $_.Groups[1].Value })
        if ($kind -eq 'instrument') {
            # Instrumento sem argumento de empresa (Drumforge) e a Bateria.
            # Instrumentos de outras bases (MINIDRUM) trazem a familia no ultimo
            # argumento ("Forja - Minis"), entao derivam dali em vez de cair em Bateria.
            $family = if ($quoted.Count -ge 3 -and $quoted[2]) { $quoted[2] -replace '^Forja\s*-\s*', '' } else { 'Bateria' }
            $categories = if ($quoted.Count -ge 2 -and $quoted[1]) { $quoted[1] } else { 'Instrument|Drum' }
        } else {
            $family = if ($quoted.Count -ge 3) { $quoted[2] -replace '^Forja\s*-\s*', '' } else { 'Forja' }
            $categories = if ($quoted.Count -ge 2) { $quoted[1] } else { 'Fx' }
        }

        $familyKey = $familyKeyMap[$family]
        if (-not $familyKey) {
            $familyKey = ((ConvertTo-AsciiText $family) -replace '[^A-Za-z0-9]', '').ToUpperInvariant()
        }

        $description = "Plugin da familia $family."
        $latency = ""
        $readmePath = Join-Path $manifest.DirectoryName "README.md"
        if (Test-Path $readmePath) {
            $readme = Get-Content -Raw -Encoding UTF8 $readmePath
            $firstLine = @($readme -split "\r?\n" | Where-Object { $_.Trim() -ne "" })[0]
            $titleMatch = [regex]::Match($firstLine, '^#\s*\S+\s*[\u2014\u2013]\s*(.+)$')
            if ($titleMatch.Success -and $titleMatch.Groups[1].Value.Trim()) {
                $description = $titleMatch.Groups[1].Value.Trim()
            }
            # O README quebra a frase em varias linhas; juntar antes de recortar.
            $flat = ($readme -replace '\s+', ' ')
            $latMatch = [regex]::Match($flat, 'Lat.ncia reportada:\s*(.+?)\.(\s|$)')
            if ($latMatch.Success) {
                $latency = ($latMatch.Groups[1].Value -replace '\*', '').Trim()
            } else {
                $latRow = [regex]::Match($readme, '\|\s*Lat.ncia\s*\|\s*([^|\r\n]+)\|')
                if ($latRow.Success) { $latency = ($latRow.Groups[1].Value -replace '\*', '').Trim() }
            }
        }

        $base = 'airwindows'
        if ($manifest.FullName -match '\\drumforge\\') { $base = 'drumforge' }
        if ($manifest.FullName -match '\\Forjarack\\') { $base = 'forjarack' }
        if ($manifest.FullName -match '\\Minis\\') { $base = 'minis' }

        # Titulos do README as vezes comecam em minuscula ("de-esser dinamico").
        if ($description.Length -gt 1 -and $description[0] -cmatch '[a-z]') {
            $description = $description.Substring(0, 1).ToUpperInvariant() + $description.Substring(1)
        }

        $found += [pscustomobject]@{
            Target      = $head.Groups[1].Value
            Product     = $product
            Family      = $family
            FamilyKey   = $familyKey
            Categories  = $categories
            Base        = $base
            Description = ConvertTo-AsciiText $description
            Latency     = ConvertTo-AsciiText $latency
        }
    }

    return @($found | Sort-Object Family, Product)
}

$plugins = Get-SuitePlugins
if ($plugins.Count -eq 0) { throw "nenhum plugin encontrado nos CMakeLists" }

if (-not (Test-Path $BuildDir)) { throw "build folder not found: $BuildDir" }
if (-not $GuidesOnly -and -not $Verify) {
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

    # O Standalone saiu do build: remove os .exe legados que ainda estejam na pasta.
    $stale = @(Get-ChildItem $Dest -Filter *-standalone.exe -ErrorAction SilentlyContinue)
    foreach ($exe in $stale) { Remove-Item $exe.FullName -Force }

    Write-Output ("{0} binarios copiados para {1}" -f $copied, $Dest)
}

$installed = @($plugins | Where-Object { Test-Path (Join-Path $Dest ("{0}.vst3" -f $_.Product)) })
$notBuilt = @($plugins | Where-Object { $installed -notcontains $_ })
foreach ($plugin in $notBuilt) {
    Write-Warning ("{0} esta no CMake mas nao tem .vst3 em {1}" -f $plugin.Product, $Dest)
}
if ($installed.Count -eq 0) { throw "nenhum .vst3 encontrado em $Dest" }

$airwindows = @($installed | Where-Object { $_.Base -eq 'airwindows' })
$instruments = @($installed | Where-Object { $_.Base -ne 'airwindows' })
$ownForja = @($installed | Where-Object { $_.Base -in @('drumforge', 'forjarack', 'minis') })
$drumforge = @($installed | Where-Object { $_.Base -eq 'drumforge' })

$families = @()
foreach ($name in $familyOrder) {
    $members = @($installed | Where-Object { $_.Family -eq $name })
    if ($members.Count -gt 0) {
        $families += [pscustomobject]@{ Name = $name; Key = $members[0].FamilyKey; Plugins = $members }
    }
}
foreach ($other in @($installed | Group-Object Family | Where-Object { $familyOrder -notcontains $_.Name })) {
    $families += [pscustomobject]@{ Name = $other.Name; Key = $other.Group[0].FamilyKey; Plugins = @($other.Group) }
}

function Set-GuideFile {
    param([string]$Name, [string[]]$Lines)
    Set-Content -Path (Join-Path $Dest $Name) -Value $Lines -Encoding ASCII
    return $Name
}

$written = @()
if (-not $Verify) {
    # ---------------- guia mestre ----------------
    $title = "FORJA - INSTALACAO NO WINDOWS"
    $lines = @($title, ('=' * $title.Length), '',
        'Gerado por tools/sync_prototipos.ps1 a partir do CMake e do build.',
        'Nao edite a mao: rode o script para atualizar.', '',
        ("Esta pasta contem os {0} plugins da suite Forja, cada um como pasta .vst3." -f $installed.Count),
        'O teste e feito no VST3 dentro da DAW; nao ha mais executavel Standalone.')

    if ($instruments.Count -gt 0) {
        $lines += ''
        $lines += Get-WrappedText -Text ("Sao {0} adaptacoes MIT da Airwindows (Chris Johnson) e {1} plugin de DSP proprio da Forja (bases drumforge, Forjarack e Minis): {2}." -f $airwindows.Count, $ownForja.Count, (($ownForja | ForEach-Object { $_.Product }) -join ', '))
    }

    $lines += ''
    $lines += '1. Feche o seu DAW.'
    $lines += '2. Copie as pastas .vst3 INTEIRAS desta pasta para:'
    $lines += '      C:\Program Files\Common Files\VST3\'
    $lines += '   Cada caminho final fica assim:'
    $lines += '      C:\Program Files\Common Files\VST3\Cinzel.vst3\Contents\...'
    $lines += '3. Nao copie o arquivo .vst3 de dentro de Contents\x86_64-win.'
    $lines += '   Copie a pasta .vst3 que esta na raiz desta pasta.'
    $lines += '4. Abra o DAW e refaca o escaneamento de plugins.'
    $lines += ''
    $lines += 'A DAW mostra o nome do plugin seguido da familia, por exemplo:'
    $lines += '   Torno (Forja - EQ)'
    $lines += '   Laminador (Forja - Fita)'
    foreach ($instrument in $instruments) {
        $suffix = if ($instrument.Base -eq 'forjarack') { ' - Rack' } else { '' }
        $lines += ('   {0} (Forja{1})' -f $instrument.Product, $suffix)
    }

    $lines += ''
    $lines += 'INSTALACAO POR FAMILIA'
    $lines += ('-' * 22)
    $lines += 'Se preferir instalar aos poucos, use os guias:'
    foreach ($family in $families) {
        $guideName = "INSTALAR-$($family.Key).txt"
        $lines += ('   {0,-26} {1}' -f $guideName, (($family.Plugins | ForEach-Object { $_.Product }) -join ', '))
    }

    $legacy = @(Get-ChildItem $Dest -Filter 'INSTALAR-*.txt' -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^INSTALAR-(BRASA|TEMPERA|PRENSA|MALHO)\.txt$' })
    if ($legacy.Count -gt 0) {
        $lines += ''
        $lines += 'Os guias individuais tambem continuam na pasta:'
        $lines += '   ' + (($legacy | ForEach-Object { $_.Name }) -join ', ')
    }

    if ($drumforge.Count -gt 0) {
        $lines += ''
        $lines += 'DRUMFORGE'
        $lines += ('-' * 9)
        $lines += Get-WrappedText -Text ("O {0} e um sampler de bateria com DSP proprio, nao uma adaptacao Airwindows. Ele nao vem com samples: importe uma pasta de WAVs pela aba KITS, abra um arquivo .dfkit (texto no estilo drumlabooh) ou use um drumkit.txt / drumkit.labooh do Drumlabooh." -f (($drumforge | ForEach-Object { $_.Product }) -join ', '))
        $lines += Get-WrappedText -Text "Kits do drum_sklad (drumkit.txt ou drumkit.labooh) colocados em C:\Users\<voce>\drum_sklad, drumlabooh-kits ou drumrox-kits aparecem na biblioteca da aba KITS sem copiar nada."
        $lines += Get-WrappedText -Text "A foto de preview do kit (image.png ou image.jpg na pasta do drum_sklad) aparece no card da aba KITS."
        $lines += Get-WrappedText -Text "Um .dfkit pode vir premixado: a mix de cada pad e do master vai em linhas de comentario #mix@N e #master e e aplicada na carga manual do kit. SALVAR KIT cria uma pasta com o nome do kit levando o .dfkit, os audios copiados para samples/ e a imagem: um pacote autonomo que abre em qualquer maquina. Veja INSTALAR-BATERIA.txt."
        $lines += Get-WrappedText -Text "Na aba MIXER cada canal traz um rail vertical com a reducao de ganho do compressor ao lado do fader, e o master mostra dois rails separados (C = compressor, L = limitador). Os knobs tem cor por funcao dentro da paleta (brasa para ganho/dinamica, ambar para HPF, gradiente no EQ). O envio S1 (ambiente SALAO) dosa com curva quadrada e teto de -12 dB: como o retorno e 100% wet, um dedo no botao mal injeta sala e a dose cresce conforme o giro."
        $lines += 'Kits e loops ficam em:'
        $lines += '      Documentos\Forja\Drumforge\Kits'
        $lines += '      Documentos\Forja\Drumforge\Loops'
    }

    $rackPlugins = @($installed | Where-Object { $_.Base -eq 'forjarack' })
    if ($rackPlugins.Count -gt 0) {
        $lines += ''
        $lines += 'FORJARACK'
        $lines += ('-' * 9)
        $lines += Get-WrappedText -Text "O ForjaRack e um rack em cadeia estilo Serie 500: oito gavetas em serie, cada uma hospedando um modulo Airwindows MIT ja validado por paridade na suite. Sao 22 modulos em seis familias: cor (CERA, MEL, CORPO, IMPETO), drive (LIXA, OXIDO, REBARBA, LAMINADOR), dinamica (TEMPERA, MURALHA, DIQUE, MALHO, PRENSA, ESCORIA), EQ (CINZEL, TORNO, LIMA), lo-fi (GRANALHA) e ambiente (FOLEGO, SALAO, REPIQUE, ECO). A gaveta toca assim que recebe um modulo, sem chave de ligar: gaveta vazia ou em BYPASS e pass-through exato. SOLO em qualquer gaveta silencia as demais, e o solo de uma gaveta ja em bypass nao conta para nao mutar o rack inteiro."
        $lines += Get-WrappedText -Text "Secao master com trim de entrada e saida (+-24 dB) e inversao de fase do canal direito. Bypass global com rampa de 10 ms. Latencia 0 (PDC): reverb e delay tem atraso musical, como nos originais."
        $lines += Get-WrappedText -Text "O plugin aceita MIDI e o registro de modulos e expansivel: futuros instrumentos (leitor de soundfont sf2/sf3, mini drum que le os kits .dfkit do Drumforge, outros mini plugins) entram como gavetas que somam sua voz na cadeia, sem instalar plugins separados."
        $lines += Get-WrappedText -Text "O rack tambem funciona em faixa de instrumento, sem entrada de audio: a saida e a voz das gavetas MINIDRUM/CANIVETE, tocando pelo teclado ao vivo como no plugin solto. O led MIDI da secao MASTER acende quando o MIDI chega em qualquer bloco: aceso e sem som e problema de roteamento/monitoramento da faixa; apagado e o teclado nao esta chegando ao plugin. O medidor de ENTRADA so mede audio, entao nao vale como prova de MIDI."
    }

    $lines += ''
    $lines += 'TESTE'
    $lines += ('-' * 6)
    $lines += 'O teste e feito dentro da DAW: copie a pasta .vst3 para'
    $lines += 'C:\Program Files\Common Files\VST3\ e refaca o escaneamento. Nao ha mais'
    $lines += 'executavel Standalone (a suite entrega so o VST3).'

    $lines += ''
    $lines += 'FUNDAMENTOS'
    $lines += ('-' * 11)
    $lines += 'O botao FUNDAMENTOS no topo abre um painel nativo (sem WebView) com o que'
    $lines += 'o plugin faz, como funciona, a descricao de cada controle e a origem/'
    $lines += 'licenca. Fecha pelo botao FECHAR, pela tecla Esc ou clicando fora do'
    $lines += 'painel. O chip REDES so aparece quando o link agregador das redes da'
    $lines += 'Forja estiver configurado.'

    $lines += ''
    $lines += 'LATENCIA'
    $lines += ('-' * 8)
    $withLatency = @($installed | Where-Object { $_.Latency -and $_.Latency -notmatch '^0\s' } | Sort-Object Product)
    if ($withLatency.Count -eq 0) {
        $lines += 'Todos os plugins desta pasta reportam latencia 0.'
    } else {
        $lines += 'A maioria dos plugins tem latencia 0. Reportam compensacao:'
        foreach ($plugin in $withLatency) {
            $lines += ('   {0,-12} {1}' -f $plugin.Product, $plugin.Latency)
        }
    }
    $lines += 'A latencia de cada plugin e medida por impulso no harness de testes.'

    $lines += ''
    $lines += 'BYPASS'
    $lines += ('-' * 6)
    $lines += 'Todo plugin tem um parametro BYPASS exposto ao host, com crossfade seco/'
    $lines += 'molhado de 10 ms. No Standalone, o botao BYPASS fica no card de sinal.'

    $lines += ''
    $lines += 'LICENCA E CREDITOS'
    $lines += ('-' * 18)
    $lines += 'DSP original: Airwindows, (c) Chris Johnson, licenca MIT. O codigo'
    $lines += 'original fica verbatim em Forja/Source/forja/vendor/airwindows/, com'
    $lines += 'LICENSE e PROVENANCE.md (URL, commit, data e SHA-256).'
    if ($instruments.Count -gt 0) {
        $lines += 'Engine, mixer, importacao e interface do Drumforge sao Forja.'
    }
    if ($rackPlugins.Count -gt 0) {
        $lines += 'Rack, registro de modulos e medidores do ForjaRack sao Forja; os'
        $lines += 'modulos reutilizam adaptadores MIT da Airwindows (Chris Johnson).'
    }
    $lines += 'Interface, medidores, bypass e a camada dinamica do Cinzel sao Forja.'

    $written += Set-GuideFile -Name 'INSTALAR-FORJA.txt' -Lines $lines

    # ---------------- guias por familia ----------------
    foreach ($family in $families) {
        if ($family.Key -eq 'BATERIA') { continue }

        $title = "FORJA - $($family.Key) - INSTALACAO NO WINDOWS"
        $names = @($family.Plugins | ForEach-Object { $_.Product })
        $lines = @($title, ('=' * $title.Length), '')
        if ($names.Count -eq 1) {
            $lines += "Plugin: $($names[0])."
        } else {
            $lines += "Plugins: $($names -join ', ')."
        }
        $lines += ''
        $lines += '1. Feche o seu DAW.'
        $lines += '2. Copie as pastas .vst3 INTEIRAS desta pasta (as que tem "Contents"'
        $lines += '   dentro) para:'
        $lines += '      C:\Program Files\Common Files\VST3\'
        $lines += '3. Nao copie o arquivo .vst3 de dentro de Contents\x86_64-win.'
        $lines += '4. Abra a DAW e refaca o escaneamento.'

        $lines += ''
        $lines += 'O QUE CADA UM FAZ'
        $lines += ('-' * 17)
        $pad = ($names | Measure-Object -Property Length -Maximum).Maximum + 2
        foreach ($plugin in $family.Plugins) {
            $lines += Get-WrappedText -Text $plugin.Description -Width 76 -FirstPrefix ($plugin.Product.PadRight($pad)) -NextPrefix (' ' * $pad)
        }

        $lines += ''
        $lines += 'LATENCIA'
        $lines += ('-' * 8)
        foreach ($plugin in $family.Plugins) {
            $value = $plugin.Latency
            if (-not $value) { $value = 'medida por impulso no harness' }
            $lines += ('   {0,-12} {1}' -f $plugin.Product, $value)
        }

        $lines += ''
        $lines += 'FUNDAMENTOS'
        $lines += ('-' * 11)
        $lines += 'O botao FUNDAMENTOS abre o painel nativo com o que o plugin faz, como'
        $lines += 'funciona, cada controle e a proveniencia. Fecha pelo botao, por Esc ou'
        $lines += 'clicando fora do painel.'

        $lines += ''
        if ($family.Key -eq 'RACK') {
            $lines += 'Modulos: adaptadores Airwindows MIT (Chris Johnson). A cadeia em serie,'
            $lines += 'as chaves de slot, o master e o registro de expansoes: Forja.'
        } else {
            $lines += 'DSP Airwindows (MIT, Chris Johnson). Interface, medidores e bypass: Forja.'
        }
        $written += Set-GuideFile -Name ("INSTALAR-{0}.txt" -f $family.Key) -Lines $lines
    }

    # ---------------- guia do Drumforge ----------------
    $battery = @($installed | Where-Object { $_.Base -eq 'drumforge' })
    if ($battery.Count -gt 0) {
        $title = "DRUMFORGE - INSTALACAO NO WINDOWS"
        $lines = @($title, ('=' * $title.Length), '')
        $lines += Get-WrappedText -Text ("{0} e o sampler de bateria da Forja: 16 pads, camadas de velocity, round-robin, choke, mesa de 16 canais e 5 barramentos de saida. O DSP e proprio, nao e uma adaptacao Airwindows." -f (($battery | ForEach-Object { $_.Product }) -join ', '))
        $lines += ''
        $lines += '1. Feche o seu DAW.'
        $lines += '2. Copie a pasta .vst3 INTEIRA desta pasta para:'
        $lines += '      C:\Program Files\Common Files\VST3\'
        $lines += '3. Nao copie o arquivo .vst3 de dentro de Contents\x86_64-win.'
        $lines += '4. Abra a DAW e refaca o escaneamento.'
        $lines += ''
        $lines += 'SEM KITS INCLUIDOS'
        $lines += ('-' * 18)
        $lines += 'Nenhum sample acompanha o plugin. A aba KITS importa uma pasta de'
        $lines += 'WAV/AIFF/FLAC (a pasta e as subpastas, classificacao pelo nome do'
        $lines += 'arquivo), abre um arquivo .dfkit (texto no estilo drumlabooh) ou le um'
        $lines += 'drumkit.txt / drumkit.labooh do Drumlabooh.'
        $lines += 'Os kits importados ficam em:'
        $lines += '      Documentos\Forja\Drumforge\Kits'
        $lines += ''
        $lines += 'Kits do drum_sklad/drumlabooh (drumkit.txt ou drumkit.labooh) sao'
        $lines += 'listados automaticamente'
        $lines += 'quando existem nas pastas da conta:'
        $lines += '      C:\Users\<voce>\drum_sklad'
        $lines += '      C:\Users\<voce>\drumlabooh-kits'
        $lines += '      C:\Users\<voce>\drumrox-kits'
        $lines += 'Cada kit e uma pasta com drumkit.txt ou drumkit.labooh e os WAVs; o'
        $lines += 'plugin toca direto'
        $lines += 'deles, sem copiar nada. Da para arrastar a pasta, o'
        $lines += 'drumkit.txt ou o drumkit.labooh para a aba KITS.'
        $lines += 'Se a pasta do kit tem image.png ou image.jpg (foto de preview do'
        $lines += 'drum_sklad), ela aparece no card da aba KITS; ao salvar com SALVAR'
        $lines += 'KIT o .dfkit leva o caminho na linha #image.'
        $lines += ''
        $lines += 'KIT PREMIXADO'
        $lines += ('-' * 14)
        $lines += 'Um .dfkit pode trazer a mix do kit junto com os samples, em linhas de'
        $lines += 'comentario #mix@N e #master com os mesmos valores 0-1 dos knobs (trim,'
        $lines += 'fader, pan, HPF, EQ, compressor e envios de cada pad, mais o master).'
        $lines += 'Na carga manual (ABRIR KIT, IMPORTAR PASTA, arrastar e soltar) o kit com'
        $lines += 'mix assume os knobs; sem mix a mesa volta ao neutro. Ao reabrir a sessao,'
        $lines += 'o estado que a DAW salvou vence a mix do arquivo. mute, solo, fase e a'
        $lines += 'saida nao vao no kit; sao decisao de sessao.'
        $lines += ''
        $lines += 'Para criar um kit premixado sem editar arquivo: abra qualquer kit (ate um'
        $lines += 'drumkit.txt ou pasta de WAVs), ajuste o mixer e use SALVAR KIT na aba'
        $lines += 'KITS. Ele cria uma pasta com o nome do kit levando o .dfkit (mapa + mix'
        $lines += 'atual), a subpasta samples/ com uma copia de cada audio e a foto como'
        $lines += 'image.png/image.jpg, tudo com caminhos relativos a pasta. O pacote e'
        $lines += 'autonomo: pode mover ou enviar para outra maquina sem perder samples.'
        $lines += 'Por padrao o Salvar como aponta para Documentos\Forja\Drumforge\Kits,'
        $lines += 'onde o pacote ja aparece na biblioteca.'
        $lines += ''
        $lines += 'AMBIENTE NA MESA'
        $lines += ('-' * 15)
        $lines += 'O envio S1 de cada canal dosa o reverb SALAO, que devolve 100% wet.'
        $lines += 'Para um dedo no botao nao injetar sala demais, o knob usa curva'
        $lines += 'quadrada com teto de -12 dB: comece em 0,5 e ajuste ouvindo. O S2'
        $lines += 'manda para a saturacao LIXA e segue linear (ali o nivel e drive).'
        $lines += ''
        $lines += 'A aba LOOPS le arquivos .mid e .midi de:'
        $lines += '      Documentos\Forja\Drumforge\Loops'
        $lines += 'Organize em subpastas (Ritmos, Fills, Chorus...): cada pasta ou'
        $lines += 'subpasta vira categoria no filtro PACOTE e o filtro de uma mae traz a'
        $lines += 'arvore inteira. IMPORTAR MIDI e o arrasto aceitam pastas inteiras,'
        $lines += 'preservando a estrutura e copiando somente os .mid.'
        $lines += 'Golpes em notas do mapa General MIDI tocam com o kit atual; notas sem'
        $lines += 'pad correspondente sao informadas na barra de status.'
        $lines += ''
        $lines += 'O preview de loops segue o BPM do host (SYNC DAW) ou o BPM manual de'
        $lines += '40 a 240. ARRASTAR MIDI entrega o arquivo selecionado para a DAW.'
        $lines += ''
        $lines += 'O teste e feito dentro da DAW (VST3); nao ha executavel Standalone.'
        $lines += 'FUNDAMENTOS'
        $lines += ('-' * 11)
        $lines += 'O botao FUNDAMENTOS abre o painel nativo com o que o plugin faz, como'
        $lines += 'funciona, cada controle e a proveniencia. Fecha pelo botao, por Esc ou'
        $lines += 'clicando fora do painel.'
        $lines += ''
        $lines += 'LATENCIA'
        $lines += ('-' * 8)
        $lines += '0 amostras (medida por impulso no harness).'
        $lines += ''
        $lines += 'LICENCA E CREDITOS'
        $lines += ('-' * 18)
        $lines += 'Engine, mixer, limiter, importacao e interface: Forja. Os efeitos do'
        $lines += 'master reutilizam adaptadores MIT da Airwindows (Chris Johnson):'
        $lines += 'ButterComp2, kRockstar2 e PurestDrive. Os leitores de drumkit.txt e'
        $lines += 'drumkit.labooh seguem a especificacao de formato do drumlabooh /'
        $lines += 'drum_sklad, de Peter Semiletov; nenhum codigo de la entra no plugin,'
        $lines += 'so lemos o formato. Nenhum sample de bateria acompanha o plugin.'
        $written += Set-GuideFile -Name 'INSTALAR-BATERIA.txt' -Lines $lines
    }
}

# ---------------- auditoria das docs ----------------
$problems = @()
function Add-Problem { param([string]$Text) $script:problems += $Text }

$readmePath = Join-Path $Root "README.md"
$readme = Get-Content -Raw -Encoding UTF8 $readmePath
$agentsText = Get-Content -Raw -Encoding UTF8 (Join-Path $Root "AGENTS.md")

$tableMatch = [regex]::Match($readme, '(?ms)^## Plugins\s*\r?\n(.*?)^## ')
if ($tableMatch.Success) {
    $rows = [regex]::Matches($tableMatch.Groups[1].Value, '(?m)^\|\s*([^|]+?)\s*\|')
    $tableNames = @($rows | ForEach-Object { ConvertTo-AsciiText $_.Groups[1].Value.Trim() } |
        Where-Object { $_ -and $_ -ne 'Plugin' -and $_ -notmatch '^-+$' })
    $expected = @($installed | ForEach-Object { $_.Product })
    foreach ($name in $expected) {
        if ($tableNames -notcontains $name) { Add-Problem "README: tabela de Plugins sem $name" }
    }
    foreach ($name in $tableNames) {
        if ($expected -notcontains $name) { Add-Problem "README: tabela de Plugins lista $name, que nao esta no build" }
    }
} else {
    Add-Problem "README: secao '## Plugins' nao encontrada"
}

$targetRow = [regex]::Match($readme, '\|\s*Targets locais[^|]*\|\s*(\d+) plugins')
if ($targetRow.Success) {
    if ([int]$targetRow.Groups[1].Value -ne $installed.Count) {
        Add-Problem ("README: 'Targets locais' diz {0} plugins, build tem {1}" -f $targetRow.Groups[1].Value, $installed.Count)
    }
} else {
    Add-Problem "README: linha 'Targets locais' nao encontrada"
}

$artRow = [regex]::Match($readme, '\|\s*Artefatos em[^|]*\|\s*(\d+) `\.vst3`')
if ($artRow.Success) {
    if ([int]$artRow.Groups[1].Value -ne $installed.Count) {
        Add-Problem ("README: 'Artefatos em prototipos' diz {0} .vst3, build tem {1}" -f $artRow.Groups[1].Value, $installed.Count)
    }
} else {
    Add-Problem "README: linha 'Artefatos em prototipos' nao encontrada"
}

$gallery = [regex]::Match($readme, 'Galeria completa \((\d+) plugins\)')
if ($gallery.Success -and [int]$gallery.Groups[1].Value -ne $airwindows.Count) {
    Add-Problem ("README: galeria diz {0} plugins, base Airwindows tem {1}" -f $gallery.Groups[1].Value, $airwindows.Count)
}

$agentsCount = [regex]::Match($agentsText, 'suite de (\d+) plugins')
if ($agentsCount.Success -and [int]$agentsCount.Groups[1].Value -ne $installed.Count) {
    Add-Problem ("AGENTS.md: diz {0} plugins, build tem {1}" -f $agentsCount.Groups[1].Value, $installed.Count)
}

$images = @(Get-ChildItem (Join-Path $Root "docs\images") -Filter *.png)
$imageRow = [regex]::Match($readme, '\|\s*Capturas da interface\s*\|\s*(\d+) imagens')
if ($imageRow.Success -and [int]$imageRow.Groups[1].Value -ne $images.Count) {
    Add-Problem ("README: diz {0} capturas, docs/images tem {1}" -f $imageRow.Groups[1].Value, $images.Count)
}
foreach ($ref in [regex]::Matches($readme, 'docs/images/([\w.\-]+\.png)')) {
    if (-not (Test-Path (Join-Path $Root ("docs\images\" + $ref.Groups[1].Value)))) {
        Add-Problem ("README: imagem citada e ausente: " + $ref.Groups[1].Value)
    }
}

if (-not $Verify -and $written.Count -gt 0) {
    Write-Output ("Guias gerados: " + ($written -join ', '))
}
Write-Output ("{0} plugins ({1} Airwindows + {2} Forja) | {3} .vst3" -f
    $installed.Count, $airwindows.Count, $ownForja.Count, $installed.Count)

if ($problems.Count -eq 0) {
    Write-Output "DOCS OK: README, AGENTS e capturas batem com o codigo."
} else {
    Write-Output ""
    Write-Output "DIVERGENCIAS NAS DOCS:"
    foreach ($problem in $problems) { Write-Output (" - " + $problem) }
    if ($Verify) { exit 1 }
}
