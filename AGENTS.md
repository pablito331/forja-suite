# Forja — guia para agentes de código

Este arquivo é a leitura obrigatória para qualquer IA que for mexer no repositório.
Ele explica de onde vem o DSP, qual padrão a suíte segue e o que **não** pode ser feito.

## O que é a Forja

Forja é uma suíte de 37 plugins VST3 em JUCE 9 (C++20) para Windows,
macOS e Linux, organizada em bases de DSP. A base `Airwindows/` adapta plugins da
[Airwindows](https://www.airwindows.com/vsts/) de **Chris Johnson**, licenciado em MIT.
A base `drumforge/` é a de DSP original: o **DRUMFORGE**, sampler de bateria com engine
própria, mesa e multi-out. A base `Forjarack/` entrega o **FORJARACK**, um rack em cadeia
estilo Série 500 que hospeda adaptadores Airwindows já validados. A base `Minis/` entrega
os minis Forja de propósito único: **CANIVETE** (player de soundfont SF2/SF3),
**MINIDRUM** (leitor de kits do Drumforge com um knob de
volume) e **MINISAT** (saturador de um knob que reusa o adaptador do BRASA). A base `Pianos/` entrega
o **PIANOFORGE**, piano de amostras que pega um soundfont comum e acrescenta o corpo físico
do instrumento. A Forja
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
Minis/                             base 4: minis Forja de propósito único (CANIVETE, MINIDRUM, MINISAT)
  Canivete/Source/canivete/        player SF2/SF3 (tsf + stb_vorbis no vendor)
  Minidrum/Source/minidrum/        wrapper do engine Drumforge como plugin autônomo
  Minisat/Source/minisat/          wrapper do adaptador do BRASA como plugin e módulo de rack
Pianos/                            base 5: pianos Forja de amostras + modelo físico (PIANOFORGE)
  Pianoforge/Source/engine/        PianoEngine (tsf num TU próprio), Bundle (.pforge), dsp portado
  Pianoforge/Source/               processor APVTS (17 params), editor (cards + MOTOR), gaveta do rack
Forja/                             biblioteca compartilhada forja_ui + DSP + vendor
  Source/forja/dsp/                adapters header-only (Forja<Modelo>.h)
  Source/forja/ui/                 chrome nativo (topBar, statusBar, cards, knobs, painel)
  Source/forja/vendor/airwindows/  originais verbatim (referência, NÃO compilados)
Tests/                             paridade, guarda de alocação, engine do Drumforge, shim VST2
cmake/ForjaSuite.cmake             helper forja_add_plugin(...)
ci/products.json                   registro único dos trens de release (fonte de verdade)
installer/                         coletor de payload + forja.iss (Inno Setup)
site/                              página pública: catalogo.json + index.html gerado
docs/                              arquitetura, guia de novo plugin, imagens do README
tools/                             scaffold, capturas, gerador/validador do site
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
  **CANIVETE** (player de soundfont SF2/SF3), **MINIDRUM** (leitor de kits do Drumforge com
  um knob de volume) e **MINISAT**
  (saturador de um knob que reusa o adaptador validado do BRASA). Cada mini reusa um bloco
  já testado como DSP (engine do DRUMFORGE ou `forja::BrasaDSP`; o CANIVETE usa o vendor de
  soundfont `tsf`/`stb_vorbis`), sem fork e sem vendor Airwindows
  novo; **não** têm paridade própria (herdada do bloco), e o teste de cada um cobre o
  wrapper, o mapeamento/estado próprio, latência, bypass, medidor, zero alocação e o
  registro no catálogo do ForjaRack. A família **MINIS** no catálogo é mais ampla que a
  pasta: agrupa esses minis Forja e os one-knobs MIT, que continuam em
  `Airwindows/OneKnob/` por regra de proveniência.
- `Pianos/` — base dos pianos Forja de amostras + modelo físico. Hoje abriga o
  **PIANOFORGE**, produto próprio (não é adaptação Airwindows nem tem paridade MIT): pega
  um soundfont de piano comum (`.sf2`/`.sf3` via `tsf` num TU próprio, como o CANIVETE) ou um
  bundle `.pforge` do projeto-irmão e acrescenta a cadeia física portada — transiente de
  martelo, ressonância simpática, EQ de corpo, tampa, chorus, reverb FDN e master. Segue o
  padrão da suíte (JUCE 9, `forja_add_instrument`, APVTS com 17 parâmetros em 0–1,
  `ForjaEditorBase`, painel FUNDAMENTOS, bypass 10 ms, zero alocação no RT). O `tsf` do
  PIANOFORGE usa `TSF_STATIC` para coexistir com o do CANIVETE dentro do FORJARACK, onde o
  PIANOFORGE também entra como **gaveta instrument** (registro append-only, carga assíncrona).
- **Referência histórica — `pianoforge/`**: era o projeto **autônomo e separado** (repo Git
  próprio, `github.com/pablito331/pianoforge`), **non-JUCE** (SDK VST3 cru da Steinberg +
  backend de janela Win32/Cocoa/X11, C++17), com engine de soundfont, o formato `.pforge` e
  painel FUNDAMENTOS caseiro. Esse repo **não** é base da suíte, **não** entra no
  `CMakeLists.txt` raiz e **não** é compilado: ele **passou a ser referência de DSP** — o
  código foi *portado* (não linkado) para o produto **PIANOFORGE** em `Pianos/`. Continua
  ignorado no `.gitignore` da Forja (o repo da suíte não rastreia seu conteúdo, para não
  gravar um gitlink quebrado). As regras deste arquivo **não** se aplicam a ele; o caminho
  SFZ/sfizz e o empacotador `.pforge` dentro do plugin ficam registrados como expansão futura.
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

- Binários: `build/Airwindows/<Família>/<Plugin>/<Plugin>_artefacts/Release/VST3/`.
  O DRUMFORGE usa `build/drumforge/Drumforge/Drumforge_artefacts/Release/`.
- Testes: `build/Tests/Release/forja_parity_<slug>.exe`, `forja_alloc_guard.exe` e
  `forja_drumforge_engine.exe`.
- Um target isolado: `cmake --build build --config Release --target Cinzel_VST3` (ou
  `Drumforge_VST3`).
- `FORJA_LTO` (padrão `ON`) liga o LTO dos plugins; a CI configura com `OFF` por tempo de
  runner. Não remova a opção.
- `FORJA_STANDALONE` (padrão `OFF`): o build da suíte é VST3-only. Ligue `ON` apenas para
  abrir um plugin sem DAW (por exemplo, para capturar a UI); os pacotes nunca incluem Standalone.
- CI/release: `.github/workflows/release.yml` compila os três SOs em qualquer tag `v*` —
  sem prefixo = suíte, com prefixo = um produto. Os jobs que falham publicam a causa em
  **anotações** pelo action compartilhado `.github/actions/diagnostico` (o log bruto de
  repositório público exige sessão), então não adianta "rodar de novo" sem ler as anotações.
- **Smoke** (`.github/workflows/smoke.yml`) compila e **liga** com o **GCC** do mesmo runner
  da CI de release: o Cinzel (puxa o `forja_ui` inteiro), os cinco produtos Forja
  (`Forjarack`, os três `Minis` e o `Drumforge`) e o `forja_alloc_guard`, que inclui os 31
  adaptadores num TU só. Dispara em push em `Forja/Source/**`, `Forja/CMakeLists.txt`,
  `Airwindows/Eq/Cinzel/**`, `Forjarack/**`, `Minis/**`, `drumforge/**`, `cmake/**` e
  `CMakeLists.txt`. Existe porque o MSVC aceita coisas que o GCC reprova, e as duas que já
  derrubaram o job inteiro de Linux estavam no mesmo default argument de `ForjaKnob`:
  `Options o = {}` e, depois do conserto aparente, o aggregate `Options` **aninhado** na
  classe com `= Options()` (o GCC exige os default member initializers de um tipo aninhado
  antes do fim da classe envolvente). O formato válido é `ForjaKnobOptions` em namespace +
  `using Options = ForjaKnobOptions` + default `Options()`, **sem** declarar construtor
  (os editores passam listas parciais e o MSVC reprova com C2665). Descobrir isso só na
  release custa 20 minutos.
- Quando o Smoke falha, ele publica os erros do GCC como **anotações** do check-run e no
  resumo do job. O log bruto pela API exige token (`/actions/jobs/<id>/logs` responde 403
  num repositório público), então `GET /repos/<dono>/<repo>/check-runs/<check_run_id>/annotations`
  é o caminho para ler a causa sem abrir a página.
- `forja_ui` é uma biblioteca **estática ligada dentro de cada plugin**, e o plugin é
  compartilhado — então `POSITION_INDEPENDENT_CODE ON` no `Forja/CMakeLists.txt` é
  obrigatório: sem `-fPIC` o `ld` do Ubuntu reprova o link dos 36 plugins
  (`relocation R_X86_64_PC32 against symbol ... can not be used when making a shared
  object`). O CMake só liga `-fPIC` por padrão em alvos `SHARED`/`MODULE`, nunca em
  `STATIC`, e o MSVC ignora a propriedade — o build de Windows nunca avisa. Vale para
  qualquer estática nova que entre no link de um plugin.
- **Outro link de Linux que o MSVC não avisa**: nunca use `std::atomic<T>` de 16 bytes
  (uma struct de medidor redonda, por exemplo) no SharedCode. No GCC de x86_64 isso vira
  chamada a `libatomic` (`__atomic_store_16`/`__atomic_load_16`), e uma **estática não
  pode adicionar `-latomic`** no link do plugin compartilhado — o `ld` reprova com
  `undefined reference`. O MSVC faz lock-free in-line e compila limpo, então só a CI de
  Linux pega. Publique medidores como atômicos POD separados (4/1 byte, sempre lock-free)
  e remonte o agregado na leitura. Foi assim que o CANIVETE consertou o link.
- **Antes de taggar**, confira localmente: `ci/test_resolve.ps1`, depois
  `installer/collect_payload.ps1 -BuildRoot build -Clean` e `ISCC.exe installer/forja.iss`.
  O `ISCC` não vem na máquina (`winget install --id JRSoftware.InnoSetup`) e é o único jeito
  de ver erro de `.iss` — um `Flags:` inexistente em `[Types]` derrubou a CI sem dizer por quê.
- **Trens de release**: a tag sem prefixo (`v0.5.2`) empacota a suíte inteira; uma tag
  prefixada (`vrack-1.0.0`, `vminis-`, `vcanivete-`, `vminidrum-`, `vminisat-`,
  `vdrumforge-`, `vairwindows-`) empacota **só aquele produto**. Build e testes correm
  inteiros no Windows e no Linux; só o empacotamento é filtrado (`collect_payload.ps1
  -OnlyPath`, lido de `ci/products.json` — o registro único dos produtos). A exceção é o
  **macOS**, que compila só os `pathFilters` e roda só os testes do `testRegex` do trem: o
  runner gratuito não fecha 36 plugins universais dentro do teto de 6 h do job (números da
  sonda em `docs/RELEASES.md`). Enquanto não houver shards, **tag de suíte para no build do
  macOS**. Se um trem novo precisar de macOS inteiro, fatiar é o trabalho, não subir o teto.
  Tag de produto **não** bumpa a versão da suíte. Passo a passo em `docs/RELEASES.md`.
- Site: `site/catalogo.json` é o conteúdo da página pública; `tools/build_site.ps1` gera o
  `site/index.html` commitado e `tools/validate_site.ps1` confere o catálogo contra os
  targets CMake, os `ForjaInfoText.h`, as capturas e o `ci/products.json` (roda na CI de
  Pages).
- **O `build_site.ps1` le as tags locais** para montar o link de Download de um produto, e o
  `-Checks` compara a pagina gerada com a commitada. Num clone raso (sem tags) o card sai com
  `/releases` em vez de `/releases/tag/vrack-<versao>` e a validacao reprova um `index.html`
  que esta certo — por isso `pages.yml` clona com `fetch-depth: 0`. Ao rodar o gerador, tenha
  `git fetch --tags` feito; o erro imprime a linha que diverge justamente para isso ser visível.
- Plugin novo exige o card no catálogo — a validação pega se você esquecer.

## Como adicionar um plugin

Siga `docs/NOVO-PLUGIN.md`. Resumo:

1. `tools/new_plugin.ps1` (Windows) ou `tools/new_plugin.py` gera a pasta em
   `Airwindows/<Família>/<Plugin>/` com processor/editor/FUNDAMENTOS/CMakeLists.
2. Vendorize o original e escreva o adapter.
3. Ajuste parâmetros, defaults e UI; escreva o README.
4. Adicione o caso de paridade e o adapter ao `alloc_guard`.
5. Rode build + testes + captura visual antes de considerar pronto.
6. Atualize `Forja — Catálogo de DSP e Mixer.md` e o README da suíte, e registre o card em
   `site/catalogo.json` (`tools/build_site.ps1` + `tools/validate_site.ps1`).

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
- DRUMFORGE compila como VST3; `forja_drumforge_engine` PASS (camadas,
  round-robin, choke, roubo de voz, multi-out, mute/solo, MIDI, latência 0, bypass,
  zero alocação, importação de pasta, biblioteca e preview de loops). As três abas
  (KITS, MIXER, LOOPS) estão implementadas; a validação em host segue pendente.
- FORJARACK compila como VST3; `forja_forjarack_engine` PASS (catálogo
  append-only, encadeamento das gavetas, bypass/solo por gaveta, seção master).
- MINIDRUM compila como VST3; `forja_minidrum` PASS (carga/adoção de kit,
  volume no trim do master, premix `.dfkit`, latência 0, bypass, medidor, troca com reap,
  rejeição de kit inválido, zero alocação e registro da gaveta instrument no ForjaRack).
- MINISAT compila como VST3; `forja_minisat` PASS (mapa drive→pad bit-exato
  com o adaptador do BRASA em seis drives, monotonia, crest caindo com o drive, rampa de
  bypass, latência 0 medida por impulso, zero alocação em estéreo e mono e registro da
  gaveta `minisat` no ForjaRack). O adaptador compartilhado `forja::BrasaDSP` entrou no
  `alloc_guard`.
- CANIVETE compila como VST3; o teste `canivete` PASS (carga de banco, presets, 16 canais
  GM, SF3 via stb_vorbis, zero alocação). A gaveta no ForjaRack também está verde:
  `forjarack_processor` e `forjarack_vst3_midi` provam que o banco assenta na gaveta, que a
  lista de instrumentos aparece e que a troca pelo seletor muda o preset em uso.
- **Carga assíncrona não é latência zero.** O `SoundFontEngine` leva o bank ao background e a
  gaveta só expõe instrumentos depois que a carga assenta — um `.sf3` real de 38 MB leva ~8 s
  nesta máquina. Teste que depende dessa carga tem que **esperar com orçamento alto e sair no
  primeiro sinal**, nunca assumir um tempo curto: com 5 s os dois casos do CANIVETE na gaveta
  falhavam contra um caminho que estava correto (e o pico do teste antigo, 0,40, era o tom
  injetado da fase anterior, não o soundfont — um falso positivo escondindo a mesma pressa).
  Arquivo do usuário no caminho é a outra metade da lição: se o teste precisa de um bank, ele
  mesmo gera o SF2 mínimo, como faz `Tests/Source/canivete.cpp`.
- Última verificação local (Windows, Release): `ctest` com **37/37** verdes.
  `tools/validate_site.ps1` PASS (36 plugins em 12 famílias).
- Site **no ar**: `https://pablito331.github.io/forja-suite/` (Pages com Source: GitHub
  Actions, deploy por `.github/workflows/pages.yml`). `tools/validate_site.ps1` PASS.
- Trem de produto: **`vrack-1.0.5` publicou** (primeiro Release de um trem, 4 assets).
  `vminis-`/`vdrumforge-`/`vpiano-1.0.3` estavam verdes em Windows/Linux mas tiveram os
  três jobs de macOS cancelados juntos — três tags no mesmo minuto lotaram o parque de
  runners. O `release.yml` agora serializa os trens (`concurrency: release-trains`, fila,
  sem cancelar): **um trem de cada vez**, e retaggar sempre do `main` atual (tags velhas
  não têm o bloco de concurrency). O gargalo deixou de ser o código: o job de
  macOS estourava o teto de 6 h do runner (medido: 37 min para o rack, ~8 min por plugin
  pequeno), e por isso hoje compila só o trem. Um tag de suíte `vX.Y.Z` continua parado no
  macOS até o job ser fatiado em shards — falta registrada em `docs/RELEASES.md`.
- Sem DAW no ambiente de desenvolvimento: automação/bypass de host seguem pendentes de
  validação manual (ver README).
- REPIQUE e ECO têm sync de BPM opcional; com `sync = false` o caminho é bit-exato.
