# ESCÓRIA — de-esser dinâmico (DeBess)

De-esser de transientes agudos construído sobre o DSP do **Airwindows DeBess**
(MIT), com a interface do padrão Forja. Projeto JUCE nativo (VST3 + Standalone),
compilado a partir da raiz da suíte e usando a biblioteca compartilhada
`forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| DSP portado (`forja::dsp::EscoriaDSP`) | Implementado |
| Paridade com o original | 44/44 casos, idêntico sample a sample em double |
| Guarda de alocação em `process()` | Passa |
| Projeto JUCE (VST3 + Standalone) | Compila; UI conferida no Standalone |
| Painel FUNDAMENTOS | Implementado |
| Validação em host (REAPER etc.) | Pendente |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Intensidade | `intense` | 0,00 – 1,00 | 0,00 | Teto da razão de redução; em 0 não há de-esser |
| Velocidade | `sharp` | 0,00 – 1,00 | 0,50 | Janela do detector, de 2 a 40 amostras |
| Profundidade | `depth` | 0,00 – 1,00 | 0,50 | Fundo da redução: 0 = máximo, 1 = nenhum |
| Filtro | `filter` | 0,00 – 1,00 | 0,50 | Ponto de separação dos agudos processados |
| Monitor | `sense` | 0,00 – 1,00 | 0,00 | Acima de 0,50 a saída toca só o removido |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

Latência reportada: **0 amostras**.

## DSP

`Forja/Source/forja/dsp/ForjaEscoria.h` transcreve `processDoubleReplacing` do
DeBess:

1. janela de derivadas de 2 a 40 amostras (`sharpness = sharp·40`);
2. derivadas das derivadas, cujo produto é o detector de transientes por canal;
3. razão de redução suavizada (`speed = 0,1 / sharpness`), limitada por
   `depth = 1/(depth+0,0001)` e alimentada por `intensity = intense⁵·8192/SR`;
4. filtro passa-baixos de um polo (`iirAmount = filter`); a redução age sobre a
   diferença entre a amostra e a versão filtrada;
5. alternância de dois conjuntos de estados a cada amostra (flip A/B);
6. em `sense > 0,49999`, a saída vira `entrada − processado` (só o removido);
7. dither omitida (só o RNG avança) e bypass suavizado.

O original tem uma particularidade preservada: no ramo B do flip, a condição
`if (ratioAL > 1.0)` usa a razão do ramo A enquanto divide pela razão do ramo B.
Como os dois ramos seguem o mesmo detector, a diferença é pequena, mas faz parte
do som.

## Interface

- Janela fixa de 1000 × 540 px; topo FORJA / DE DINÂMICA / ESCÓRIA.
- Cards: Intensidade (knob + peak meter + bypass), Tempo (velocidade e
  profundidade com notas) e Foco (filtro e monitor).
- Badge de estado, SR, buffer, resumo de parâmetros e linha de DSP/latência na
  barra inferior. FUNDAMENTOS abre o painel nativo (sem WebView).

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Escoria_VST3 Escoria_Standalone
```

Artefatos:

- `build/Airwindows/Dynamics/Escoria/Escoria_artefacts/Release/VST3/Escoria.vst3`
- `build/Airwindows/Dynamics/Escoria/Escoria_artefacts/Release/Standalone/Escoria.exe`

## Licença e créditos

- DSP: **Airwindows DeBess** © Chris Johnson, licença MIT
  (commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`).
- Adaptador, interface e integração: Forja.
- Original preservado em `Forja/Source/forja/vendor/airwindows/DeBess/`
  com `LICENSE` e `PROVENANCE.md` (hashes SHA-256). Não é compilado no plugin.
