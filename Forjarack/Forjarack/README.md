# ForjaRack — rack em cadeia estilo Série 500 com módulos Airwindows MIT

Plugin VST3 da suíte **Forja** e primeira entrega da base
`Forjarack/`. É um **rack em cadeia estilo Série 500**: oito gavetas em série,
cada uma hospedando um adaptador **Airwindows** (MIT, Chris Johnson) já
validado por paridade amostra a amostra na suíte. A interface das gavetas sai
um pouco do padrão Forja — cada família tem cor de fachada própria (cor,
drive, dinâmica, EQ, ambiente e lo-fi) — mas mantém a paleta ferro, os knobs e o
painel FUNDAMENTOS da casa.

## Papel

Montar uma fileira completa de timbre → dinâmica → EQ → ambiente em um único
plugin, com uma chave de on/off e uma de escuta por gaveta (bypass/solo) e uma
seção master de trim e fase. Útil em barramentos e buses onde você quer encadear
modelos Airwindows sem abrir cinco instâncias.

## Módulos disponíveis (fatia 1 — timbre e espaço)

| Gaveta | Módulo | Original Airwindows | Knobs | Defaults documentados |
| --- | --- | --- | --- | --- |
| Cor | CERA | PurestWarm | CALOR | 0,50 |
| Cor | MEL | Sweeten | DOCE | 0,00 |
| Drive | LIXA | PurestDrive | DRIVE | 0,00 |
| Drive | OXIDO | IronOxide5 | ENTRADA, AG FITA, GR FITA, FLUTTER, RUÍDO, SAÍDA, DRY/WET | 0,50 / 0,5623413 / 0,5623413 / 0,50 / 0,50 / 0,50 / 1,00 |
| Dinâmica | TEMPERA | ButterComp2 | COMP, SAÍDA, MIX | 0,00 / 0,50 / 1,00 |
| Dinâmica | MURALHA | Dynamics (ADClip9) | BOOST, MATCH, RUÍDO, TETO, MODO | 0,00 / 0,00 / 0,70 / 0,75 / 0,00 |
| EQ | CINZEL | Parametric | AGUDO, ALTO MED, BAIXO MED, FREQ AG, FREQ BM, MIX | todos 0,50 exceto MIX 1,00 |
| Ambiente | FOLEGO | kStation | REGEN, DEREZ, FILTRO, EARLY, POSIÇÃO, DRY/WET | 0,50 (posição 0,90; dry/wet 0,50) |
| Ambiente | SALAO | kRockstar2 | REGEN, POSIÇÃO, DISTÂNCIA, DOWNREZ, DRY/WET | 0,50 / 0,75 / 0,25 / 1,00 / 0,50 |
| Ambiente | REPIQUE | TapeDelay2 | TEMPO, REGEN, FREQ, RESO, FLUTTER, DRY/WET | 1,00 / 0,00 / 0,50 / 0,00 / 0,00 / 1,00 |
| — | vazio | painel cego | — | pass-through |

## Módulos disponíveis (fatia 2 — núcleo de mix)

Acrecentados ao fim do registro; o índice gravado nos presets da fatia 1 não
mudou. Entram EQs completos, um compressor/limitador/grave extra e mais fita e
lo-fi.

| Gaveta | Módulo | Original Airwindows | Knobs | Defaults documentados |
| --- | --- | --- | --- | --- |
| Cor | CORPO | Mojo | CORPO | 0,50 |
| Cor | IMPETO | Dyno | IMPETO | 0,50 |
| Drive | REBARBA | Slew3 | SUAVIDADE | 0,00 |
| Drive | LAMINADOR | ToTape9 | ENTRADA, INCLINAÇÃO, FORMA, FLUT, VEL. FLUT, BIAS, CABEÇOTE, FREQ. CABEÇOTE, SAÍDA | todos 0,50 |
| Dinâmica | DIQUE | FinalClip | — (limiter fixo, sem knob) | — |
| Dinâmica | MALHO | Pop2 | COMPRESSÃO, ATAQUE, SOLTURA, DRIVE, MIX | 0,50 / 0,50 / 0,50 / 0,50 / 1,00 |
| Dinâmica | PRENSA | Pressure5 | PRESSÃO, VELOCIDADE, MIADO, GARRA, SAÍDA, MIX | 0,00 / 0,25 / 1,00 / 0,50 / 0,50 / 1,00 |
| Dinâmica | ESCORIA | DeBess | INTENSIDADE, VELOCIDADE, PROFUNDIDADE, FILTRO, SENSIBILIDADE | 0,00 / 0,50 / 0,50 / 0,50 / 0,00 |
| EQ | TORNO | Baxandall3 | ENTRADA, AGUDOS, GRAVES | todos 0,50 |
| EQ | LIMA | SmoothEQ2 | AGUDO, MÉDIO AGUDO, MÉDIO GRAVE, GRAVE, FREQ AG, FREQ MA, FREQ MG, FREQ GR | todos 0,50 |
| Lofi | GRANALHA | DeRez5 | TAXA, BITS, QUADRADO, BRILHO, GRAVEZA, SAÍDA | 1,00 / 1,00 / 0,00 / 1,00 / 0,00 / 1,00 |
| Ambiente | ECO | Echojam | ESPAÇO, REGEN, VELOCIDADE, DRY/WET | todos 0,50 |

