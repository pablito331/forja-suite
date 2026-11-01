# DRUMFORGE — sampler de bateria (engine Forja)

Sampler de bateria de 16 pads com camadas de velocity, round-robin, choke, mesa de mixagem
por canal e multi-out de 5 barramentos. É a primeira base de **DSP próprio** da Forja: o
engine, o mixer, o limiter e a interface foram escritos para a suíte, sem paridade com a
Airwindows. Projeto JUCE nativo (VST3), compilado a partir da raiz da suíte
com `forja_add_instrument` e a biblioteca compartilhada `forja_ui`.

## Estado

| Item | Estado |
| --- | --- |
| Engine (`forja::drumforge::DrumEngine`) | Implementado: 64 vozes, camadas, RR, choke, roubo |
| Importação de kit (pasta de samples) | Implementada: classificação por nome + marcadores `vl`/`rr` |
| Kit em arquivo (`.dfkit`) | Implementado: texto no estilo drumlabooh (notas, camadas, `@`pad, `~`choke) e mix premixada (`#mix@`/`#master`); SALVAR KIT cria pacote portável com os audios |
| Kit de texto (`drumkit.txt` / `drumkit.labooh` do drumlabooh) | Implementado: mapa MIDI, camadas, `>`/`*`/`^`, listas `.txt`, subpastas |
| Processor | APVTS de 249 parâmetros, MIDI, multi-out, estado e bypass |
| Interface | Abas KITS, MIXER e LOOPS MIDI prontas |
| Loops MIDI | Biblioteca, busca/filtro por pacote e categoria (subpastas aninhadas), grade de 1/16, preview e arrasto para a DAW |
| Painel FUNDAMENTOS | Implementado |
| Testes (`forja_drumforge_engine`) | PASS: camadas, RR, choke, roubo, multi-out, mute/solo, MIDI, latência, RT-safety, importação, drumkit.txt, loops |
| Latência | 0 amostras (medida por impulso no harness) |
| Validação em host (REAPER etc.) | Pendente · não há DAW no ambiente |

Este plugin **não** tem teste de paridade: não existe original de terceiro para comparar.
O que é verificado é o comportamento do engine (o teste monta um kit sintético e mede a
saída) e a ausência de alocação no caminho de áudio.

## Parâmetros

Cada pad tem 15 parâmetros de automação: 11 contínuos, 3 booleanos e 1 escolha de saída.

| Parâmetro | ID | Faixa | Padrão | Função |
| --- | --- | --- | --- | --- |
| Trim | `padNN_trim` | 0,00 – 1,00 | 0,50 | Ganho de entrada, −24 a +24 dB |
| Fader | `padNN_fader` | 0,00 – 1,00 | 0,91 | Nível de saída, −60 a +6 dB (0,91 = 0 dB) |
| Pan | `padNN_pan` | 0,00 – 1,00 | 0,50 | Posição estéreo, lei de potência constante |
| HPF | `padNN_hpf` | 0,00 – 1,00 | 0,00 | Passa-altos de 20 a 400 Hz; em 0 fica aberto |
| EQ Low | `padNN_eqLow` | 0,00 – 1,00 | 0,50 | Shelf em 80 Hz, ±15 dB |
| EQ Mid | `padNN_eqMid` | 0,00 – 1,00 | 0,50 | Sino em 1,2 kHz, ±15 dB |
| EQ High | `padNN_eqHigh` | 0,00 – 1,00 | 0,50 | Shelf em 8 kHz, ±15 dB |
| Comp Threshold | `padNN_compThr` | 0,00 – 1,00 | 0,70 | Limiar de −40 a 0 dB |
| Comp Ratio | `padNN_compRatio` | 0,00 – 1,00 | 0,10 | Razão de 1:1 a 20:1 |
| Send FX1 | `padNN_send1` | 0,00 – 1,00 | 0,00 | Envio pós-fader ao SALÃO; curva quadrada, topo em −12 dB (o retorno é 100% wet) |
| Send FX2 | `padNN_send2` | 0,00 – 1,00 | 0,00 | Envio pós-fader à LIXA |
| Mute / Solo / Fase | `padNN_mute`, `padNN_solo`, `padNN_phase` | on/off | off | Mute, solo e inversão de fase |
| Saída | `padNN_out` | Master, Aux 1–4 | Master | Barramento de destino do canal |

