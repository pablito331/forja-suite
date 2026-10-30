# TORNO — shelves de tom (Baxandall3)

Equalizador de tom de duas bandas construído sobre o DSP do **Airwindows
Baxandall3** (MIT), com a interface do padrão Forja. Projeto JUCE nativo
(VST3 + Standalone), compilado a partir da raiz da suíte e usando a biblioteca
compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::TornoDSP`) | Implementado |
| Paridade com o original | 30/30 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3 + Standalone) | Compila; UI conferida no Standalone |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Entrada | `input` | 0,00 – 1,00 | 0,50 | Ganho de entrada e intensidade do Console5 |
| Agudos | `treble` | 0,00 – 1,00 | 0,50 | Shelf de agudos (frequência sobe com o ganho) |
| Graves | `bass` | 0,00 – 1,00 | 0,50 | Shelf de graves (frequência desce com o ganho) |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

O algoritmo só é neutro com os três controles em 0,50. O botão **PLANO** devolve
os três ao padrão de uma vez. Latência reportada: **0 amostras**.

## DSP

`Forja/Source/forja/dsp/ForjaTorno.h` transcreve `processDoubleReplacing` do
Baxandall3:

1. codificação senoidal (Console5) com ganho `(input·2)²`;
2. dois filtros shelving de segunda ordem com Q de Bessel (0,57735026919), um
   para agudos e outro para graves, alternando a cada amostra entre dois
   conjuntos de estados (A/B);
3. ganhos `(treble·2)²` e `(bass·2)²` aplicados às parcelas filtradas;
4. decodificação por `asin` limitada a ±0,99999;
5. dither `fpdL/fpdR` e bypass suavizado.

O acoplamento entre ganho e frequência de cada shelf é original e não foi
"corrigido". A saturação do Console5 também é original, por isso TORNO não tem
curva desenhada na UI.

## Interface

- Janela fixa de 1000 × 540 px; topo FORJA / DE EQUALIZADORES / TORNO.
- Cards: Entrada (knob + bypass), Agudos (knob + nota de neutro),
  Graves (knob + nota + PLANO) e Saída (peak meter + CLIP).
- Badge de estado, SR, buffer, resumo de parâmetros e linha de DSP/latência na
  barra inferior. FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Torno_VST3 Torno_Standalone
```

Artefatos:

- `build/Eq/Torno/Torno_artefacts/Release/VST3/Torno.vst3`
- `build/Eq/Torno/Torno_artefacts/Release/Standalone/Torno.exe`

## Licença e créditos

- DSP: **Airwindows Baxandall3** © Chris Johnson, licença MIT
  (commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`).
- Adaptador, interface e integração: Forja.
- Original preservado em `Forja/Source/forja/vendor/airwindows/Baxandall3/`
  com `LICENSE` e `PROVENANCE.md` (hashes SHA-256). Não é compilado no plugin.
