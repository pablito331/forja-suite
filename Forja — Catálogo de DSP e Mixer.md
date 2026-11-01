# Forja — Catálogo de DSP e Mixer

Versão 0.3 · Camada de áudio do ecossistema Forja
Complementa [Padrão de UI — Forja](./Padrão%20de%20UI%20—%20Forja.md): aquele garante que os plugins
**se pareçam** iguais; este garante que os blocos comuns **soem e se comportem** iguais.

---

## 1. Objetivo

Nenhum bloco de DSP ou de mesa nasce dentro de um plugin. Todo bloco nasce aqui, com ID, contrato,
proveniência e estado declarado, e é consumido por Drumforge, PianoForge e o que vier depois.

Consequência: um bug de compressor corrigido uma vez vale para todos; uma nova saturação entra
para todos; e o operador que aprende a BIGORNA no Drumforge já sabe operar o strip no PianoForge.

---

## 2. Regra de camadas

A dependência aponta **sempre para dentro**. Se um bloco do catálogo precisa incluir header de
plugin, ele está na camada errada.

```
┌─ <plugin> ───────────────────────────────────────────────┐
│  APVTS, engine, editor, persistência, formato            │
│                                                          │
│   ┌─ forja::mixer ──────┐   ┌─ forja::ui ────────────┐   │
│   │  strip, cadeia,     │   │  ForjaLookAndFeel      │   │
│   │  bus, grupos        │   │  tokens, fontes        │   │
│   │                     │   │                        │   │
│   │   ┌─ forja::dsp ──┐ │   │   (não conhece áudio)  │   │
│   │   │  sem estado   │ │   └────────────────────────┘   │
│   │   │  de plugin    │ │                                │
│   │   └───────────────┘ │                                │
│   └─────────────────────┘                                │
└──────────────────────────────────────────────────────────┘
```

| Camada | Pode depender de | Nunca pode |
|---|---|---|
| `forja::dsp` | C++ padrão (e JUCE só se for inevitável, documentado) | JUCE editor, APVTS, qualquer plugin |
| `forja::mixer` | `forja::dsp` | APVTS, qualquer plugin |
| `forja::ui` | JUCE GUI | áudio, buffers, parâmetros |
| `<plugin>` | as três | — |

**Regra de ouro:** nenhum bloco de `forja::dsp` aloca memória, faz log ou toca em `this` de plugin
dentro de `process()`.

---

## 3. Layout de arquivos

```
suite de plugins/
├── CMakeLists.txt                  FetchContent do JUCE + bases + Tests
├── cmake/ForjaSuite.cmake          helper forja_add_plugin
├── Airwindows/                     base 1: adaptações MIT da Airwindows (31 plugins)
│   ├── CMakeLists.txt
│   ├── Dynamics/  Saturation/  Eq/  Strips/  Tape/  Space/  Lofi/  OneKnob/  Amps/
│   │   └── <Plugin>/
│   │       ├── CMakeLists.txt
│   │       ├── README.md
│   │       └── Source/
│   │           └── PluginProcessor.* · PluginEditor.* · ForjaInfoText.h
├── drumforge/                      base 2: sampler de bateria, DSP Forja
│   ├── ideia para o plugin.md
│   ├── Drum Forge_ protótipo de UI.html
│   └── Drumforge/
│       ├── CMakeLists.txt          forja_add_instrument(Drumforge …)
│       ├── README.md
│       └── Source/
│           ├── PluginProcessor.* · PluginEditor.* · ForjaInfoText.h
│           ├── engine/             Kit, SampleStore, VoicePool, DrumEngine,
│           │                       ChannelStrip, Compressor, Filters, MasterBus
│           ├── loops/              LoopLibrary (leitura/varredura) e LoopPlayer (preview)
│           └── ui/                 KitPanel, MixerPanel, LoopsPanel, MiniKnob
├── Forja/
│   ├── CMakeLists.txt              add_library(forja_ui STATIC …)
│   └── Source/forja/
│       ├── ui/                     tema, card, knob, LookAndFeel, medidores, topo,
│       │                           status, GR + histórico, curva de EQ, painel Fundamentos
│       ├── dsp/                    adaptadores Forja<Modelo>.h (header-only)
│       └── vendor/                 código de terceiros, verbatim
│           └── airwindows/
│               ├── LICENSE
│               ├── README.md       URL, commit, data, o que adaptamos
│               └── ButterComp2/ · Pressure5/ · Pop2/ · Parametric/ ·
│                   Baxandall3/ · SmoothEQ2/ · DeBess/ · CStrip2/ · X2Buss/ ·
│                   ToTape9/ · IronOxide5/ · TapeDelay2/ · Echojam/ · kRockstar2/ ·
│                   kStation/ · DeRez5/ · PurestDrive/ · Sweeten/ · PurestWarm3/ ·
│                   Mojo/ · Dyno/ · Slew3/ · Cabs2/ · PointyGuitar/ ·
│                   ChimeyGuitar2/ · CrickBass/ · Wolfbot/ · ChimeyDeluxe/ ·
│                   ADClip9/ · FinalClip/
└── Tests/
    ├── Source/parity_<modelo>.cpp · parity_common.h · scenarios.h · alloc_guard.cpp
    └── third_party/vst2_shim/
```

