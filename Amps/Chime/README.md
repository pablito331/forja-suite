# CHIME — amp chime comprimido (ChimeyGuitar2)

Amp de guitarra com compressão multibanda embutida na distorção, construído
sobre o DSP do **Airwindows ChimeyGuitar2** (MIT), com a interface do padrão
Forja. Projeto JUCE nativo (VST3 + Standalone), compilado a partir da raiz da
suíte e usando a biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::ChimeDSP`) | Implementado |
| Paridade com o original | 66/66 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3 + Standalone) | Compila; UI conferida no Standalone |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Compres | `compres` | 0,00 – 1,00 | 0,60 | Polos e limiar da compressão por banda |
| Presence | `presence` | 0,00 – 1,00 | 0,80 | Ataque do pré |
| High | `high` | 0,00 – 1,00 | 0,70 | Agudos do pré |
| Mid | `mid` | 0,00 – 1,00 | 0,50 | Médios do pré |
| Low | `low` | 0,00 – 1,00 | 0,70 | Graves do pré |
| Sub | `sub` | 0,00 – 1,00 | 0,40 | Subgraves do pré |
| Highspeaker | `hspeaker` | 0,00 – 1,00 | 0,80 | Corte de topo do gabinete |
| Lowspeaker | `lspeaker` | 0,00 – 1,00 | 0,40 | Corte de corpo do gabinete |
| Speed | `speed` | 0,00 – 1,00 | 0,80 | Velocidade do envelope por banda |
| Output | `output` | 0,00 – 1,00 | 0,80 | Nível de saída |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras**.

## DSP

`Forja/Source/forja/dsp/ForjaChime.h` transcreve `processDoubleReplacing` do
ChimeyGuitar2:

1. `compres` define o número de polos (`compres³ · 20`) e o limiar
   (`1 − (compres · 0,9)²`);
2. cada polo percorre nove bandas e aplica um compressor mu-law com velocidade
   própria (`speed` → `adjSpd`);
3. a saída de cada polo passa por um limitador de seno em ±π/2;
4. dois bancos de alto-falante moldam topo e corpo;
5. RNG do dither avança (o dither em si não é injetado) e o bypass é suavizado.

Desvios do adaptador:

- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- hook de teste `setNoiseState()` para semear o RNG como o original;
- sem ganho de compensação (o original também não tem).

## Interface

- Janela fixa de 1000 × 540 px; quatro cards (amp, tom, alto-falante e sinal).
- Medidor de saída com CLIP; BYPASS no card de sinal; badge de estado.
- FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Chime_VST3 Chime_Standalone
```

Artefatos:

- `build/Amps/Chime/Chime_artefacts/Release/VST3/Chime.vst3`
- `build/Amps/Chime/Chime_artefacts/Release/Standalone/Chime.exe`

## Licença e créditos

DSP original: Airwindows ChimeyGuitar2, © Chris Johnson, licença MIT
(`Forja/Source/forja/vendor/airwindows/ChimeyGuitar2`). Adaptador Forja: ver
`Forja/Source/forja/dsp/ForjaChime.h`.
