# TRACO — EQ gráfico (engine Forja)

Equalizador paramétrico de controle primário pelo gráfico: passa-alto e passa-baixa de
esquina nas pontas, cinco bandas flexíveis e um de-esser dedicado na sexta, com RTA de
entrada e de saída desenhados atrás da curva composta. Clicar num nó da curva abre o painel
de controles daquela seção no centro embaixo do gráfico — o fluxo de operação do Pro-Q. É a
sexta base da suíte e a segunda de **DSP 100% próprio** depois do Drumforge: o engine, o
grafo interativo e a interface foram escritos para a Forja, sem paridade com a Airwindows.
Projeto JUCE nativo (VST3), compilado a partir da raiz com `forja_add_plugin` e a biblioteca
compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| Engine (`forja::traco::TracoEngine`) | Implementado: cascata série de biquadros RBJ, processamento em double, rampa de 10 ms |
| De-esser | Implementado: detector por diferença de envelope na banda 4–12 kHz, redução até −24 dB, escuta isolada |
| Processor | APVTS de 42 parâmetros, medidores atômicos, bypass com crossfade, round-trip de estado |
| Interface | Grafo interativo em `ForjaEqGraph` + painel contextual por seção + coluna de medidores (in/out) |
| Painel FUNDAMENTOS | Implementado |
| Testes (`forja_traco`, `forja_traco_processor`) | PASS: resposta por tom, cascata, inclinações, de-esser, latência, zero alocação e costura ponta a ponta |
| Latência | 0 amostras (medida por impulso no harness) |
| Validação em host (REAPER etc.) | Pendente · não há DAW no ambiente |

Este plugin **não** tem teste de paridade: não existe original de terceiro para comparar. O
que é verificado é o comportamento do engine — resposta medida por tom, cascatas, inclinações
de esquina, a redução do de-esser e a curva composta que a UI lê — e a ausência de alocação no
caminho de áudio.

## Origem do DSP

Os coeficientes de cada biquad vêm do **Audio EQ Cookbook** de Robert Bristow-Johnson (as
fórmulas são de domínio público; nenhuma linha de código de terceiros entra aqui). A cadeia é
fixa e séria:

```
HP → LP → banda 1 → banda 2 → banda 3 → banda 4 → banda 5 → de-esser
```

Cada bloco é um `Biquad` com os coeficientes RBJ recalculados só quando o parâmetro muda,
com troca suavizada por rampa de 10 ms (`ForjaSmoother`), para o clique de coeficiente não
aparecer no áudio. O processamento interno é em `double`; as amostras entram e saem em
`float`. Não há oversampling nem Mid/Side nesta primeira camada — ficam para fatias futuras.

O **de-esser** compara o envelope rápido da banda com a média lenta da mesma banda: sibilo é
excesso relativo, não nível absoluto — o conceito de diferença filtrada já documentado no
ESCÓRIA (Airwindows DeBess), reimplementado do zero como camada Forja, sem lookahead. A
latência medida por impulso é 0.

A **curva** vista no gráfico não é uma segunda fonte de verdade: `TracoEngine` expõe um
`compositeDbAt (Params, sampleRate, freq)` estático que reproduz exatamente a resposta do
engine, e a UI o amostra para desenhar a linha. Assim o gráfico nunca duplica a matemática do
filtro nem toca no engine vivo da thread de áudio.

A medição **RTA** (entrada e saída) reusa o `ForjaSpectrum` da camada de medição da suíte — o
mesmo validado no CINZÉL e na LIMA. Medição não altera o som e segue ativa no bypass.

## Componente novo de UI

`Forja/Source/forja/ui/ForjaEqGraph.h/.cpp` — grafo de EQ interativo genérico (biblioteca
`forja_ui`): grade log 20 Hz–20 kHz, ±18 dB, nós com glifo por tipo, arrasto muda frequência e
ganho, `Shift`-arrasto/scroll muda a largura (Q) quando a banda permite, duplo-clique reseta,
clique com botão direito abre o menu de tipo e clicar no fundo deseleciona. O nó selecionado
vem em destaque ember; os demais, muted. Desenha o RTA de entrada como fundo e a curva
composta por cima. É reutilizável: recebe os nós e um `responseSampler`, e devolve callbacks —
não conhece o APVTS nem o TRACO.

## Parâmetros

Todos os parâmetros contínuos cruzam a fronteira do APVTS normalizados 0–1 (passo 0,01); a
unidade física é resolvida dentro do engine pelas funções `scale*`.

