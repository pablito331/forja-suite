# MALHO — Impacto (Pop2)

Compressor de impacto com ataque e soltura separados, construído sobre o DSP do
**Airwindows Pop2** (MIT) com a interface do padrão Forja. Projeto JUCE nativo
(VST3) compilado a partir da raiz da suíte, usando a biblioteca
compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::MalhoDSP`) | Implementado |
| Paridade com o original | 35/35 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3) | Compila; UI conferida via Standalone local (`FORJA_STANDALONE=ON`) |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Compressão | `compress` | 0,00 – 1,00 | 0,50 | Threshold `1 − (1 − (1−A)²)·0,9` e pré-ganho `1/threshold` |
| Ataque | `attack` | 0,00 – 1,00 | 0,50 | `(B⁴·100000 + 10) · overallscale` |
| Soltura | `release` | 0,00 – 1,00 | 0,50 | `(C⁵·2000000 + 20) · overallscale`, limitado a 4× |
| Drive | `drive` | 0,00 – 1,00 | 0,50 | Makeup `sqrt(1/threshold) · D` |
| Mistura | `mix` | 0,00 – 1,00 | 1,00 | Dry/wet depois do ClipOnly2 |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

O bypass é exposto via `getBypassParameter()`. Latência reportada:
**1 amostra** (ver desvios).

## DSP

`Forja/Source/forja/dsp/ForjaMalho.h` transcreve `processDoubleReplacing` do
Pop2:

1. pré-ganho de threshold por canal;
2. dois envelopes independentes L/R com conjuntos A/B alternados por amostra
   (`flip`) — cada canal tem seu próprio detector;
3. ataque aplicado quando o sinal está abaixo do threshold, soltura quando
   acima, com teto `maxRelease = 4·release`;
4. ganho por canal `muCoefficient²` e makeup `sqrt(1/threshold)·drive`;
5. ClipOnly2 por canal **antes** do dry/wet;
6. dither `fpdL/fpdR`.

O adaptador acrescenta bypass com rampa de 10 ms, medidores e GR. `process()`
não aloca, não registra log e não bloqueia.

**Desvios documentados:**

- ClipOnly2 usa 17 posições, então `spacing == 16` (sample rates muito altos)
  não escreve fora do buffer; é assim que o port funciona em 705,6 kHz.
- Latência real de 1 amostra (o shift descendente do ClipOnly2 atrasa o
  sinal). O caminho seco do bypass também é atrasado 1 amostra para não virar
  comb filter durante o crossfade.
- Estados inicializados conforme o construtor original: `muSpeed{A,B}{L,R} =
  1000`, `muCoefficient = 1`, `muNewSpeed = 1000`, `muVary/muAttack = 0`,
  `flip = false`.

GR exibida: `muCoefficient²` do canal com maior redução, limitado a 1, em dB.

## Interface

- Janela fixa de 1000 × 540 px; topo FORJA / DE COMPRESSORES / MALHO.
- Cards: Compressão (knob + medidor de GR), Tempo (Ataque + Soltura), Drive
  (Drive + histórico de GR), Mistura (Mistura + peak meter + bypass).
- Badge de estado, SR, buffer, resumo de parâmetros e linha de DSP/latência na
  barra inferior; LED CLIP clicável no card de saída.
- Botão **FUNDAMENTOS** no topo abre o painel nativo (fechado por FECHAR, `Esc`
  ou clique fora). Sem WebView.

## Build (Windows)

A partir da raiz da suíte:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Malho_VST3
```

Artefatos:

- `build/Airwindows/Dynamics/Malho/Malho_artefacts/Release/VST3/Malho.vst3`

Para instalar, copie a pasta `Malho.vst3` para
`C:\Program Files\Common Files\VST3\`.

## Licença e créditos

- DSP: **Airwindows Pop2** © Chris Johnson, licença MIT
  (commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`).
- Adaptador, interface e integração: Forja.
- Original preservado como referência em
  `Forja/Source/forja/vendor/airwindows/Pop2/` com `LICENSE` e
  `PROVENANCE.md` (hashes SHA-256). Não é compilado no plugin.
