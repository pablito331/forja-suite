# GRANALHA — redutor de bits (DeRez5)

Bitcrusher/lo-fi construído sobre o DSP do **Airwindows DeRez5** (MIT), com a
interface do padrão Forja. Projeto JUCE nativo (VST3), compilado a
partir da raiz da suíte e usando a biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::GranalhaDSP`) | Implementado |
| Paridade com o original | 46/46 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3) | Compila; UI conferida via Standalone local (`FORJA_STANDALONE=ON`) |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Derez | `derez` | 0,00 – 1,00 | 1,00 | Redução de taxa (1,00 = sem redução) |
| Trim | `trim` | 0,00 – 1,00 | 1,00 | Redução de resolução da palavra |
| Square | `square` | 0,00 – 1,00 | 0,00 | Forma do degrau (patamar × curva) |
| Bright | `bright` | 0,00 – 1,00 | 1,00 | Undersampling de agudos |
| Bassis | `bassis` | 0,00 – 1,00 | 0,00 | Undersampling de graves |
| Output | `output` | 0,00 – 1,00 | 1,00 | Nível de saída |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras**.

## DSP

`Forja/Source/forja/dsp/ForjaGranalha.h` transcreve `processDoubleReplacing` do
DeRez5:

1. um contador de direção limita quantos pontos novos a onda recebe por trecho;
2. dois undersamplers Bezier (brilho e graves) reconstroem a forma em vez de
   segurar o valor anterior;
3. o parâmetro Square escolhe entre degrau em patamar e interpolação curva;
4. o trim de saída fecha o caminho;
5. RNG do dither avança (o dither em si não é injetado) e o bypass é suavizado.

Desvios do adaptador:

- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- hook de teste `setNoiseState()` para semear o RNG como o original;
- sem ganho de compensação (o original também não tem).

## Interface

- Janela de 1000 × 540 px com quatro cards (grão, bits, forma de onda e saída).
- Medidor de saída com CLIP; BYPASS no card de sinal; badge de estado.
- FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Granalha_VST3
```

Artefatos:

- `build/Airwindows/Lofi/Granalha/Granalha_artefacts/Release/VST3/Granalha.vst3`

## Licença e créditos

DSP original: Airwindows DeRez5, © Chris Johnson, licença MIT
(`Forja/Source/forja/vendor/airwindows/DeRez5`). Adaptador Forja: ver
`Forja/Source/forja/dsp/ForjaGranalha.h`.