O `Tube2` usado pelo BRASA é a exceção: fica no vendor privado do plugin, em
`Airwindows/Saturation/Brasa/Source/forja/vendor/airwindows/Tube2/`.

**Regra do `vendor/`:** nada lá dentro é editado. O adaptador que liga o algoritmo ao contrato
`forja::dsp` mora em `dsp/`, nunca no arquivo original. MIT e BSD entram assim; GPL não entra
em `vendor/` (contamina o catálogo inteiro).

---

## 4. Catálogo — DSP

IDs estáveis. Um bloco não muda de ID depois de publicado, mesmo que a implementação mude.

| ID | Bloco | O que faz | Implementação | Origem / licença | Estado | Usado em |
|---|---|---|---|---|---|---|
| `DSP-001` | Input gain | trim −24…+24 dB, rampa de 10 ms | própria | Forja | planejado | Drum, Piano |
| `DSP-002` | Pan | pan equal-power, −3 dB no centro | própria | Forja | existe no engine | Drum |
| `DSP-003` | Phase | inversão de polaridade | própria | Forja | planejado | Drum |
| `DSP-004` | HPF | 12/18 dB-oct, 20–400 Hz variável | própria | Forja (linhagem Ciclotron VEGA) | parcial (`hp{n}`, reso) | Drum, Piano |
| `DSP-005` | LPF | 12 dB-oct, reso | `CResoFilter` | Drumlabooh (Peter Semiletov) | existe | Drum |
| `DSP-006` | EQ 3 bandas | shelf 12 kHz · peaking sweep · shelf 80 Hz, ±15 dB, EQ IN/OUT | própria | Forja (freq. Ciclotron) | parcial: insert coberto por CINZÉL (`DSP-018`); o EQ do mixer segue no contrato | Drum, Piano |
| `DSP-007` | Comp de cola | compressor de bus, quatro compressores paralelos | Airwindows **ButterComp2** (`forja::dsp::Tempera`) | MIT · Chris Johnson | parcial: DSP, paridade e UI prontos; host pendente | Drum, Piano |
| `DSP-008` | Console sat | saturação de console | Airwindows **Console5** | MIT · Chris Johnson | planejado | Drum |
| `DSP-009` | Tape sat | saturação de fita | Airwindows **ToTape9** (LAMINADOR, `DSP-024`) e **IronOxide5** (ÓXIDO, `DSP-025`) | MIT · Chris Johnson | parcial: DSP, paridade e UI prontos; host pendente | Drum, Piano |
| `DSP-010` | Room | reverb de ambiente curto (send) | Airwindows **kStation** (FÔLEGO, `DSP-029`) e **kRockstar2** (SALÃO, `DSP-028`) | MIT · Chris Johnson | parcial: DSP, paridade e UI prontos; host pendente | Drum |
| `DSP-011` | Delay | delay sincronizado ao BPM | Airwindows **TapeDelay2** (REPIQUE, `DSP-026`) e **Echojam** (ECO, `DSP-027`) | MIT · Chris Johnson | parcial: DSP e UI prontos; sincronia com BPM pendente | Drum |
| `DSP-012` | Meter | peak / RMS / VU, hold, latch de clip; GR em dB e histórico de 6 s | própria (`forja::ui`) | Forja | parcial: peak, GR, histórico e curva de EQ por FFT prontos; telemetria de host pendente | todos |
| `DSP-013` | Resampler | sinc, offline | `resampler.c` | David Bryant · BSD-3 | existe | Drum |
| `DSP-014` | Guard | denormal, smoothing, rampas anti-zip | própria (`forja::dsp`: `ForjaSmoother`, `ForjaLane`) | Forja | parcial: bypass 10 ms, alinhamento de latência e suavizadores nos adaptadores; host pendente | todos |
| `DSP-015` | Valve sat | saturação de válvula assimétrica + trim de saída | Airwindows **Tube2** (port em `forja::BrasaDSP`) | MIT · Chris Johnson | parcial: DSP, UI e FUNDAMENTOS prontos; host pendente | Drum, Piano, todos |
| `DSP-016` | Comp vari-mu | densidade e caráter vari-mu, detector estéreo linkado | Airwindows **Pressure5** (`forja::dsp::Prensa`) | MIT · Chris Johnson | parcial: DSP, paridade (39/39) e UI prontos; host pendente | Drum, Piano |
| `DSP-017` | Comp de impacto | ataque e release separados, envelopes L/R independentes | Airwindows **Pop2** (`forja::dsp::Malho`) | MIT · Chris Johnson | parcial: DSP, paridade (35/35) e UI prontos; host pendente | Drum, Piano |
| `DSP-018` | EQ paramétrico dinâmico | 10 bandas do Parametric + camada dinâmica Forja por banda | Airwindows **Parametric** + Forja (`forja::dsp::Cinzel`) | MIT · Chris Johnson / Forja | parcial: DSP, paridade 66/66 com `dyn = 0`, UI, curva FFT e nós arrastáveis; host pendente | CINZÉL |
| `DSP-019` | EQ de tom | shelves Bessel entrelaçados, calibrado em 0,5 | Airwindows **Baxandall3** (`forja::dsp::Torno`) | MIT · Chris Johnson | parcial: DSP, paridade 30/30 e UI; host pendente | TORNO |
| `DSP-020` | EQ suave de bus | quatro bandas largas, sem saturação | Airwindows **SmoothEQ2** (`forja::dsp::Lima`) | MIT · Chris Johnson | parcial: DSP, paridade 55/55 e UI com curva/nós; host pendente | LIMA |
| `DSP-021` | De-esser | detector de agudos com filtro dinâmico | Airwindows **DeBess** (`forja::dsp::Escoria`) | MIT · Chris Johnson | parcial: DSP, paridade 44/44 e UI; host pendente | ESCÓRIA |
| `DSP-022` | Channel strip | EQ 3 bandas + HP/LP + compressor + saída | Airwindows **CStrip2** (`forja::dsp::Bigorna`) | MIT · Chris Johnson | parcial: DSP, paridade 69/69 e UI; host pendente | BIGORNA |
| `DSP-023` | Strip de bus | 4 bandas + 4 frequências + threshold + fader | Airwindows **X2Buss** (`forja::dsp::Cadinho`) | MIT · Chris Johnson | parcial: DSP, paridade 70/70, latência 1; host pendente | CADINHO |
| `DSP-024` | Fita 2 pistas | head bump, flutter, bias, shape/tilt, ClipOnly3 | Airwindows **ToTape9** (`forja::dsp::Laminador`) | MIT · Chris Johnson | parcial: DSP, paridade 65/65, latência 1; host pendente | LAMINADOR |
| `DSP-025` | Fita cassete | saturação de cassete, ruído, dry/wet invertido | Airwindows **IronOxide5** (`forja::dsp::Oxido`) | MIT · Chris Johnson | parcial: DSP, paridade 55/55; host pendente | ÓXIDO |
| `DSP-026` | Delay de fita | regen ressonante, filtro na realimentação, flutter | Airwindows **TapeDelay2** (`forja::dsp::Repique`) | MIT · Chris Johnson | parcial: DSP, paridade 50/50; atraso musical sem PDC; host pendente | REPIQUE |
| `DSP-027` | Delay multi-espaço | 17 espaços discretos (passo 1/16), regen e speed | Airwindows **Echojam** (`forja::dsp::Eco`) | MIT · Chris Johnson | parcial: DSP, paridade 52/52 com as 17 posições; host pendente | ECO |
| `DSP-028` | Reverb grande | regen, posição, downrez e distância | Airwindows **kRockstar2** (`forja::dsp::Salao`) | MIT · Chris Johnson | parcial: DSP, paridade 44/44; host pendente | SALÃO |
| `DSP-029` | Reverb de ambiente | early reflections, filtro e posição | Airwindows **kStation** (`forja::dsp::Folego`) | MIT · Chris Johnson | parcial: DSP, paridade 49/49; host pendente | FÔLEGO |
| `DSP-030` | Bitcrusher | redução de taxa/palavra com undersampling em Bèzier | Airwindows **DeRez5** (`forja::dsp::Granalha`) | MIT · Chris Johnson | parcial: DSP, paridade 46/46 e UI; host pendente | GRANALHA |
| `DSP-031` | Drive limpo | saturação suave de um knob | Airwindows **PurestDrive** (`forja::dsp::Lixa`) | MIT · Chris Johnson | parcial: DSP, paridade 21/21 e UI; host pendente | LIXA |
| `DSP-032` | Brilho | abertura de agudos de um knob | Airwindows **Sweeten** (`forja::dsp::Mel`) | MIT · Chris Johnson | parcial: DSP, paridade 21/21 e UI; host pendente | MEL |
| `DSP-033` | Calor | aquecimento de um knob | Airwindows **PurestWarm3** (`forja::dsp::Cera`) | MIT · Chris Johnson | parcial: DSP, paridade 21/21 e UI; host pendente | CERA |
| `DSP-034` | Corpo | corpo/graves de um knob | Airwindows **Mojo** (`forja::dsp::Corpo`) | MIT · Chris Johnson | parcial: DSP, paridade 21/21 e UI; host pendente | CORPO |
| `DSP-035` | Compressão rápida | compressão de um knob | Airwindows **Dyno** (`forja::dsp::Impeto`) | MIT · Chris Johnson | parcial: DSP, paridade 21/21 e UI; host pendente | ÍMPETO |
| `DSP-036` | Slew | suavização de transientes de um knob | Airwindows **Slew3** (`forja::dsp::Rebarba`) | MIT · Chris Johnson | parcial: DSP, paridade 21/21 e UI; host pendente | REBARBA |
| `DSP-037` | Gabinete | gabinete faseado, 21 polos HP + 13 polos LP | Airwindows **Cabs2** (`forja::dsp::Gabinete`) | MIT · Chris Johnson | parcial: DSP, paridade 26/26 e UI; host pendente | GABINETE |
| `DSP-038` | Amp de guitarra | distorção, EQ de 4 bandas, gabinete e gate | Airwindows **PointyGuitar** (`forja::dsp::Ponta`) | MIT · Chris Johnson | parcial: DSP, paridade 66/66 e UI; host pendente | PONTA |
| `DSP-039` | Amp comprimido | compressão multibanda embutida na distorção | Airwindows **ChimeyGuitar2** (`forja::dsp::Chime`) | MIT · Chris Johnson | parcial: DSP, paridade 66/66 e UI; host pendente | CHIME |
| `DSP-040` | Baixo de dois captadores | seis biquads, skew e saturação em cascata | Airwindows **CrickBass** (`forja::dsp::Crick`) | MIT · Chris Johnson | parcial: DSP, paridade 26/26, latência 1/3/5; host pendente | CRICK |
| `DSP-041` | DI de transformador | dois preditores Kalman em série, bandpass e corpo | Airwindows **Wolfbot** (`forja::dsp::Lobo`) | MIT · Chris Johnson | parcial: DSP, paridade 16/16 e UI compacta; host pendente | LOBO |
| `DSP-042` | DI comprimido | dez bandas fixas, 16 cascatas e seguidores Bèzier | Airwindows **ChimeyDeluxe** (`forja::dsp::Deluxe`) | MIT · Chris Johnson | parcial: DSP, paridade 46/46 e UI; host pendente | DELUXE |
| `DSP-043` | Limitador adaptativo | clip adaptativo com Boost/Match, ceiling dinâmico e ClipOnly3 | Airwindows **ADClip9** (`forja::dsp::Muralha`) | MIT · Chris Johnson | parcial: DSP, paridade 41/41, latência 1; host pendente | MURALHA |
| `DSP-044` | Clipe de pico extremo | pré-clipe ±4, slew áureo e ClipOnly2; atua só acima de +4,18 dBFS | Airwindows **FinalClip** (`forja::dsp::Dique`) | MIT · Chris Johnson | parcial: DSP, paridade 17/17, latência 1; host pendente | DIQUE |
| `DSP-045` | Engine de sampler | 64 vozes, 8 camadas de velocity, 16 round-robins, choke de 5 ms, roubo da voz mais antiga, interpolação linear | própria (`forja::drumforge::DrumEngine`, `VoicePool`) | Forja | parcial: existe no engine do DRUMFORGE, com teste de RT-safety; promoção para biblioteca pendente | DRUMFORGE |
| `DSP-046` | Compressor de canal | estéreo-linkado, joelho suave de 6 dB, limiar −40…0 dB e razão 1:1…20:1 | própria (`forja::drumforge::Compressor`) | Forja | parcial: existe no engine do DRUMFORGE; promoção para `forja::dsp` pendente | DRUMFORGE |
| `DSP-047` | Limiter de pico | teto −0,3 dBFS, ataque instantâneo e retorno de 80 ms, sem lookahead | própria (`forja::drumforge::MasterBus`) | Forja | parcial: existe no engine do DRUMFORGE; latência 0 medida por impulso | DRUMFORGE |
| `DSP-048` | Player SoundFont | render SF2/SF3 multi-timbral (16 canais MIDI), escolha de instrumento no canal 0 (camada Forja), carga assíncrona do bank, adoção por token sem alocar no áudio, reap do bank substituído, medidor por bloco e bypass de 10 ms | **TinySoundFont** (Bernhard Schelling) via wrapper `forja::canivete::SoundFontEngine` | MIT · Bernhard Schelling | parcial: DSP, UI, FUNDAMENTOS e teste próprio (`forja_canivete`) prontos; SF3 ativo via `stb_vorbis` v1.22 vendorizado; host pendente | CANIVETE |

