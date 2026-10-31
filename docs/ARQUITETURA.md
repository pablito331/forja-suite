# Arquitetura da Forja

## Visão geral

A suíte é dividida em **bases**: pastas irmãs na raiz que agrupam produtos por origem de
DSP. `Airwindows/` guarda as 31 adaptações MIT com paridade; `drumforge/` será a primeira
base de DSP original Forja (em concepção). A camada Forja — UI, adaptadores, tema e
vendor — é compartilhada por todas.

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
