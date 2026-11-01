# BRASA — Saturador de válvula (Forja)

Plugin de saturação de válvula com DSP do **Airwindows Tube2** (MIT) e interface
no padrão visual Forja. Este diretório contém o projeto nativo JUCE (VST3 +
Standalone). O protótipo web com o mesmo DSP e a mesma UI está em
`../../Brasa-Concept-v1/`. O chrome visual (tema, cards, knobs, medidores, topo,
status e painel Fundamentos) vem da biblioteca compartilhada `forja_ui`, em
`../../Forja/`.

## Estado

| Item | Estado |
| --- | --- |
| Protótipo web (HTML/CSS + AudioWorklet) | Implementado e validado |
| DSP portado para C++ (`forja::BrasaDSP`) | Implementado e conferido contra o protótipo (8 casos, resultado idêntico) |
| Projeto JUCE (VST3 + Standalone) | Compila; UI aberta e conferida no Standalone |
| Validação em host (REAPER etc.) | Pendente |
| Métricas/CLIP, curva de transferência, válvula animada | Implementados |
| Painel FUNDAMENTOS (operação, controles, origem/licença) | Implementado |
| Presets de fábrica | Não — apenas o estado padrão |

## Parâmetros

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Entrada | `input` | 0,00 – 1,00 | 0,50 | Pad de entrada do Tube2 |
| Válvula | `tube` | 0,00 – 1,00 | 0,50 | Drive/caráter do Tube2 |
| Saída | `output` | −24,0 – +24,0 dB | 0,0 dB | Trim Forja pós-válvula |
| Bypass | `bypass` | on/off | off | Crossfade seco/molhado de 10 ms |

O bypass é exposto ao host via `getBypassParameter()`, portanto automação e o
botão de bypass da DAW usam o mesmo parâmetro. A latência reportada é 0 amostras.

## DSP

`Forja/Source/forja/dsp/ForjaBrasa.h` (biblioteca compartilhada) porta fielmente
`Tube2::processDoubleReplacing`:

1. pad de entrada (`input`);
2. média simples em sample rates > 1,9 × 44,1 kHz;
3. modelador assimétrico (base achatada, topo pontudo);
4. iterações de potência controladas por `powerfactor` (`tube`);
5. segunda média em sample rates altos;
6. histerese/slew, clamp em ±0,52 e ganho de 1,923…;
7. dither de 32 bits com o mesmo `fpdL/fpdR` inicial (22222).

Depois da válvula entram dois estágios Forja: trim de saída (com rampa de 10 ms)
e bypass por crossfade (também 10 ms). O adaptador mede peaks de entrada/saída e
latches de clip para a UI. `process()` não aloca, não registra log e não bloqueia.

O port foi conferido contra `../../Brasa-Concept-v1/js/brasa-dsp.js` com oito casos
(senoide, DC, silêncio, bypass, trim, tube 0 e 1, 44,1/48/96 kHz): RMS, peaks,
média e última amostra batem casa decimal a casa decimal.

O código original do Airwindows fica em
`Source/forja/vendor/airwindows/Tube2/` apenas como referência de proveniência
(commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`, MIT — ver `LICENSE`). Ele não
é compilado porque depende do SDK VST2.

## Interface

- Janela fixa de 1000 × 540 px.
- Cards: Entrada, Válvula, Curva de transferência, Saída.
- Medidores peak L/R com hold de 900 ms, escala 0…−60 dBFS e LED CLIP clicável.
- Curva estática da válvula (sem histerese) atualizada em tempo real.
- Válvula com filamento que esquenta conforme o sinal de entrada e o drive.
- Botão **FUNDAMENTOS** no topo: abre um painel nativo (sem WebView) com o que o
  plugin faz, como o algoritmo funciona, o que cada controle faz e a origem
  Airwindows/MIT. Fecha pelo botão FECHAR, por `Esc` ou clicando fora da folha.
- Duplo clique no knob = padrão; setas = ajuste fino; roda = passo do parâmetro.

## Build (Windows)

O build é feito pela raiz da suíte, que baixa o JUCE 9.0.4 uma única vez e
compila `forja_ui` + os quatro plugins + os testes.

Requisitos:

- CMake ≥ 3.22;
- Visual Studio 2022 Build Tools com a carga **Desenvolvimento para desktop com C++**
  (MSVC v143 + Windows SDK);
- conexão com a internet na primeira configuração (JUCE 9.0.4 via FetchContent).

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Artefatos:

- `build/Airwindows/Saturation/Brasa/Brasa_artefacts/Release/VST3/Brasa.vst3`
- `build/Airwindows/Saturation/Brasa/Brasa_artefacts/Release/Standalone/Brasa.exe`

Para instalar o VST3, copie a pasta `Brasa.vst3` inteira para
`C:\Program Files\Common Files\VST3\`.

## Estrutura

```
Airwindows/Saturation/Brasa/
  CMakeLists.txt
  Source/
    PluginProcessor.h/.cpp     APVTS, bypass do host, atomics de medidores
    PluginEditor.h/.cpp        layout 1000x540, cards e integração do painel
    ForjaInfoText.h            conteúdo do painel FUNDAMENTOS
    forja/
      ui/                      curva de transferência e válvula (específicos)
      vendor/airwindows/Tube2/ original MIT (referência, não compilado)
```

O adaptador do Tube2 mora na biblioteca compartilhada:
`Forja/Source/forja/dsp/ForjaBrasa.h` (MINISAT o reutiliza como bloco interno).

Componentes visuais compartilhados (tema, card, knob, LookAndFeel, medidores,
topo, status e painel Fundamentos) ficam em `../../Forja/Source/forja/ui/`.

## Licença e créditos

- DSP: **Airwindows Tube2** © Chris Johnson, licença MIT.
- Interface, adaptador e integração: Forja.
- O texto da licença MIT do Tube2 está preservado em
  `Source/forja/vendor/airwindows/Tube2/LICENSE`.