**Estados possíveis:** `existe` · `parcial` · `planejado` · `descartado`.
Nunca escreva `existe` sem ter rodado no host.

---

## 5. Catálogo — Mixer

| ID | Bloco | O que faz | Estado |
|---|---|---|---|
| `MIX-001` | Strip (BIGORNA) | cadeia ordenada de `DSP-*`: gain → TÊMPERA → CINZÉL → SALÃO/FÔLEGO → BRASA → pan → M/S/ø → PESO | parcial: o DRUMFORGE implementa a ordem com blocos próprios (`DSP-046` no lugar da TÊMPERA, EQ fixo no lugar do CINZÉL); promoção para `forja::mixer` pendente |
| `MIX-002` | Bus / Master | soma em bus, master strip 1.4× com EQ, comp e limiter | parcial: o DRUMFORGE soma 5 barramentos com trim, TÊMPERA e limiter (`DSP-047`); EQ de master pendente |
| `MIX-003` | Mute group (MARCA) | 4 grupos, membros por peça, sobrescrita manual sobre a classificação automática | planejado |
| `MIX-004` | Solo | solo-in-place e PFL, com exclusão de mute herdado | parcial: solo-in-place no DRUMFORGE, com rampa de 12 ms; PFL pendente |
| `MIX-005` | Routing chip | endereçamento multi-out junto ao fader (L/R, 1-2, 3-4…) | parcial: chip MAIN/AUX 1-4 por canal no DRUMFORGE |
| `MIX-006` | Meter bank | N strips + master, telemetria read-only, sem matemática na thread de UI | parcial: pico, GR, clip e hold publicados em atômicos e consumidos a 30 Hz no DRUMFORGE |
| `MIX-007` | Strip preset | estado do strip serializável, independente do plugin | planejado |
| `MIX-008` | Dynamic layout | contagem de strips derivada do engine; 54–96 px por strip; < 64 px colapsa rótulo | parcial: 16 canais fixos de 92 px com `MiniKnob` e scroll horizontal |
| `MIX-009` | Vocabulário | léxico compartilhado: TÊMPERA, BRASA, MALHO, CINZÉL, TORNO, LIMA, ESCÓRIA, BIGORNA, CADINHO, LAMINADOR, ÓXIDO, REPIQUE, ECO, SALÃO, FÔLEGO, GRANALHA, LIXA, MEL, CERA, CORPO, ÍMPETO, REBARBA, GABINETE, PONTA, CHIME, CRICK, LOBO, DELUXE, MARCA, PESO | definido |