Desvios registrados na fatia 2:

- **ECO** roda assíncrono (`sync = false`), como o REPIQUE; o tempo vem do knob
  VELOCIDADE e não do playhead do host.
- **DIQUE** (FinalClip) é um hard limiter de teto fixo — o modelo original não
  expõe parâmetro, então a gaveta aparece sem knob e só roteja.

A gaveta começa a tocar assim que recebe um módulo — não existe "ligar" a parte.
Gaveta vazia ou em **bypass** é pass-through exato. Solo em qualquer gaveta
silencia as demais; o solo de uma gaveta já em bypass não conta, para não
silenciar o rack inteiro.

> Power e mute existiram na fatia 1 e foram removidos: numa cadeia em série
> "desligar", "mudar" e "bypassar" a gaveta são a mesma operação, então os três
> controles só duplicavam a decisão. O parâmetro `slot{N}_power` e o
> `slot{N}_mute` saíram do layout; presets antigos simplesmente os ignoram.

## Parâmetros (barramento)

| Parâmetro | Faixa | Padrão | Função |
| --- | --- | --- | --- |
| `slot{1..8}_type` | escolha (24) | vazio | Módulo da gaveta; ao trocar, knobs assumem os defaults da tabela (HOSPEDADO usa knobs dinâmicos do plugin) |
| `slot{N}_bypass` | ligado/desligado | desligado | Chave de on/off da gaveta; ligada = pass-through |
| `slot{N}_solo` | ligado/desligado | desligado | Tira as demais gavetas da cadeia |
| `slot{N}_knob{0..9}` | 0,00–1,00 (passo 0,01) | 0,50 | Parâmetros do módulo; o escalonamento mora no DSP |
| `input_trim` / `output_trim` | 0,00–1,00 | 0,50 | Trim de ±24 dB do barramento |
| `phase_invert` | ligado/desligado | desligado | Inverte o canal direito na saída |
| `bypass` | ligado/desligado | desligado | Comparação A/B com rampa de 10 ms |

## DSP

Nenhum DSP é inventado. Cada módulo é um wrapper fino (`Source/rack/Module.h`)
sobre o adaptador Forja correspondente em `Forja/Source/forja/dsp/`, o mesmo
compilado nos plugins individuais e coberto pelos testes de paridade da base
Airwindows. A cadeia em série, as chaves de slot e o master são camada Forja
de roteamento, documentada no painel FUNDAMENTOS (`Source/ForjaInfoText.h`).

Desvios registrados por módulo:

- **SALAO** roda com `roomTone = 0` (sem entrada, o master não pode assobiar
  com a mesa parada — mesma decisão do Drumforge).
- **REPIQUE** roda assíncrono (`sync = false`); o tempo vem do knob TEMPO.
- **ECO** roda assíncrono (`sync = false`), mesmo caminho do REPIQUE; o tempo
  vem do knob VELOCIDADE.
- **DIQUE** (FinalClip) não expõe parâmetro no modelo original; a gaveta fica
  sem knob e atua só como limiter de teto fixo.

## Identidade visual (camada Forja)

Cada gaveta traz uma **marca d'água** esmaecida atrás dos knobs, desenhada em
vetor na cor de acento da família, para dar "cara de hardware" ao módulo sem
mexer no layout (os controles ficam exatamente onde estavam; o motivo é
sempre por baixo deles). A face é declarada por módulo no catálogo
(`ModuleFace`, ortogonal à `ModuleCategory` que só decide a cor):

- **fita** (OXIDO, LAMINADOR): chapa metálica acinzentada + dois rolos com a
  trilha de fita.
- **lo-fi** (GRANALHA): painel embutido com grade de pixels (bit-crush).
- **dinâmica** (TEMPERA, MURALHA, DIQUE, MALHO, PRENSA, ESCORIA): mostrador VU
  com ponteiro.