O master tem seis controles: `master_trim` (padrão 0,50), `master_comp` (0,00, TÊMPERA),
`master_limiter` (ligado), `master_fx1Regen` (0,50), `master_fx1Position` (1,00) e
`master_fx2Drive` (0,20). O preview de loops tem `loop_sync` (ligado, segue o BPM do host)
e `loop_bpm` (0,40, de 40 a 240 BPM quando o sync está desligado). O `bypass` é exposto por
`getBypassParameter()` e usa rampa de 10 ms sem cortar as vozes em andamento.

`NN` é o número do pad com dois dígitos: `pad01` a `pad16`.

## Engine

`DrumEngine` é dono de tudo o que toca áudio. Nada aloca em `process()`:

1. **Kit** — o editor/importador monta um `LoadedKit` fora da thread de áudio e publica o
   ponteiro com um token. O áudio adota o kit no início do bloco e o anterior só é liberado
   depois que o áudio confirma a troca; a thread de importação espera esse token.
2. **Disparo** — MIDI e cliques da UI entram por uma fila SPSC de 128 eventos com
   `pad` + `velocity`. O bloco é dividido nos pontos exatos dos eventos de MIDI, então a
   peça começa na amostra certa. O preview de loops usa o mesmo caminho: calcula o
   deslocamento em amostras do golpe e enfileira o disparo com esse offset.
3. **Voz** — a camada vem da velocity (até 8 camadas) e a tomada do cursor de round-robin
   daquela camada (até 16). Reprodução com interpolação linear e correção da taxa do
   arquivo para a da sessão. Com 64 vozes ocupadas, a mais antiga cede o lugar.
4. **Choke** — tocar um pad de um grupo desliga as vozes dos outros pads do mesmo grupo
   com rampa de 5 ms. O kit padrão põe HI-HAT e OPEN HAT no grupo 1.
5. **Canal** — trim → HPF (12 dB/oct) → compressor estéreo-linkado com joelho de 6 dB →
   EQ de três bandas → pan → fader. Os envios saem pós-fader; como o SALÃO devolve 100%
   wet, o SEND 1 dosa com curva quadrada e teto de −12 dB (um dedo no botão mal injeta
   sala), e o SEND 2 segue linear porque ali o nível é drive, não dose de retorno.
6. **Master** — soma dos 16 canais para o MAIN, trim, TÊMPERA (`master_comp`, desligado em
   0), limiter de teto −0,3 dBFS (ataque instantâneo, retorno de 80 ms, sem lookahead) e
   os retornos de FX 1 (SALÃO) e FX 2 (LIXA).

Medidores: cada canal publica pico e redução de ganho em atômicos; o master publica pico
L/R, clip e a redução já separada em compressor e limitador (dois campos), para a UI desenhar
cada rail à parte. O editor lê uma vez por tique (30 Hz) e envia o retrato para a aba
visível — a leitura consome o valor, então não há corrida entre abas.

Os blocos de efeito do master reutilizam adaptadores Airwindows MIT já validados na suíte
(ButterComp2 / kRockstar2 / PurestDrive). O resto é Forja.

O kRockstar2 original injeta *room tone* próprio (pico perto de −52 dBFS) mesmo com entrada
digitalmente silenciosa. No master do Drumforge esse gerador fica em 0, senão a mesa
parada assobia; o SALÃO standalone mantém o room tone do original. O adaptador tem um
campo `roomTone` (padrão 1,0 = bit-exato com o original), usado só pelo Drumforge.

## Kit e importação

**Pasta de samples.** `IMPORTAR PASTA` (ou arrastar uma pasta para a aba KITS) varre a
pasta **e as subpastas** por `.wav`, `.aif`, `.aiff` e `.flac` e classifica pelo nome do
arquivo:

