# SALÃO — sala grande (kRockstar2)

Reverb de sala grande com reflexões iniciais 3×3, corpo de seis vias por canal
e filtro de distância de 25 estágios, construído sobre o DSP do
**Airwindows kRockstar2** (MIT), com a interface do padrão Forja. Projeto JUCE
nativo (VST3), compilado a partir da raiz da suíte e usando a
biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::SalaoDSP`) | Implementado |
| Paridade com o original | 44/44 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3) | Compila; UI conferida via Standalone local (`FORJA_STANDALONE=ON`) |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Regeneração | `regen` | 0,00 – 1,00 | 0,50 | `(1 − (1 − A)³) · 0,0078125` |
| Posição | `position` | 0,00 – 1,00 | 0,75 | `start = int(B · 27)` na tabela de reflexões |
| DownRez | `derez` | 0,00 – 1,00 | 1,00 | Subamostragem do corpo por Bézier |
| Distância | `distance` | 0,00 – 1,00 | 0,25 | `(C · 5)²` → até 25 estágios de filtro |
| Mistura | `mix` | 0,00 – 1,00 | 0,50 | Seco/molhado linear; 1,00 = só sala |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras**. O tempo interno da sala é musical; não
entra no PDC.

## DSP

`Forja/Source/forja/dsp/ForjaSalao.h` transcreve `processDoubleReplacing` do
kRockstar2:

1. rede de reflexões iniciais 3×3 por canal, nove linhas alimentadas pela
   tabela `early[]` de 36 posições (2–193 ms no comentário do original);
2. corpo de realimentação 6×6 por canal em cinco estágios (36 linhas por
   canal), cada estágio somando as seis saídas com pesos cruzados;
3. subamostragem do corpo por Bézier (`downRez`), com a entrada e a saída
   reconstruídas pela curva;
4. filtro de distância de 25 estágios de média de um polo, aplicado dentro e
   fora da subamostragem, com passo fracionário;
5. piso de ruído de sala alimentado pelo RNG do dither;
6. dither de 64 bits omitida (só o RNG avança, inclusive o passo extra
   assimétrico de `fpdR`) e bypass suavizado.

Desvios do adaptador:

- as 90 linhas de atraso ficam no heap (`std::vector`) para caber na pilha do
  harness; a alocação acontece só na construção;
- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- hook de teste `setNoiseState()` para semear o RNG como o `rand()` original;
- campo `roomTone` em `Parameters` (padrão 1,0, bit-exato com o original). O DRUMFORGE usa
  0 no FX 1 do master para a mesa ficar em silêncio com nada tocando; o SALÃO standalone
  continua com o piso de ruído de sala do kRockstar2.

## Interface

- Janela fixa de 1000 × 540 px; topo FORJA / DE SALA / SALÃO.
- Cards: Sala (nota), Tempo (regeneração e posição), Distância (downrez,
  distância e nota) e Saída (mistura, peak meter + bypass).
- Badge de estado, SR, buffer, resumo de parâmetros e linha de DSP/PDC na barra
  inferior. FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Salao_VST3
```

Artefatos:

- `build/Airwindows/Space/Salao/Salao_artefacts/Release/VST3/Salao.vst3`

## Licença e créditos

- DSP: **Airwindows kRockstar2** © Chris Johnson / Airwindows, licença MIT
  (commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`).
- Adaptador, interface e integração: Forja.
- Original preservado em `Forja/Source/forja/vendor/airwindows/kRockstar2/`
  com `LICENSE` e `PROVENANCE.md` (hashes SHA-256). Não é compilado no plugin.
