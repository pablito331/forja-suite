# MANUAL-GAVETA — Plugins de gaveta no ForjaRack (gaveta HOSPEDADO)

Este manual explica como colocar **qualquer plugin** dentro do ForjaRack sem
recompilar a suíte, e como criar seus próprios plugins pensado para rodar como
gaveta. Ele é a continuação natural da filosofia do rack: os 22 módulos
Airwindows continuam compilados e parity-testados, e a categoria **HOSPEDADO**
abre uma porta dinâmica para o resto do mundo — inclusive plugins de outras
licenças.

---

## 1. Como funciona (visão geral)

O ForjaRack tem duas famílias de módulos:

| Família | De onde vem | Licença | Estado |
|---|---|---|---|
| **Compilados** (CERA, MEL, LIXA, …) | adaptadores Airwindows MIT dentro da suíte | MIT (Chris Johnson) | paridade amostra a amostra validada |
| **HOSPEDADO** | arquivos `.vst3` (ou `.component` no macOS) soltos em `Documentos/Forja/Rack` | **a licença do próprio plugin** | carregado em tempo de execução |

O plugin hospedado NÃO é copiado, compilado ou linkado à suíte. Ele é
carregado em runtime pelo gerenciador de formatos do JUCE — exatamente como uma
DAW faz. Por isso:

- **Licenças fechadas, proprietárias ou comerciais funcionam.** A Forja não
  distribui esses plugins; cada pessoa usa os que já possui.
- O ForjaRack continua MIT. Hospedar não muda a licença de nada.
- Se o plugin sumir da pasta, a gaveta volta a ser pass-through.

### O caminho do sinal

```
host (DAW) ──> gavetas 0..7 em cadeia ──> host
                 │
                 ├─ módulo compilado: DSP Airwindows adaptado
                 └─ HOSPEDADO: bridge HostedModule -> plugin carregado
```

- **Efeito** (tem bus de entrada): processa o barramento estéreo in place, como
  qualquer gaveta da cadeia.
- **Instrumento** (não tem bus de entrada): recebe o **MIDI completo da faixa**
  (notas, CC, pitch bend, program) pelo caminho nativo e soma sua voz à cadeia.
  A detecção do papel é automática, pelo layout de buses do plugin.
- **Latência/PDC**: se o plugin reporta latência (compensação de delay), o
  ForjaRack propaga esse valor para o host — o maior valor entre as gavetas
  vivas vira o `setLatencySamples` do rack inteiro.

---

## 2. Usando: colocando um plugin na gaveta

1. Copie o `.vst3` (Windows/Linux) ou `.component` (macOS) para:

   ```
   Documentos/Forja/Rack/
   ```

   Subpastas também valem (a varredura é recursiva). Organize como quiser:

   ```
   Documentos/Forja/Rack/
     meus-favoritos/
       SuperChorus.vst3
       MiniDrum.vst3
     licenca-comercial/
       SomeEQ.vst3
   ```

2. Abra o ForjaRack na DAW, escolha **HOSPEDADO** no seletor da gaveta.
3. No menu **ARQUIVO** da gaveta, escolha o plugin. Se ele não aparecer,
   aperte **BUSCAR** para revarrer a pasta.
4. Os **knobs da gaveta** passam a ser os parâmetros do plugin — nomes e
   valores vêm do próprio plugin, atualizados ao vivo.
5. **EDITAR** abre a interface do plugin:
   - Plugin tem editor próprio? Abre **a UI nativa dele em janela separada**
     (o "abrir em aba separada" para UIs não-compatíveis). Enquanto essa janela
     estiver aberta, os knobs do rack não escrevem nos parâmetros — a UI do
     plugin manda, sem conflito. Feche a janela e o rack volta a comandar.
   - Plugin sem editor? Abre a **lista genérica de parâmetros** do JUCE, com
     tudo mapeado em 0–1.
6. A escolha do arquivo **grava no preset** do rack: ao recarregar o projeto, a
   gaveta volta com o mesmo plugin.

### Limites conhecidos (v1)

- Máximo de knobs exibidos na gaveta: os 8 primeiros parâmetros do plugin
  (os demais continuam acessíveis pelo EDITAR).
- O editor nativo vive em janela separada (não "dentro" da gaveta) — é
  exatamente assim que ele se comporta em qualquer DAW.