### Ordem da cadeia (não é negociável entre plugins)

```
gain → HPF → TÊMPERA (comp) → CINZÉL (EQ3, agora dinâmico) → SALÃO/FÔLEGO (send de espaço) → BRASA (sat) → pan → M / S / ø → PESO → bus
```

Um plugin que não tenha um bloco simplesmente o **omite**; não reordena os outros.

O léxico cresce sem colidir com os papéis já definidos: CINZÉL continua sendo o EQ do
insert, agora com camada dinâmica por banda; TORNO e LIMA são EQs alternativos de tom e de
bus; BIGORNA continua o channel strip completo e CADINHO é a variante de bus; ESCÓRIA é o
de-esser; LAMINADOR e ÓXIDO são as fitas; REPIQUE e ECO são os delays; SALÃO e FÔLEGO são
os reverbs; GRANALHA é o lo-fi; LIXA, MEL, CERA, CORPO, ÍMPETO e REBARBA são os one-knobs
da família MINIS; GABINETE, PONTA, CHIME, CRICK, LOBO e DELUXE são os amps e gabinetes.

---

## 6. Contratos

Esqueleto do contrato de DSP. É proposital que não haja JUCE aqui:

```cpp
namespace forja::dsp {

// 0..1 normalizado na fronteira; a unidade (dB, Hz, ms) é resolvida dentro do bloco.
class Processor
{
public:
    virtual ~Processor() = default;

    virtual void prepare (double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;

    // in-place, estéreo. Nada de alocar, logar ou bloquear aqui dentro.
    virtual void process (float* left, float* right, int numFrames) = 0;

    virtual void  setParam (int id, float normalised) = 0;
    virtual float getParam (int id) const = 0;

    virtual void  setBypassed (bool shouldBypass) = 0;
};

} // namespace forja::dsp
```

