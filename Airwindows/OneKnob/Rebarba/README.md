# REBARBA — suavização de slew (Slew3)

Limitador de passo de um só controle, construído sobre o DSP do
**Airwindows Slew3** (MIT), com a interface do padrão Forja. Projeto JUCE
nativo (VST3 + Standalone), compilado a partir da raiz da suíte e usando a
biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::RebarbaDSP`) | Implementado |
| Paridade com o original | 21/21 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3 + Standalone) | Compila; UI conferida no Standalone |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Suavidade | `slew` | 0,00 – 1,00 | 0,00 | `(1 − SUAVIDADE)^4 / escala` |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras**.

## DSP

`Forja/Source/forja/dsp/ForjaRebarba.h` transcreve `processDoubleReplacing` do
Slew3:

1. três amostras por canal guardam a trajetória;
2. a predição usa os pesos áureos 0,381966 e 0,618033;
3. se o passo estimado passar do limite `(1 − controle)^4 / escala`, o sample é
   puxado para perto do anterior;
4. uma média com os mesmos pesos alisa a transição;
5. RNG do dither avança e o bypass é suavizado.

Desvios do adaptador:

- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- hook de teste `setNoiseState()` para semear o RNG como o `rand()` original.

## Interface

- Janela fixa de 560 × 260 px (layout compacto); topo FORJA / DE MIX / REBARBA.
- Card único com knob de 108 px, leitura numérica, nota e BYPASS.
- Badge de estado, SR, buffer e linha de DSP/PDC na barra inferior compacta.
  FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Rebarba_VST3 Rebarba_Standalone
```

Artefatos:

- `build/Airwindows/OneKnob/Rebarba/Rebarba_artefacts/Release/VST3/Rebarba.vst3`
- `build/Airwindows/OneKnob/Rebarba/Rebarba_artefacts/Release/Standalone/Rebarba.exe`

## Licença e créditos

DSP original: Airwindows Slew3, © Chris Johnson, licença MIT
(`Forja/Source/forja/vendor/airwindows/Slew3`). Adaptador Forja: ver
`Forja/Source/forja/dsp/ForjaRebarba.h`.