- **EQ** (CINZEL, TORNO, LIMA): curva de resposta.
- **ambiente** (FOLEGO, SALAO, REPIQUE, ECO): ondas concêntricas de eco.
- **drive** (LIXA, REBARBA): onda senoidal clipada.
- **cor** (CERA, MEL, CORPO, IMPETO): arcos de calor.

As faces **valve** (válvula) e **drum** (kit) já têm desenho pronto mas ainda
não têm morador no rack: acendem sozinhas quando um saturador de válvula ou o
mini drum (instrumento) entrarem no registro. Um instrumento genérico cai em
**drum** automaticamente. É só decoração — nenhum parâmetro, DSP ou roteamento
muda com a face.

## Arquitetura e latência

- O grafo de módulos é montado na thread de mensagens sob
  `suspendProcessing`; o controle por bloco chega ao engine como snapshot POD
  construído na pilha a partir de ponteiros atômicos do APVTS (automação
  amiga do host, zero alocação no caminho de áudio).
- **Latência reportada: 0 amostras (PDC)**. Reverb e delay têm atraso musical,
  não compensação automática, como nos originais da família Space.
- Medição: LED de pico por gaveta (com decaimento) e barras estéreo de
  entrada/saída no master, com CLIP latchado.

## Validação

- `Tests/Source/forjarack_engine.cpp` (`ctest: forjarack_engine`) — 19
  checagens funcionais: rack vazio bit-exato, inversão de fase, módulo
  carregado tocando sem nenhuma chave ligada e publicando pico, bypass de slot,
  solo e a regra do solo em gaveta bypassada, registro/rejeição de expansão,
  instrumento somando voz na cadeia, isolamento por instância (sem estado
  global/RNG), restauração de pass-through ao remover o módulo e a fumaça do
  catálogo (todo módulo publicado instancia, prepara e roda seus defaults sem
  NaN/inf e publica pico). **PASS** na suíte verde.
- Não há paridade do rack em si: a autoridade de timbre é a paridade de cada
  módulo nos 31 testes da base Airwindows.

## Expansões (instrumentos e módulos futuros)

O catálogo é um **registro expansível** (`Source/rack/ModuleCatalog.h`): cada
módulo é uma ficha com `id` ASCII estável, rótulo, categoria/cor, papel
(`effect` ou `instrument`), mapa de knobs com defaults e uma função fábrica.
`ModuleCatalog::registerModule(spec)` publica uma expansão na inicialização da
thread de mensagens — foi assim que CERA…REPIQUE entraram, e é assim que
novos módulos entrarão.

Regras do registro (protegem presets de usuários):

- O seletor grava o **índice** de registro; módulos publicados nunca são
  removidos ou reordenados — novas expansões são sempre **acrescentadas ao
  fim**.
- O `id` é o contrato de longo prazo; o rótulo pode mudar, a string não.
- O caminho de áudio nunca toca o registro: o engine guarda os módulos criados
  sob `suspendProcessing`; índice inválido = pass-through.

Suporte a instrumentos já instalado:

- O plugin declara `acceptsMidi() = true`; o processor converte as notas do
  bloco em um `MidiEventSpan` POD (até 256 eventos) e o engine entrega aos
  slots com papel `instrument` via `ModuleBase::processMidi`.
- Instrumentos **somam sua voz à cadeia** (o sinal recebido passa intacto):
  o engine zera um scratch pré-alocado em `prepare`, o módulo escreve ali e o
  engine soma. Gavetas de instrumento exibem a chapa `INSTR` no cabeçalho.
- Nada de alocação no caminho de áudio em nenhum dos dois papéis.

Candidatos conscientes dessa porta (por enquanto, apenas intenção registrada):

- **Leitor SF2/SF3** — soundfont player como módulo de instrumento.
- **Mini drum** — lê os kits `.dfkit` do Drumforge, sem o mixer completo.
- **Outros mini plugins** — qualquer adaptador Forja vira módulo com uma ficha.

## Gaveta HOSPEDADO — hosting dinâmico (sem recompilar, qualquer licença)

A ficha **HOSPEDADO** no seletor abre a única gaveta dinâmica do rack: qualquer
pessoa solta um `.vst3` (ou `.component` no macOS) em
`Documentos/Forja/Rack/` e o plugin roda ali dentro, sem tocar no código da
suíte.