Contrato do strip:

```cpp
namespace forja::mixer {

enum class Stage { InputGain, HPF, Temper, Cinzel, Folego, Brasa, Pan, SoloMute, Peso };

struct StripState          // serializável, sem ponteiro para plugin
{
    float gainDb   = 0.0f;
    float hpfHz    = 20.0f;
    float temper   = 0.0f;   // 0..1, one-knob
    float eqLowDb  = 0.0f;
    float eqMidDb  = 0.0f;
    float eqMidHz  = 800.0f;
    float eqHighDb = 0.0f;
    float folego   = 0.0f;   // send
    float brasa    = 0.0f;
    float pan      = 0.5f;
    bool  mute     = false;
    bool  solo     = false;
    bool  phase    = false;
    int   muteGroup = -1;    // -1 = sem grupo
    int   outBus     = 0;
};

class Strip
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void setState (const StripState&);
    StripState getState() const;
    void process (float* left, float* right, int numFrames);
    float meterPeakDb() const;   // telemetria read-only para a UI
};

} // namespace forja::mixer
```

**Amarração com o plugin:** o `<plugin>` cria um adapter que traduz `AudioProcessorValueTreeState`
→ `StripState` uma vez por bloco, e `meterPeakDb()` → atomics lidos pela UI. O strip nunca conhece
o APVTS, e a UI nunca conhece o strip.

---

## 7. Convenções

1. **Normalização na fronteira.** Todo parâmetro cruza a API como `0..1`; o mapeamento para
   dB/Hz/ms vive dentro do bloco, com `NormalisableRange` documentado no header.
2. **Sample rate nunca é inventado.** Vem de `prepare()`. Nenhum default embutido.
3. **`process()` é sagrado:** sem `new`, sem `std::function`, sem log, sem lock, sem exceção.
4. **Reset explícito.** `reset()` limpa estado e denormals; chamado em `prepare()` e no transporte.
5. **Estéreo in-place.** Mono vira `left == right`; não criar caminho separado.
6. **Meter é telemetria.** O bloco publica valores; quem calcula escala e cor é a UI, com
   limiares vindos do engine.
7. **Nomes de arquivo** `Forja<Bloco>.h`; classes em `forja::dsp` / `forja::mixer`.
8. **Proveniência obrigatória.** Todo bloco declara no header: origem (própria ou terceiro),
   licença e ID do catálogo.

---

## 8. Como um plugin adota o catálogo

1. Ler a tabela de IDs e marcar o que já existe (`existe` / `parcial`).
2. Criar o adapter de parâmetros do plugin → `StripState`.
3. Montar as cadeias com `MIX-001`, omitindo os blocos ausentes — **sem reordenar**.
4. Ligar o meter bank à telemetria publicada.
5. Só desenhar UI depois que o bloco tiver contrato; a UI consome estado, não o contrário.

