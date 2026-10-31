# CERA — calor assimétrico (PurestWarm3)

Saturação assimétrica de um só controle, construída sobre o DSP do
**Airwindows PurestWarm3** (MIT), com a interface do padrão Forja. Projeto JUCE
nativo (VST3 + Standalone), compilado a partir da raiz da suíte e usando a
biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::CeraDSP`) | Implementado |
| Paridade com o original | 21/21 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3 + Standalone) | Compila; UI conferida no Standalone |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Calor | `warmth` | 0,00 – 1,00 | 0,50 | `asym = (CALOR − 0,50) · 8` |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras**. Em 0,50 o bloco é pulado e o sinal passa
intacto.

## DSP

`Forja/Source/forja/dsp/ForjaCera.h` transcreve `processDoubleReplacing` do
PurestWarm3:

1. o sinal é deslocado por `asym` e passa por uma curva cúbica;
2. o deslocamento é desfeito e a diferença vira a primeira derivada;
3. uma cadeia de quatro derivadas guarda a história curta do erro;
4. o sinal é multiplicado por `1 + |dC|·0,0618 + |dD|·(−0,05982) + |dE|·0,0206`;
5. RNG do dither avança (passo extra assimétrico de `fpdR` incluído) e o bypass
   é suavizado.

Desvios do adaptador:

- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- hook de teste `setNoiseState()` para semear o RNG como o `rand()` original;
- sem ganho de compensação.

## Interface

- Janela fixa de 560 × 260 px (layout compacto); topo FORJA / DE MIX / CERA.
- Card único com knob de 108 px, leitura numérica, nota e BYPASS.
- Badge de estado, SR, buffer e linha de DSP/PDC na barra inferior compacta.
  FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Cera_VST3 Cera_Standalone
```

Artefatos:

- `build/Airwindows/OneKnob/Cera/Cera_artefacts/Release/VST3/Cera.vst3`
- `build/Airwindows/OneKnob/Cera/Cera_artefacts/Release/Standalone/Cera.exe`

## Licença e créditos

DSP original: Airwindows PurestWarm3, © Chris Johnson, licença MIT
(`Forja/Source/forja/vendor/airwindows/PurestWarm3`). Adaptador Forja: ver
`Forja/Source/forja/dsp/ForjaCera.h`.
