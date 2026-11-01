# MEL — brilho suave (Sweeten)

Adoçador de topo de um só controle, construído sobre o DSP do
**Airwindows Sweeten** (MIT), com a interface do padrão Forja. Projeto JUCE
nativo (VST3), compilado a partir da raiz da suíte e usando a
biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::MelDSP`) | Implementado |
| Paridade com o original | 21/21 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3) | Compila; UI conferida via Standalone local (`FORJA_STANDALONE=ON`) |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Sweeten | `sweeten` | 0,00 – 1,00 | 0,00 | Quantidade da subtração quadrada |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras**.

## DSP

`Forja/Source/forja/dsp/ForjaMel.h` transcreve `processDoubleReplacing` do
Sweeten:

1. o sinal passa por uma pilha de médias antes do quadrado (2 a 8 amostras
   conforme a taxa);
2. o resultado é elevado ao quadrado e escalado por `2^(controle·10 − 10)`;
3. uma segunda pilha de médias suaviza o produto;
4. o valor é subtraído do sinal seco, por canal;
5. RNG do dither avança e o bypass é suavizado.

Desvios do adaptador:

- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- hook de teste `setNoiseState()` para semear o RNG como o `rand()` original;
- sem ganho de compensação.

## Interface

- Janela fixa de 560 × 260 px (layout compacto); topo FORJA / DE MIX / MEL.
- Card único com knob de 108 px, leitura numérica, nota e BYPASS.
- Badge de estado, SR, buffer e linha de DSP/PDC na barra inferior compacta.
  FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Mel_VST3
```

Artefatos:

- `build/Airwindows/OneKnob/Mel/Mel_artefacts/Release/VST3/Mel.vst3`

## Licença e créditos

DSP original: Airwindows Sweeten, © Chris Johnson, licença MIT
(`Forja/Source/forja/vendor/airwindows/Sweeten`). Adaptador Forja: ver
`Forja/Source/forja/dsp/ForjaMel.h`.
