# LAMINADOR — fita de estúdio (ToTape9)

Emulação de fita de estúdio de duas pistas construída sobre o DSP do
**Airwindows ToTape9** (MIT), com a interface do padrão Forja. Projeto JUCE
nativo (VST3 + Standalone), compilado a partir da raiz da suíte e usando a
biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::LaminadorDSP`) | Implementado |
| Paridade com o original | 65/65 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3 + Standalone) | Compila; UI conferida no Standalone |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Entrada | `input` | 0,00 – 1,00 | 0,50 | Ganho de entrada, `(2·A)²`; 0,50 é unitário |
| Inclinação | `tilt` | 0,00 – 1,00 | 0,50 | TILT: quantidade de Dubly na codificação e na decodificação |
| Forma | `shape` | 0,00 – 1,00 | 0,50 | SHAPE: reparte o filtro Dubly entre codificar e decodificar |
| Flutter | `flutter` | 0,00 – 1,00 | 0,50 | Profundidade da modulação, `D⁶·SR/44100·50` (teto 498 amostras) |
| Vel. flutter | `flutSpd` | 0,00 – 1,00 | 0,50 | Velocidade da varredura, `0,02·E³` (por amostra de 44,1 kHz) |
| Bias | `bias` | 0,00 – 1,00 | 0,50 | Offsets da fita, `F·2−1`; em 0,50 a escada fica inativa |
| Cabeçote | `headBmp` | 0,00 – 1,00 | 0,50 | Drive (`G·0,1`) e mistura (`G·0,5`) do reforço de grave |
| Freq. cabeçote | `headFrq` | 0,00 – 1,00 | 0,50 | Frequência do reforço, de 25 a 200 Hz |
| Saída | `output` | 0,00 – 1,00 | 0,50 | Ganho de saída, `2·I`; 0,50 é unitário |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **1 amostra** (anel do ClipOnly3). O flutter é uma
modulação de tempo musical e por isso não entra na compensação de atraso da DAW.

## DSP

`Forja/Source/forja/dsp/ForjaLaminador.h` transcreve `processDoubleReplacing`
do ToTape9:

1. `overallscale = SR/44100`; `spacing` e `slewsing` (escala do ClipOnly3 e das
   médias móveis) com os mesmos clamps do original;
2. ganho de entrada `(2·A)²`;
3. codificação Dubly: diferença contra um filtro de um polo
   (`iirEncFreq = (1−C)/overallscale`), compressão logarítmica
   `log(1+255·d)/2,40823996531`, média lenta `compEnc` e adição de
   `highPart·compEnc·2B`;
4. flutter: anel de 1000 amostras, `offset = depth + depth·sin(sweep)`, leitura
   interpolada, período sorteado entre 0,24 e 0,98 a cada volta da varredura;
5. bias: escada de nove estágios de slew, cada um 1,618 vezes o anterior, com
   `underBias`/`overBias` dependentes do sinal de `bias`;
6. histerese fina de ±0,011449 multiplicada por `(1−|x|)²·0,012`;
7. TapeHack2: médias móveis de 2 a 32 amostras conforme `slewsing`, misturadas
   por fator de slew, antes e depois da distorção;
8. TapeHack: clamp em ±2,305929007734908 e série de Taylor ímpar até a 11ª
   potência (aproximação de seno);
9. cabeçote: oscilador cúbico e dois biquads em cascata na frequência escolhida
   e em 93,75% dela, Q 0,618, somados por `G·0,5`;
10. decodificação Dubly com `outlyAmount = (1−B)·−2` (teto −1);
11. ganho de saída `2·I` e ClipOnly3 com ruído vindo do RNG;
12. dither de 64 bits omitida (só o RNG avança) e bypass suavizado.

Desvios do adaptador, todos documentados no `FUNDAMENTOS`:

- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- ClipOnly3 usa o bloco compartilhado `ClipOnly3Channel` (anel maior; os
  buffers 18/34 do original já estavam em bounds porque `spacing` para em 16);
- a quadratura de `H` é feita em float antes de virar double, como no original;
- a troca entre canais no cálculo do próximo período de flutter foi preservada
  de propósito (L usa `sweepR`/`nextmaxR` e R usa `sweepL`/`nextmaxL`).

## Interface

- Janela fixa de 1000 × 540 px; topo FORJA / DE FITA / LAMINADOR.
- Cards: Entrada (input e inclinação com nota), Cor e cabeçote (forma, bias,
  cabeçote e frequência), Flutter (profundidade e velocidade com nota) e Saída
  (saída + peak meter + bypass).
- Arte de máquina de fita (`ForjaTapeArt`, carretéis e caminho da fita) no pé do
  card Flutter, sem custo de áudio.
- Badge de estado, SR, buffer, resumo de parâmetros e linha de DSP/latência na
  barra inferior. FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Laminador_VST3 Laminador_Standalone
```

Artefatos:

- `build/Airwindows/Tape/Laminador/Laminador_artefacts/Release/VST3/Laminador.vst3`
- `build/Airwindows/Tape/Laminador/Laminador_artefacts/Release/Standalone/Laminador.exe`

## Licença e créditos

- DSP: **Airwindows ToTape9** © Chris Johnson, licença MIT
  (commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`).
- Adaptador, interface e integração: Forja.
- Original preservado em `Forja/Source/forja/vendor/airwindows/ToTape9/`
  com `LICENSE` e `PROVENANCE.md` (hashes SHA-256). Não é compilado no plugin.
