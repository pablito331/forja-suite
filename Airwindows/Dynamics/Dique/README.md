# DIQUE — Clipe de pico extremo (FinalClip)

Proteção de picos extremos construída sobre o DSP do **Airwindows FinalClip**
(MIT) com a interface do padrão Forja. Projeto JUCE nativo (VST3)
compilado a partir da raiz da suíte, usando a biblioteca compartilhada `forja_ui`.

O DIQUE **não é um limitador de 0 dBFS**: o FinalClip só age acima de
**+4,18 dBFS** (pico linear 1,618). Sinais até 0 dBFS passam inalterados. Para
limitar em 0 dBFS use o MURALHA (ADClip9); o DIQUE serve para picos que ainda
ultrapassem +4,18 dBFS.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::DiqueDSP`) | Implementado |
| Paridade com o original | 17/17 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3) | Compila; UI conferida via Standalone local (`FORJA_STANDALONE=ON`) |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

O bypass é exposto via `getBypassParameter()`. Latência reportada:
**1 amostra** (ver desvios).

## Faixa de atuação

Medição no adaptador (`ForjaDique.h`), seno de 1 kHz a 48 kHz, pico na segunda
metade do buffer:

| Entrada | Saída |
| --- | --- |
| −18 dBFS | −18,00 dBFS |
| 0 dBFS | 0,00 dBFS |
| +3 dBFS | +3,00 dBFS |
| +6 dBFS | +4,18 dBFS |
| +12 dBFS | +4,18 dBFS |
| +18 dBFS | +4,18 dBFS |

O teto assintótico é o pico 1,618 do ClipOnly2, ou seja **+4,18 dBFS**. Por
isso o DIQUE é um clipe de segurança para picos extremos: em material que já
está em 0 dBFS ele é transparente. A redução só aparece no medidor quando a
entrada ultrapassa o gatilho.

## DSP

`Forja/Source/forja/dsp/ForjaDique.h` transcreve `processDoubleReplacing` do
FinalClip:

1. pré-clipe de cada amostra em **±4**;
2. limite de slew de **0,618033988749894** em relação à amostra anterior;
3. estágio **ClipOnly2** por canal, com detecção de clipe positivo/negativo,
   reconstrução da transição e fila `intermediate`;
4. sem ganho, sem mix e sem RNG — o processamento é determinístico.

O adaptador acrescenta bypass com rampa de 10 ms e medidores. `process()` não
aloca, não registra log e não bloqueia.

**Desvios documentados:**

- O original dimensiona `intermediate[16]` mas escreve no índice `spacing`
  (até 16). O adaptador usa **18 posições**, eliminando o acesso fora dos
  limites em sample rates altos (705,6 kHz coberto no teste).
- Latência real de 1 amostra. O caminho seco do bypass também é atrasado 1
  amostra para não virar comb filter durante o crossfade.
- Estado por instância; nenhum `static` de áudio.

GR exibida: `abs(saída)/abs(entrada)` do canal com maior redução, em dB.

## Interface

- Janela compacta de 560 × 260 px; topo FORJA / PROTEÇÃO FINAL / DIQUE.
- Card único com peak meter, nota explicativa e bypass; LED CLIP clicável.
- Badge de estado, SR, buffer e linha de DSP/latência na barra inferior.
- Botão **FUNDAMENTOS** no topo abre o painel nativo (fechado por FECHAR, `Esc`
  ou clique fora). Sem WebView.

## Build (Windows)

A partir da raiz da suíte:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Dique_VST3
```

Artefatos:

- `build/Airwindows/Dynamics/Dique/Dique_artefacts/Release/VST3/Dique.vst3`

Para instalar, copie a pasta `Dique.vst3` para
`C:\Program Files\Common Files\VST3\`.

## Licença e créditos

- DSP: **Airwindows FinalClip** © Chris Johnson, licença MIT
  (commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`).
- Adaptador, interface e integração: Forja.
- Original preservado como referência em
  `Forja/Source/forja/vendor/airwindows/FinalClip/` com `LICENSE` e
  `PROVENANCE.md` (hashes SHA-256). Não é compilado no plugin.
