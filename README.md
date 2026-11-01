# Forja — suíte de plugins

36 plugins VST3 em JUCE 9: 31 com DSP adaptado do **Airwindows** (MIT, Chris Johnson) e
cinco entregas da Forja — o sampler original **DRUMFORGE**, o rack **FORJARACK** e os minis
**MINIDRUM** (engine do Drumforge), **MINISAT** (adaptador do BRASA) e **CANIVETE** (player
SoundFont SF2/SF3) — com interface nativa
do padrão Forja (sem WebView), painel FUNDAMENTOS em português e validação de paridade amostra
a amostra contra o código original nas adaptações Airwindows. A suíte entrega o VST3;
o build é VST3-only por padrão (o Standalone só aparece se você ligar `FORJA_STANDALONE=ON`).

A suíte cobre EQ, dinâmica, channel strips, fita, espaço, lo-fi, uma linha de one-knobs de
mixagem e amps/gabinetes para guitarra, baixo e DI. Cada plugin tem README próprio com
parâmetros, DSP, latência medida, desvios do adaptador e créditos. Os editores são
redimensionáveis com aspecto fixo (70% a 200% do tamanho de projeto).

O repositório é organizado por **bases de DSP**: `Airwindows/` reúne as 31 adaptações
MIT; `drumforge/` é a segunda base, com DSP original da Forja — o **DRUMFORGE**, sampler
de bateria com engine própria, mesa de 16 canais e multi-out, já compilado junto da suíte;
`Forjarack/` é a base do **FORJARACK**, um rack em cadeia estilo Série 500 que empilha oito
gavetas com os adaptadores MIT já validados; `Minis/` é a base dos minis originais da Forja,
começando pelo **MINIDRUM** — leitor de kits do Drumforge em formato compacto, que reutiliza
o engine do DRUMFORGE como bloco próprio — e pelo **MINISAT**, saturador de um knob que toca
o adaptador do BRASA (Tube2) através de um mapa de drive documentado; e pelo **CANIVETE SUÍÇO**,
player SoundFont multi-timbral (16 canais MIDI) que lê `.sf2`/`.sf3` de `Documentos/Forja/Canivete`
sobre o TinySoundFont (MIT, Bernhard Schelling), com suporte a SF3 ligado só quando o decodificador
Ogg está presente no vendor.

Fora dessas bases convive no mesmo working tree o **`pianoforge/`**: um projeto irmão
**autônomo e non-JUCE** (SDK VST3 cru + backend de janela próprio, engine de soundfont),
com repo, build e guia próprios. Ele **não** faz parte do build nem dos pacotes da Forja e
segue as regras do seu próprio repositório, não as deste README.

![CINZÉL](docs/images/cinzel.png)

## Download

Cada release publica três pacotes com os VST3 e um guia de instalação:

| Plataforma | Arquivo | Conteúdo |
| --- | --- | --- |
| Windows | `Forja-<versão>-windows-x64.zip` | VST3 x64 e `LEIA-ME.txt` |
| macOS | `Forja-<versão>-macos-universal.zip` | VST3 universais (Apple Silicon + Intel) |
| Linux | `Forja-<versão>-linux-x64.zip` | VST3 x64 e `LEIA-ME.txt` |

Os pacotes trazem apenas o VST3. O build é VST3-only por padrão; o Standalone não é
mais gerado (ligue `FORJA_STANDALONE=ON` na configuração se precisar abrir um plugin sem DAW).

Os binários também ficam em `C:\Users\pablo\Documents\prototipos` neste ambiente de
desenvolvimento.

## Estado

