# TÊMPERA — Compressor de cola (ButterComp2)

Compressor de bus suave construído sobre o DSP do **Airwindows ButterComp2**
(MIT) com a interface do padrão Forja. Projeto JUCE nativo (VST3),
compilado a partir da raiz da suíte e usando a biblioteca compartilhada
`forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::TemperaDSP`) | Implementado |
| Paridade com o original | 27/27 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3) | Compila; UI conferida via Standalone local (`FORJA_STANDALONE=ON`) |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Compressão | `compress` | 0,00 – 1,00 | 0,00 | Ganho de entrada (`10^(14·A/20)`) e fator de compressão |
| Saída | `output` | 0,00 – 1,00 | 0,50 | Trim do algoritmo (`B·2`) |
| Mistura | `mix` | 0,00 – 1,00 | 1,00 | Dry/wet interno |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

O bypass é exposto via `getBypassParameter()`, então automação e o botão da DAW
usam o mesmo parâmetro. Latência reportada: **0 amostras**.

## DSP

`Forja/Source/forja/dsp/ForjaTempera.h` transcreve `processDoubleReplacing` do
ButterComp2:

1. ruído "air" do Butter por canal (resíduo determinístico somado à entrada);
2. ganho de entrada e `compfactor = 0,012·A/135`;
3. envelope com divisor dependente de `lastOutput` do sample anterior;
4. quatro compressores paralelos — positivo/negativo × A/B — escolhidos em
   alternância por amostra (`flip`);
5. multiplicador combinado por canal, compensação de `outputgain` e trim;
6. dry/wet, atualização de `lastOutput`, dither `fpdL/fpdR` (semente 22222).

O adaptador acrescenta trim de bypass com rampa de 10 ms, medidores de pico e
o mínimo de GR do bloco. `process()` não aloca, não registra log e não bloqueia.

**Desvio documentado:** no original `noisesourceL/R` são `static` dentro do laço,
o que compartilha estado entre instâncias. Aqui são membros por instância,
inicializados em 0 e 850010. Fora isso o caminho de áudio é idêntico.

GR exibida: `min(totalmultiplierL, totalmultiplierR)` limitado a 1, em dB.

## Interface

- Janela fixa de 1000 × 540 px; topo FORJA / DE COMPRESSORES / TÊMPERA.
- Cards: Compressão (knob + medidor de GR), Saída (knob + peak meter),
  Movimento (histórico de GR de ~6 s), Mistura (knob + bypass).
- Badge de estado, SR, buffer, resumo de parâmetros e linha de DSP/latência na
  barra inferior; LED CLIP clicável no card de saída.
- Botão **FUNDAMENTOS** no topo abre o painel nativo (fechado por FECHAR, `Esc`
  ou clique fora). Sem WebView.

## Build (Windows)

A partir da raiz da suíte:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Tempera_VST3
```

Artefatos:

- `build/Airwindows/Dynamics/Tempera/Tempera_artefacts/Release/VST3/Tempera.vst3`

Para instalar, copie a pasta `Tempera.vst3` para
`C:\Program Files\Common Files\VST3\`.

## Licença e créditos

- DSP: **Airwindows ButterComp2** © Chris Johnson, licença MIT
  (commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`).
- Adaptador, interface e integração: Forja.
- Original preservado como referência em
  `Forja/Source/forja/vendor/airwindows/ButterComp2/` com `LICENSE` e
  `PROVENANCE.md` (hashes SHA-256). Não é compilado no plugin.
