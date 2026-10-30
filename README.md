# Forja — suíte de plugins

29 plugins VST3 + Standalone para JUCE 9, com DSP adaptado do **Airwindows** (MIT, Chris
Johnson), interface nativa do padrão Forja (sem WebView), painel FUNDAMENTOS em português
e validação de paridade amostra a amostra contra o código original.

A suíte cobre EQ, dinâmica, channel strips, fita, espaço, lo-fi, uma linha de one-knobs de
mixagem e amps/gabinetes para guitarra, baixo e DI. Cada plugin tem README próprio com
parâmetros, DSP, latência medida, desvios do adaptador e créditos. Os editores são
redimensionáveis com aspecto fixo (70% a 200% do tamanho de projeto).

![CINZÉL](docs/images/cinzel.png)

## Download

Cada release publica três pacotes com VST3 + Standalone e um guia de instalação:

| Plataforma | Arquivo | Conteúdo |
| --- | --- | --- |
| Windows | `Forja-<versão>-windows-x64.zip` | plugins x64, `.exe` standalone e `LEIA-ME.txt` |
| macOS | `Forja-<versão>-macos-universal.zip` | VST3 e `.app` universais (Apple Silicon + Intel) |
| Linux | `Forja-<versão>-linux-x64.zip` | VST3 e executáveis standalone x64 |

Os binários também ficam em `C:\Users\pablo\Documents\prototipos` neste ambiente de
desenvolvimento.

## Estado

| Item | Estado |
| --- | --- |
| Targets Release (`<Nome>_VST3`, `<Nome>_Standalone`) | 29 plugins · 58 binários |
| Paridade com os originais Airwindows | Verde em todos os executáveis |
| Guarda de alocação em `process()` | PASS (28 adaptadores; BRASA usa DSP próprio) |
| Latência por impulso | Medida em todos os modelos |
| Interface e FUNDAMENTOS | Conferidos por captura DPI-aware |
| UIs redimensionáveis | 29 editores com aspecto fixo, conferidos em 100% e 150% |
| Builds Windows/macOS/Linux | GitHub Actions em tags `v*` |
| Artefatos em `Documents/prototipos` | 29 `.vst3` + 29 Standalone |
| Capturas da interface | 30 imagens em `docs/images/` |
| Gerador de plugins | `new_plugin.ps1` gera plugin que compila de imediato |
| Validação em host (DAW) | Pendente · não há DAW no ambiente |

## Famílias

| Família | Pasta | Plugins | Papel |
| --- | --- | --- | --- |
| Raiz | `Brasa/` … `Malho/` | BRASA, TÊMPERA, PRENSA, MALHO | válvula, comp de cola, vari-mu e impacto |
| EQ | `Eq/` | CINZÉL, TORNO, LIMA | paramétrico dinâmico, tom e bus/master |
| Dinâmica | `Dynamics/` | ESCÓRIA | de-esser de agudos |
| Channel Strip | `Strips/` | BIGORNA, CADINHO | strip completo e strip de bus |
| Fita | `Tape/` | LAMINADOR, ÓXIDO | 2 pistas/master e cassete/lo-fi |
| Espaço | `Space/` | REPIQUE, ECO, SALÃO, FÔLEGO | delays de fita/multi-espaço e reverbs |
| Lo-Fi | `Lofi/` | GRANALHA | bitcrusher com undersampling |
| Mix | `OneKnob/` | LIXA, MEL, CERA, CORPO, ÍMPETO, REBARBA | um controle, mixagem rápida |
| Amp | `Amps/` | GABINETE, PONTA, CHIME, CRICK, LOBO, DELUXE | amps, baixo e DI |

## Plugins

| Plugin | Família | Modelo Airwindows | Controles | Latência |
| --- | --- | --- | --- | --- |
| BRASA | Raiz | Tube2 | 3 | 0 |
| TÊMPERA | Raiz | ButterComp2 | 3 | 0 |
| PRENSA | Raiz | Pressure5 | 6 | 1 |
| MALHO | Raiz | Pop2 | 5 | 1 |
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

