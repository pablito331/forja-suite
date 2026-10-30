# LIXA — drive limpo (PurestDrive)

Amaciador de picos de um só controle, construído sobre o DSP do
**Airwindows PurestDrive** (MIT), com a interface do padrão Forja. Projeto JUCE
nativo (VST3 + Standalone), compilado a partir da raiz da suíte e usando a
biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::LixaDSP`) | Implementado |
| Paridade com o original | 21/21 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3 + Standalone) | Compila; UI conferida no Standalone |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Drive | `drive` | 0,00 – 1,00 | 0,00 | Quantidade da curva de seno |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras**.

## DSP

`Forja/Source/forja/dsp/ForjaLixa.h` transcreve `processDoubleReplacing` do
PurestDrive:

1. o sinal seco é guardado por canal;
2. a forma de onda é trocada por `sin(x)`;
3. a quantidade aplicada é `|anterior + sin(x)| / 2 · drive`;
4. a saída mistura sinal seco e seno com essa quantidade;
5. o sample anterior guarda `sin(seco)` para o próximo ciclo;
6. RNG do dither avança (o dither em si não é injetado) e o bypass é suavizado.

Desvios do adaptador:

- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- hook de teste `setNoiseState()` para semear o RNG como o `rand()` original;
- sem ganho de compensação (o original também não tem).

## Interface

- Janela fixa de 560 × 260 px (layout compacto); topo FORJA / DE MIX / LIXA.
- Card único com knob de 108 px, leitura numérica, nota e BYPASS.
- Badge de estado, SR, buffer e linha de DSP/PDC na barra inferior compacta.
  FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Lixa_VST3 Lixa_Standalone
```

Artefatos:

- `build/OneKnob/Lixa/Lixa_artefacts/Release/VST3/Lixa.vst3`
- `build/OneKnob/Lixa/Lixa_artefacts/Release/Standalone/Lixa.exe`

## Licença e créditos

DSP original: Airwindows PurestDrive, © Chris Johnson, licença MIT
(`Forja/Source/forja/vendor/airwindows/PurestDrive`). Adaptador Forja: ver
`Forja/Source/forja/dsp/ForjaLixa.h`.