| Item | Estado |
| --- | --- |
| Targets locais (`<Nome>_VST3`) | 36 plugins · 36 binários VST3 |
| Paridade com os originais Airwindows | Verde em todos os executáveis |
| Guarda de alocação em `process()` | PASS (adaptadores; DRUMFORGE, FORJARACK, MINIDRUM, MINISAT e CANIVETE cobertos por testes de engine próprios) |
| Latência por impulso | Medida em todos os modelos |
| Interface e FUNDAMENTOS | Conferidos por captura DPI-aware nas 31 adaptações; capturas dos produtos Forja pendentes |
| UIs redimensionáveis | 36 editores com aspecto fixo; os 31 da Airwindows conferidos em 100% e 150% |
| Builds Windows/macOS/Linux | GitHub Actions em tags `v*` |
| Artefatos em `Documents/prototipos` | 36 `.vst3` espelhados (31 Airwindows + DRUMFORGE + FORJARACK + MINIDRUM + MINISAT + CANIVETE) |
| Capturas da interface | 35 imagens em `docs/images/` |
| Gerador de plugins | `new_plugin.ps1` gera plugin que compila de imediato |
| DRUMFORGE (sampler) | Engine, mesa, multi-out e abas KITS/MIXER/LOOPS prontos |
| Loops MIDI do DRUMFORGE | Biblioteca, grade de passos, preview no BPM do host e arrasto prontos |
| FORJARACK (rack) | Oito gavetas em série, bypass/solo, master e barra de redução de ganho |
| MINIDRUM (mini) | Leitor de kits do Drumforge: puxar kit, foto e volume |
| MINISAT (mini) | Saturador de um knob: drive sobre o adaptador do BRASA, gaveta no rack |
| CANIVETE (mini) | Player SoundFont SF2/SF3 multi-timbral: pasta de banks, seletor de instrumento e volume |
| Validação em host (DAW) | Pendente · não há DAW no ambiente |

## Famílias

| Família | Pasta | Plugins | Papel |
| --- | --- | --- | --- |
| Saturação | `Airwindows/Saturation/` | BRASA | válvula e calor de tubo |
| EQ | `Airwindows/Eq/` | CINZÉL, TORNO, LIMA | paramétrico dinâmico, tom e bus/master |
| Dinâmica | `Airwindows/Dynamics/` | TÊMPERA, PRENSA, MALHO, ESCÓRIA, MURALHA, DIQUE | compressores, vari-mu, impacto, de-esser, limitador e clipe de pico |
| Channel Strip | `Airwindows/Strips/` | BIGORNA, CADINHO | strip completo e strip de bus |
| Fita | `Airwindows/Tape/` | LAMINADOR, ÓXIDO | 2 pistas/master e cassete/lo-fi |
| Espaço | `Airwindows/Space/` | REPIQUE, ECO, SALÃO, FÔLEGO | delays de fita/multi-espaço e reverbs |
| Lo-Fi | `Airwindows/Lofi/` | GRANALHA | bitcrusher com undersampling |
| Mix | `Airwindows/OneKnob/` | LIXA, MEL, CERA, CORPO, ÍMPETO, REBARBA | um controle, mixagem rápida |
| Amp | `Airwindows/Amps/` | GABINETE, PONTA, CHIME, CRICK, LOBO, DELUXE | amps, baixo e DI |
| Bateria | `drumforge/Drumforge/` | DRUMFORGE | sampler de bateria com mesa e multi-out |
| Rack | `Forjarack/Forjarack/` | FORJARACK | rack em cadeia com módulos Airwindows MIT |
| Minis | `Minis/Minidrum/`, `Minis/Minisat/` e `Minis/Canivete/` | MINIDRUM, MINISAT, CANIVETE | minis Forja: leitor de kits, saturador de um knob e player SoundFont |

## Plugins