| Termos no nome | Pad |
| --- | --- |
| `kick`, `bumbo`, `bd` | 01 KICK |
| `snare`, `caixa`, `sn` | 02 SNARE |
| `tom`, `tm` (1º e 2º arquivos) | 03 TOM 1 / 04 TOM 2 |
| `floor` | 05 FLOOR |
| `hat`, `hh`, `chimbal` | 06 HI-HAT |
| `open` + `hat` | 07 OPEN HAT |
| `crash` | 08 CRASH |
| `ride`, `condu` | 09 RIDE |
| `rim`, `aro` | 10 RIM |
| `clap`, `palma` | 11 CLAP |
| qualquer outro | 12 PERC |

Marcadores no nome definem camada e tomada: `vl`, `vel`, `layer` ou `v` seguidos de número
para camada de velocity; `rr`, `rt` ou `take` seguidos de número para round-robin.
`Kick_vl2_rr1.wav` e `Kick_vl2_rr2.wav` formam a camada 2 com duas tomadas. Pasta sem
marcação vira uma camada única com todas as tomadas em round-robin. Buracos na numeração
(só `rr1` e `rr3`) são compactados.

**Arquivo `.dfkit`.** É o formato nativo, agora em **texto**, na mesma linguagem do
`drumkit.txt` do drumlabooh: uma linha por pad, `[nota]@pad~choke Nome=arquivos`, com
vírgula separando camadas de velocity e `+` separando as tomadas de round-robin de uma
camada. Os marcadores Forja (`@` para travar o índice do pad e `~` para o grupo de choke) e
as linhas de cabeçalho `#forja-dfkit`, `#name` e `#desc` são comentários `#`, então o corpo
continua legível por qualquer leitor de kit de texto. Caminhos dentro da pasta do kit são
gravados relativos. `ABRIR KIT` carrega de qualquer pasta; a importação também grava o kit
em `Documentos/Forja/Drumforge/Kits`, onde ele aparece na biblioteca. Um `.dfkit` antigo em
XML (`<DrumforgeKit>`) ainda abre, por detecção de conteúdo.

**Kit premixado.** Um `.dfkit` pode carregar a mixagem junto com o mapa de samples, para
abrir já premixado. A mix vai em linhas de comentário, uma por pad mais o master, com os
mesmos valores normalizados 0–1 (passo 0,01) dos knobs:

```
#forja-dfkit 1
#name Meu kit
[36]@0 KICK=kick.wav
#mix@0 trim=0.62 fader=0.88 pan=0.40 hpf=0.10 eqLow=0.58 eqMid=0.50 eqHigh=0.55 compThr=0.70 compRatio=0.10 send1=0.20 send2=0.00
#master trim=0.50 comp=0.30 fx1Regen=0.50 fx1Position=1.00 fx2Drive=0.20
```

São comentários `#`, então um leitor de kit de texto estrangeiro ignora as linhas `#mix`/
`#master` e continua lendo o kit normal. Entram trim, fader, pan, HPF, os três EQs, o
compressor (threshold/ratio) e os dois envios de cada pad, mais os controles contínuos do
master; mute, solo, fase e a escolha de saída **não** vão no kit (são decisão de sessão). A
mix é aplicada **só na carga manual** (`ABRIR KIT`, `IMPORTAR PASTA`, arrastar-e-soltar):
kit com `#mix` assume os knobs, kit sem `#mix` volta a mesa ao neutro. Ao **reabrir a
sessão**, o estado que a DAW salvou vence a mix do arquivo, para o plugin não brigar com o
preset/automação do projeto. A importação grava o `.dfkit` sem mix; para premixar e levar os
áudios junto, use o botão **SALVAR KIT** no rodapé da aba KITS: ele pega o kit aberto (mesmo
vindo de `drumkit.txt`, `drumkit.labooh` ou pasta de samples), captura o estado atual do
mixer (os 11 controles contínuos de cada pad mais os do master) e cria um **pacote
portável** — uma pasta com o nome do kit contendo o `.dfkit`, a subpasta `samples/` com uma
cópia de cada WAV (um sample usado em vários pads é copiado só uma vez) e a foto normalizada
como `image.png`/`image.jpg`. Os caminhos no `.dfkit` ficam relativos à própria pasta, então
dá para mover o pacote ou mandar para outra máquina que o kit abre sozinho, sem depender dos
WAVs originais. "Salvar como" aponta para a biblioteca e a varredura recursiva já mostra o
`.dfkit` do pacote na lista. Assim fecha-se o ciclo: abrir um kit estrangeiro, ajustar a mesa
e salvar tudo junto no nosso formato premixado.

