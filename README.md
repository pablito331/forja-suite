# Forja — suíte de plugins

São **36 plugins VST3 em quatro linhas**, em JUCE 9 (C++20) para Windows, macOS e Linux: 31
com DSP adaptado do **Airwindows** (MIT, Chris Johnson) e cinco entregas originais da Forja —
o rack **FORJARACK**, o sampler **DRUMFORGE** e os minis **CANIVETE**, **MINIDRUM** e
**MINISAT**. Interface nativa do padrão Forja (sem WebView), painel FUNDAMENTOS em português,
bypass suavizado, medição e, nas adaptações Airwindows, paridade amostra a amostra contra o
código original. A suíte entrega o VST3; o build é VST3-only por padrão (o Standalone só
aparece com `FORJA_STANDALONE=ON`).

**A página pública é o catálogo** — captura, descrição, estado e download de cada plugin em
<https://pablito331.github.io/forja-suite/>. Este README cuida só do repositório: linhas,
trens de release, build, validação e instalação.

![CINZÉL](docs/images/cinzel.png)

## Download

**Baixar é na página ou no Releases**: <https://pablito331.github.io/forja-suite/> mostra o
link certo por produto; a lista de releases fica em
<https://github.com/pablito331/forja-suite/releases>. Cada release publica um pacote por
plataforma com os VST3 e o guia de instalação:

| Plataforma | Arquivo | Conteúdo |
| --- | --- | --- |
| Windows | `<produto>-<versão>-windows-x64.zip` + instalador `.exe` | VST3 x64, `LEIA-ME.txt` e instalador Inno Setup |
| macOS | `<produto>-<versão>-macos-universal.zip` | VST3 universais (Apple Silicon + Intel) |
| Linux | `<produto>-<versão>-linux-x64.zip` | VST3 x64 e `LEIA-ME.txt` |

`<produto>` é `Forja` na tag da suíte e `ForjaRack`, `ForjaMinis`, `ForjaCanivete`,
`ForjaMinidrum`, `ForjaMinisat`, `ForjaDrumforge` ou `ForjaAirwindows` nas tags de produto
(`ci/products.json` é o registro). Os pacotes trazem apenas o VST3; o Standalone não é gerado
por padrão (ligue `FORJA_STANDALONE=ON` para abrir um plugin sem DAW).

Os binários também ficam em `C:\Users\pablo\Documents\prototipos` neste ambiente de
desenvolvimento.

## Estado

| Item | Estado |
| --- | --- |
| Targets locais (`<Nome>_VST3`) | 36 plugins · 36 binários VST3 |
| Paridade com os originais Airwindows | Verde em todos os executáveis |
| Guarda de alocação em `process()` | PASS (adaptadores; os produtos Forja têm testes de engine próprios) |
| Latência por impulso | Medida em todos os modelos |
| UIs redimensionáveis | 36 editores com aspecto fixo; os 31 da Airwindows conferidos em 100% e 150% |
| Builds Windows/macOS/Linux | GitHub Actions em tags `v*`, com Smoke de GCC nos pushes de `Forja/Source/**` |
| Página pública | No ar em <https://pablito331.github.io/forja-suite/> (GitHub Pages + `tools/build_site.ps1`) |
| Trens de release por produto | CI pronta; nenhum Release de produto publicado ainda — o tag `vrack-1.0.0` aponta para um commit anterior aos consertos |
| CANIVETE na gaveta do rack | Verde: a troca de preset pela gaveta está provada (`forjarack_processor`, `forjarack_vst3_midi`) |
| Gerador de plugins | `new_plugin.ps1` gera plugin que compila de imediato |
| Validação em host (DAW) | Pendente · não há DAW no ambiente |

## Produtos

São 36 plugins em quatro linhas. **O catálogo completo — captura, descrição e download de
cada um — fica na página pública**: <https://pablito331.github.io/forja-suite/>. Este README
fala do repositório: como compilar, testar e instalar.

