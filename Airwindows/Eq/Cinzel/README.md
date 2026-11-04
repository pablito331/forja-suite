# CINZÉL — EQ paramétrico dinâmico (Parametric + camada Forja)

Equalizador paramétrico de três bandas construído sobre o DSP do **Airwindows
Parametric** (MIT), com uma camada dinâmica por banda exclusiva da Forja. Projeto
JUCE nativo (VST3), compilado a partir da raiz da suíte e usando a
biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::CinzelDSP`) | Implementado |
| Paridade com o original (`dyn = 0`) | 66/66 casos, idêntico sample a sample em double |
| Comportamento dinâmico (`dyn = 0,5`) | Rajada de 1 kHz na banda alvo: −12 dB; banda vizinha: 0,00 dB |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3) | Compila; UI conferida via Standalone local (`FORJA_STANDALONE=ON`) |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Freq. alta | `highFreq` | 0,00 – 1,00 | 0,50 | Centro da banda alta (1,5–16 kHz, curva cúbica) |
| Ganho alto | `highGain` | 0,00 – 1,00 | 0,50 | Ganho da banda alta (0,5 = neutro) |
| Ressonância alta | `highReso` | 0,00 – 1,00 | 0,50 | Foco da banda alta |
| Freq. média alta | `highMidFreq` | 0,00 – 1,00 | 0,50 | Centro da banda média alta (600 Hz–7 kHz) |
| Ganho médio alto | `highMidGain` | 0,00 – 1,00 | 0,50 | Ganho da banda média alta |
| Ressonância média alta | `highMidReso` | 0,00 – 1,00 | 0,50 | Foco da banda média alta |
| Freq. média baixa | `lowMidFreq` | 0,00 – 1,00 | 0,50 | Centro da banda média baixa (20 Hz–2,2 kHz) |
| Ganho médio baixo | `lowMidGain` | 0,00 – 1,00 | 0,50 | Ganho da banda média baixa |
| Ressonância média baixa | `lowMidReso` | 0,00 – 1,00 | 0,50 | Foco da banda média baixa |
| Mistura | `mix` | 0,00 – 1,00 | 1,00 | Dry/wet do EQ |
| Dinâmica | `dyn` | 0,00 – 1,00 | 0,00 | Camada dinâmica por banda (Forja) |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

O bypass é exposto via `getBypassParameter()`. Latência reportada: **0 amostras**.

## DSP

`Forja/Source/forja/dsp/ForjaCinzel.h` transcreve `processDoubleReplacing` do
Parametric:

1. cada banda aplica três biquads em cascata, com ressonâncias 1,93185165 /
   0,70710678 / 0,51763809 e coeficientes dependentes de frequência e
   ressonância;
2. o ganho da banda segue `level = (nonlin·2) − 1`, com realces dobrados;
3. as três bandas são somadas e adicionadas ao sinal seco, com `mix`;
4. dither `fpdL/fpdR` (semente 22222) e ruído de desnormalização como no original.

Camada Forja (`dyn > 0`): detector estéreo-linkado `max(|L|,|R|)` na saída de
cada banda, envelope de ataque 10 ms e liberação 150 ms, limiar
`−6 − 30·dyn` dBFS e redução de até 12 dB. Realces encolhem na direção de 0 dB e
cortes ficam mais profundos; o ganho é suavizado em 10 ms. Com `dyn = 0` o bloco
inteiro é ignorado, garantindo paridade bit-exata.

**Desvio documentado:** a camada dinâmica não existe na Airwindows. O caminho
estático é fiel; `dyn > 0` é comportamento Forja.

## Interface

- Janela fixa de 1000 × 540 px; topo FORJA / DE EQUALIZADORES / CINZÉL.
- Cards: Curva (curva por FFT do próprio filtro + peak meter + CLIP),
  Bandas (3 × 3 knobs: frequência, ganho e ressonância por banda) e
  Dinâmica (mistura, dinâmica, medidor de redução e bypass).
- A curva é medida em nível baixo (−60 dBFS) e rotulada **curva estática ·
  rta na entrada · dyn = 0**; os nós A / MA / MB indicam os centros das bandas em tempo real e
  são arrastáveis: horizontal muda a frequência, vertical muda o ganho da banda.
- Atrás da curva, o **RTA de entrada** (medição Forja) pinta em tempo real o
  espectro do sinal antes do EQ: 32 bandas log, FFT própria em janela de 2048
  amostras, sem alocação no caminho de áudio e sem alterar o som.
- O chip REDES aparece no topo quando `forja::ui::kSocialUrl` está preenchido.
- Badge de estado, SR, buffer, resumo de parâmetros e linha de DSP/latência na
  barra inferior. FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Cinzel_VST3
```

Artefatos:

- `build/Airwindows/Eq/Cinzel/Cinzel_artefacts/Release/VST3/Cinzel.vst3`

## Licença e créditos

- DSP base: **Airwindows Parametric** © Chris Johnson, licença MIT
  (commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`).
- Camada dinâmica, adaptador, interface e integração: Forja.
- Original preservado em `Forja/Source/forja/vendor/airwindows/Parametric/`
  com `LICENSE` e `PROVENANCE.md` (hashes SHA-256). Não é compilado no plugin.