| Plugin | Família | Modelo Airwindows | Controles | Latência |
| --- | --- | --- | --- | --- |
| BRASA | Saturação | Tube2 | 3 | 0 |
| TÊMPERA | Dinâmica | ButterComp2 | 3 | 0 |
| PRENSA | Dinâmica | Pressure5 | 6 | 1 |
| MALHO | Dinâmica | Pop2 | 5 | 1 |
| MURALHA | Dinâmica | ADClip9 | 5 | 1 |
| DIQUE | Dinâmica | FinalClip | 0 | 1 |
| CINZÉL | EQ | Parametric + dinâmica Forja | 11 | 0 |
| TORNO | EQ | Baxandall3 | 3 | 0 |
| LIMA | EQ | SmoothEQ2 | 8 | 0 |
| ESCÓRIA | Dinâmica | DeBess | 5 | 0 |
| BIGORNA | Channel Strip | CStrip2 | 10 | 0 |
| CADINHO | Channel Strip | X2Buss | 10 | 1 |
| LAMINADOR | Fita | ToTape9 | 9 | 1 |
| ÓXIDO | Fita | IronOxide5 | 7 | 0 |
| REPIQUE | Espaço | TapeDelay2 | 6 | 0 (PDC) |
| ECO | Espaço | Echojam | 4 | 0 (PDC) |
| SALÃO | Espaço | kRockstar2 | 5 | 0 (PDC) |
| FÔLEGO | Espaço | kStation | 6 | 0 (PDC) |
| GRANALHA | Lo-Fi | DeRez5 | 6 | 0 |
| LIXA | Mix | PurestDrive | 1 | 0 |
| MEL | Mix | Sweeten | 1 | 0 |
| CERA | Mix | PurestWarm3 | 1 | 0 |
| CORPO | Mix | Mojo | 1 | 0 |
| ÍMPETO | Mix | Dyno | 1 | 0 |
| REBARBA | Mix | Slew3 | 1 | 0 |
| GABINETE | Amp | Cabs2 | 2 | 0 |
| PONTA | Amp | PointyGuitar | 10 | 0 |
| CHIME | Amp | ChimeyGuitar2 | 10 | 0 |
| CRICK | Amp | CrickBass | 2 | 1 / 3 / 5 |
| LOBO | Amp | Wolfbot | 0 | 0 |
| DELUXE | Amp | ChimeyDeluxe | 10 | 0 |
| DRUMFORGE | Bateria | engine Forja (FX: SALÃO, LIXA) | 249 | 0 |
| FORJARACK | Rack | 22 adaptadores MIT como módulos | gavetas + master | 0 (PDC) |
| MINIDRUM | Minis | engine DRUMFORGE (bloco próprio) | volume + kit | 0 |
| MINISAT | Minis | BRASA (Tube2) como bloco | 1 (drive) | 0 |
| CANIVETE | Minis | TinySoundFont (MIT, player SF2/SF3) | volume + banco + instrumento | 0 |

Contagens não incluem o bypass Forja, presente em todos e exposto por
`getBypassParameter()`. Em REPIQUE, ECO, SALÃO e FÔLEGO o atraso interno é musical e não
compensação: o PDC é 0. A latência do CRICK varia com a taxa (1 em 44,1/48 kHz, 3 em
88,2/96 kHz, 5 em 192 kHz). Todas as demais são medidas por impulso no harness.
O DIQUE (FinalClip) é um clipe de pico extremo: só atua acima de +4,18 dBFS e é
transparente em material que já está em 0 dBFS. Para limitar em 0 dBFS use o MURALHA.

### DRUMFORGE

O sampler de bateria da base `drumforge/` é DSP próprio da Forja, não uma adaptação: o
engine de vozes, o mixer, o limiter e a interface são originais. Apenas os dois efeitos do
master reutilizam adaptadores já validados da suíte — SALÃO (kRockstar2) no FX 1 e LIXA
(PurestDrive) no FX 2 —, e o compressor do master é o TÊMPERA (ButterComp2).

- 16 pads fixos, até 8 camadas de velocity por pad, até 16 tomadas em round-robin e
  grupos de choke com rampa de 5 ms.
- 64 vozes com roubo da mais antiga; interpolação linear e correção da taxa do arquivo
  para a da sessão.
- 5 saídas estéreo: MAIN e AUX 1 a 4, escolhidas por pad.
- Canal por pad: trim, HPF, compressor, EQ de três bandas, pan, mute, solo, fase, dois
  envios pós-fader e fader com medidor. Master com trim, compressor, limiter a −0,3 dBFS
  e os dois efeitos.
- Kit por pasta de samples (classificação por nome, marcadores `vl/v/vel/layer` e
  `rr/rt/take`) ou por arquivo `.dfkit`; a importação grava o kit em
  `Documentos/Forja/Drumforge/Kits`.
- MIDI por General MIDI (36 bumbo, 38 caixa, 42/46 chimbais, 49 prato…), com o evento
  aplicado na amostra exata. Latência 0, sem preamp, sem oversampling.
- Loops MIDI: biblioteca em `Documentos/Forja/Drumforge/Loops` (`.mid`/`.midi`, com
  subpastas por pacote), busca e filtro por pacote, grade de 1/16 com uma faixa por pad
  que o arquivo toca e preview que toca o groove no BPM do host (`loop_sync`) ou num BPM
  manual de 40 a 240. Os golpes entram no mesmo ponto do bloco que um evento MIDI;
  o loop fecha em compassos inteiros.
- Arrasto do `.mid` selecionado para a DAW pela alça ARRASTAR MIDI.
- 249 parâmetros de automação (15 por pad, 6 do master, 2 do preview de loops e o bypass).

