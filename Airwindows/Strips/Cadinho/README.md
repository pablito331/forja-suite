# CADINHO — strip de bus (X2Buss)

Strip de bus de quatro bandas com dinâmica de bandas, fader e ClipOnly3,
construído sobre o DSP do **Airwindows X2Buss** (MIT), com a interface do padrão
Forja. Projeto JUCE nativo (VST3 + Standalone), compilado a partir da raiz da
suíte e usando a biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::CadinhoDSP`) | Implementado |
| Paridade com o original | 70/70 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3 + Standalone) | Compila; UI conferida no Standalone |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Agudos | `high` | 0,00 – 1,00 | 0,50 | Ganho da banda alta (lei cúbica) |
| Médio alto | `highMid` | 0,00 – 1,00 | 0,50 | Ganho da banda médio-alta |
| Médio baixo | `lowMid` | 0,00 – 1,00 | 0,50 | Ganho da banda médio-baixa |
| Graves | `bass` | 0,00 – 1,00 | 0,50 | Ganho da banda grave |
| Freq. agudos | `highFreq` | 0,00 – 1,00 | 0,50 | Cruzamento e Q dos agudos |
| Freq. médio alto | `highMidFreq` | 0,00 – 1,00 | 0,50 | Cruzamento e Q do médio-alto |
| Freq. médio baixo | `lowMidFreq` | 0,00 – 1,00 | 0,50 | Cruzamento e Q do médio-baixo |
| Freq. graves | `bassFreq` | 0,00 – 1,00 | 0,50 | Cruzamento dos graves |
| Dinâmica | `thresh` | 0,00 – 1,00 | 1,00 | Compressão de bandas; 1,00 desliga |
| Fader | `fader` | 0,00 – 1,00 | 0,50 | Ganho de saída (0,50 = unitário) |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **1 amostra** (medida por impulso em 44,1/48/96/192 kHz).

## DSP

`Forja/Source/forja/dsp/ForjaCadinho.h` transcreve `processDoubleReplacing` do
X2Buss:

1. **Console**: saturação exponencial de razão áurea na entrada, com limite ±1;
2. **crossovers**: três biquadráticos em cascata por banda, com Q 2,247 / 0,802 /
   0,555, seguidos de um estágio de IIRs exponenciais;
3. **ganhos**: desvio em torno de 0,50 com lei cúbica (`1 + d·|d|·|d|`);
4. **SmoothEQ2/Dynamics2**: duas janelas de energia (rápida e lenta) e curva de
   Bézier para reduzir a soma acima do limiar;
5. **fader**: `fader·2`, suavizado ao longo do bloco;
6. **ClipOnly3**: limitador com suavização de slew, responsável pela latência de
   1 amostra;
7. dither omitida (só o RNG avança) e bypass suavizado.

O original guarda 18/34 posições no ClipOnly3, o que fica fora dos limites acima
de ~372 kHz com `spacing` alto. O port usa 33/65 para permanecer definido; a
paridade é verificada até 352,8 kHz, onde o original ainda está dentro da
memória. As tensões de Bézier começam em 1,0 (como no construtor do X2Buss).

## Interface

- Janela fixa de 1000 × 540 px; topo FORJA / DE CHANNEL STRIPS / CADINHO.
- Cards: Agudos (ganho e frequência), Médios (médio-alto e médio-baixo em grade
  2×2), Graves (ganho e frequência + peak meter) e Dinâmica e saída (dinâmica,
  fader, redução de ganho e bypass).
- Badge de estado, SR, buffer, resumo de parâmetros e linha de DSP/latência na
  barra inferior. FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Cadinho_VST3 Cadinho_Standalone
```

Artefatos:

- `build/Airwindows/Strips/Cadinho/Cadinho_artefacts/Release/VST3/Cadinho.vst3`
- `build/Airwindows/Strips/Cadinho/Cadinho_artefacts/Release/Standalone/Cadinho.exe`

## Licença e créditos

- DSP: **Airwindows X2Buss** © Chris Johnson, licença MIT
  (commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`).
- Adaptador, interface e integração: Forja.
- Original preservado em `Forja/Source/forja/vendor/airwindows/X2Buss/`
  com `LICENSE` e `PROVENANCE.md` (hashes SHA-256). Não é compilado no plugin.