Contagens não incluem o bypass Forja, presente em todos e exposto por
`getBypassParameter()`. Em REPIQUE, ECO, SALÃO e FÔLEGO o atraso interno é musical e não
compensação: o PDC é 0. A latência do CRICK varia com a taxa (1 em 44,1/48 kHz, 3 em
88,2/96 kHz, 5 em 192 kHz). Todas as demais são medidas por impulso no harness.

## Interface

- Janela completa de 1000 × 540 px ou compacta de 560 × 260 px (LOBO).
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
- Todo texto acentuado passa por `forja::ui::utf8()`.

## Capturas

| | |
| --- | --- |
| ![LAMINADOR](docs/images/laminador.png) | ![REPIQUE](docs/images/repique.png) |
| Fita 2 pistas / master (ToTape9) | Delay de fita com sync de BPM (TapeDelay2) |
| ![BIGORNA](docs/images/bigorna.png) | ![CHIME](docs/images/chime.png) |
| Channel strip completo (CStrip2) | Amp comprimido (ChimeyGuitar2) |
| ![LIXA](docs/images/lixa.png) | ![FUNDAMENTOS](docs/images/fundamentos-cinzel.png) |
| One-knob compacto (PurestDrive) | Painel FUNDAMENTOS nativo |

<details>
<summary><strong>Galeria completa (29 plugins)</strong></summary>

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
| ![LOBO](docs/images/lobo.png) | ![DELUXE](docs/images/deluxe.png) | |

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
cmake --build build --config Release --target Cinzel_VST3 Cinzel_Standalone
```

Os alvos ficam em `build/<Família>/<Plugin>/<Plugin>_artefacts/Release/`. Os testes ficam
em `build/Tests/Release/` (`forja_parity_<modelo>.exe` e `forja_alloc_guard.exe`).

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
- `ctest` executa os 26 executáveis de paridade (TÊMPERA/PRENSA/MALHO juntos + 25 modelos
  novos) e o `forja_alloc_guard` nos três SOs.
- Última execução local (Windows, Release): build dos 29 targets e `ctest` com 27/27
  testes verdes; 29 `.vst3` e 29 Standalone regravados após as mudanças de UI.

## Instalação

Cada release do GitHub traz um pacote por plataforma com VST3, Standalone e `LEIA-ME.txt`
com o caminho de instalação. Neste ambiente, os binários prontos e os instaladores por
família ficam em `C:\Users\pablo\Documents\prototipos`:

- `INSTALAR-FORJA.txt` — guia mestre (copiar as pastas `.vst3` inteiras para
  `C:\Program Files\Common Files\VST3\`).
- `INSTALAR-EQ.txt`, `INSTALAR-DINAMICA.txt`, `INSTALAR-STRIPS.txt`, `INSTALAR-FITA.txt`,
  `INSTALAR-ESPACO.txt`, `INSTALAR-LOFI.txt`, `INSTALAR-MIX.txt`, `INSTALAR-AMP.txt` —
  um por família.
- `INSTALAR-BRASA.txt`, `INSTALAR-TEMPERA.txt`, `INSTALAR-PRENSA.txt`,
  `INSTALAR-MALHO.txt` — os quatro individuais originais.

Cada Standalone (`<Nome>-standalone.exe`) abre o plugin sem instalar.

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
git commit -m "Forja: suíte de 29 plugins com DSP Airwindows"
git remote add origin git@github.com:<usuario>/forja.git
git push -u origin main
```

O workflow aceita `workflow_dispatch` (aba Actions → Release → Run workflow) para gerar
os três pacotes sem tag. Para publicar uma versão:

```bash
git tag v0.4.0
git push origin v0.4.0
```

O `.gitignore` já exclui `build/`, artefatos compilados, capturas locais, `.qoder/` e o
material de referência (`_ref/`, `_src/`, zips de conceito).

## Licença e créditos

DSP original: Airwindows, © Chris Johnson, licença MIT. O código original fica verbatim em
`Forja/Source/forja/vendor/airwindows/<Modelo>/`, com `LICENSE` e `PROVENANCE.md` (URL,
commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`, data e SHA-256). Nenhum original é
compilado dentro dos plugins: os adaptadores Forja ficam em `Forja/Source/forja/dsp/`.
Interface, medidores, bypass e camada dinâmica do CINZÉL são Forja.
