# DELUXE — DI comprimido (ChimeyDeluxe)

Condicionador de DI para guitarra e baixo, construído sobre o DSP do
**Airwindows ChimeyDeluxe** (MIT), com a interface do padrão Forja. Projeto
JUCE nativo (VST3 + Standalone), compilado a partir da raiz da suíte e usando a
biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::DeluxeDSP`) | Implementado |
| Paridade com o original | 46/46 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3 + Standalone) | Compila; UI conferida no Standalone |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Hell | `hell` | 0,00 – 1,00 | 0,50 | Agudo extremo; também abre o drive |
| Fizz | `fizz` | 0,00 – 1,00 | 0,50 | Chiado e brilho do topo |
| Pick | `pick` | 0,00 – 1,00 | 0,50 | Ataque da palheta |
| Satan | `satan` | 0,00 – 1,00 | 0,50 | Médio agudo |
| Danger | `danger` | 0,00 – 1,00 | 0,50 | Médio central |
| Critical | `critical` | 0,00 – 1,00 | 0,50 | Médio grave |
| H Meat | `hMeat` | 0,00 – 1,00 | 0,50 | Corpo alto |
| L Meat | `lMeat` | 0,00 – 1,00 | 0,50 | Corpo baixo |
| Swing | `swing` | 0,00 – 1,00 | 0,50 | Graves |
| Rarely | `rarely` | 0,00 – 1,00 | 0,50 | Subgraves |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras** (nenhuma linha de atraso no caminho).

## DSP

`Forja/Source/forja/dsp/ForjaDeluxe.h` transcreve `processDoubleReplacing` do
ChimeyDeluxe:

1. as dez bandas do painel viram ganhos de banda; o maior ganho define o drive,
   o menor define o pad;
2. o sinal atravessa dezesseis blocos em cascata, cada um com doze bandas de
   filtros acoplados (banda 0 = agudo extremo, banda 11 = grave);
3. em cada banda, um seguidor Bezier acumula o módulo do sinal e fecha o ganho
   por uma curva de seno · compressão multibanda em série;
4. a saída é grampeada em ± 1;
5. RNG do dither avança (o dither em si não é injetado) e o bypass é suavizado.

Desvios do adaptador:

- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- hook de teste `setNoiseState()` para semear o RNG como o original;
- sem ganho de compensação (o original também não tem).

## Interface

- Janela fixa de 1000 × 540 px; quatro cards (amp, tom, corpo e sinal).
- Rótulos originais preservados (Hell, Fizz, Pick…); o papel musical de cada
  banda aparece como legenda em português.
- Medidor de saída com CLIP; BYPASS no card de sinal; badge de estado.
- FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Deluxe_VST3 Deluxe_Standalone
```

Artefatos:

- `build/Airwindows/Amps/Deluxe/Deluxe_artefacts/Release/VST3/Deluxe.vst3`
- `build/Airwindows/Amps/Deluxe/Deluxe_artefacts/Release/Standalone/Deluxe.exe`

## Licença e créditos

DSP original: Airwindows ChimeyDeluxe, © Chris Johnson, licença MIT
(`Forja/Source/forja/vendor/airwindows/ChimeyDeluxe`). Adaptador Forja: ver
`Forja/Source/forja/dsp/ForjaDeluxe.h`.
