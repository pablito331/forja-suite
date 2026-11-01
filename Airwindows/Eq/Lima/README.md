# LIMA — EQ de bus (SmoothEQ2)

Equalizador de quatro bandas para bus e masterização construído sobre o DSP do
**Airwindows SmoothEQ2** (MIT), com a interface do padrão Forja. Projeto JUCE
nativo (VST3), compilado a partir da raiz da suíte e usando a
biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::LimaDSP`) | Implementado |
| Paridade com o original | 55/55 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3) | Compila; UI conferida via Standalone local (`FORJA_STANDALONE=ON`) |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Banda alta | `high` | 0,00 – 1,00 | 0,50 | Ganho da banda aguda |
| Banda média alta | `highMid` | 0,00 – 1,00 | 0,50 | Ganho da banda média alta |
| Banda média baixa | `lowMid` | 0,00 – 1,00 | 0,50 | Ganho da banda média baixa |
| Banda grave | `bass` | 0,00 – 1,00 | 0,50 | Ganho da banda de graves |
| Frequência alta | `highFreq` | 0,00 – 1,00 | 0,50 | Centro/abertura da banda alta |
| Frequência média alta | `highMidFreq` | 0,00 – 1,00 | 0,50 | Centro da banda média alta |
| Frequência média baixa | `lowMidFreq` | 0,00 – 1,00 | 0,50 | Centro da banda média baixa |
| Frequência grave | `bassFreq` | 0,00 – 1,00 | 0,50 | Centro da banda grave |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Ganho segue a lei original `1 + x·|x|·|x|` com `x = (valor − 0,5)·2`: 0,5 é
neutro, acima sobe até +6 dB e abaixo corta. Latência reportada:
**0 amostras**.

## DSP

`Forja/Source/forja/dsp/ForjaLima.h` transcreve `processDoubleReplacing` do
SmoothEQ2:

1. três estágios de crossover idênticos (A, B, C), cada um com três biquads
   (alta, média e baixa) com ressonâncias 2,24697960 / 0,80193774 / 0,55495813;
2. um estágio exponencial (um polo) por banda completa a separação;
3. as quatro partes são somadas com os ganhos de banda;
4. frequências e Qs são calculados a partir dos quatro controles de frequência,
   que se influenciam mutuamente;
5. dither `fpdL/fpdR` e bypass suavizado. Não há saturação.

## Interface

- Janela fixa de 1000 × 540 px; topo FORJA / DE EQUALIZADORES / LIMA.
- Cards: Curva (curva por FFT do próprio filtro + peak meter + CLIP),
  Ganhos (4 knobs) e Frequências (4 knobs + bypass).
- A curva é medida em nível baixo (−60 dBFS) e rotulada **curva estática ·
  4 bandas**; os nós A / M / G marcam os três crossovers e são arrastáveis na
  horizontal para reposicionar as frequências (os knobs acompanham).
- Badge de estado, SR, buffer, resumo de parâmetros e linha de DSP/latência na
  barra inferior. FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Lima_VST3
```

Artefatos:

- `build/Airwindows/Eq/Lima/Lima_artefacts/Release/VST3/Lima.vst3`

## Licença e créditos

- DSP: **Airwindows SmoothEQ2** © Chris Johnson, licença MIT
  (commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`).
- Adaptador, interface e integração: Forja.
- Original preservado em `Forja/Source/forja/vendor/airwindows/SmoothEQ2/`
  com `LICENSE` e `PROVENANCE.md` (hashes SHA-256). Não é compilado no plugin.