### FORJARACK

O **FORJARACK** (base `Forjarack/`) é um rack em cadeia estilo Série 500: oito gavetas em
série, cada uma hospedando um adaptador **Airwindows** já validado por paridade na suíte —
sem DSP novo, apenas empilhado. Não tem teste de paridade do rack em si (cada módulo já é
parity-testado isoladamente); o teste do engine cobre a cadeia em série, o pass-through, o
bypass/mute/solo por gaveta, a fase e o isolamento por instância. A UI sai um pouco do padrão
(menu duplo categoria/módulo por gaveta, cor de fachada por família e barra de redução de
ganho nos compressores), mas mantém a paleta, os knobs e o painel FUNDAMENTOS da casa. Os
créditos MIT a Chris Johnson aparecem por módulo.

### MINIDRUM

O **MINIDRUM** (base `Minis/`) é o primeiro mini Forja: um instrumento compacto que toca um
kit do Drumforge sem abrir o sampler completo. Ele **reutiliza o engine do DRUMFORGE como
bloco** (nada de DSP forkado) e expõe apenas um botão para puxar um kit da biblioteca
compartilhada (`Documentos/Forja/Drumforge/Kits`), a foto do kit e um knob de volume — o kit
já carrega a própria mix. Também funciona como gaveta instrument dentro do FORJARACK. Sem
paridade com vendor (DSP herdado, já coberto pelo teste do engine); o teste próprio cobre
carga/adoção de kit, volume, premix, latência 0, bypass, medidor, troca com reap e ausência
de alocação no áudio.

### MINISAT

O **MINISAT** (base `Minis/`) é o segundo mini Forja: um saturador de um knob (DRIVE) que
**reutiliza o adaptador validado do BRASA (Airwindows Tube2, MIT) como bloco**, agora em
`Forja/Source/forja/dsp/ForjaBrasa.h`, compartilhado entre os dois. A única camada Forja é o
mapa do knob: `drive` sobe o pad de entrada original de 0,50 (default documentado do BRASA,
equivalente a drive 0) até 1,00, com Válvula 0,50 e Saída 0 dB fixos — sem makeup gain, sem
normalização, sem DSP novo. A paridade amostra a amostra mora no BRASA; o teste do MINISAT
cobra o mapa (bit a bit igual ao adaptador nos drives 0, 0,25, 0,50, 0,75 e 1), o amolecimento
do timbre com o drive, a rampa de bypass, latência 0 medida por impulso, zero alocação e o
registro da gaveta `minisat` no FORJARACK — a primeira a usar a fachada valve.

### CANIVETE

