# MINISAT — saturador de válvula de um knob (Forja)

Mini da base `Minis/`. Formato compacto (560 × 260), fachada `valve`, e funciona como
gaveta de instrumento no FORJARACK.

## Estado

| Item | Estado |
| --- | --- |
| Paridade com vendor | Não se aplica: o DSP é o adaptador do BRASA, já parity-testado |
| Teste próprio | `forja_minisat` (mapa, monotonia, bypass, latência, alocação, gaveta) |
| `alloc_guard` | Cobre o adaptador compartilhado `forja::BrasaDSP` |
| Latência | 0, medida por impulso no harness do BRASA |
| Build | VST3 (Windows, macOS, Linux) |

## Parâmetros

| Nome | Tipo | Faixa | Default | Nota |
| --- | --- | --- | --- | --- |
| `drive` | float | 0–1, passo 0,01 | 0,00 | move só o pad de entrada do BRASA |
| `bypass` | bool | — | `false` | crossfade de 10 ms, sem resetar estado |

`Válvula` fica em 0,50 e `Saída` em 0 dB, ambos fixos — são os defaults documentados do
BRASA, não controles deste plugin.

## DSP

O plugin **reutiliza o adaptador validado do BRASA** (Airwindows Tube2, MIT, Chris
Johnson) como bloco, através de `Forja/Source/forja/dsp/ForjaBrasa.h`, compartilhado entre
os dois. Nada de DSP forkado, nada de DSP novo.

A única camada Forja é o mapa do knob: `drive` sobe o pad de entrada original de 0,50
(equivalente a drive 0) até 1,00. Sem makeup gain, sem normalização, sem oversampling.

## Interface

Um knob DRIVE e a curva de transferência do Tube2 (reaproveitamento do componente do
BRASA), desenhada com os parâmetros mapeados. Medidor de saída com CLIP, barra de status e
painel FUNDAMENTOS no padrão da casa.

## Testes

`Tests/Source/` cobre o mapa `drive`→pad bit a bit contra o adaptador nos drives 0, 0,25,
0,50, 0,75 e 1; o amolecimento do timbre com o drive; a rampa de bypass com estado
preservado; latência 0 medida por impulso; zero alocação em estéreo e mono; e o registro da
gaveta `minisat` no FORJARACK.

## Licença e créditos

Interface, mapa do knob e chrome: Forja, MIT. O DSP vem do BRASA (Airwindows Tube2, © Chris
Johnson, MIT) — `Forja/Source/forja/vendor/airwindows/Tube2/` com `LICENSE` e
`PROVENANCE.md`; o crédito aparece no FUNDAMENTOS e na página pública.