- Formatos v1: VST3 em todas as plataformas; Audio Unit (`.component`) no
  macOS. VST2 não é escaneado.
- A varredura usa o mecanismo do JUCE: em DAWs que já indexaram o plugin, nada
  muda; plugins problemáticos ficam de fora e o resto do rack segue.

---

## 3. Criando um plugin para virar gaveta

Você **não precisa** usar o SDK da Forja, nem o código da suíte, nem pedir
autorização. Qualquer plugin VST3 padrão já funciona como gaveta, com knobs
dinâmicos + UI genérica ou nativa. Para uma gaveta "de primeira", siga este
guia.

### 3.1 Requisitos mínimos (já bastam)

- Build VST3 (e/ou AU no macOS) — JUCE, VSTSDK, Waveform, ou qualquer
  framework que produza um `.vst3` válido.
- Parâmetros normalizados em 0–1 (é o padrão VST3; qualquer host espera isso).
- Latência real declarada via API do formato (opcional, mas recomendável se o
  DSP atrasa o sinal — o rack propaga para o host).
- Instrumentos: sem bus de entrada e lendo `processBlock` com o `MidiBuffer`
  (padrão de qualquer synth).

Coloque o `.vst3` em `Documentos/Forja/Rack`, teste no rack, distribua como
quiser (com a sua licença).

### 3.2 Boas práticas de design de som para gaveta

- **Primeiros 8 parâmetros = os knobs da frente.** O rack expõe os 8 primeiros
  na face da gaveta. Declare os controles mais musicais primeiro na lista de
  parâmetros; os técnicos (calibração, oversampling, A/B) ficam depois e só
  aparecem no EDITAR.
- **Nomes curtos e sem redundância.** O rótulo do parâmetro vira o texto sob o
  knob (`"Tone"` e não `"SuperDelayToneControl"`).
- **Um parâmetro "mix" ou blend** (seco/parallel) faz a gaveta funcionar bem em
  cadeia — regra que vale para toda a família Space do rack.
- **Latência honesta.** Reporte a latência que existe; o rack cuida do PDC com o
  host. Não declare latência de delay musical "compensada" — ela é audível por
  design (é assim no REPIQUE e no ECO também).
- **Sem estado global.** O rack pode criar várias instâncias (8 gavetas podem
  hospedar o mesmo plugin 8 vezes). Estado por instância sempre.

### 3.3 A estratégia "duas UIs"

Um plugin seu pode ter duas interfaces, e o rack usa a que fizer sentido:

1. **UI própria (nativa)** — a que abre no EDITAR, em janela separada. É a UI
   completa do plugin, idêntica ao comportamento em qualquer DAW.
2. **Face de gaveta** — os knobs que o rack desenha na própria gaveta. Você não
   escreve código de UI para isso: basta ter bons parâmetros (itens 3.1/3.2) e o
   ForjaRack monta a face automaticamente: nome do knob, valor atual, medidor de
   saída, bypass/solo da gaveta, tudo no padrão visual Forja.

Ou seja: **nenhum trabalho extra de UI para virar gaveta**. Se você quiser uma
face desenhada sob medida (arte própria, layout fixo estilo Forja), isso é um
módulo compilado na suíte — abra uma issue/PR no repositório e a gente adapta
com os testes de paridade que a casa exige (o caminho dos Mini).

### 3.4 Exemplo: fluxo de um criador de plugin

```
1. Desenvolva seu VST3 em qualquer framework.
2. Teste em uma DAW comum (Reaper, Host, Bitwig...).
3. Copie o .vst3 para Documentos/Forja/Rack/
4. Abra o ForjaRack: gaveta HOSPEDADO -> ARQUIVO -> seu plugin.
5. Ajuste a ordem dos parâmetros para que os 8 primeiros sejam a "frente".
6. Publique com a sua licença. O rack não distribui seu binário.
```

### 3.5 Testando sem DAW

Para validar o comportamento DSP do seu plugin antes de jogá-lo no rack, os
hosts gratuitos de plugin (vchost64 no Windows, PluginOperator, DAWTEST) mostram
se a latência reportada está correta e se os parâmetros respondem em 0–1 — os
dois pontos que o bridge do rack mais usa.

---

## 4. Reutilizando plugins da própria suíte

