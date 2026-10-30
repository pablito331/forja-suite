# FÔLEGO — ambiente de clube (kStation)

Reverb de ambiente pequeno com reflexões iniciais 3×3, corpo de seis vias por
canal e dois estágios de subamostragem por Bézier, construído sobre o DSP do
**Airwindows kStation** (MIT), com a interface do padrão Forja. Projeto JUCE
nativo (VST3 + Standalone), compilado a partir da raiz da suíte e usando a
biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::FolegoDSP`) | Implementado |
| Paridade com o original | 49/49 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3 + Standalone) | Compila; UI conferida no Standalone |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Regeneração | `regen` | 0,00 – 1,00 | 0,50 | `(1 − (1 − A)²) · fdb6ck` |
| Derez | `derez` | 0,00 – 1,00 | 0,50 | Subamostragem do corpo; 0,50 = fronteira dos modos |
| Filtro | `filter` | 0,00 – 1,00 | 0,50 | Bézier interno dedicado ao filtro |
| Reflexões | `early` | 0,00 – 1,00 | 0,50 | Peso das reflexões iniciais |
| Posição | `position` | 0,00 – 1,00 | 0,90 | `start = int(E · 27)` na tabela do clube |
| Mistura | `mix` | 0,00 – 1,00 | 0,50 | Seco/molhado linear; 1,00 = só ambiente |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras**. O tempo interno do clube é musical; não
entra no PDC.

## DSP

`Forja/Source/forja/dsp/ForjaFolego.h` transcreve `processDoubleReplacing` do
kStation:

1. rede 3×3 de reflexões iniciais por canal, com tempos da tabela `early[]`
   (1–93 ms no comentário do original, clube de 272 lugares);
2. corpo de realimentação 6×6 por canal em cinco estágios (36 linhas por
   canal);
3. Bézier externo do corpo (`derez`), com modo em degraus até 0,50 e modo
   contínuo acima;
4. Bézier interno do filtro (`filter`), aplicado por dentro da subamostragem;
5. mistura seca/molhada linear e injeção de ruído de dither na entrada;
6. dither de 64 bits omitida (só o RNG avança) e bypass suavizado.

Desvios do adaptador:

- as 88 linhas de atraso ficam no heap (`std::vector`) para caber na pilha do
  harness; a alocação acontece só na construção;
- metering de pico de entrada/saída e leitura de clipe;
- bypass com crossfade de 10 ms;
- hook de teste `setNoiseState()` para semear o RNG como o `rand()` original.

## Interface

- Janela fixa de 1000 × 540 px; topo FORJA / DE AMBIENTE / FÔLEGO.
- Cards: Clube (nota), Corpo (regeneração e derez), Filtro e Reflexões
  (filtro, reflexões, posição e nota) e Saída (mistura, peak meter + bypass).
- Badge de estado, SR, buffer, resumo de parâmetros e linha de DSP/PDC na barra
  inferior. FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Folego_VST3 Folego_Standalone
```

Artefatos:

- `build/Space/Folego/Folego_artefacts/Release/VST3/Folego.vst3`
- `build/Space/Folego/Folego_artefacts/Release/Standalone/Folego.exe`

## Licença e créditos

- DSP: **Airwindows kStation** © Chris Johnson / Airwindows, licença MIT
  (commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`).
- Adaptador, interface e integração: Forja.
- Original preservado em `Forja/Source/forja/vendor/airwindows/kStation/`
  com `LICENSE` e `PROVENANCE.md` (hashes SHA-256). Não é compilado no plugin.
