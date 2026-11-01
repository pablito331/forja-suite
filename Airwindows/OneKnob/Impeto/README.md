# ÍMPETO — compressão rápida (Dyno)

Compressão de forma de onda de um só controle, construída sobre o DSP do
**Airwindows Dyno** (MIT), com a interface do padrão Forja. Projeto JUCE
nativo (VST3), compilado a partir da raiz da suíte e usando a
biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::ImpetoDSP`) | Implementado |
| Paridade com o original | 21/21 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3) | Compila; UI conferida via Standalone local (`FORJA_STANDALONE=ON`) |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Ímpeto | `input` | 0,00 – 1,00 | 0,50 | Ganho de entrada de −12 a +12 dB |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras**.

## DSP

`Forja/Source/forja/dsp/ForjaImpeto.h` transcreve `processDoubleReplacing` do
Dyno:

1. o ganho de entrada é `10^((controle·24 − 12)/20)`;
2. `dyno = |x|^4` mede o porte do sample;
3. a saída é `sin(x·dyno) / dyno · 1,1654321`, por canal;
4. RNG do dither avança e o bypass é suavizado.

Não há detector, ataque nem release: a curva age no próprio sample.

Desvios do adaptador:

- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- hook de teste `setNoiseState()` para semear o RNG como o `rand()` original;
- sem ganho de compensação.

## Interface

- Janela fixa de 560 × 260 px (layout compacto); topo FORJA / DE MIX / ÍMPETO.
- Card único com knob de 108 px, leitura em dB, nota e BYPASS.
- Badge de estado, SR, buffer e linha de DSP/PDC na barra inferior compacta.
  FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Impeto_VST3
```

Artefatos:

- `build/Airwindows/OneKnob/Impeto/Impeto_artefacts/Release/VST3/Impeto.vst3`

## Licença e créditos

DSP original: Airwindows Dyno, © Chris Johnson, licença MIT
(`Forja/Source/forja/vendor/airwindows/Dyno`). Adaptador Forja: ver
`Forja/Source/forja/dsp/ForjaImpeto.h`.