- **Menu ARQUIVO** lista os plugins encontrados; **BUSCAR** revarre a pasta.
- Os **knobs da gaveta são os parâmetros do plugin** (nome, quantidade e valor
  vêm do plugin vivo; os 8 primeiros na frente da gaveta).
- **EDITAR** abre a UI nativa do plugin **em janela separada**; sem editor,
  abre a lista genérica de parâmetros. Com a janela aberta, o rack trava seus
  knobs e não briga com o editor; ao fechar, volta a comandar.
- **Efeito e instrumento**: o papel é detectado pelo layout de buses. O
  instrumento recebe o MIDI completo da faixa (nota, CC, pitch bend, program)
  pelo caminho nativo e soma sua voz à cadeia.
- **Latência/PDC**: a latência reportada pelo plugin propaga para o host (o
  rack adota o máximo entre as gavetas vivas).
- **Licença**: plugins hospedados são obras separadas carregadas em runtime
  (modelo de qualquer DAW). Mantêm a própria licença; nada entra na suíte, que
  continua MIT, e os módulos compilados (paridade) continuam intactos.
- A escolha do arquivo vive no preset do rack (propriedade de estado, pois a
  lista é dinâmica).

Manual completo — uso, criação de plugins para gaveta, estratégia "duas UIs",
boas práticas e troubleshooting — em [`docs/MANUAL-GAVETA.md`](../../docs/MANUAL-GAVETA.md).

Validação: `Tests/Source/forjarack_hosted.cpp` (`ctest: forjarack_hosted`) com
stub de `AudioPluginInstance` — 20 checagens: adoção de efeito/instrumento,
rejeição de instância sem saída, knobs end-to-end, mapa dinâmico de parâmetros,
leitura de valor, trava da UI nativa, MIDI nativo (CC pass-through), pico e
propagação/restauração de latência. **PASS**. A carga de arquivos VST3 reais
depende do ambiente (não há DAW/plugins na máquina de dev) e fica registrada
como pendência de validação manual.

## Banda de arquivos nas gavetas de instrumento

As gavetas de instrumento com biblioteca própria — **MINIDRUM** (kits `.dfkit`
de `Documentos/Forja/Drumforge/Kits`, incluindo os txt-kit roots do Drumforge)
e **CANIVETE** (soundfonts `.sf2`/`.sf3` de `Documentos/Forja/Canivete`) —
mostram uma banda de arquivos abaixo das chaves da gaveta, no mesmo espírito
da banda da gaveta HOSPEDADO:

- O **seletor** lista o conteúdo da biblioteca da gaveta; escolher pela lista
  sincroniza o knob de índice (o engine reenvia os knobs a cada bloco, então a
  escolha precisa viver no parâmetro) e o nome exibido é o material vivo do
  módulo, não o último caminho gravado.
- **ARQUIVO** abre um kit/soundfont **fora da biblioteca** via `FileChooser`
  nativo (para o MINIDRUM também aceita pasta de kit). Enquanto o arquivo
  externo toca, o knob de índice fica mudo; a próxima escolha pela lista
  devolve o comando ao knob. Se o arquivo sumir, a gaveta volta ao knob em
  silêncio, sem crash.
- A escolha vive no preset do rack como propriedade de estado (`drawer_slotN`),
  pois a lista é dinâmica demais para um `AudioParameterChoice` — mesmo
  mecanismo da escolha hospedada.

Validação: `testDrawerFiles()` em `forja_minidrum` e `forja_canivete` cobre o
contrato no nível do módulo (banda exposta, rejeição de nome fora da lista,
colheita do externo pelo `idle()`, knob mudo enquanto o externo manda e
reversão graciosa quando o arquivo não existe). O `FileChooser` em si é UI
nativa e depende de sessão gráfica; fica registrado como pendência de
validação manual em host, como o resto da banda HOSPEDADO.

## Licença

DSP dos módulos: Airwindows (PurestWarm, Sweeten, PurestDrive, IronOxide5,
ButterComp2, Dynamics, Parametric, kStation, kRockstar2, TapeDelay2, Mojo, Dyno,
Slew3, FinalClip, Baxandall3, SmoothEQ2, Pop2, Pressure5, DeBess, ToTape9,
DeRez5, Echojam), © Chris Johnson, MIT — referência vendorizada em
`Forja/Source/forja/vendor/airwindows/` com `PROVENANCE.md` por modelo
(commit `b110fac3991fff7c4655cc2d25d6a3e221f9799c`). Rack, engine e interface:
Forja, MIT. Plugins hospedados na gaveta HOSPEDADO mantêm a licença do próprio
autor — o ForjaRack apenas os carrega em tempo de execução.
