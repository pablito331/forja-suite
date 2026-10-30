# BIGORNA — channel strip completo (CStrip2)

Channel strip com EQ de três bandas, filtros passa-baixos/passa-altos, compressor
e saída Spiral, construído sobre o DSP do **Airwindows CStrip2** (MIT), com a
interface do padrão Forja. Projeto JUCE nativo (VST3 + Standalone), compilado a
partir da raiz da suíte e usando a biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::BigornaDSP`) | Implementado |
| Paridade com o original | 69/69 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3 + Standalone) | Compila; UI conferida no Standalone |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Agudos | `treble` | 0,00 – 1,00 | 0,50 | Ganho da banda alta, −6 a +6 dB |
| Médios | `mid` | 0,00 – 1,00 | 0,50 | Ganho da banda média |
| Graves | `bass` | 0,00 – 1,00 | 0,50 | Ganho da banda grave |
| Freq. agudos | `trebFreq` | 0,00 – 1,00 | 0,50 | Split agudos/médios do crossover |
| Freq. graves | `bassFreq` | 0,00 – 1,00 | 0,50 | Split médios/graves do crossover |
| Passa-baixos | `lowCap` | 0,00 – 1,00 | 1,00 | Corta agudos; 1,00 = aberto |
| Passa-altos | `hiCap` | 0,00 – 1,00 | 0,00 | Corta graves; 0,00 = aberto |
| Compressão | `compres` | 0,00 – 1,00 | 0,00 | Ganho de entrada do ButterComp |
| Velocidade | `compSpd` | 0,00 – 1,00 | 0,00 | Tempo do envelope do compressor |
| Saída | `output` | 0,00 – 1,00 | 0,33 | Ganho de saída e acionamento do Spiral |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras**. Padrão neutro: 0,50/0,50/0,50, 1,00, 0,00,
0,00, 0,00, 0,33.

## DSP

`Forja/Source/forja/dsp/ForjaBigorna.h` transcreve `processDoubleReplacing` do
CStrip2, estágio por estágio:

1. **capacitor/gearbox**: três polos de passa-altos e três de passa-baixos,
   revezados entre seis conjuntos de estados por um contador de 0 a 5;
2. **ButterComp**: detector de picos/vales com mistura de razão áurea,
   `inputgain = compres⁴·35+1`, `divisor = 0,008·compSpd²+0,0004` escalado por
   SR/44100; só processa quando `inputgain > 1`;
3. **EQ de três bandas**: crossover por IIRs de um polo em `trebFreq`/`bassFreq`;
   ganhos de −6 a +6 dB convertidos para multiplicador; drive seno/cosseno por
   banda; alternância entre três conjuntos de estados (`flipthree` 1–3);
4. **saída**: `outputgain = output·3`; acima de ~0,333 o Spiral entra
   (`sin(x·|x|)`) com camada extra de "phat" (`sin(x·π/2)`);
5. dither omitida (só o RNG avança) e bypass suavizado.

O original usa duas escalas diferentes: `overallscale` é a taxa de amostragem
bruta nos IIRs e `compscale` é SR/44100 no compressor. As duas foram preservadas
exatamente. O mostrador do original anuncia ±12 dB nos ganhos de EQ, mas a lei
interna é ±6 dB (`A·12−6`); o adaptador mantém a lei.

## Interface

- Janela fixa de 1000 × 540 px; topo FORJA / DE CHANNEL STRIPS / BIGORNA.
- Cards: Tom (agudos, médios, graves), Frequências (splits), Filtros e saída
  (passa-baixos, passa-altos, saída + peak meter) e Compressor (compressão,
  velocidade, redução de ganho e bypass).
- Badge de estado, SR, buffer, resumo de parâmetros e linha de DSP/latência na
  barra inferior. FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Bigorna_VST3 Bigorna_Standalone
```

Artefatos:

- `build/Strips/Bigorna/Bigorna_artefacts/Release/VST3/Bigorna.vst3`
- `build/Strips/Bigorna/Bigorna_artefacts/Release/Standalone/Bigorna.exe`

## Licença e créditos

- DSP: **Airwindows CStrip2** © Chris Johnson, licença MIT
  (commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`).
- Adaptador, interface e integração: Forja.
- Original preservado em `Forja/Source/forja/vendor/airwindows/CStrip2/`
  com `LICENSE` e `PROVENANCE.md` (hashes SHA-256). Não é compilado no plugin.
