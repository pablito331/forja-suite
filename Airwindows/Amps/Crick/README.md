# CRICK — amp de baixo de dois captadores (CrickBass)

Amp de baixo com seis estágios de saturação e dois caminhos de captador,
construído sobre o DSP do **Airwindows CrickBass** (MIT), com a interface do
padrão Forja. Projeto JUCE nativo (VST3 + Standalone), compilado a partir da
raiz da suíte e usando a biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::CrickDSP`) | Implementado |
| Paridade com o original | 26/26 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3 + Standalone) | Compila; UI conferida no Standalone |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Drive | `drive` | 0,00 – 1,00 | 0,50 | Ganho dos seis estágios |
| Tone | `tone` | 0,00 – 1,00 | 0,50 | Corte de graves e abertura |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **1 amostra** em 44,1/48 kHz, **3** em 88,2/96 kHz e **5**
em 192 kHz, conforme a subamostragem do original (medida por impulso).

## DSP

`Forja/Source/forja/dsp/ForjaCrick.h` transcreve `processDoubleReplacing` do
CrickBass:

1. seis biquads ultrassônicos fixos filtram a entrada;
2. um estágio de *skew* usa a derivada do sinal para orientar o ataque;
3. seis estágios de saturação com filtros de estado variável se sucedem;
4. o canal esquerdo passa por uma tabela de gabinete de 82 coeficientes;
5. o canal direito passa por outra tabela e termina em ClipOnly;
6. acima de 44,1 kHz o processamento entra em ciclo de subamostragem e
   interpola a saída;
7. RNG do dither avança (o dither em si não é injetado) e o bypass é suavizado.

Desvios do adaptador:

- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- hook de teste `setNoiseState()` para semear o RNG como o original;
- estados `lastSlewL/lastSlewR` inicializados (o original os deixa sem uso);
- sem ganho de compensação (o original também não tem).

## Interface

- Janela fixa de 1000 × 540 px; três cards (drive, tom e sinal).
- Medidor de saída com CLIP; nota de PDC variável com a taxa.
- FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Crick_VST3 Crick_Standalone
```

Artefatos:

- `build/Airwindows/Amps/Crick/Crick_artefacts/Release/VST3/Crick.vst3`
- `build/Airwindows/Amps/Crick/Crick_artefacts/Release/Standalone/Crick.exe`

## Licença e créditos

DSP original: Airwindows CrickBass, © Chris Johnson, licença MIT
(`Forja/Source/forja/vendor/airwindows/CrickBass`). Adaptador Forja: ver
`Forja/Source/forja/dsp/ForjaCrick.h`.
