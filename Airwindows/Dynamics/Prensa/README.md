# PRENSA — Vari-mu (Pressure5)

Compressor vari-mu com densidade e caráter, construído sobre o DSP do
**Airwindows Pressure5** (MIT) com a interface do padrão Forja. Projeto JUCE
nativo (VST3 + Standalone) compilado a partir da raiz da suíte, usando a
biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::PrensaDSP`) | Implementado |
| Paridade com o original | 39/39 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3 + Standalone) | Compila; UI conferida no Standalone |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Pressão | `pressure` | 0,00 – 1,00 | 0,00 | Threshold `1 − 0,95·A` e makeup `1/threshold` |
| Velocidade | `speed` | 0,00 – 1,00 | 0,25 | Release `(1,28 − B)^5 · 32768`, dividido pelo `overallscale` |
| Miado | `mewiness` | 0,00 – 1,00 | 1,00 | Mistura entre o coeficiente linear e o quadrado/raiz |
| Garra | `pawclaw` | 0,00 – 1,00 | 0,50 | Modula `mewiness` pelo slew (`sin(C + slewMax·PawClaw)`) |
| Saída | `output` | 0,00 – 1,00 | 0,50 | Ganho `(2·E)²` |
| Mistura | `mix` | 0,00 – 1,00 | 1,00 | Dry/wet antes do ClipOnly2 |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

O bypass é exposto via `getBypassParameter()`. Latência reportada:
**1 amostra** (ver desvios).

## DSP

`Forja/Source/forja/dsp/ForjaPrensa.h` transcreve `processDoubleReplacing` do
Pressure5:

1. detector estéreo linkado (`max(|L|,|R|)`) depois da compensação de threshold;
2. dois conjuntos de envelope vari-mu (A/B) alternados por amostra (`flip`);
3. `mewiness` recalculada por amostra a partir do slew máximo do sample
   anterior e de `PawClaw`;
4. coeficiente aplicado: quadrado (positivo) ou raiz (negativo), interpolado
   pela `mewiness`;
5. ganho de saída, alternância de `flip`;
6. filtro ultrassônico fixo em 24 kHz (Q 0,7071), ativo apenas acima de ~48 kHz;
7. dry/wet, atualização de `slewMax`, ClipOnly2, dither `fpdL/fpdR`.

O adaptador acrescenta bypass com rampa de 10 ms, medidores e GR. `process()`
não aloca, não registra log e não bloqueia.

**Desvios documentados:**

- `muNewSpeed` e `muAttack` começam em 0 (o construtor original os deixa sem
  inicialização).
- ClipOnly2 usa 17 posições, então `spacing == 16` (sample rates muito altos)
  não escreve fora do buffer; é assim que o port funciona em 705,6 kHz.
- Latência real de 1 amostra (o shift descendente do ClipOnly2 atrasa o
  sinal). O caminho seco do bypass também é atrasado 1 amostra para não virar
  comb filter durante o crossfade.

GR exibida: `coefficient` aplicado, limitado a 1, em dB.

## Interface

- Janela fixa de 1000 × 540 px; topo FORJA / DE COMPRESSORES / PRENSA.
- Cards: Pressão (knob + medidor de GR), Tempo (Velocidade + Miado), Garra
  (Garra + histórico de GR), Saída (Saída + Mistura + peak meter + bypass).
- Badge de estado, SR, buffer, resumo de parâmetros e linha de DSP/latência na
  barra inferior; LED CLIP clicável no card de saída.
- Botão **FUNDAMENTOS** no topo abre o painel nativo (fechado por FECHAR, `Esc`
  ou clique fora). Sem WebView.

## Build (Windows)

A partir da raiz da suíte:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Prensa_VST3 Prensa_Standalone
```

Artefatos:

- `build/Airwindows/Dynamics/Prensa/Prensa_artefacts/Release/VST3/Prensa.vst3`
- `build/Airwindows/Dynamics/Prensa/Prensa_artefacts/Release/Standalone/Prensa.exe`

Para instalar, copie a pasta `Prensa.vst3` para
`C:\Program Files\Common Files\VST3\`.

## Licença e créditos

- DSP: **Airwindows Pressure5** © Chris Johnson, licença MIT
  (commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`).
- Adaptador, interface e integração: Forja.
- Original preservado como referência em
  `Forja/Source/forja/vendor/airwindows/Pressure5/` com `LICENSE` e
  `PROVENANCE.md` (hashes SHA-256). Não é compilado no plugin.
