# Como criar um novo plugin Forja

Este é o passo a passo completo. O resumo para agentes está em `AGENTS.md`; a visão de
arquitetura está em `docs/ARQUITETURA.md`.

## 0. Antes de começar

- O modelo existe na Airwindows e é **MIT**? Se não, não entra.
- Confira o commit fixado do vendor: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`.
- Escolha família dentro de `Airwindows/` (`Saturation`, `Eq`, `Dynamics`, `Strips`,
  `Tape`, `Space`, `Lofi`, `OneKnob`, `Amps` ou uma pasta nova), nome do produto, código
  VST3 de 4 caracteres e slug.
- Regra: nenhum controle inventado. Só entram parâmetros do original ou camadas Forja
  aprovadas e documentadas.

## 1. Gerar o scaffold

Windows (PowerShell 5.1+, sem dependências):

```powershell
powershell -ExecutionPolicy Bypass -File tools/new_plugin.ps1 `
  -Family OneKnob -Product Areia -Code Area -Slug areia -Model Areia `
  -Airwindows PurestAir -FamilyLabel "DE MIX" -Subtitle "AMACIADOR" `
  -Status "Amaciador de picos · DSP Airwindows PurestAir" `
  -Footer "DSP Airwindows PurestAir · MIT · latência 0 amostras" `
  -Categories Fx -Company "Forja"
```

macOS/Linux (Python 3.9+):

```bash
python3 tools/new_plugin.py \
  --family OneKnob \
  --product Areia \
  --code Area \
  --slug areia \
  --model Areia \
  --airwindows PurestAir \
  --family-label "DE MIX" \
  --subtitle "AMACIADOR" \
  --status "Amaciador de picos · DSP Airwindows PurestAir" \
  --footer "DSP Airwindows PurestAir · MIT · latência 0 amostras" \
  --categories Fx \
  --company "Forja"
```

Ambos aceitam `--dry-run` / `-DryRun` para listar os arquivos sem escrever.

O gerador cria:

- `Airwindows/OneKnob/Areia/` com `CMakeLists.txt`, `Source/PluginProcessor.*`,
  `Source/PluginEditor.*`, `Source/ForjaInfoText.h` e `README.md`;
- `Forja/Source/forja/dsp/ForjaAreia.h` (adapter de passagem, com TODO);
- a linha `add_subdirectory(Areia)` em `Airwindows/OneKnob/CMakeLists.txt`;
- `add_subdirectory(OneKnob)` em `Airwindows/CMakeLists.txt`, se a família ainda não
  estiver listada.

O código gerado **compila**, mas o DSP é pass-through até o passo 3. Não publique assim.

`--company` é o `COMPANY_NAME` do VST3 e aparece na DAW junto do produto. Use ASCII para
evitar problemas de decodificação no host: `"Forja - Mix"`. Acentos ficam na UI, via
`utf8()`.

## 2. Vendorizar o original

```powershell
$vendor = "Forja/Source/forja/vendor/airwindows/Areia"
New-Item -ItemType Directory -Force $vendor
Copy-Item <airwindows-src>/Areia.cpp, <airwindows-src>/Areia.h, `
          <airwindows-src>/AreiaProc.cpp, <airwindows-src>/LICENSE $vendor
