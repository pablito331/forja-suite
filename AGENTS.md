# Forja — guia para agentes de código

Este arquivo é a leitura obrigatória para qualquer IA que for mexer no repositório.
Ele explica de onde vem o DSP, qual padrão a suíte segue e o que **não** pode ser feito.

## O que é a Forja

Forja é uma suíte de 35 plugins VST3 + Standalone em JUCE 9 (C++20) para Windows,
macOS e Linux, organizada em bases de DSP. A base `Airwindows/` adapta plugins da
[Airwindows](https://www.airwindows.com/vsts/) de **Chris Johnson**, licenciado em MIT.
A base `drumforge/` é a de DSP original: o **DRUMFORGE**, sampler de bateria com engine
própria, mesa e multi-out. A base `Forjarack/` entrega o **FORJARACK**, um rack em cadeia
estilo Série 500 que hospeda adaptadores Airwindows já validados. A base `Minis/` entrega
os minis Forja de propósito único: **MINIDRUM** (leitor de kits do Drumforge com um knob de
volume) e **MINISAT** (saturador de um knob que reusa o adaptador do BRASA). A Forja
acrescenta interface nativa (sem WebView), painel
FUNDAMENTOS em português, bypass suavizado, medição, presets/estado via APVTS e validação
de paridade amostra a amostra contra o código original nas adaptações Airwindows.

Regra de ouro da base Airwindows: **não inventamos DSP**. O timbre e o comportamento vêm
do original. O que a Forja adiciona é explicitamente marcado como camada Forja (por
exemplo, a dinâmica do CINZÉL e o sync de BPM de REPIQUE/ECO) e sempre documentado no
README, no FUNDAMENTOS e no `PROVENANCE.md`. O Drumforge segue a especificação em
`drumforge/ideia para o plugin.md`, com a UI já simplificada para três abas (Kits, Mixer e
Loops MIDI) — racks de FX dedicados, Settings e editor de mapas ficam para depois. O DSP é
próprio, sem vendor Airwindows; só os efeitos do master (SALÃO, LIXA) e o compressor
(TÊMPERA) reutilizam adaptadores já validados.

## Mapa do repositório

```
Airwindows/                        base 1: adaptações MIT da Airwindows (31 plugins)
  Dynamics/ Saturation/ Eq/        famílias (cada plugin é um target CMake próprio)
  Strips/ Space/ Lofi/ OneKnob/ Amps/ Tape/
drumforge/                         base 2: sampler de bateria, DSP Forja
  Drumforge/Source/engine/         Kit, SampleStore, VoicePool, DrumEngine, mixer
  Drumforge/Source/loops/          LoopLibrary (MIDI em beats) e LoopPlayer (preview)
  Drumforge/Source/ui/             abas Kits, Mixer e Loops, MiniKnob
Forjarack/                         base 3: FORJARACK, rack Série 500 de módulos Airwindows
  Forjarack/Source/rack/           ModuleCatalog (registro append-only), módulos
Minis/                             base 4: minis Forja de propósito único (MINIDRUM, MINISAT)
  Minidrum/Source/minidrum/        wrapper do engine Drumforge como plugin autônomo
  Minisat/Source/minisat/          wrapper do adaptador do BRASA como plugin e módulo de rack
Forja/                             biblioteca compartilhada forja_ui + DSP + vendor
  Source/forja/dsp/                adapters header-only (Forja<Modelo>.h)
  Source/forja/ui/                 chrome nativo (topBar, statusBar, cards, knobs, painel)
  Source/forja/vendor/airwindows/  originais verbatim (referência, NÃO compilados)
Tests/                             paridade, guarda de alocação, engine do Drumforge, shim VST2
cmake/ForjaSuite.cmake             helper forja_add_plugin(...)
docs/                              arquitetura, guia de novo plugin, imagens do README
tools/                             gerador de scaffold de plugin
```

## Bases

- `Airwindows/` — 31 adaptações, uma pasta por família. Paridade, vendor e
  `PROVENANCE.md` são obrigatórios.
- `drumforge/` — produto Forja original (sampler, mixer e groove browser). Não usa
  vendor Airwindows por padrão e **não** tem teste de paridade; os testes cobrem sampler,
  MIDI, biblioteca/preview de loops, estado, multi-out e ausência de alocação no áudio.
  Pode reutilizar adaptadores Airwindows como blocos de canal.
- `Forjarack/` — produto Forja original (o **FORJARACK**, rack em cadeia estilo Série 500).
  Cada gaveta hospeda um adaptador Airwindows já validado; o DSP de cada módulo é o
  original MIT, reusado como bloco de canal. Não tem vendor novo nem teste de paridade
  próprio (a paridade é herdada dos módulos); o teste do engine cobre o catalogo
  append-only, o encadeamento das gavetas, bypass/solo por gaveta e ausência de alocação.
- `Minis/` — base dos minis originais Forja: plugins compactos de propósito único —
  **MINIDRUM** (leitor de kits do Drumforge com um knob de volume) e **MINISAT**
  (saturador de um knob que reusa o adaptador validado do BRASA). Cada mini reusa um bloco
  já testado como DSP (engine do DRUMFORGE ou `forja::BrasaDSP`), sem fork e sem vendor
  novo; **não** têm paridade própria (herdada do bloco), e o teste de cada um cobre o
  wrapper, o mapeamento/estado próprio, latência, bypass, medidor, zero alocação e o
  registro no catálogo do ForjaRack. A família **MINIS** no catálogo é mais ampla que a
  pasta: agrupa esses minis Forja e os one-knobs MIT, que continuam em
  `Airwindows/OneKnob/` por regra de proveniência.
- **Exceção registrada — `pianoforge/`**: é um projeto **autônomo e separado** (repo
  Git próprio, `github.com/pablito331/pianoforge`), **não** uma base Forja. É **non-JUCE**
  (SDK VST3 cru da Steinberg + backend de janela Win32/Cocoa/X11 próprio, C++17), tem
  engine de soundfont (tsf/sfizz), formato de bundle `.pforge`, parâmetros próprios e
  painel FUNDAMENTOS caseiro. **Não** entra no `CMakeLists.txt` raiz, **não** usa
  `forja_add_plugin`/`ForjaEditorBase`/APVTS/`utf8()` e **não** compartilha a UI da suíte.
  Está ignorado no `.gitignore` da Forja (o repo da suíte não rastreia seu conteúdo, para
  não gravar um gitlink quebrado). As regras deste arquivo **não** se aplicam a ele; quem
  mexer no pianoforge segue o guia dentro do próprio repo.
- Uma base nova é uma pasta irmã com `CMakeLists.txt` listando suas famílias/plugins e
  `add_subdirectory(<Base>)` na raiz. `forja_add_plugin` já é agnóstico de pasta; os
  geradores `tools/new_plugin.*` são específicos da base Airwindows.

## Pipeline de um plugin

1. **Vendorização** — copie `Modelo.cpp`, `Modelo.h` e `ModeloProc.cpp` do commit
   `b110fac3991fff7c4655cc2d25d6a3e221f9799c` da Airwindows para
   `Forja/Source/forja/vendor/airwindows/<Modelo>/` junto com `LICENSE` e um
   `PROVENANCE.md` com URL, commit, data e SHA-256 de cada arquivo. O original nunca é
   alterado nem compilado dentro do plugin.
2. **Adapter** — transcreva `processDoubleReplacing` para
   `Forja/Source/forja/dsp/Forja<Modelo>.h`, header-only, em `namespace forja::<slug>`.
   `processDouble` é a autoridade de paridade. Converta `static` em membros por instância,
   dimensione buffers com folga para 192 kHz, zere estado em `prepare`, use RNG por
   instância e não aloque no caminho de áudio.
3. **Processor** — `PluginProcessor` cria o APVTS, expõe `bypass` por
   `getBypassParameter()`, lê o playhead quando o plugin tem sync, chama o adapter em
   `processBlock` e publica medidores atômicos.
4. **Editor** — derive de `forja::ui::ForjaEditorBase`, monte cards/knobs em
   `layoutContent` e o painel FUNDAMENTOS em `ForjaInfoText.h`.
5. **Testes** — um executável `Tests/Source/parity_<slug>.cpp` comparando o original
   vendorizado com o adapter, entrada no `alloc_guard`, e medição de latência por impulso.
6. **Documentação** — README do plugin, entrada no catálogo (`DSP-0xx`), créditos MIT.

## Regras invioláveis

- **Paridade**: com os parâmetros originais e sem camadas Forja ativas, a saída em double
  deve ser idêntica (erro máx ≤ 1e-9, RMS ≤ −140 dBFS). Se o libm do sistema impedir,
  registre e aceite ≤ 1e-7.
- **Nada de controles inventados**: um parâmetro só existe se vier do original ou for uma
  camada Forja aprovada e documentada. Onde a UI usa um nome em português, o rótulo
  original fica no FUNDAMENTOS.
- **Params em 0–1**, passo 0,01, escalonamento no DSP; `bypass` é bool e padrão `false`.
  Exceção documentada: `division` do sync usa passo 0,2 com 6 divisões.
- **Latência**: `setLatencySamples` recebe o valor **medido por impulso** no harness, não o
  presumido. Delays/reverbs têm atraso musical e PDC 0.
- **Bypass** sempre com crossfade de 10 ms; estado do adapter nunca é resetado no bypass.
- **Sem alocação** em `prepareToPlay`/`processBlock` (o `alloc_guard` cobre isso).
- **Texto acentuado** sempre por `forja::ui::utf8("...")` ou literais `\u00XX`; nunca
  confie no encoding do compilador/CMake.
- **Interface**: sem WebView, sem assets externos em runtime, sem dependências novas de UI.
  Use `ForjaCard`, `ForjaKnob`, `ForjaMeter`, `ForjaGainReductionMeter`, `ForjaGRHistory`,
  `ForjaEqCurve`, `ForjaChoice`, `ForjaTapeArt`, `ForjaLookAndFeel`.
- **Resizable**: os editores desenham em coordenadas fixas (1000×540 ou 560×260) e
  `forja::ui::ForjaCanvas.h` aplica a escala/aspecto em `resized()`. Não posicione nada em
  função de `getWidth()`/`getHeight()` do editor; use o canvas virtual.
- **Proveniência**: toda cópia da Airwindows mantém `LICENSE` (MIT, Chris Johnson) e
  `PROVENANCE.md`; nenhum arquivo do vendor entra em `target_sources` de plugin (só nos
  executáveis de paridade).

## Convenções de código

- Namespaces: `forja::dsp` (adapters), `forja::ui`, `forja::<slug>` (info/README text).
- Adapter: `struct Parameters` + `prepare(double sampleRate)` +
  `processDouble(double* left, double* right, int frames)` + `readMeters` +
  `latencySamples()`. Mono usa buffer scratch interno.
- Canais/ordem L-R devem seguir o original; não "corrija" assimetrias do modelo.
- Nada de comentários óbvios; comentários só para desvios e invariantes.
- Código e pastas em ASCII. Acentos só em strings via `utf8()` e nos docs.
- `COMPANY_NAME` recebe a família em ASCII no CMake (ex.: `"Forja - EQ"`); o metadado
  do host não decodifica UTF-8 de forma confiável. Acentos existem só na UI, via `utf8()`.

## Build e testes

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

- Binários: `build/Airwindows/<Família>/<Plugin>/<Plugin>_artefacts/Release/{VST3,Standalone}/`.
  O DRUMFORGE usa `build/drumforge/Drumforge/Drumforge_artefacts/Release/`.
- Testes: `build/Tests/Release/forja_parity_<slug>.exe`, `forja_alloc_guard.exe` e
  `forja_drumforge_engine.exe`.
- Um target isolado: `cmake --build build --config Release --target Cinzel_VST3` (ou
  `Drumforge_Standalone`).
- `FORJA_LTO` (padrão `ON`) liga o LTO dos plugins; a CI configura com `OFF` por tempo de
  runner. Não remova a opção.
- CI/release: `.github/workflows/release.yml` compila os três SOs em tags `v*`.

## Como adicionar um plugin

Siga `docs/NOVO-PLUGIN.md`. Resumo:

1. `tools/new_plugin.ps1` (Windows) ou `tools/new_plugin.py` gera a pasta em
   `Airwindows/<Família>/<Plugin>/` com processor/editor/FUNDAMENTOS/CMakeLists.
2. Vendorize o original e escreva o adapter.
3. Ajuste parâmetros, defaults e UI; escreva o README.
4. Adicione o caso de paridade e o adapter ao `alloc_guard`.
5. Rode build + testes + captura visual antes de considerar pronto.
6. Atualize `Forja — Catálogo de DSP e Mixer.md` e o README da suíte.

## O que não fazer

- Não portar código GPL ou de licença incompatível; a Airwindows é MIT.
- Não modificar arquivos em `vendor/` para "consertar" o original.
- Não adicionar makeup gain, normalização, oversampling ou "melhorias" de som sem
  aprovação explícita.
- Não trocar o DSP por outro modelo "parecido" mantendo o nome.
- Não usar `static` para estado de áudio, relógio, RNG ou buffers.
- Não criar presets que mudem defaults documentados sem atualizar README/FUNDAMENTOS.
- Não commitar `build/`, artefatos, capturas locais, `.qoder/` ou material de referência.

## Estado conhecido

- Paridade verde nos 31 modelos; `alloc_guard` PASS; latências medidas.
- DRUMFORGE compila como VST3 e Standalone; `forja_drumforge_engine` PASS (camadas,
  round-robin, choke, roubo de voz, multi-out, mute/solo, MIDI, latência 0, bypass,
  zero alocação, importação de pasta, biblioteca e preview de loops). As três abas
  (KITS, MIXER, LOOPS) estão implementadas; a validação em host segue pendente.
- FORJARACK compila como VST3 e Standalone; `forja_forjarack_engine` PASS (catálogo
  append-only, encadeamento das gavetas, bypass/solo por gaveta, seção master).
- MINIDRUM compila como VST3 e Standalone; `forja_minidrum` PASS (carga/adoção de kit,
  volume no trim do master, premix `.dfkit`, latência 0, bypass, medidor, troca com reap,
  rejeição de kit inválido, zero alocação e registro da gaveta instrument no ForjaRack).
- MINISAT compila como VST3 e Standalone; `forja_minisat` PASS (mapa drive→pad bit-exato
  com o adaptador do BRASA em seis drives, monotonia, crest caindo com o drive, rampa de
  bypass, latência 0 medida por impulso, zero alocação em estéreo e mono e registro da
  gaveta `minisat` no ForjaRack). O adaptador compartilhado `forja::BrasaDSP` entrou no
  `alloc_guard`.
- Última verificação local (Windows, Release): `ctest` com 33/33 testes verdes.
- Sem DAW no ambiente de desenvolvimento: automação/bypass de host seguem pendentes de
  validação manual (ver README).
- REPIQUE e ECO têm sync de BPM opcional; com `sync = false` o caminho é bit-exato.