Qualquer plugin da suíte (BRASA, CINZÉL, MiniDrum, CANIVETE…) também pode ser
carregado como gaveta HOSPEDADA — os binários VST3 compilados funcionam igual a
um plugin de terceiros. Isso é útil para:

- experimentar um DSP novo dentro de um rack de outro projeto sem recompilar;
- ter duas instâncias de um plugin com presets diferentes na mesma cadeia;
- rodar um plugin da suíte em uma DAW que só conhece "ForjaRack" (um plug-in,
  vários caminhos).

**Não faça isso com os módulos compilados da mesma família na mesma gaveta**: o
seletor já oferece o adaptador nativo com paridade validada; o caminho
compilado é sempre o preferido (menos overhead, mesma licença MIT).

---

## 5. Internais (para quem quer entender o bridge)

- `Forjarack/Source/rack/HostedModule.h/.cpp` — `ModuleBase` sobre um
  `juce::AudioPluginInstance`: ponte de buffers, MIDI nativo (`void*` no core
  JUCE-free), leitura de params/nomes/valores, latência, trava de params com a
  UI nativa aberta, `createNativeEditor` com fallback na UI genérica.
- `Forjarack/Source/rack/HostedLibrary.h/.cpp` — pasta, varredura por formato e
  carga pelo `AudioPluginFormatManager` (VST3; AU no macOS).
- `RackEngine::adoptSlotModule` — encaixa um módulo pré-construído no slot (o
  catálogo não fabrica o HOSPEDADO: a fábrica da ficha é nula de propósito).
- A escolha do arquivo é persistida como propriedade do estado APVTS
  (`hosted_slot<N>`), porque `AudioParameterChoice` congela opções na
  construção e a lista de plugins é dinâmica.
- Testes: `Tests/Source/forjarack_hosted.cpp` (target `forja_forjarack_hosted`)
  cobre adoção efeito/instrumento, mapa de params, trava da UI nativa, MIDI
  nativo e propagação de latência com um stub de `AudioPluginInstance`.
- Regras que o hosting **não fura**: sem alocação no caminho de áudio, bypass
  com rampa de 10 ms, estado do rack não é resetado no bypass, e o core do rack
  continua compilável sem JUCE processors.

---

## 6. Troubleshooting

| Sintoma | Causa provável | Ação |
|---|---|---|
| Plugin não aparece no ARQUIVO | extensão errada (`.dll` solta não é VST3) ou pasta errada | confirme que é um `.vst3` (pasta, não arquivo) em `Documentos/Forja/Rack`; aperte BUSCAR |
| Gaveta muda para HOSPEDADO mas sem knobs | plugin com 0 parâmetros ou carregamento falhou | abra em outra gaveta; veja se o host de teste o carrega; CHECK o badge/tooltip |
| EDITAR abre janela cinza minúscula | plugin com editor dependente de OpenGL/Vulkan exótico | use os knobs da gaveta; registre o caso — UI genérica continua funcionando |
| Latência somando com outra gaveta | comportamento esperado: o rack reporta o máximo | é o PDC correto em cadeia série |
| Parâmetros "brigar" com a UI do plugin | só acontece com o editor nativo aberto, e o lock trata | feche a janela do editor para o rack voltar a mandar |
| Gaveta de instrumento muda, bank carregado, mas nada soa ao tocar o teclado | o **wrapper VST3** não anunciava a entrada de MIDI (o plugin foi declarado como efeito puro: `NEEDS_MIDI_INPUT FALSE` → macro `JucePlugin_WantsMidiInput=0` → sem barramento `kEvent` e sem a conversão `data.inputEvents` → `MidiBuffer`; o `acceptsMidi()` do C++ não vale para VST3), ou o MIDI não está entrando na faixa | use o binário atual (o rack agora declara `FORJA_WANTS_MIDI_INPUT` e aparece nas duas listas, FX e instrumento); toque olhando o **led MIDI** do MASTER: aceso = o MIDI chega (o problema é de som/roteamento da faixa), apagado = o teclado não chega à faixa. A rede contra esse defeito é o teste `forjarack_vst3_midi`, que carrega o `.vst3` real pelo formato VST3 |

---

Consulte também `docs/ARQUITETURA.md` (estrutura do rack) e
`docs/NOVO-PLUGIN.md` (criar um módulo compilado com paridade).
