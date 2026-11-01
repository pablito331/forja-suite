# ÓXIDO — fita de óxido de ferro (IronOxide5)

Emulação de fita de óxido de ferro com foco em textura e lo-fi, construída
sobre o DSP do **Airwindows IronOxide5** (MIT), com a interface do padrão
Forja. Projeto JUCE nativo (VST3), compilado a partir da raiz da
suíte e usando a biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::OxidoDSP`) | Implementado |
| Paridade com o original | 55/55 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3) | Compila; UI conferida via Standalone local (`FORJA_STANDALONE=ON`) |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Entrada | `input` | 0,00 – 1,00 | 0,50 | Trim de entrada, `10^((36·A−18)/20)` (−18 a +18 dB) |
| Fita agudo | `tapeHigh` | 0,00 – 1,00 | 0,5623 | Velocidade da fita, `(B²)²·148,5 + 1,5` IPS (×1,1 no DSP) |
| Fita grave | `tapeLow` | 0,00 – 1,00 | 0,5623 | Velocidade do lado grave, define o passa-altas |
| Flutter | `flutter` | 0,00 – 1,00 | 0,50 | Profundidade da modulação, `D²·SR/44100`; em 0 desliga |
| Ruído | `noise` | 0,00 – 1,00 | 0,50 | Grão de fita, `E·0,5`; em 0 desliga |
| Saída | `output` | 0,00 – 1,00 | 0,50 | Trim de saída de −18 a +18 dB |
| Mistura | `invDryWet` | 0,00 – 1,00 | 1,00 | Inv/Dry/Wet: 0,50 só seco, 1,00 só fita, abaixo de 0,50 invertido |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras**. O flutter é modulação de tempo musical, não
uma linha de atraso fixa.

## DSP

`Forja/Source/forja/dsp/ForjaOxido.h` transcreve `processDoubleReplacing` do
IronOxide5:

1. flutter: anel de 100 amostras com deslocamento de 0 a 30 amostras, leitura
   interpolada, profundidade `D²·overallscale`, varredura com `0,00581/SR` e
   sorteio de novo período entre 0,02 e 1,00;
2. passa-altas de um polo alternando entre os estados A e B a cada amostra,
   `iirAmount = lps/430` corrigido pela taxa;
3. ganho de entrada `10^((36·A−18)/20)`;
4. limitador suave em seno (teto 1,57079633) antes da distorção;
5. modelador Iron Oxide: anel de 264 amostras, integradores rápido e lento com
   taps esparsos (`/2` em cascata e soma final `/128`), ramos A/B alternados,
   `fastTaper = ips/15`, `slowTaper = 2/lps²`, `lowspeedscale = 5/ips`, todos
   corrigidos pela taxa;
6. segundo limitador em seno;
7. grão de fita: `randy = (0,55 + tempRandy + fpdR/UINT32_MAX·tempRandy)·noise`
   com `tempRandy = 0,04 + 0,11/√ips`; mistura com a amostra seca anterior;
8. ganho de saída e bloco Inv/Dry/Wet
   (`invdrywet = 2G−1`, `dry = 1−invdrywet` quando positivo);
9. dither de 64 bits omitida (só o RNG avança) e bypass suavizado.

Desvios do adaptador:

- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- a quarta potência de `tapeHigh`/`tapeLow` é feita em float antes de virar
  double, como no original;
- membros não usados do original (`iirSamplehA/B`, `fastIIH*`, `slowIIH*`) foram
  omitidos; eles nunca tocam o sinal;
- o Inv/Dry/Wet preservado não é um crossfade comum: em 0,50 a saída é só seca.

## Interface

- Janela fixa de 1000 × 540 px; topo FORJA / DE FITA / ÓXIDO.
- Cards: Entrada e saída (dois trims com nota), Fita (duas velocidades em IPS e
  arte de cassete `ForjaTapeArt` no pé do card), Textura (flutter e ruído) e
  Mistura (Inv/Dry/Wet + peak meter + bypass).
- Badge de estado, SR, buffer, resumo de parâmetros e linha de DSP/latência na
  barra inferior. FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Oxido_VST3
```

Artefatos:

- `build/Airwindows/Tape/Oxido/Oxido_artefacts/Release/VST3/Oxido.vst3`

## Licença e créditos

- DSP: **Airwindows IronOxide5** © Chris Johnson, licença MIT
  (commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`).
- Adaptador, interface e integração: Forja.
- Original preservado em `Forja/Source/forja/vendor/airwindows/IronOxide5/`
  com `LICENSE` e `PROVENANCE.md` (hashes SHA-256). Não é compilado no plugin.
