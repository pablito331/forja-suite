# ECO — espaços de eco (Echojam)

Eco de sala com dezessete espaços fixos, dezesseis taps por canal e uma matriz de
realimentação que troca energia entre canais, construído sobre o DSP do
**Airwindows Echojam** (MIT), com a interface do padrão Forja. Projeto JUCE
nativo (VST3), compilado a partir da raiz da suíte e usando a
biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::EcoDSP`) | Implementado |
| Paridade com o original | 52/52 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3) | Compila; UI conferida via Standalone local (`FORJA_STANDALONE=ON`) |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Espaço | `select` | 17 posições | 0,50 (espaço 09) | `clearcoat = int(A · 16,999)`; troca os 16 tempos e zera as linhas |
| Regeneração | `regen` | 0,00 – 1,00 | 0,50 | Retorno, `(1 − (1 − B)²) · 0,0625` |
| Velocidade | `speed` | 0,00 – 1,00 | 0,50 | Redução de taxa interna, `C^(taxa/44100)` |
| Mistura | `mix` | 0,00 – 1,00 | 0,50 | `wet = 1 − (1 − D)²`; 0,50 já é 75 % molhado |
| Sync | `sync` | on/off | off | Velocidade segue o BPM do host; com BPM válido o knob VELOCIDADE fica inativo |
| Divisão | `division` | 6 posições | 1/4 | 1/16, 1/8, 1/4, 1/2, 1/1 ou 2/1; só vale com Sync ligado |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras**. Os tempos internos (1–519 ms de tap) são o
espaço musical; não entram no PDC.

Os dezessete espaços, com o comentário de tempo/lugares do original:

| # | Tempo | Lugares | # | Tempo | Lugares |
| --- | --- | --- | --- | --- | --- |
| 01 | 1–419 ms | 5.286 | 10 | 2–473 ms | 6.729 |
| 02 | 2–423 ms | 5.392 | 11 | 3–483 ms | 7.027 |
| 03 | 1–436 ms | 5.711 | 12 | 3–489 ms | 7.203 |
| 04 | 1–436 ms | 5.721 | 13 | 2–489 ms | 7.206 |
| 05 | 2–463 ms | 6.453 | 14 | 2–492 ms | 7.281 |
| 06 | 3–472 ms | 6.729 | 15 | 2–519 ms | 8.101 |
| 07 | 3–444 ms | 5.975 | 16 | 2–423 ms | 5.399 |
| 08 | 2–457 ms | 6.291 | 17 | 2–473 ms | 6.725 |
| 09 | 2–461 ms | 6.401 | | | |

## Sync de BPM

Com `sync` ligado e um BPM válido do host, a parede mais longa do espaço
selecionado é alinhada à divisão: `derez = parede / (taxa · segundos)`, limitado
a `[0,0001, 1]`, e a velocidade equivalente é `derez^(44100 / taxa)`. Em 120 BPM,
1/4 pede 500 ms; se a parede não alcança, o valor é grampeado e a leitura mostra
"máx". A tabela de paredes (`EcoDSP::maxWallFor`) é fixa por espaço. Com `sync`
desligado — ou sem BPM do host — o caminho é exatamente o original.

## DSP

`Forja/Source/forja/dsp/ForjaEco.h` transcreve `processDoubleReplacing` do
Echojam:

1. `clearcoat` seleciona um dos 17 espaços; quando muda, as 32 linhas são
   zeradas e os contadores voltam a 1 (primeiro bloco sempre inicializa);
2. a cada amostra o ciclo acumula `derez`, a entrada é somada escalada e, ao
   cruzar 1,0, o núcleo roda: 16 taps por canal, matriz `2h − Σvizinhos`
   distribuída entre os canais opostos;
3. `regen = (1 − (1 − B)²) · 0,0625`; `derez = C^(taxa/44100)` limitado a
   `[0,0001, 1]` e convertido em fração `1/bezFraction`;
4. reconstrução por Bézier entre as três últimas amostras de cada canal,
   escalada por `0,125`;
5. `wet = 1 − (1 − D)²` mistura seco e molhado;
6. dither de 64 bits omitida (só o RNG avança, inclusive o passo extra
   assimétrico de `fpdR`) e bypass suavizado.

Desvios do adaptador:

- as 32 linhas de atraso ficam no heap (`std::vector`) para caber na pilha do
  harness; a alocação acontece só na construção;
- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- hook de teste `setNoiseState()` para semear o RNG como o `rand()` original.

## Interface

- Janela fixa de 1000 × 540 px; topo FORJA / DE ESPAÇO / ECO.
- Cards: Espaço (`ForjaChoice` com os 17 espaços e a legenda de tempo/lugares),
  Eco (regeneração, velocidade, botão SYNC e seletor de divisão), Mistura
  (dry/wet) e Saída (peak meter + bypass). Com SYNC ligado o knob de Velocidade
  fica inativo e a leitura mostra a divisão e o tempo resolvido.
- O combo e o parâmetro `select` ficam sincronizados nos dois sentidos: a
  automação do host move o combo e vice-versa.
- Badge de estado, SR, buffer, resumo de parâmetros e linha de DSP/PDC na barra
  inferior. FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Eco_VST3
```

Artefatos:

- `build/Airwindows/Space/Eco/Eco_artefacts/Release/VST3/Eco.vst3`

## Licença e créditos

- DSP: **Airwindows Echojam** © Chris Johnson / Airwindows, licença MIT
  (commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`).
- Adaptador, interface e integração: Forja.
- Original preservado em `Forja/Source/forja/vendor/airwindows/Echojam/`
  com `LICENSE` e `PROVENANCE.md` (hashes SHA-256). Não é compilado no plugin.
