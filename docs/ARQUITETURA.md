# Arquitetura da Forja

## Visão geral

A suíte é dividida em **bases**: pastas irmãs na raiz que agrupam produtos por origem de
DSP. `Airwindows/` guarda as 31 adaptações MIT com paridade; `drumforge/` é a primeira base
de DSP original Forja, já com o sampler DRUMFORGE compilado. A camada Forja — UI,
adaptadores, tema e vendor — é compartilhada por todas.

```
             ┌─────────────────────────────────────────────────────┐
Airwindows   │  vendor/airwindows/<Modelo>/  (referência, MIT)     │
original  ──▶│    Modelo.cpp · Modelo.h · ModeloProc.cpp          │
             └─────────────────────────────────────────────────────┘
                        │ transcrição manual (nunca modificado)
                        ▼
             ┌─────────────────────────────────────────────────────┐
             │  Forja/Source/forja/dsp/Forja<Modelo>.h            │
             │  header-only · estado por instância · processDouble │
             └─────────────────────────────────────────────────────┘
                        │                                │
                        ▼                                ▼
     ┌──────────────────────────────┐      ┌──────────────────────────────┐
     │ Airwindows/<Família>/        │      │ Tests/Source/parity_<slug>.cpp│
     │ <Plugin>/Source/             │      │ original vendorizado × adapter│
     │ PluginProcessor (APVTS/áudio)│      └──────────────────────────────┘
     │ PluginEditor (ForjaEditorBase)│
     └──────────────────────────────┘
```

