# GABINETE — gabinete faseado (Cabs2)

Simulador de alto-falante de dois filtros em cascata, construído sobre o DSP do
**Airwindows Cabs2** (MIT), com a interface do padrão Forja. Projeto JUCE
nativo (VST3 + Standalone), compilado a partir da raiz da suíte e usando a
biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::GabineteDSP`) | Implementado |
| Paridade com o original | 26/26 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3 + Standalone) | Compila; UI conferida no Standalone |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Lowpass | `lowpass` | 0,00 – 1,00 | 1,00 | Fecha o topo do gabinete |
| Hipass | `hipass` | 0,00 – 1,00 | 0,00 | Corta o corpo do gabinete |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras**.

## DSP

`Forja/Source/forja/dsp/ForjaGabinete.h` transcreve `processDoubleReplacing`
do Cabs2:

1. o passa-altos aplica 21 polos em sequência, subtraindo a média do sinal;
2. o passa-baixos aplica 13 polos em sequência;
3. as frequências vêm de `pow(parâmetro, overallscale)`, sem pré-warping;
4. com passa-baixos em 1,00 e passa-altos em 0,00 os blocos ficam em repouso;
5. RNG do dither avança (o dither em si não é injetado) e o bypass é suavizado.

Desvios do adaptador:

- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- hook de teste `setNoiseState()` para semear o RNG como o original;
- sem saturação, drive ou ganho de compensação (o original também não tem).

## Interface

- Janela fixa de 1000 × 540 px; três cards (passa-baixos, passa-altos e sinal).
- Curva de resposta não é desenhada: o filtro é não linear em fase e a curva
  estática enganaria; o FUNDAMENTOS explica a dependência da taxa.
- Badge de estado, SR, buffer, latência e DSP na barra inferior.

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Gabinete_VST3 Gabinete_Standalone
```

Artefatos:

- `build/Airwindows/Amps/Gabinete/Gabinete_artefacts/Release/VST3/Gabinete.vst3`
- `build/Airwindows/Amps/Gabinete/Gabinete_artefacts/Release/Standalone/Gabinete.exe`

## Licença e créditos

DSP original: Airwindows Cabs2, © Chris Johnson, licença MIT
(`Forja/Source/forja/vendor/airwindows/Cabs2`). Adaptador Forja: ver
`Forja/Source/forja/dsp/ForjaGabinete.h`.
