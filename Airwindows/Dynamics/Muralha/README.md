# MURALHA — Limitador adaptativo (ADClip9)

Limitador de clipe adaptativo construído sobre o DSP do **Airwindows ADClip9**
(MIT) com a interface do padrão Forja. Projeto JUCE nativo (VST3 + Standalone)
compilado a partir da raiz da suíte, usando a biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::MuralhaDSP`) | Implementado |
| Paridade com o original | 41/41 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3 + Standalone) | Compila; UI conferida no Standalone |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Boost | `boost` | 0,00 – 1,00 | 0,00 | Ganho de entrada de 0 a +18 dB |
| Match | `match` | 0,00 – 1,00 | 0,00 | Ganho de 0 a +18 dB usado no modo MATCH |
| Noise | `noise` | 0,00 – 1,00 | 0,70 | Nível de ruído da transição de clipe |
| Ceiling | `ceiling` | 0,00 – 1,00 | 0,75 | Teto do clipe |
| Modo | `mode` | 0,00 – 1,00 (0 / 0,5 / 1) | 0,00 | BOOST / MATCH / CLIP ONLY |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

O bypass é exposto via `getBypassParameter()`. Latência reportada:
**1 amostra** (ver desvios).

## DSP

`Forja/Source/forja/dsp/ForjaMuralha.h` transcreve `processDoubleReplacing` do
ADClip9:

1. ganho de entrada `10^(boost·18/20)` ou `10^(match·18/20)` conforme o modo;
2. estágio **ClipOnly3** por canal: detecção de clipe positivo/negativo,
   comparação com `noiseLevel`, fila `intermediate[18]` e histórico de slew
   `slew[34]`;
3. teto efetivo `ceiling / (1 + maxSlew·1.3986013)`, que aperta o limite
   quando o sinal se move rápido;
4. pós-processamento do modo: MATCH divide pela match, CLIP ONLY subtrai o
   overshoot e divide pelo ganho de entrada;
5. LCG `fpdL/fpdR` por instância, com a mesma sequência do original.

O adaptador acrescenta bypass com rampa de 10 ms, medidores e GR. `process()`
não aloca, não registra log e não bloqueia.

**Desvios documentados:**

- Estados e RNG são membros por instância (o original usa `static`), então duas
  instâncias não compartilham sequência de dither nem buffers.
- O modo é exposto como parâmetro 0–1 com os três pontos do original (0, 0,5 e
  1). A UI mostra apenas BOOST, MATCH e CLIP ONLY.
- Latência real de 1 amostra. O caminho seco do bypass também é atrasado 1
  amostra para não virar comb filter durante o crossfade.

GR exibida: `abs(saída)/abs(entrada)` do canal com maior redução, em dB.

## Interface

- Janela fixa de 1000 × 540 px; topo FORJA / DE LIMITADORES / MURALHA.
- Cards: Entrada (Boost + Match), Clipe (Ceiling + Noise), Modo (escolha +
  medidor de GR), Saída (peak meter + bypass).
- Badge de estado, SR, buffer, resumo de parâmetros e linha de DSP/latência na
  barra inferior; LED CLIP clicável no card de saída.
- Botão **FUNDAMENTOS** no topo abre o painel nativo (fechado por FECHAR, `Esc`
  ou clique fora). Sem WebView.

## Build (Windows)

A partir da raiz da suíte:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Muralha_VST3 Muralha_Standalone
```

Artefatos:

- `build/Airwindows/Dynamics/Muralha/Muralha_artefacts/Release/VST3/Muralha.vst3`
- `build/Airwindows/Dynamics/Muralha/Muralha_artefacts/Release/Standalone/Muralha.exe`

Para instalar, copie a pasta `Muralha.vst3` para
`C:\Program Files\Common Files\VST3\`.

## Licença e créditos

- DSP: **Airwindows ADClip9** © Chris Johnson, licença MIT
  (commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`).
- Adaptador, interface e integração: Forja.
- Original preservado como referência em
  `Forja/Source/forja/vendor/airwindows/ADClip9/` com `LICENSE` e
  `PROVENANCE.md` (hashes SHA-256). Não é compilado no plugin.