| Linha | Plugins | O que é | Página | Trem de release |
| --- | --- | --- | --- | --- |
| Airwindows forjados | 31 | EQ, dinâmica, channel strips, fita, espaço, lo-fi, um-knobs de mixagem e amps/DI, adaptados do código MIT de Chris Johnson | [por família](https://pablito331.github.io/forja-suite/#eq) | `vairwindows-X.Y.Z` |
| FORJARACK | 1 | rack em cadeia estilo Série 500: oito gavetas com os adaptadores já validados, hosting dinâmico e as gavetas MINIDRUM/CANIVETE | [ForjaRack](https://pablito331.github.io/forja-suite/#rack) | `vrack-X.Y.Z` |
| Minis | 3 | CANIVETE (player SoundFont SF2/SF3 multi-timbral), MINIDRUM (kits do Drumforge em um knob) e MINISAT (a válvula do BRASA em um knob) | [Minis](https://pablito331.github.io/forja-suite/#minis) | `vminis-X.Y.Z` |
| DRUMFORGE | 1 | sampler de bateria com engine própria: 16 pads, camadas, round-robin, choke, mesa de 16 canais, multi-out e loops MIDI | [Drumforge](https://pablito331.github.io/forja-suite/#drumforge) | `vdrumforge-X.Y.Z` |

A suíte inteira (os 36 plugins) continua saindo da tag sem prefixo, `vX.Y.Z`; tag de produto
não bumpa a versão da suíte. Passo a passo dos trens em `docs/RELEASES.md`.

O repositório é organizado por **bases de DSP**: `Airwindows/` (as 31 adaptações MIT),
`Forjarack/`, `Minis/` e `drumforge/`. Cada plugin é um target CMake próprio; o pipeline
vendor → adapter → processor → editor está em `docs/ARQUITETURA.md`.

O detalhe técnico de cada plugin — modelo Airwindows de origem, lista de parâmetros, latência
medida por impulso, desvios do adaptador, proveniência e créditos — está no README do próprio
plugin (`Airwindows/<Família>/<Plugin>/README.md`, `Forjarack/Forjarack/README.md`,
`Minis/<Mini>/README.md` e `drumforge/Drumforge/README.md`) e na especificação por bloco de
DSP, o `Forja — Catálogo de DSP e Mixer.md`.

### O que a Forja acrescenta

Nas 31 adaptações o timbre vem do original: a Forja põe interface nativa, painel FUNDAMENTOS,
medição, bypass com crossfade de 10 ms e as camadas explicitamente marcadas no FUNDAMENTOS (a
dinâmica do CINZÉL, o sync de BPM de REPIQUE e ECO). DRUMFORGE, FORJARACK e os três Minis são
DSP próprio da Forja, sem vendor Airwindows (o CANIVETE toca sobre o TinySoundFont, MIT). A
ficha técnica de cada um está no README da base e nos cards da página.

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
- Todo texto acentuado passa por `forja::ui::utf8()`.

As variações por produto (as abas KITS/MIXER/LOOPS do DRUMFORGE, as oito gavetas com menu
categoria/módulo do FORJARACK, a curva de transferência do MINISAT) estão nos cards da página
e no README de cada base.

## Capturas

A galeria completa, com a captura real de cada um dos 36 editores, está nos cards da página:
<https://pablito331.github.io/forja-suite/>. Aqui ficam as duas que mostram o padrão da casa:

| | |
| --- | --- |
| ![FUNDAMENTOS](docs/images/fundamentos-cinzel.png) | ![DRUMFORGE MIXER](docs/images/drumforge-mixer.png) |
| Painel FUNDAMENTOS nativo, em português | Mesa do DRUMFORGE: canais, master e medidores |

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
- `ctest` executa os executáveis de paridade, o `forja_alloc_guard` e os testes de engine
  dos produtos Forja nos três SOs.
- Os produtos Forja não têm paridade (DSP próprio ou herdado de um adaptador já testado):
  cada um tem executável e casos próprios, listados na seção **Testes** do README da base —
  `drumforge/Drumforge/README.md`, `Forjarack/Forjarack/README.md` e `Minis/<Mini>/README.md`.
  O rack e os minis também exercitam a gaveta correspondente no FORJARACK.
- Última execução local (Windows, Release): `ctest` com 37/37 verdes. Os dois casos do
  CANIVETE na gaveta demoram: um `.sf3` real de 38 MB leva ~8 s para assentar em
  background, e o seletor da gaveta só existe depois — os testes esperam a carga e saem no
  primeiro som.

## Instalação

Os guias por plataforma ficam em `docs/instalar/` (`INSTALAR-WINDOWS.txt`,
`INSTALAR-MACOS.txt`, `INSTALAR-LINUX.txt`) e dentro de cada pacote como `LEIA-ME.txt`. No
Windows o instalador `.exe` faz o serviço; copiar à mão é só mover as pastas `.vst3` inteiras
para a pasta de VST3 do sistema. O teste é feito no VST3 dentro da DAW, então o build não gera
Standalone por padrão — para abrir um plugin sem DAW (por exemplo, para capturar a UI),
configure com `-DFORJA_STANDALONE=ON`. Os pacotes de release nunca incluem Standalone.

## Repositório e novos plugins

- `AGENTS.md` — guia obrigatório para agentes de código: base Airwindows, padrão Forja e
  regras invioláveis.
- `docs/ARQUITETURA.md` — pipeline vendor → adapter → processor → editor, paridade,
  canvas redimensionável.
- `docs/NOVO-PLUGIN.md` — passo a passo para portar um modelo novo.
- `tools/new_plugin.ps1` (Windows) e `tools/new_plugin.py` — geram plugin, adapter e
  entradas de CMake a partir de um comando.
- `ci/products.json` — registro único de produtos: prefixo de tag, filtros de path, base do
  nome do artefato e regex de testes de cada trem.
- `docs/RELEASES.md` — os trens de release: tag da suíte sem prefixo, tag por produto.
- `installer/collect_payload.ps1` + `installer/forja.iss` — coletam o payload por família e
  montam o instalador Windows com componentes condicionais.
- `site/catalogo.json` + `site/index.html` — a página pública (catálogo, capturas e links de
  download). `tools/build_site.ps1` materializa os cards a partir do catálogo e do
  `ci/products.json`; `tools/validate_site.ps1` confere contagem, famílias e sincronia.
  `.github/workflows/pages.yml` publica a pasta `site/` no GitHub Pages.
- `.github/workflows/release.yml` — build, testes e release dos três SOs em qualquer `v*`,
  com empacotamento filtrado pelo trem.
- `.github/workflows/smoke.yml` — compila um plugin com o GCC do runner de Linux nos pushes
  de `Forja/Source/**`, para pegar na hora o que o MSVC aceita e o GCC reprova.

Fora das bases da suíte convive no mesmo working tree o **`pianoforge/`**: projeto irmão
**autônomo e non-JUCE** (SDK VST3 cru, backend de janela próprio, engine de soundfont), com
repo, build e guia próprios. Ele **não** faz parte do build nem dos pacotes da Forja e segue
as regras do seu próprio repositório.

### Publicar no GitHub

```bash
git init -b main
git add .
git commit -m "Forja: suíte de 36 plugins em quatro linhas"
git remote add origin git@github.com:<usuario>/forja-suite.git
git push -u origin main
```

O workflow aceita `workflow_dispatch` (aba Actions → Release → Run workflow) para gerar
os três pacotes sem tag. Para publicar uma versão:

```bash
git tag v0.5.2          # a suíte inteira
git tag vrack-1.0.1     # só o ForjaRack; os outros prefixos estão em ci/products.json
git push origin vrack-1.0.1
```

A página precisa ser ligada uma vez: **Settings → Pages → Source: GitHub Actions** — sem
isso o `pages.yml` roda verde e não publica nada. O `.gitignore` já exclui `build/`,
artefatos compilados, capturas locais, `.qoder/` e o material de referência (`_ref/`,
`_src/`, zips de conceito).

## Licença e créditos

DSP original: Airwindows, © Chris Johnson, licença MIT. O código original fica verbatim em
`Forja/Source/forja/vendor/airwindows/<Modelo>/`, com `LICENSE` e `PROVENANCE.md` (URL,
commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`, data e SHA-256). Nenhum original é
compilado dentro dos plugins: os adaptadores Forja ficam em `Forja/Source/forja/dsp/`.
Interface, medidores, bypass e camada dinâmica do CINZÉL são Forja.

Os produtos Forja não inventam DSP: o DRUMFORGE é engine própria da suíte (nenhum sample ou
biblioteca de bateria acompanha o plugin), o FORJARACK apenas empilha adaptadores já
validados — cada módulo carrega a licença MIT e a proveniência do seu modelo — e o MINIDRUM
reutiliza o engine do DRUMFORGE como bloco, sem fork. Onde um produto Forja usa um adaptador
Airwindows, o crédito a Chris Johnson aparece no README e no FUNDAMENTOS: é o caso do MINISAT
(Tube2) e dos efeitos do master do DRUMFORGE (ButterComp2, kRockstar2 e PurestDrive).

A única exceção é o CANIVETE, cujo DSP vem do TinySoundFont, © Bernhard Schelling, MIT,
vendorizado byte a byte em `Forja/Source/forja/vendor/soundfont/` com `LICENSE` e
`PROVENANCE.md`; `stb_vorbis.c` (domínio público, Sean Barrett) entra só para o SF3. Não se
usa FluidSynth: a regra da suíte é manter o binário livre de GPL.
