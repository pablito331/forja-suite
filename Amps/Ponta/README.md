# PONTA — amp de guitarra (PointyGuitar)

Amp de guitarra completo com distorção, EQ de quatro faixas, gabinete e
gate, construído sobre o DSP do **Airwindows PointyGuitar** (MIT), com a
interface do padrão Forja. Projeto JUCE nativo (VST3 + Standalone), compilado a
partir da raiz da suíte e usando a biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::PontaDSP`) | Implementado |
| Paridade com o original | 66/66 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3 + Standalone) | Compila; UI conferida no Standalone |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Drive | `drive` | 0,00 – 1,00 | 0,60 | Ganho e estágios de distorção |
| Presence | `presence` | 0,00 – 1,00 | 0,50 | Ataque do pré |
| High | `high` | 0,00 – 1,00 | 0,80 | Agudos do pré |
| Mid | `mid` | 0,00 – 1,00 | 0,60 | Médios do pré |
| Low | `low` | 0,00 – 1,00 | 0,40 | Graves do pré |
| Sub | `sub` | 0,00 – 1,00 | 0,50 | Subgraves do pré |
| Highspeaker | `hspeaker` | 0,00 – 1,00 | 0,70 | Corte de topo do gabinete |
| Lowspeaker | `lspeaker` | 0,00 – 1,00 | 0,40 | Corte de corpo do gabinete |
| Gate | `gate` | 0,00 – 1,00 | 0,00 | Limiar do gate |
| Output | `output` | 0,00 – 1,00 | 0,70 | Nível de saída |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras** (medida por impulso não se aplica: o gate
zera um impulso isolado; a estrutura não tem linhas de atraso).

## DSP

`Forja/Source/forja/dsp/ForjaPonta.h` transcreve `processDoubleReplacing` do
PointyGuitar:

1. o número de polos vem de `(drive + 0,618) · 10`;
2. cada polo passa por nove bandas Bessel-ish entrelaçadas;
3. a soma das diferenças de banda alimenta o dobrador `x − clamp(x³ · 0,654)`;
4. o gate usa passagem por zero e um limiar derivado de `gate³ / 3`;
5. dois bancos de alto-falante (passa-baixos e passa-altos) fecham o caminho;
6. RNG do dither avança (o dither em si não é injetado) e o bypass é suavizado.

Desvios do adaptador:

- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- estados de gate inicializados em 0,0 (o original os deixa sem inicialização);
- hook de teste `setNoiseState()` para semear o RNG como o original;
- sem ganho de compensação (o original também não tem).

## Interface

- Janela fixa de 1000 × 540 px; quatro cards (amp, tom, alto-falante e sinal).
- Medidor de saída com CLIP; BYPASS no card de sinal; badge de estado.
- FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Ponta_VST3 Ponta_Standalone
```

Artefatos:

- `build/Amps/Ponta/Ponta_artefacts/Release/VST3/Ponta.vst3`
- `build/Amps/Ponta/Ponta_artefacts/Release/Standalone/Ponta.exe`

## Licença e créditos

DSP original: Airwindows PointyGuitar, © Chris Johnson, licença MIT
(`Forja/Source/forja/vendor/airwindows/PointyGuitar`). Adaptador Forja: ver
`Forja/Source/forja/dsp/ForjaPonta.h`.