Get-FileHash $vendor/*.cpp, $vendor/*.h -Algorithm SHA256
```

Crie `PROVENANCE.md` no formato dos existentes:

```markdown
# Areia — provenance

- Original: Airwindows Areia, Copyright (c) 20XX Chris Johnson.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`.
- Downloaded: AAAA-MM-DD.
- SHA-256:
  - `Areia.cpp`: ...
  - `Areia.h`: ...
  - `AreiaProc.cpp`: ...

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaAreia.h`.

Deviations in the Forja adapter:

- ...
```

O `LICENSE` pode ser copiado de `Forja/Source/forja/vendor/airwindows/LICENSE` se for o
mesmo texto MIT.

## 3. Escrever o adapter

Arquivo: `Forja/Source/forja/dsp/Forja<Modelo>.h`, header-only, namespace
`forja::dsp::<slug>`. Use os adapters existentes como referência de estrutura
(`ForjaLixa.h` para one-knob, `ForjaBigorna.h` para strip, `ForjaSalao.h` para estado
grande).

Checklist:

- [ ] Transcrever `processDoubleReplacing` **literalmente** para `processSamples<Sample>`.
- [ ] `static` → membros por instância (buffers, contadores, filtros, RNG).
- [ ] RNG com semente `22222u` e avanço idêntico ao original; sem estado compartilhado.
- [ ] Buffers dimensionados para 192 kHz com folga; nada de `new` em `process`.
- [ ] `prepare()` chama `reset()`; `reset()` zera todo o estado.
- [ ] Bypass suavizado de 10 ms com dry atrasado pela latência do modelo.
- [ ] Medidores de pico/CLIP publicados em `readMeters`.
- [ ] `latencySamples()` devolve a latência **medida**.
- [ ] Ordem L/R e canais iguais ao original; mono usa o buffer scratch do processor.
- [ ] Parâmetros normalizados 0–1; escalonamento dentro do adapter.
- [ ] Comentário de cabeçalho com modelo, commit e desvios.

Se o modelo tiver delay/reverb, a latência de PDC é 0 (o atraso é musical); documente.

## 4. Processor

No `PluginProcessor.cpp` gerado:

- Ajuste o `createParameterLayout()` com **todos** os parâmetros do original, na ordem
  original, passo 0,01, mais `bypass` (bool, padrão `false`).
- Preencha `Parameters` em `processBlock` a partir do APVTS.
- Se o plugin tiver sync de tempo, leia o BPM do playhead por bloco e caia no controle
  manual quando não houver host — veja `Airwindows/Space/Repique/Source/PluginProcessor.cpp`.
- `setLatencySamples (dsp.latencySamples())` em `prepareToPlay`.
- Não aloque em `processBlock` (mono scratch já é alocado em `prepareToPlay`).

## 5. Editor

- O editor gerado usa `ForjaEditorBase` compacto (560×260). Para layout completo, passe
  `false` no último argumento do construtor e use o grid de 1000×540:
  - topo `{7,7,986,49}`; cards `y = 63`, `h = 388`; status `{7,458,986,75}`.
  - 3 cards com curva: larguras 386/292/292 (gap 8).
  - 4 cards: 190/236/294/236.
  - compacto: topo `{7,7,546,44}`, card `{7,59,546,148}`, status `{7,215,546,38}`.
- Knobs: `ForjaKnob { rótulo, dica, diâmetro, min, max, passo, padrão, formatador,
  Options { minWidth, showHint, showValue, showLabel } }`.
- `ForjaKnob` com `minWidth = 138` em colunas duplas; 80 para três colunas.
- Textos acentuados **sempre** por `forja::ui::utf8("...")` ou `\u00XX`.
- Nomes em português vêm do papel musical; o rótulo original fica no FUNDAMENTOS.
- Layout é em coordenadas fixas: a escala/aspecto é aplicada por `ForjaCanvas.h`. Não use
  `getWidth()`/`getHeight()` do editor para posicionar nada.
- Arquivos de UI novos entram em `Forja/CMakeLists.txt`; os do plugin, no CMakeLists dele.

## 6. Paridade

Crie `Tests/Source/parity_<slug>.cpp` no molde de `parity_lixa.cpp`:

```cpp
#include "parity_common.h"
#include "Areia.h"
#include "forja/dsp/ForjaAreia.h"

bool runAreiaParity()
{
    Areia original (nullptr);
    forja::dsp::AreiaDSP adapter;

    const auto setParameters = [] (Areia& orig, forja::dsp::AreiaDSP& dsp,
                                   const std::vector<float>& p)
    {
        orig.setParameter (0, p[0]);

        forja::dsp::AreiaDSP::Parameters params;
        params.amount = p[0];
        params.bypass = false;
        dsp.setParameters (params);
    };

    parity::Case c;
    c.signal = parity::Signal::sine;
    c.levelDb = -12.0;
    c.frequency = 1000.0;
    c.sampleRate = 48000.0;
    c.params = { 0.5f };

    const auto result = parity::runCase (original, adapter, c, setParameters);
    return result.ok;
}
```

Depois, em `Tests/CMakeLists.txt`:

1. `set(AREIA_SOURCES ${FORJA_VENDOR}/Areia/Areia.cpp ${FORJA_VENDOR}/Areia/AreiaProc.cpp)`
2. `forja_add_parity_model(areia Areia Areia ${AREIA_SOURCES})`
3. Adicione o adapter ao `Tests/Source/alloc_guard.cpp` (mesmo padrão dos outros).
4. Adicione o vendor ao include path do executável (feito pela função).

Cenários mínimos: seno −18/0 dBFS, 60 Hz, ruído estéreo, quadrada 0 dBFS, DC ±1, silêncio,
impulso, degrau, rajada, varredura de cada parâmetro em 0/0,25/0,5/0,75/1, SR 44,1/48/88,2/
96/192 kHz, mono e estéreo, 5 s por caso. Modelos com estado **exigem** original e adapter
novos a cada caso (como em `parity_repique.cpp`) para comparar estados equivalentes.

Critérios: sem NaN/Inf, erro máx ≤ 1e-9 e RMS ≤ −140 dBFS (ou ≤ 1e-7 documentado), zero
alocação, silêncio → ~0, latência de impulso igual à tabela.

## 7. Documentação

- `README.md` do plugin: papel, tabela de parâmetros, DSP, latência medida, desvios,
  validação e licença.
- `ForjaInfoText.h`: FUNDAMENTOS completo (o que faz, como funciona, operação, controles,
  proveniência), em português, com acentos via `utf8`.
- `Forja — Catálogo de DSP e Mixer.md`: nova linha `DSP-0xx` e apontamentos de
  substitutos (`DSP-006`, `DSP-009`, …) quando for o caso.
- `README.md` da suíte: família, modelo, número de controles e latência.
- `MIX-009`: amplie o léxico se o plugin for de mixagem.

## 8. Verificação

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

- Abra o Standalone: layout, knobs, medidores, bypass, CLIP, status, FUNDAMENTOS (abrir,
  rolar até o fim, fechar por botão/Esc/clique fora).
- Redimensione a janela e confira que o layout escala sem cortes.
- Confira a latência medida contra `setLatencySamples`.
- Atualize `Documents/prototipos` só depois do build final.

## Dicas específicas

- **Estado grande** (delays/reverbs): zere em `reset`, dimensione com folga, use factory
  por caso na paridade.
- **Não linear** (TORNO, saturação): não desenhe curva de EQ; o FUNDAMENTOS explica.
- **Sync**: use `ForjaSync.h` e o modelo de REPIQUE/ECO; com `sync = false` o resultado
  deve continuar bit-exato.
- **Curva de EQ**: use `ForjaEqCurve` (FFT da resposta do adapter) e nós arrastáveis com
  callback; nunca duplique a matemática do filtro.
- **One-knob**: o controle único precisa ser musical nos extremos; sem makeup inventado.

## Como adicionar uma base não-Airwindows

Uma **base** é uma pasta irmã na raiz que agrupa produtos com a mesma origem de DSP. Hoje
existem duas: `Airwindows/` (adaptações MIT, com paridade contra o original) e
`drumforge/` (DSP original Forja, em concepção). `Forja/` continua sendo a biblioteca
compartilhada de UI e adaptadores para as duas.

Passo a passo:

1. Crie `Foo/CMakeLists.txt` listando as famílias da base, exatamente como
   `Airwindows/CMakeLists.txt` faz.
2. No `CMakeLists.txt` da raiz, adicione `add_subdirectory(Foo)` **depois** de
   `add_subdirectory(Airwindows)` e **antes** de `add_subdirectory(Tests)`.
3. Cada plugin continua sendo um target criado por
   `forja_add_plugin(target, product, code, bundle, source_dir, categories, company)`,
   com `source_dir` apontando para `Foo/<Família>/<Plugin>`.
4. Reuse `forja_ui`, `ForjaEditorBase`, `ForjaCanvas` e os componentes do tema. A UI é
   independente da origem do DSP.
5. **Testes**: a regra de paridade vale só para `Airwindows/`. DSP original é validado por
   testes funcionais (comportamento esperado, estado, round-trip) e pela guarda de
   alocação — sem vendor nem `parity_<slug>.cpp`.
6. Documente a base na arquitetura e no catálogo, deixando claro que ela não pertence ao
   catálogo `DSP-0xx` de adaptações.

O gerador `tools/new_plugin.ps1`/`.py` é específico de Airwindows (vendor, adapter e
paridade). Uma base nova escreve o próprio scaffold, ou começa copiando os templates e
removendo a etapa de vendor.

**Instrumentos e multi-out:** `forja_add_plugin` hoje fixa `IS_SYNTH FALSE`,
`NEEDS_MIDI_INPUT FALSE` e `NEEDS_MIDI_OUTPUT FALSE`, e não expõe barramentos múltiplos.
Uma base de instrumento (como o Drumforge) precisa dessas opções no helper antes do
primeiro target.

O Drumforge é o exemplo vivo: o conceito em `drumforge/ideia para o plugin.md` é amplo,
mas o protótipo de UI (`drumforge/Drum Forge_ protótipo de UI.html`) já o simplificou
para três abas — Kits, Mixer e Loops MIDI — com TRIM/PAN, HPF/LO/MID/HI, THR/RATIO, M/S,
fader/medidor, limiter no master e grade de 16 passos com BPM/sync/drag. Sends, racks de
FX, Settings e editor de mapas ficam para depois.

## Backlog conhecido (não fazer sem aprovação)

Consoles (ConsoleX3), filtros XYZ, VerbSevens, ADClip9 e a família Bass completa.