| Parâmetro | ID | Faixa física | Padrão | Função |
| --- | --- | --- | --- | --- |
| Bypass | `bypass` | on/off | off | Sinal seco com rampa de 10 ms; estado do filtro nunca é resetado |
| HP liga | `hpOn` | on/off | off | Habilita o passa-alto de esquina |
| HP frequência | `hpFreq` | 20 Hz – 20 kHz (log) | 0,06 (≈ 30 Hz) | Frequência de corte do passa-alto |
| HP inclinação | `hpSlope` | 12 / 24 dB/oct | 12 dB/oct | Um ou dois estágios em cascata, Q fixo de Butterworth |
| LP liga | `lpOn` | on/off | off | Habilita o passa-baixa de esquina |
| LP frequência | `lpFreq` | 20 Hz – 20 kHz (log) | 0,95 (≈ 14,1 kHz) | Frequência de corte do passa-baixa |
| LP inclinação | `lpSlope` | 12 / 24 dB/oct | 12 dB/oct | Um ou dois estágios em cascata, Q fixo de Butterworth |
| Banda N liga | `bNOn` | on/off | só b1 | Habilita a banda (N = 1 a 5) |
| Banda N tipo | `bNType` | 6 posições (passo 0,2) | campainha | Campainha, prateleira baixa, prateleira alta, passa-alta, passa-baixa, notch |
| Banda N frequência | `bNFreq` | 20 Hz – 20 kHz (log) | 0,57 (≈ 1,03 kHz) | Frequência central da banda |
| Banda N ganho | `bNGain` | −18 a +18 dB | 0,50 (0 dB) | Ganho de pico/shelf |
| Banda N Q | `bNQ` | 0,1 a 10 (log) | 0,50 (Q 1,0) | Largura da banda |
| Banda N ouvir | `bNListen` | on/off | off | Escuta isolada do que a banda muda no sinal |
| De-esser liga | `essOn` | on/off | off | Habilita a sexta banda dedicada |
| De-esser centro | `essFreq` | 4 kHz – 12 kHz | 0,31 (≈ 6,5 kHz) | Centro da banda-passa sibilante |
| De-esser largura | `essWidth` | Q 0,3 – 3 (log) | 0,50 (Q ≈ 0,95) | Largura da banda-passa |
| De-esser alcance | `essRange` | 0 – 24 dB | 0,38 (≈ 9 dB) | Teto da redução aplicada à faixa sibilante |
| De-esser ouvir | `essListen` | on/off | off | Escuta isolada da banda tratada |

São **42 parâmetros** no total: 1 bypass, 3 no HP, 3 no LP, 6 em cada uma das 5 bandas (30) e
5 no de-esser. Os HP/LP têm Q fixo de Butterworth (0,707), por isso só a frequência é editável.

## Interface

- Canvas de projeto 1000 × 540, redimensionável de 70% a 200% com aspecto fixo (`ForjaCanvas`).
- Grafo `ForjaEqGraph` ocupa a área principal; à direita, uma coluna com o medidor de entrada e
  o de saída (pico/CLIP). O RTA de entrada aparece como fundo muted do gráfico, a curva
  composta por cima.
- Clicar num nó abre o **painel contextual** (`ForjaCard`) centrado na base do gráfico, com os
  controles daquela seção: tipo (`ForjaChoice`), frequência/ganho/Q (`ForjaKnob`) e
  liga/ouvir. Some ao clicar no fundo.
- Na barra de rodapé, o seletor global de **bypass**.
- Arrastar um nó muda frequência (x) e ganho (y); o eixo vertical de largura (Q) entra por
  `Shift`-arrasto ou scroll nas bandas que a permitem; duplo-clique reseta a seção; botão
  direito troca o tipo.
- Todo texto acentuado passa por `forja::ui::utf8()`.

## Testes

```
cmake --build build --config Release --target forja_traco forja_traco_processor
ctest --test-dir build -C Release -R traco
```

- `Tests/Source/traco_engine.cpp` (`forja_traco`): resposta medida por tom para cada tipo de
  banda e para os cortes de esquina, cascatas, inclinações de 12/24 dB/oct, comportamento do
  de-esser (redução proporcional ao excesso, alcance e escuta), latência 0 por impulso,
  determinismo após `prepare` e **zero alocação** em `process`/`setParams`/curva.
- `Tests/Source/traco_processor.cpp` (`forja_traco_processor`): o processador real dirigido
  pela API do `AudioProcessor` ponta a ponta — APVTS → engine, medidores acompanhando o sinal,
  rampa do bypass (entrada e assentamento no seco e re-crossfade da voltada), sobrevivência do
  estado ao bypass e ao `getState`/`setState` em outro processador, e o layout mono aceito e
  medido pelo canal 0.

## Pendências conhecidas

- Validação de automação/bypass em host real, como nos demais plugins da suíte.
- Mid/Side e oversampling fora desta camada (fatia futura).
- A gaveta do TRACO no ForjaRack fica para depois.

## Créditos

Engine de EQ (biquadros RBJ), de-esser por diferença de envelope, grafo interativo
`ForjaEqGraph` e interface: Forja. Os coeficientes seguem as fórmulas de domínio público do
**Audio EQ Cookbook**, de **Robert Bristow-Johnson**. O conceito de detecção por diferença
filtrada do de-esser é inspirado no comportamento do ESCÓRIA (Airwindows DeBess, MIT, de
**Chris Johnson**), reimplementado do zero — nenhum código Airwindows entra neste plugin. A
medição RTA reusa o `ForjaSpectrum` da suíte.