O **CANIVETE SUÍÇO** (base `Minis/`) é o terceiro mini Forja: um player SoundFont genérico e
multi-timbral (16 canais MIDI) que toca banks `.sf2`/`.sf3` depositados em
`Documentos/Forja/Canivete`. Diferente das outras bases, aqui o DSP **não é nosso** — é o
[TinySoundFont](https://github.com/schellingb/TinySoundFont) de Bernhard Schelling (MIT),
vendorizado byte a byte em `Forja/Source/forja/vendor/soundfont/tsf.h`. A Forja acrescenta só o
wrapper em tempo real (`SoundFontEngine`): carga assíncrona do bank, adoção por token sem alocar
no áudio, reap do bank substituído, medidor por bloco e a camada de bypass de 10 ms. Sem teste de
paridade (o timbre vem do original); o teste próprio (`forja_canivete`) gera um SF2 mínimo válido
no harness e cobre silêncio sem bank, carga/adoção, trim de volume, latência 0, velocidade,
bypass, medidor, troca com reap, bank inválido, CC/program change multi-timbral, escolha de
instrumento pelo canal 0, ausência de
alocação e a gaveta `canivete` no FORJARACK. Como camada Forja, o banco de instrumentos do
bank aparece no seletor INSTRUMENTO (canal melódico 0); program change do host continua
mandando no canal. O SF3 (amostras Ogg) acende automaticamente quando
`stb_vorbis.c` está na pasta de vendor (hoje presente: o build lê `.sf3` de fábrica).

## Interface

- Janela completa de 1000 × 540 px ou compacta de 560 × 260 px (LOBO, DIQUE, MINIDRUM, MINISAT e CANIVETE).
- Redimensionável com aspecto fixo: o layout é desenhado no canvas de projeto e escalado
  de 70% a 200% por `ForjaCanvas.h`, sem reflow nem perda de nitidez.
- Topo com produto, família, chip da fonte Airwindows, chip MIT, chip REDES e FUNDAMENTOS.
- Cards por função, knobs Forja (arraste vertical, duplo clique volta ao padrão, ponto
  âmbar quando modificado), medidores de pico com CLIP e medidores de redução de ganho
  nos compressores.
- Curva de EQ por FFT em CINZÉL e LIMA, com nós arrastáveis; TORNO é só leitura por ser
  um processo não linear.
- FUNDAMENTOS abre um painel nativo (sem WebView) com o que faz, como funciona, controles,
  operação e proveniência; fecha por botão, `Esc` ou clique fora.
- LAMINADOR e ÓXIDO trazem arte de máquina de fita no cabeçalho.
- DRUMFORGE tem três abas no topo (KITS, MIXER e LOOPS): a biblioteca de kits com
  arrastar-e-soltar de pasta, o mapa dos 16 pads, a mesa com 16 canais mais o master fixo
  à direita e o navegador de grooves MIDI com grade de 1/16, preview no BPM do host e
  arrasto do `.mid` para a DAW. Os knobs da mesa são compactos (`MiniKnob`) para caber um
  canal inteiro em 92 px de largura.
- Todo texto acentuado passa por `forja::ui::utf8()`.
- FORJARACK empilha oito gavetas em série; cada uma tem dois menus suspensos (categoria e
  módulo, um sobre o outro), chave on/off e de escuta (bypass/solo), e os compressores exibem
  barra de redução de ganho; o master traz trim e fase.
- MINIDRUM é compacto: um botão para puxar o kit da biblioteca do Drumforge, a foto do kit e
  um knob de volume, sobre o chrome Forja de sempre.
- MINISAT é compacto: um knob DRIVE e a curva de transferência do Tube2 (reuse do componente
  do BRASA), alimentada pelos parâmetros mapeados, com medidor de saída e CLIP.

## Capturas

| | |
| --- | --- |
| ![LAMINADOR](docs/images/laminador.png) | ![REPIQUE](docs/images/repique.png) |
| Fita 2 pistas / master (ToTape9) | Delay de fita com sync de BPM (TapeDelay2) |
| ![BIGORNA](docs/images/bigorna.png) | ![CHIME](docs/images/chime.png) |
| Channel strip completo (CStrip2) | Amp comprimido (ChimeyGuitar2) |
| ![LIXA](docs/images/lixa.png) | ![FUNDAMENTOS](docs/images/fundamentos-cinzel.png) |
| One-knob compacto (PurestDrive) | Painel FUNDAMENTOS nativo |
| ![DRUMFORGE KITS](docs/images/drumforge-kits.png) | ![DRUMFORGE MIXER](docs/images/drumforge-mixer.png) |
| Sampler: biblioteca e mapa dos 16 pads | Sampler: mesa de 16 canais e master |
| ![DRUMFORGE LOOPS](docs/images/drumforge-loops.png) | |
| Sampler: biblioteca de loops MIDI, grade e preview | |

<details>
<summary><strong>Galeria completa (31 adaptações Airwindows)</strong></summary>

| | | |
| --- | --- | --- |
| ![BRASA](docs/images/brasa.png) | ![TÊMPERA](docs/images/tempera.png) | ![PRENSA](docs/images/prensa.png) |
| ![MALHO](docs/images/malho.png) | ![CINZÉL](docs/images/cinzel.png) | ![TORNO](docs/images/torno.png) |
| ![LIMA](docs/images/lima.png) | ![ESCÓRIA](docs/images/escoria.png) | ![BIGORNA](docs/images/bigorna.png) |
| ![CADINHO](docs/images/cadinho.png) | ![LAMINADOR](docs/images/laminador.png) | ![ÓXIDO](docs/images/oxido.png) |
| ![REPIQUE](docs/images/repique.png) | ![ECO](docs/images/eco.png) | ![SALÃO](docs/images/salao.png) |
| ![FÔLEGO](docs/images/folego.png) | ![GRANALHA](docs/images/granalha.png) | ![LIXA](docs/images/lixa.png) |
| ![MEL](docs/images/mel.png) | ![CERA](docs/images/cera.png) | ![CORPO](docs/images/corpo.png) |
| ![ÍMPETO](docs/images/impeto.png) | ![REBARBA](docs/images/rebarba.png) | ![GABINETE](docs/images/gabinete.png) |
| ![PONTA](docs/images/ponta.png) | ![CHIME](docs/images/chime.png) | ![CRICK](docs/images/crick.png) |
| ![LOBO](docs/images/lobo.png) | ![DELUXE](docs/images/deluxe.png) | ![MURALHA](docs/images/muralha.png) |
| ![DIQUE](docs/images/dique.png) | | |

</details>

## Build

Windows:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

macOS:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Linux (Ubuntu 22.04+):

```bash
sudo apt-get install -y build-essential cmake ninja-build pkg-config \
  libasound2-dev libx11-dev libxext-dev libxinerama-dev libxrandr-dev \
  libxcursor-dev libxcomposite-dev libxrender-dev libfreetype6-dev \
  libfontconfig1-dev libglu1-mesa-dev libcurl4-openssl-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Cada plugin também compila sozinho, por exemplo:

```powershell
cmake --build build --config Release --target Cinzel_VST3
```

Os alvos ficam em `build/Airwindows/<Família>/<Plugin>/<Plugin>_artefacts/Release/`. O
DRUMFORGE fica fora dessa árvore, em `build/drumforge/Drumforge/Drumforge_artefacts/Release/`;
o FORJARACK e os minis seguem o mesmo esquema sob `build/Forjarack/` e `build/Minis/`.
Os testes ficam em `build/Tests/Release/` (`forja_parity_<modelo>.exe`,
`forja_alloc_guard.exe`, `forja_drumforge_engine.exe`, `forja_forjarack_engine.exe`,
`forja_minidrum.exe`, `forja_minisat.exe` e `forja_canivete.exe`).

LTO é o padrão local (`-DFORJA_LTO=OFF` desliga). A CI usa `FORJA_LTO=OFF` para os três
SOs caberem no tempo do runner; o código e os testes são os mesmos.

## Validação

- Paridade amostra a amostra em double contra o `*Proc.cpp` vendorizado, com cenas de
  seno, graves, ruído estéreo, quadrada, DC, silêncio, impulso, degrau e rajada; SR de
  44,1 a 192 kHz; mono e estéreo; varredura de cada parâmetro.
- Critérios: sem NaN/Inf, erro máximo ≤ 1e-9 e RMS ≤ −140 dBFS, zero alocação, silêncio
  → saída ~0, latência de impulso igual ao `setLatencySamples`.
- CINZÉL: paridade bit-exata com `dyn = 0` e teste comportamental em `dyn = 0,5`.
- ECO: as 17 posições de espaço são testadas uma a uma.
- Delays e reverbs usam factory por caso para comparar estados equivalentes.
- `ctest` executa os 28 executáveis de paridade (TÊMPERA/PRENSA/MALHO juntos + 27 modelos
  individuais), o `forja_alloc_guard` e os testes de engine do DRUMFORGE, FORJARACK,
  MINIDRUM e MINISAT nos três SOs.
- DRUMFORGE não tem paridade (o DSP é Forja): o teste do engine monta um kit sintético e
  cobre troca de kit sem alocar no áudio, camadas de velocity, round-robin, choke,
  roubo de voz, isolamento dos 5 barramentos, mute/solo, mapa General MIDI, latência
  0 por impulso, rampa de bypass com vozes preservadas, zero alocação em `process()` e a
  importação de pasta (WAV, classificação, `.dfkit` ida e volta). O mesmo executável grava
  MIDI na hora e valida a biblioteca de loops, o fechamento em compasso inteiro, a
  importação e o preview disparando no frame medido, com o cursor voltando ao início.
- FORJARACK não tem paridade do rack (cada módulo já é parity-testado isolado): o teste do
  engine cobre cadeia em série, pass-through, bypass/mute/solo por gaveta, fase e
  isolamento por instância.
- MINIDRUM não tem paridade (DSP herdado do engine do DRUMFORGE): o teste cobre carga e
  adoção de kit, volume, premix do kit, latência 0, bypass, medidor, troca com reap,
  ausência de alocação no áudio e o registro da gaveta instrument.
- MINISAT não tem paridade própria (o DSP é o adaptador do BRASA, já parity-testado): o
  teste cobre o mapa drive→pad bit a bit contra o adaptador em seis drives, monotonia,
  achatamento do timbre com o drive, rampa de bypass, latência 0 por impulso, zero
  alocação em estéreo e mono e o registro da gaveta `minisat` no rack. O `alloc_guard`
  passou a cobrir o adaptador compartilhado `forja::BrasaDSP`.
- Última execução local (Windows, Release): `ctest` com 33/33 testes verdes.

## Instalação

Cada release do GitHub traz um pacote por plataforma com os VST3 e `LEIA-ME.txt` com o
caminho de instalação. Neste ambiente, os binários prontos e os instaladores por família
ficam em `C:\Users\pablo\Documents\prototipos`:

- `INSTALAR-FORJA.txt` — guia mestre (copiar as pastas `.vst3` inteiras para
  `C:\Program Files\Common Files\VST3\`).
- `INSTALAR-EQ.txt`, `INSTALAR-DINAMICA.txt`, `INSTALAR-STRIPS.txt`, `INSTALAR-FITA.txt`,
  `INSTALAR-ESPACO.txt`, `INSTALAR-LOFI.txt`, `INSTALAR-MIX.txt`, `INSTALAR-AMP.txt` —
  um por família.
- `INSTALAR-BRASA.txt`, `INSTALAR-TEMPERA.txt`, `INSTALAR-PRENSA.txt`,
  `INSTALAR-MALHO.txt` — os quatro individuais originais.

O teste é feito no VST3 dentro da DAW, então o build não gera Standalone por padrão.
Para abrir um plugin sem DAW (por exemplo, para capturar a UI), configure com
`-DFORJA_STANDALONE=ON`. Os pacotes de release nunca incluem Standalone.

## Repositório e novos plugins

- `AGENTS.md` — guia obrigatório para agentes de código: base Airwindows, padrão Forja e
  regras invioláveis.
- `docs/ARQUITETURA.md` — pipeline vendor → adapter → processor → editor, paridade,
  canvas redimensionável.
- `docs/NOVO-PLUGIN.md` — passo a passo para portar um modelo novo.
- `tools/new_plugin.ps1` (Windows) e `tools/new_plugin.py` — geram plugin, adapter e
  entradas de CMake a partir de um comando.
- `.github/workflows/release.yml` — build e release automáticos para Windows, macOS e
  Linux em tags `v*`.

### Publicar no GitHub

```bash
git init -b main
git add .
git commit -m "Forja: suíte de 31 plugins com DSP Airwindows"
git remote add origin git@github.com:<usuario>/forja-suite.git
git push -u origin main
```

O workflow aceita `workflow_dispatch` (aba Actions → Release → Run workflow) para gerar
os três pacotes sem tag. Para publicar uma versão:

```bash
git tag v0.4.1
git push origin v0.4.1
```

O `.gitignore` já exclui `build/`, artefatos compilados, capturas locais, `.qoder/` e o
material de referência (`_ref/`, `_src/`, zips de conceito).

## Licença e créditos

DSP original: Airwindows, © Chris Johnson, licença MIT. O código original fica verbatim em
`Forja/Source/forja/vendor/airwindows/<Modelo>/`, com `LICENSE` e `PROVENANCE.md` (URL,
commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`, data e SHA-256). Nenhum original é
compilado dentro dos plugins: os adaptadores Forja ficam em `Forja/Source/forja/dsp/`.
Interface, medidores, bypass e camada dinâmica do CINZÉL são Forja.

O DRUMFORGE é DSP da Forja: engine de sampler, mixer, limiter, importação de pasta e
interface foram escritos para esta suíte e não derivam de código de terceiros. Os blocos
de efeito do master usam os mesmos adaptadores Airwindows MIT já validados na suíte
(ButterComp2, kRockstar2 e PurestDrive), e o crédito a Chris Johnson aparece no README,
no FUNDAMENTOS e no `PROVENANCE.md` de cada modelo. Nenhum sample ou biblioteca de
bateria acompanha o plugin.

O FORJARACK não cria DSP: ele empacota os adaptadores Airwindows já validados, então cada
módulo carrega a licença MIT e a proveniência do seu modelo. O MINIDRUM reutiliza o engine
do DRUMFORGE como bloco — mesmo DSP próprio da Forja, sem fork e sem vendor.
