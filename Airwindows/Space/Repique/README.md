# REPIQUE — delay de fita (TapeDelay2)

Delay de fita com regeneração ressonante, filtro no caminho de realimentação e
flutter, construído sobre o DSP do **Airwindows TapeDelay2** (MIT), com a
interface do padrão Forja. Projeto JUCE nativo (VST3), compilado a
partir da raiz da suíte e usando a biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::RepiqueDSP`) | Implementado |
| Paridade com o original | 50/50 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3) | Compila; UI conferida via Standalone local (`FORJA_STANDALONE=ON`) |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Tempo | `time` | 0,00 – 1,00 | 1,00 | Velocidade da cabeça, `25·A⁴ + 1`; atraso = `88200 / speed` amostras |
| Regeneração | `regen` | 0,00 – 1,00 | 0,00 | Retorno do eco, `B²` |
| Frequência | `freq` | 0,00 – 1,00 | 0,50 | Corte do filtro de realimentação, `0,4·C³ + 0,0001` |
| Ressonância | `reso` | 0,00 – 1,00 | 0,00 | Amortecimento, `D² + 0,01` |
| Flutter | `flutter` | 0,00 – 1,00 | 0,00 | Profundidade da modulação, `E⁵ · speed · (freq·0,09 + 0,025)` |
| Mistura | `mix` | 0,00 – 1,00 | 1,00 | Dry/wet submix: `wet = 2F`, `dry = 2 − wet`, ambos limitados a 1 |
| Sync | `sync` | on/off | off | Tempo segue o BPM do host; com BPM válido o knob TEMPO fica inativo |
| Divisão | `division` | 6 posições | 1/4 | 1/16, 1/8, 1/4, 1/2, 1/1 ou 2/1; só vale com Sync ligado |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras**. O anel de 88200 amostras é o eco musical;
ele não é compensação de latência e não entra no PDC. Com TEMPO em 0 o atraso é
de 88200 amostras (~1,8 s em 48 kHz); com TEMPO em 1, cerca de 3392 amostras
(~71 ms em 48 kHz).

## Sync de BPM

Com `sync` ligado e um BPM válido do host, a divisão vira segundos
(`60/BPM · batidas`), a velocidade necessária é `88200 / (taxa · segundos)`,
limitada a `[1, 26]`, e o TEMPO equivalente é `((speed − 1) / 25)^(1/4)`. O anel
fixo limita o atraso a ≈2 s em 44,1 kHz; divisões maiores são grampeadas e a
leitura mostra "máx". Com `sync` desligado — ou sem BPM do host — o caminho é
exatamente o original.

## DSP

`Forja/Source/forja/dsp/ForjaRepique.h` transcreve `processDoubleReplacing` do
TapeDelay2:

1. `cycleEnd` = 1, 2, 3 ou 4 conforme a taxa (2 acima de 44,1/48 kHz, 4 em
   176/192 kHz) e processamento do núcleo só no fim de cada ciclo;
2. `speed = 25·A⁴ + 1` mais o flutter `E⁵ · speed · (0,09·freq + 0,025)`
   modulado por `sin(sweep) + 1`; o `sweep` avança com `0,05·x²` do sinal de
   entrada e dá a volta em 2π;
3. anel de 88200 amostras: a cabeça recua `speed` posições por ciclo e o trecho
   percorrido recebe uma interpolação linear entre a amostra anterior e a nova;
4. filtro de realimentação de estado variável (corte `C³`, ressonância `D²+0,01`)
   e filtro de saída com o mesmo corte e ressonância × 1,618033988749895;
5. interpolação de retorno (`lastRef[0..4]`) e médias sucessivas por ciclo
   (`lastRef[5..8]`);
6. submix dry/wet do original (0,50 = seco cheio + eco cheio, não crossfade);
7. dither de 64 bits omitida (só o RNG avança) e bypass suavizado.

Desvios do adaptador:

- anel e buffers de estado ficam no heap (`std::vector`) para caber na pilha do
  harness; a alocação acontece só na construção;
- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- hook de teste `setNoiseState()` para semear o RNG como o `rand()` original.

## Interface

- Janela fixa de 1000 × 540 px; topo FORJA / DE ESPAÇO / REPIQUE.
- Cards: Tempo (controle grande com leitura em ms/s, botão SYNC e seletor de
  divisão), Fita (regeneração, flutter e arte de carretéis `ForjaTapeArt`),
  Filtro (frequência e ressonância) e Mistura (dry/wet + peak meter + bypass).
  Com SYNC ligado o knob de Tempo fica inativo e a leitura mostra a divisão e o
  tempo resolvido.
- Badge de estado, SR, buffer, resumo de parâmetros e linha de DSP/PDC na barra
  inferior. FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Repique_VST3
```

Artefatos:

- `build/Airwindows/Space/Repique/Repique_artefacts/Release/VST3/Repique.vst3`

## Licença e créditos

- DSP: **Airwindows TapeDelay2** © Chris Johnson, licença MIT
  (commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`).
- Adaptador, interface e integração: Forja.
- Original preservado em `Forja/Source/forja/vendor/airwindows/TapeDelay2/`
  com `LICENSE` e `PROVENANCE.md` (hashes SHA-256). Não é compilado no plugin.