O `.dfkit` é **gerado pela importação**: não é preciso escrever a linha à mão. O mínimo que
o leitor aceita é uma linha com o `=` e ao menos um arquivo (relativo à pasta do `.dfkit`):

```
#forja-dfkit 1
#name Meu kit
[36]@0 KICK=kick.wav
```

**Kits de texto do drumlabooh (`drumkit.txt` / `drumkit.labooh`).** Formato nativo de
[drumlabooh](https://github.com/psemiletov/drumlabooh)/[drum_sklad](https://github.com/psemiletov/drum_sklad)
(especificação 1.1): uma pasta com os WAVs e um `drumkit.txt`, uma linha por peça.

```
[36]>Bass Drum=bass/b01.wav,bass/b02.wav
Snare=snare/snare.txt
hihat close=cymbals/hat-closed-1.wav,cymbals/hat-closed-2.wav
```

| O que o kit declara | O que o Drumforge faz |
| --- | --- |
| `[36]` antes do nome | nota MIDI do mapa embutido no kit; o pad daquela nota recebe a peça |
| linha sem nota | o nome da peça é classificado (kick, snare, rim/`side stick`, hat, open hat, floor, crash, ride, tom, clap, perc) e herda o pad e a nota GM |
| lista separada por vírgula | camadas de velocity, da mais suave para a mais forte |
| `>` round robin, `*` ordem aleatória | uma única camada com as tomadas em round-robin (o motor gira em ordem sequencial) |
| `^` | multicamada normal; aqui a velocity já só escolhe a camada |
| item em subpasta (`Kick/k01.wav`) | resolvido relativo à pasta do kit |
| item terminando em `.txt` | lista de arquivos (um nome por linha), lida e expandida |
| item apontando uma pasta | os arquivos de áudio da pasta, em ordem de nome |
| `info.txt` | vira a descrição do kit |
| `image.png` / `image.jpg` na pasta | foto de preview exibida no card da aba KITS; um `.dfkit` salvo leva o caminho na linha comentário `#image` |

Além do `drumkit.txt`, o Drumforge também lê o `drumkit.labooh` (XML `<root type="alt">`
com um `<sample name note layer_index_mode>` por peça), formato que alguns kits do
drum_sklad trazem no lugar do texto. O XML é convertido nas mesmas linhas do leitor de
texto: o nome, a nota declarada, a lista separada por vírgula, as subpastas e os arquivos
`.txt` funcionam igual; `type="alt"` no `<root>` ou `layer_index_mode="alt"`/`"random"` numa
peça viram round-robin. SFZ **não** é lido. Como nenhum dos dois formatos declara mute
group, o único choke herdado é o par chimbal fechado/aberto (grupo 1).

A escolha de pad roda em estágios, para o mapa do kit vencer a ordem do arquivo: a mesma
peça em outra linha (mesmo nome ou mesma nota) é mesclada; depois as notas declaradas
ocupam o pad daquela nota; depois os pads padrão do tipo classificado; depois qualquer pad
livre do mapa GM e, no fim, os quatro pads reservados, que recebem a primeira nota de
percussão ainda livre. Peças além das 16 são descartadas e contadas na descrição do kit.
Mais de 8 camadas numa linha são agrupadas em blocos contíguos (as sobras viram round-robin
da própria camada) e uma peça com mais de 16 tomadas é truncada.

`IMPORTAR PASTA` numa pasta de kit, arrastar a pasta ou o próprio `drumkit.txt` /
`drumkit.labooh` para a aba KITS, e `ABRIR KIT` no arquivo todos passam por esse leitor. Se
a pasta tem `drumkit.txt`, ele manda; na ausência, o `drumkit.labooh` assume; sem qualquer
definição — ou se ela não apontar nenhum sample — a varredura pelo nome dos arquivos roda.

**Biblioteca.** Além dos `.dfkit` de `Documentos/Forja/Drumforge/Kits`, a lista da aba KITS
mostra os kits de texto e `drumkit.labooh` encontrados em `drum_sklad`, `drumlabooh-kits` e
`drumrox-kits` na pasta da conta do usuário, aceitando um nível extra quando o kit vem direto
do ZIP do GitHub. Nada é copiado: os pads tocam dos WAVs originais.

## Mapa MIDI

O kit padrão segue a General MIDI, então o plugin já responde a grooves prontos:
36 bumbo, 38 caixa, 50/47 tons, 43 floor, 42 chimbal fechado, 46 aberto, 49 crash,
51 condução, 37 aro, 39 palma e 56 percussão. Pads sem nota ficam reservados.
Notas desconhecidas são ignoradas; o mapa é reconstruído a cada troca de kit.

## Loops MIDI

A aba **LOOPS** é um navegador de grooves com preview próprio, para escolher o loop sem
sair do plugin.

**Biblioteca.** `Documentos/Forja/Drumforge/Loops` é varrida recursivamente por `.mid` e
`.midi`; cada pasta ou subpasta vira uma categoria no filtro `PACOTE` — organize por ritmo,
`Fills`, `Chorus` etc. abrindo subpastas dentro da biblioteca (ex.: `Ritmos/Anos 60/Fills`).
O filtro de uma categoria-pai traz a subárvore inteira (`Ritmos` mostra tudo embaixo dela).
A busca casa nome do arquivo e caminho. `IMPORTAR MIDI` copia `.mid` avulsos **ou pastas
inteiras** para a biblioteca, preservando as subpastas (só os `.mid` vão junto; o resto do
conteúdo fica de fora) — o último arquivo importado já fica selecionado. Arrastar `.mid`
ou pastas para a aba faz o mesmo, e `ABRIR PASTA` revela a biblioteca no sistema. A lista
mostra o caminho, BPM de origem, compassos e número de golpes.

**Leitura.** `LoopLibrary` converte o arquivo para **beats**, não para segundos: o BPM do
primeiro meta-evento de tempo define a origem (120 se não houver) e cada nota vira um
`Hit { beat, note, velocity }`. O loop é fechado no compasso inteiro seguinte ao último
golpe, então uma virada no fim do compasso não é cortada; arquivos vazios ou sem notas são
recusados com mensagem. O arquivo nunca é reescrito.

**Preview.** Selecionar uma linha carrega o loop; `TOCAR` inicia e `PARAR` interrompe.
`SYNC DAW` ligado usa o BPM do playhead; desligado, usa `loop_bpm` (40 a 240 BPM). O
`LoopPlayer` roda antes do `DrumEngine::process`, calcula o deslocamento de cada golpe em
amostras e enfileira o disparo com esse offset — o mesmo caminho do MIDI ao vivo, então o
preview não depende do tamanho do bloco. No fim do loop o cursor volta ao início sem parar
as vozes que ainda estão soando, e o transporte mostra o compasso atual.

A troca de loop usa o mesmo esquema de token do kit: a sequência antiga só é liberada
depois que o áudio confirma a adoção da nova, então não há alocação nem ponteiro pendurado
no caminho de áudio. O `loopPath` e o `loopName` ficam no estado do plugin e o loop é
recarregado ao reabrir o projeto.

**Grade.** Uma faixa por pad que o arquivo realmente toca e uma coluna por 1/16, com até
quatro compassos visíveis. A velocity define a opacidade da célula e a coluna tocando é
destacada. Notas que não casam com o mapa do kit atual não somem em silêncio: a barra de
status informa quantos golpes ficaram fora. `ARRASTAR MIDI` entrega o `.mid` selecionado
para a DAW (Windows, macOS e Linux) pelo arrasto de arquivo do sistema, sem passar pelo
buffer de áudio.

## Interface

- Canvas de projeto 1000 × 540, redimensionável de 70% a 200% com aspecto fixo.
- Aba **KITS**: biblioteca em `Documentos/Forja/Drumforge/Kits`, importação, mapa dos 16
  pads e o painel do kit selecionado (foto grande do kit à esquerda; à direita, pads com
  áudio, amostras, camadas e RR). Clicar em
  um pad dispara o som com velocity pela posição vertical; arrastar uma pasta importa.
- Aba **MIXER**: 16 canais de 92 px com `MiniKnob` (10 controles em 5 linhas, coloridos por
  função dentro da paleta: brasa para ganho/dinâmica, âmbar para HPF, gradiente verde→cinza→
  laranja no EQ, branco nos envios), mute/solo/
  fase, chip de saída, fader estreito com medidor, rail vertical de GR do canal e leitura de
  GR, além do master fixo à direita com medidor estéreo e dois rails separados (C =
  compressor, L = limitador, 0 a −24 dB).
- Aba **LOOPS**: biblioteca com busca e filtro por pacote/categoria (subpastas aninhadas),
  detalhe do loop com `TOCAR`, `SYNC DAW`, `BPM`, transporte, grade de 1/16 e
  `ARRASTAR MIDI`.
- Todo texto acentuado passa por `forja::ui::utf8()`.

## Testes

```
cmake --build build --config Release --target forja_drumforge_engine
build/Tests/Release/forja_drumforge_engine.exe
```

`Tests/Source/drumforge_engine.cpp` monta um kit sintético (bumbo com 3 camadas, caixa com
3 tomadas, percussão em AUX, dois chimbais no grupo de choke e um clique curto) e verifica:
adoção e isolamento de kit, escolha de camada por velocity, rotação de round-robin, choke,
roubo de voz com 100 disparos, roteamento para AUX sem vazar no master, mute/solo, mapa GM,
latência 0 por impulso, rampa de bypass preservando vozes, zero alocação em `process()`,
master em silêncio com a mesa parada (sem o room tone do SALÃO), ida e volta do XML e a
importação de uma pasta com WAVs gerados na hora.

O leitor de `drumkit.txt` é coberto com fixtures do formato: nota embutida, nomes sem nota,
prefixos `>`/`*`/`^`, camadas em vírgula, lista `.txt`, subpastas, `info.txt` como descrição,
`IMPORTAR PASTA` delegando ao leitor, 11 camadas agrupadas em 8, kit de 18 peças ocupando os
16 pads com notas únicas e um layout no estilo do Jazzkit do drum_sklad (linha repetida,
aspas no nome, peças sem nota) checando pad count, choke dos chimbais e ausência de nota
repetida.

Para os loops, o mesmo executável grava MIDI na hora e cobre: leitura em beats, BPM do
meta-evento, fechamento em compasso inteiro (1 e 2 compassos), recusa de arquivo inválido e
de MIDI sem notas, varredura da pasta, `folder` como caminho relativo multinível
(`Pacote A/Viradas`), importação de pasta inteira preservando categorias e filtrando
não-MIDI, importação para a biblioteca e o preview disparando
os golpes no frame medido (tolerância de 1 amostra em 48 kHz a 120 BPM), inclusive depois da
volta do loop, com o cursor sempre dentro do loop e nenhum disparo novo após `stop`.

## Pendências conhecidas

- Validação de automação/bypass em host real, como nos demais plugins da suíte.
- O preview e o arrasto dos loops ainda não foram exercitados numa DAW (não há host no
  ambiente).
- A grade de loops é só de leitura: não há editor de passos nem gravação de groove na UI.
- Sem UI de edição por pad (nota, choke, nome) para além do que o `.dfkit` carrega.

## Créditos

Engine de sampler, mixer, limiter, importação e interface: Forja. Os blocos de efeito do
master usam adaptadores dos originais Airwindows de **Chris Johnson** (MIT): ButterComp2
(TÊMPERA), kRockstar2 (SALÃO) e PurestDrive (LIXA), com o código original em
`Forja/Source/forja/vendor/airwindows/`. Os leitores de `drumkit.txt` e `drumkit.labooh`
seguem a especificação de formato do [drumlabooh / drum_sklad](https://github.com/psemiletov/drum_sklad),
de **Peter Semiletov** — nenhum código de lá entra no plugin, apenas lemos o formato.
Nenhum sample ou biblioteca de bateria acompanha o plugin.
