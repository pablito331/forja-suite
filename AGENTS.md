# Forja — guia para agentes de código

Este arquivo é a leitura obrigatória para qualquer IA que for mexer no repositório.
Ele explica de onde vem o DSP, qual padrão a suíte segue e o que **não** pode ser feito.

## O que é a Forja

Forja é uma suíte de 29 plugins VST3 + Standalone em JUCE 9 (C++20) para Windows,
macOS e Linux. O áudio não é original: cada plugin adapta um plugin da
[Airwindows](https://www.airwindows.com/vsts/) de **Chris Johnson**, licenciado em MIT.
A Forja acrescenta interface nativa (sem WebView), painel FUNDAMENTOS em português,
bypass suavizado, medição, presets/estado via APVTS e validação de paridade amostra a
amostra contra o código original.

Regra de ouro: **não inventamos DSP**. O timbre e o comportamento vêm do original. O que
a Forja adiciona é explicitamente marcado como camada Forja (por exemplo, a dinâmica do
CINZÉL e o sync de BPM de REPIQUE/ECO) e sempre documentado no README, no FUNDAMENTOS e
no `PROVENANCE.md`.

## Mapa do repositório

```
Brasa/ Tempera/ Prensa/ Malho/     plugins originais (não usam ForjaEditorBase)
Eq/ Dynamics/ Strips/ Tape/        famílias (cada plugin é um target CMake próprio)
Space/ Lofi/ OneKnob/ Amps/
Forja/                             biblioteca compartilhada forja_ui + DSP + vendor
  Source/forja/dsp/                adapters header-only (Forja<Modelo>.h)
  Source/forja/ui/                 chrome nativo (topBar, statusBar, cards, knobs, painel)
  Source/forja/vendor/airwindows/  originais verbatim (referência, NÃO compilados)
Tests/                             paridade, guarda de alocação e shim VST2
cmake/ForjaSuite.cmake             helper forja_add_plugin(...)
docs/                              arquitetura, guia de novo plugin, imagens do README
tools/                             gerador de scaffold de plugin
```

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
- `COMPANY_NAME` recebe a família em escapes hexadecimais UTF-8 no CMake
  (ex.: `"Forja \\xC2\\xB7 EQ"`), porque o CMake converte para a página de código local.

## Build e testes

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

- Binários: `build/<Família>/<Plugin>/<Plugin>_artefacts/Release/{VST3,Standalone}/`.
- Testes: `build/Tests/Release/forja_parity_<slug>.exe` e `forja_alloc_guard.exe`.
- Um target isolado: `cmake --build build --config Release --target Cinzel_VST3`.
- `FORJA_LTO` (padrão `ON`) liga o LTO dos plugins; a CI configura com `OFF` por tempo de
  runner. Não remova a opção.
- CI/release: `.github/workflows/release.yml` compila os três SOs em tags `v*`.

## Como adicionar um plugin

Siga `docs/NOVO-PLUGIN.md`. Resumo:

1. `tools/new_plugin.ps1` (Windows) ou `tools/new_plugin.py` gera a pasta com
   processor/editor/FUNDAMENTOS/CMakeLists.
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

- Paridade verde nos 29 modelos; `alloc_guard` PASS; latências medidas.
- Sem DAW no ambiente de desenvolvimento: automação/bypass de host seguem pendentes de
  validação manual (ver README).
- REPIQUE e ECO têm sync de BPM opcional; com `sync = false` o caminho é bit-exato.