**Nunca:** copiar `ForjaStrip.cpp` para dentro do plugin e editar lá. Se precisa mudar,
muda no catálogo e todos ganham.

---

## 9. Proveniência de DSP de terceiros

Mesma disciplina prevista para o Drumforge (`drumforge/`):

| Origem | Licença | Entra em `vendor/`? | Como |
|---|---|---|---|
| Airwindows — Chris Johnson | MIT | sim | código verbatim + `README.md` (URL, commit, data) + `LICENSE`; adaptador separado |
| David Bryant resampler | BSD-3-Clause | sim | `Source/license.txt` já presente |
| Drumlabooh — Peter Semiletov | domínio público / GPLv3 no repo | sim, com nota | manter cabeçalho original; registrar modificação |
| Código GPLv3 de terceiros | GPLv3 | **não** | contamina o catálogo; avaliar caso a caso |
| Código proprietário | — | **não** | incompatível |

---

## 10. Estado atual e pendências

**Já existe:** `DSP-002`, `DSP-005`, `DSP-013` e, dentro do DRUMFORGE, `DSP-045` a
`DSP-047`; `MIX-009` definido; `forja::ui` implementado como biblioteca estática
`forja_ui` (tema, card, knob, medidores, topo, status, painel Fundamentos, curva de EQ por
FFT e chip REDES oculto enquanto não há link).

A base **Airwindows** tem **31 plugins** em nove famílias, todos com DSP, paridade, UI e
FUNDAMENTOS:

| Família | Plugins |
| --- | --- |
| `Airwindows/Saturation/` | BRASA (Tube2) |
| `Airwindows/Eq/` | CINZÉL (Parametric + dinâmica Forja), TORNO (Baxandall3), LIMA (SmoothEQ2) |
| `Airwindows/Dynamics/` | TÊMPERA (ButterComp2), PRENSA (Pressure5), MALHO (Pop2), ESCÓRIA (DeBess), MURALHA (ADClip9), DIQUE (FinalClip) |
| `Airwindows/Strips/` | BIGORNA (CStrip2), CADINHO (X2Buss) |
| `Airwindows/Tape/` | LAMINADOR (ToTape9), ÓXIDO (IronOxide5) |
| `Airwindows/Space/` | REPIQUE (TapeDelay2), ECO (Echojam), SALÃO (kRockstar2), FÔLEGO (kStation) |
| `Airwindows/Lofi/` | GRANALHA (DeRez5) |
| `Airwindows/OneKnob/` | LIXA (PurestDrive), MEL (Sweeten), CERA (PurestWarm3), CORPO (Mojo), ÍMPETO (Dyno), REBARBA (Slew3) |
| `Airwindows/Amps/` | GABINETE (Cabs2), PONTA (PointyGuitar), CHIME (ChimeyGuitar2), CRICK (CrickBass), LOBO (Wolfbot), DELUXE (ChimeyDeluxe) |

A família **MINIS** é o guarda-chuva dos plugins compactos de propósito único. Ela agrupa
os seis one-knobs da tabela — que continuam fisicamente em `Airwindows/OneKnob/` por regra
de proveniência MIT — e os minis originais Forja, que vivem na base `Minis/` (pasta irmã).
O **MINIDRUM** é o leitor de kits `.dfkit` do Drumforge em formato de gaveta,
com um knob de volume, reutilizando o engine `drumforge` como bloco próprio. O **MINISAT**
é o saturador de um knob (DRIVE): reusa o adaptador validado do BRASA (Tube2), promovido a
`Forja/Source/forja/dsp/ForjaBrasa.h` justamente para ser bloco compartilhado. A única camada
Forja ali é o mapa `input = 0,50 + 0,50·drive` (Válvula 0,50 e Saída 0 dB fixos, sem makeup
gain), com drive 0 equivalendo ao default documentado do BRASA; latência 0 medida por impulso.
No rack, registra-se como gaveta `minisat` (categoria drive, fachada valve). Os minis Forja
não têm paridade com vendor; seguem os testes funcionais e de RT-safety da base deles e
podem reutilizar adaptadores Airwindows como blocos internos.

Os adaptadores rodam com paridade amostra a amostra contra o código Airwindows
original e passam pela guarda de alocação. Latências medidas: 0 na maioria; 1 amostra em
PRENSA, MALHO, CADINHO, LAMINADOR, MURALHA e DIQUE; 1/3/5 em CRICK conforme a taxa; delays
e reverbs não usam PDC (o atraso interno é musical). CINZÉL é o único DSP híbrido: em
`dyn = 0` é bit-exato ao Parametric e acima disso é comportamento Forja documentado como
desvio. Delays e reverbs internos são testados com factory por caso; a tabela de paridade
mede a latência por impulso em vez de presumi-la.

**Fora do catálogo DSP-0xx:** o **DRUMFORGE** (`drumforge/Drumforge/`) é a primeira base de
DSP original Forja — sampler de bateria com kits, mixer e multi-out. Não é adaptação da
Airwindows, então não tem paridade com vendor: os testes dele são funcionais (sampler,
mapeamento MIDI, estado, multi-out, round-robin/velocity, choke, roubo de voz) e de
RT-safety. Estado atual:

