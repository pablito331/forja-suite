# CORPO — corpo e graves (Mojo)

Curva de corpo de um só controle, construída sobre o DSP do
**Airwindows Mojo** (MIT), com a interface do padrão Forja. Projeto JUCE
nativo (VST3 + Standalone), compilado a partir da raiz da suíte e usando a
biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::CorpoDSP`) | Implementado |
| Paridade com o original | 21/21 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3 + Standalone) | Compila; UI conferida no Standalone |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Corpo | `input` | 0,00 – 1,00 | 0,50 | Ganho de entrada de −12 a +12 dB |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras**.

## DSP

`Forja/Source/forja/dsp/ForjaCorpo.h` transcreve `processDoubleReplacing` do
Mojo:

1. o ganho de entrada é `10^((controle·24 − 12)/20)`;
2. `mojo = |x|^0,25` mede o porte do sinal;
3. a saída é `sin(x·mojo·π/2) / mojo · 0,987654321`, por canal;
4. RNG do dither avança e o bypass é suavizado.

Desvios do adaptador:

- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- hook de teste `setNoiseState()` para semear o RNG como o `rand()` original;
- sem ganho de compensação.

## Interface

- Janela fixa de 560 × 260 px (layout compacto); topo FORJA / DE MIX / CORPO.
- Card único com knob de 108 px, leitura em dB, nota e BYPASS.
- Badge de estado, SR, buffer e linha de DSP/PDC na barra inferior compacta.
  FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Corpo_VST3 Corpo_Standalone
```

Artefatos:

- `build/OneKnob/Corpo/Corpo_artefacts/Release/VST3/Corpo.vst3`
- `build/OneKnob/Corpo/Corpo_artefacts/Release/Standalone/Corpo.exe`

## Licença e créditos

DSP original: Airwindows Mojo, © Chris Johnson, licença MIT
(`Forja/Source/forja/vendor/airwindows/Mojo`). Adaptador Forja: ver
`Forja/Source/forja/dsp/ForjaCorpo.h`.