Bases de DSP original não passam por vendor nem por paridade: o DSP mora no próprio
`<Base>/<Família>/<Plugin>/Source/`, e os testes são funcionais e de RT-safety (ver
[Testes](#testes-e-paridade)).

## Camadas

### 1. Vendor (`Forja/Source/forja/vendor/airwindows/`)

Cópia verbatim do código da Airwindows no commit
`b110fac3991fff7c4655cc2d25d6a3e221f9799c`, com `LICENSE` (MIT) e `PROVENANCE.md`
contendo URL, commit, data e SHA-256. Serve para (a) auditoria, (b) compilação nos testes
de paridade. **Nunca** é compilado nos plugins que o usuário instala. Exceção: o `Tube2`
usado pelo BRASA fica no vendor privado do plugin, em
`Airwindows/Saturation/Brasa/Source/forja/vendor/airwindows/Tube2/`.

### 2. Adapter (`Forja/Source/forja/dsp/Forja<Modelo>.h`)

- Header-only, sem dependências de UI, namespace `forja::dsp::<slug>`.
- `struct Parameters` com valores normalizados 0–1 (ou bool) exatamente na ordem do
  original; o adapter faz o escalonamento.
- `prepare(sampleRate)`, `reset()`, `processDouble(L, R, frames)`.
- `processDouble` transcreve fielmente `processDoubleReplacing` do original. `static`
  locais viram membros; RNG ganha estado por instância; buffers são dimensionados para
  192 kHz.
- `readMeters(bool reset)` publica picos atômicos para a UI; `latencySamples()` devolve a
  latência medida.
- `Parameters` carrega `bypass` e, quando aplicável, `sync`/`division`/`hostBpm` (REPIQUE
  e ECO); o processor preenche o BPM do playhead a cada bloco.
- Bypass: crossfade de 10 ms com dry atrasado pela latência do modelo.
- Mono: buffer scratch interno; o original continua vendo o mesmo par L/R da transcrição.

### 3. Processor (`Airwindows/<Família>/<Plugin>/Source/PluginProcessor.*`)

- APVTS com `bypass` + parâmetros do modelo; `getBypassParameter()` aponta para `bypass`.
- `prepareToPlay` chama `adapter.prepare`; `setLatencySamples` usa o valor medido.
- `processBlock` monta `Parameters`, trata mono/estéreo, chama o adapter e atualiza os
  medidores. Nenhuma alocação.
- Plugins com sync (REPIQUE, ECO) leem `getPlayHead()->getPosition()->getBpm()` por bloco;
  sem BPM válido o adapter cai no controle manual.
- Estado: `APVTS::State` (parâmetros) e nada mais — sem estado oculto.

### 4. Editor

- Editores novos derivam de `forja::ui::ForjaEditorBase`: topo, barra de status, painel
  FUNDAMENTOS, timer de 30 Hz, badge/bypass e layout. A subclasse só monta cards e
  implementa `layoutContent`/`updateTelemetry`.
- Os quatro plugins originais (BRASA, TÊMPERA, PRENSA, MALHO) mantêm editor próprio com
  o mesmo desenho e usam `ForjaCanvas.h` para escala.
- Canvas virtual: tudo é posicionado em coordenadas de projeto (1000×540 ou 560×260).
  `applyCanvasScale` aplica um `AffineTransform` a cada filho em `resized()`, com aspecto
  fixo via `ComponentBoundsConstrainer`. O JUCE 9 renderiza filhos transformados como
  vetor no contexto do pai, então não há perda de nitidez.
- Textos acentuados: `forja::ui::utf8()`; rótulos originais aparecem no FUNDAMENTOS.

## Base drumforge (DSP próprio)

O DRUMFORGE não segue o pipeline vendor → adapter: o DSP nasce em
`drumforge/Drumforge/Source/engine/` e a UI em `Source/ui/`. O alvo é criado por
`forja_add_instrument` (VST3 instrumento, `NEEDS_MIDI_INPUT`, categoria `Instrument|Drum`).

- **`Kit` / `SampleStore`** — o kit é um modelo puro (pads, camadas, caminhos), lido e
  gravado em XML `.dfkit`. Os samples são decodificados em uma thread do `ThreadPool` e o
  resultado vira um `LoadedKit` imutável.
- **Publicação do kit** — `requestKit(ptr, token)` guarda ponteiro + token em atômicos; o
  áudio adota no início do bloco e publica `acceptedToken`. Quem importou espera o token
  ser aceito antes de liberar o kit anterior — troca sem lock e sem alocação no áudio.
- **`VoicePool`** — 64 vozes fixas, cada uma com sample, posição fracionária, incremento
  (taxa do arquivo ÷ taxa da sessão), envelope de choke e grupo. Roubo da voz mais antiga.
- **`DrumEngine`** — fila SPSC de disparos (128), segmentação do bloco nos eventos de MIDI,
  cursor de round-robin por pad e camada, mapa nota → pad, 16 `ChannelStrip` e 5
  `AudioBuffer` de saída (MAIN + AUX 1–4).
- **Canal** — `Filters` (biquads RBJ em double, forma direta transposta II),
  `Compressor` (estéreo-linkado, joelho suave, GR por ganho de make-up) e `MasterBus`
  (trim, TÊMPERA, limiter de −0,3 dBFS, retornos de FX 1 e FX 2).
- **Medidores** — pico, GR e clip por pad e por master em `std::atomic`; a leitura
  consome o valor (`exchange`), então a UI lê uma vez por tique e repassa o retrato só
  para a aba visível.
- **Multi-out** — o processor expõe 5 saídas estéreo e usa `getBusBuffer` por barramento
  em buffers de vista (`AudioBuffer` que aponta para o buffer do host, sem cópia).
- **Biblioteca de loops** — `Source/loops/LoopLibrary.h` lê `.mid`/`.midi` para uma
  sequência em **beats** (o meta-evento de tempo define o BPM de origem) e fecha o loop no
  compasso inteiro seguinte ao último golpe. A varredura de
  `Documentos/Forja/Drumforge/Loops` trata cada subpasta como pacote.
- **Preview de loops** — `Source/loops/LoopPlayer.h` roda antes de `DrumEngine::process` e
  enfileira cada golpe com o offset exato em amostras, no mesmo caminho do MIDI ao vivo.
  A sequência é trocada por token, como o kit, então o áudio nunca lê ponteiro liberado.
  O BPM vem do playhead (`loop_sync`) ou de `loop_bpm`; o fim do loop volta o cursor sem
  cortar as vozes que ainda soam.

## Testes e paridade

`Tests/Source/parity_common.h` fornece:

- geração de sinais (seno, 60 Hz, quadrada, ruído estéreo, DC ±1, silêncio, impulso,
  degrau, rajada);
- comparação amostra a amostra com erro máximo e RMS em dBFS;
- `measureLatency` por impulso;
- `checkSilence`;
- `measureDspLatency`; e em cada teste com estado (delays, reverbs) o laço cria original
  e adapter novos por caso, para comparar estados equivalentes.

Cada `parity_<slug>.cpp` compila o `*Proc.cpp` vendorizado contra
`Tests/third_party/vst2_shim/audioeffectx.h` e compara com o adapter. O `_plug.cpp` não é
compilado; o harness transcreve as inicializações do construtor original.

Critérios: sem NaN/Inf, erro máx ≤ 1e-9, RMS ≤ −140 dBFS, silêncio → ~0, latência igual à
medida, zero alocação (`alloc_guard`).

**Bases de DSP original** (Drumforge e futuras) não têm original vendorizado para
comparar. Os testes delas cobrem o comportamento esperado — sampler, mapeamento MIDI,
serialização de estado, barramentos múltiplos, round-robin/velocity, carregamento de
samples — e a ausência de alocação no caminho de áudio. A guarda de alocação continua
valendo para toda base.

O `forja_drumforge_engine` segue esse modelo: monta um `LoadedKit` sintético na memória
(sem depender de biblioteca de samples), substitui `operator new`/`delete` para flagrar
alocação e mede a saída por barramento. Além do comportamento do engine, ele exporta WAVs
em uma pasta temporária para exercitar a importação e o `.dfkit` de ida e volta, e grava
MIDI na hora para cobrir `LoopLibrary` (leitura em beats, BPM do meta-evento, fechamento em
compasso inteiro, recusa de arquivo inválido/sem notas, varredura e importação) e
`LoopPlayer` (disparo no frame medido, volta do loop, cursor dentro do loop e `stop`).

## Build

- `cmake/ForjaSuite.cmake` expõe
  `forja_add_plugin(target, product, code, bundle, source_dir, categories, company)`.
- Cada plugin tem um `CMakeLists.txt` de poucas linhas; a família só lista os
  `add_subdirectory`, e a base (`Airwindows/CMakeLists.txt`) lista as famílias.
- A raiz encadeia `Forja` (biblioteca compartilhada) → `Airwindows` → bases futuras →
  `Tests`.
- `Forja/CMakeLists.txt` compila a biblioteca estática `forja_ui` e expõe os headers.
- JUCE 9.0.4 via `FetchContent`, preso por `GIT_TAG`.
- Artefatos espelham a árvore: `build/Airwindows/<Família>/<Plugin>/<Plugin>_artefacts/`.

## Decisões que parecem estranhas e são intencionais

- TORNO só é neutro em Treble = Bass = 0,5; o botão PLANO existe por isso.
- Curva de EQ medida por FFT da resposta ao impulso do adapter (baixo nível), não por
  fórmula duplicada; em TORNO é só leitura porque o processo é não linear.
- Delays/reverbs têm PDC 0: o atraso é musical.
- Latências de CRICK variam com a taxa; o valor é medido, não presumido.
- O fabricante do VST3 carrega a família (`Forja - EQ`, `Forja - Fita`, …) para a DAW
  categorizar; o nome é ASCII porque o host não decodifica UTF-8 no metadado.
