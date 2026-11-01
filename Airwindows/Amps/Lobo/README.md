# LOBO — DI de transformador (Wolfbot)

Condicionador de entrada sem controles, construído sobre o DSP do **Airwindows
Wolfbot** (MIT), com a interface do padrão Forja. Projeto JUCE nativo (VST3),
compilado a partir da raiz da suíte e usando a biblioteca
compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::LoboDSP`) | Implementado |
| Paridade com o original | 16/16 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3) | Compila; UI conferida via Standalone local (`FORJA_STANDALONE=ON`) |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras**.

## DSP

`Forja/Source/forja/dsp/ForjaLobo.h` transcreve `processDoubleReplacing` do
Wolfbot:

1. um preditor Kalman de passa-altos com três amostras de história;
2. um preditor Kalman de passa-baixos em série;
3. cada estágio tem ganho adaptativo limitado pela taxa de amostragem;
4. a saída passa por `sin(x · 0,7943) · 1,2589`;
5. RNG do dither avança (o dither em si não é injetado) e o bypass é suavizado.

Desvios do adaptador:

- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- hook de teste `setNoiseState()` para semear o RNG como o original;
- nenhum parâmetro inventado (o original não tem controles).

## Interface

- Janela compacta de 560 × 260 px; card único de DI com medidor, nota e BYPASS.
- Badge de estado, SR, buffer e DSP/PDC na barra inferior compacta.
- FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Lobo_VST3
```

Artefatos:

- `build/Airwindows/Amps/Lobo/Lobo_artefacts/Release/VST3/Lobo.vst3`

## Licença e créditos

DSP original: Airwindows Wolfbot, © Chris Johnson, licença MIT
(`Forja/Source/forja/vendor/airwindows/Wolfbot`). Adaptador Forja: ver
`Forja/Source/forja/dsp/ForjaLobo.h`.