| Item | Estado |
| --- | --- |
| Engine (`DrumEngine`, `VoicePool`, `SampleStore`) | Pronto: 64 vozes, camadas, RR, choke, importação assíncrona |
| Kit | `.dfkit` (XML com caminhos relativos) e importação por pasta com classificação por nome |
| Processor | 249 parâmetros, MIDI GM, 5 barramentos estéreo, estado, bypass |
| Loops MIDI (`LoopLibrary`, `LoopPlayer`) | Pronto: leitura em beats, biblioteca por pacote, preview por amostra e arrasto |
| UI | Abas KITS, MIXER e LOOPS MIDI prontas |
| Testes | `forja_drumforge_engine` no `ctest`, incluindo zero alocação em `process()` |
| Latência | 0 (medida por impulso) |

O engine mora hoje em `Source/engine/` do próprio plugin. `ChannelStrip`, `Compressor`,
`Filters` e `MasterBus` são candidatos naturais a subir para `forja::mixer`/`forja::dsp`
(`DSP-045` a `DSP-047`, `MIX-001`, `MIX-002`) quando um segundo produto precisar deles —
até lá, ficam no plugin para não inventar contrato antes do segundo consumidor.

O protótipo de UI simplificou o conceito para três abas (Kits, Mixer e Loops MIDI), e as
três estão implementadas. Sends, racks de FX dedicados, tela de Settings e editor de mapas
de pads ficam para depois; os dois envios pós-fader para SALÃO e LIXA já existem. Os
adaptadores Airwindows entram como blocos de canal e de master, sem mudar o catálogo.

O preview de loops tem módulo próprio (`Source/loops/`): o arquivo MIDI é lido em beats,
o loop fecha em compasso inteiro e o `LoopPlayer` enfileira cada golpe com o offset exato
em amostras no mesmo caminho do MIDI ao vivo. O BPM vem do playhead (`loop_sync`) ou do
controle manual `loop_bpm`, e o `.mid` selecionado pode ser arrastado para a DAW.

**Também fora do catálogo DSP-0xx:** o **FORJARACK** (`Forjarack/Forjarack/`) é a terceira
base da suíte — um rack em cadeia estilo Série 500 com oito gavetas em série, cada uma
hospedando um adaptador Airwindows já validado por paridade (CERA, MEL, LIXA, ÓXIDO,
TÊMPERA, MURALHA, CINZÉL, FÔLEGO, SALÃO e mais, conforme as fatias avançarem). O DSP é o
dos módulos; o FORJARACK acrescenta apenas o encadeamento, a chave on/off e de escuta por
gaveta e a seção master de trim e fase. O `ModuleCatalog` é append-only: minis como o
MINIDRUM e o CANIVETE registram-se como gaveta `instrument` sem alterar o rack. Essas
gavetas de instrumento com biblioteca própria mostram banda de arquivos: um seletor com o
conteúdo da pasta da biblioteca (sincronizando o knob de índice ao escolher) e um botão
ARQUIVO que abre kit/soundfont fora da biblioteca via FileChooser — enquanto o externo
toca, o knob fica mudo; a escolha pela lista devolve o comando a ele. A gaveta CANIVETE
mostra ainda a fileira INSTRUMENTO — os presets do bank carregado, como no plugin solto,
resolvidos pelo nome (`drawerInst_slotN`) porque o índice não é estável entre banks; a
fileira some durante a carga em segundo plano e volta quando o bank fica tocável. Desde
a gaveta
**HOSPEDADO**, o registro também aceita hosting dinâmico: qualquer `.vst3` (AU no macOS)
solto em `Documentos/Forja/Rack` vira gaveta em tempo de execução, com knobs vindos do
próprio plugin, editor nativo em janela separada (travando os knobs do rack enquanto
aberto), MIDI completo para instrumentos e latência/PDC propagados — plugins hospedados
mantêm a própria licença (carregados em runtime, modelo de DAW; nada entra na suíte).
Manual: `docs/MANUAL-GAVETA.md`. Testes próprios: `forja_forjarack_engine` (catálogo,
encadeamento, bypass/solo e zero alocação) e `forja_forjarack_hosted` (stub de
`AudioPluginInstance`: adoção efeito/instrumento, params dinâmicos, trava da UI nativa,
MIDI nativo e latência) no `ctest`. A banda de arquivos das gavetas de instrumento é
coberta por `testDrawerFiles()` dentro de `forja_minidrum` e `forja_canivete`; a banda de
instrumento do CANIVETE, por `testDrawerInstruments()`; e o áudio real saindo pela trilha
do span (`processMidi`+`process` com bank carregado), por `testDrawerAudio()`. Falta a
costura do plugin, coberta por `forja_forjarack_processor`
(`Tests/Source/forjarack_processor.cpp`): o **processador real** dirigido pela API do
`AudioProcessor` como uma DAW faz — layout de instrumento aceito, tecla entregando
canal/nota/velocidade certos ao módulo, voz saindo audível, MIDI do host consumido,
medidor master reportando MIDI e, quando há bank na pasta do usuário, o CANIVETE de
verdade soando pelo rack (a seção se declara SKIP sem soundfont). Foi esse teste que
pegou o bug de velocidade: o coletor tratava `getVelocity()` (que já vem 0..127) como
0..1 e remultiplicava por 127, estourando o `uint8`. O rack
aceita saída estéreo com entrada **estéreo ou desabilitada**: com entrada ligada roda como
FX em cadeia; numa faixa de instrumento (entrada desabilitada) o processador trata a
entrada como silêncio e a saída é a voz dos slots-instrumento — o mesmo caminho dos minis
soltos, para o teclado ao vivo tocar sem exigir fonte de áudio na faixa. Um **led MIDI**
na seção MASTER acende em todo bloco que chega com MIDI (nota, CC ou pitch bend) para
qualquer gaveta — o medidor de ENTRADA só mostra áudio, então sem esse led não havia
como separar "o teclado não chega à faixa" de "chega e o módulo não gera voz".
A costura que faltava é coberta por `forjarack_vst3_midi`
(`Tests/Source/forjarack_vst3_midi.cpp`): **carrega o `.vst3` real do build pelo formato
VST3 do JUCE**, como uma DAW faz, e verifica que o wrapper anuncia a fila de eventos de
entrada, que o layout de instrumento é aceito, que sem nota a saída é silêncio e que a
nota do host atravessa `data.inputEvents` e faz o CANIVETE soar. Sem bundle ou sem
soundfont na biblioteca o teste se declara SKIP (exit 0). Foi exatamente essa camada que
os outros testes não enxergavam — todos chamam `processBlock` no objeto C++ e por isso
ficaram verdes com o plugin mudo na DAW.

**Pendências, em ordem:**

- [ ] Rodar os 31 plugins no host: bypass da DAW, automação, mono, SR 96 kHz.
- [ ] Validar a gaveta HOSPEDADO no host: carregar `.vst3` reais de
      `Documentos/Forja/Rack` (efeito e instrumento), abrir editor nativo em janela
      separada e conferir o PDC com a DAW — o bridge já tem teste próprio
      (`forja_forjarack_hosted`), mas a carga de formato real depende do ambiente.
- [ ] Validar no host a banda de arquivos das gavetas MINIDRUM/CANIVETE: escolher pela
      lista, abrir kit/soundfont fora da biblioteca via ARQUIVO e conferir o knob mudo
      enquanto o externo toca — o contrato já tem teste (`testDrawerFiles()`), mas o
      FileChooser depende de sessão gráfica. Na gaveta CANIVETE, conferir também a
      fileira INSTRUMENTO (`testDrawerInstruments()`) e o toque do banco pela trilha
      do span (`testDrawerAudio()`, verde no CI). O teclado ao vivo dentro do rack tinha
      uma causa de silêncio **fora do C++**: `forja_add_plugin` declarava
      `NEEDS_MIDI_INPUT FALSE`, a macro `JucePlugin_WantsMidiInput` do wrapper VST3 ficava
      em `0`, o plugin não anunciava o barramento `kEvent` e o wrapper pulava a conversão
      `data.inputEvents` → `MidiBuffer` — `acceptsMidi()` não tem efeito nenhum no VST3.
      O rack agora opta por `FORJA_WANTS_MIDI_INPUT` com a categoria `Instrument;Fx`, e a
      costura está travada por `forjarack_vst3_midi` (validada por mutação: com a flag
      desligada o pico cai a `0.0000`). O que resta no host é o de sempre: automação,
      PDC e monitoramento da faixa. O led MIDI do MASTER continua sendo o diagnóstico:
      aceso = o MIDI chega (culpa de roteamento/monitoramento da faixa), apagado = a
      faixa não está recebendo o teclado.
- [ ] Implementar o DRUMFORGE no host: automação dos 249 parâmetros, multi-out em DAW,
      bypass, preview sincronizado e arrasto de loop para a timeline; aqui não há DAW,
      então segue pendente como nos demais.
- [ ] Editor de passos/gravação de groove no DRUMFORGE (a grade atual é só de leitura).
- [ ] Preencher `forja::ui::kSocialUrl` quando o link agregador das redes existir (o chip
      REDES aparece sozinho).
- [ ] Implementar `MIX-003` (MARCA) no `ValueTreeState`.
- [ ] Sincronizar REPIQUE/ECO ao BPM do host (`DSP-011`).
- [ ] Validar o catálogo no PianoForge: se um bloco não serve lá, o contrato está errado.
- [ ] Backlog fora do build: consoles (ConsoleX3), filtros XYZ, VerbSevens e a
      família Bass completa.

---

## 11. Checklist antes de publicar um bloco

- [ ] Tem ID e o ID não mudou depois de publicado.
- [ ] `forja::dsp` não inclui header de plugin nem de UI.
- [ ] `process()` não aloca, não loga, não trava.
- [ ] Parâmetros entram normalizados e a unidade está documentada no header.
- [ ] `prepare()` respeita o sample rate do host; `reset()` limpa denormals.
- [ ] Proveniência e licença declaradas no header e no `vendor/README.md`.
- [ ] Rodou no host, nos estados vazio / pronto / clip / bypass.
- [ ] A tabela deste catálogo foi atualizada, inclusive o `Estado`.
