# PIANOFORGE — piano de amostras + modelo físico (Forja)

Produto da base `Pianos/` (a quinta da suíte). Formato cheio (1000 × 540), instrumento de
uma tecla, e funciona como gaveta de instrumento dentro do FORJARACK.

A razão de existir do PIANOFORGE: pegar um soundfont de piano **comum** e acrescentar o
**comportamento físico do instrumento** — o que um player só de amostras não tem. A cadeia
física modelada não é adaptação Airwindows nem tem paridade MIT: ela foi **portada** (não
linkada) do repo-irmão autônomo `pianoforge/`, que fica como referência histórica. Das duas
camadas de molho, ambas são **DSP original da Forja**: o **Compressor** é um compressor de um
knob (`engine/dsp/Compressor.h`) de attack finito e ganho suavizado (nada modula dentro do ciclo,
por isso **não degrada** o áudio) que achata a macro-dinâmica com makeup de laço fechado segurando
o nível, e o **Molho** é um
shimmer espectral com a técnica reimplementada a partir do **Atlantis Reverb** (Geraint Luff),
sem copiar código dele (a licença do Atlantis não é MIT, então nada dele é vendorizado).

## Estado

| Item | Estado |
| --- | --- |
| Paridade com vendor | Não se aplica: o timbre vem do TinySoundFont + cadeia física portada, não há adapter Airwindows |
| Teste próprio | `forja_pianoforge_engine` (gera um SF2 e um `.pforge` mínimos no harness) |
| Guarda de alocação | Zero alocação no caminho de áudio (classes do `engine/dsp/` no `forja_alloc_guard`) |
| Latência | 0 — o `tsf` entrega no mesmo bloco; valor medido por impulso no teste |
| SF3 (amostras Ogg) | Ligado no standalone e nos testes por `FORJA_CANIVETE_SF3`; desligado na gaveta do FORJARACK (veja abaixo) |
| `.pforge` | Legível: leitor `Bundle` (`PFG1` + diretório + CRC32 + manifest via `juce::JSON`) |
| Build | VST3 (Windows, macOS, Linux) |

## Parâmetros

Vinte e três parâmetros: treze controles audíveis nos cards da tela PIANO + dez discos de ajuste
fino na aba MOTOR. A tela PIANO ainda traz o painel gráfico da **Tampa** (um piano de cauda
cuja tampa levanta conforme o parâmetro `lid`), que é display, não controle. Todos vivem no
APVTS em 0–1 (passo 0,01); o escalonamento acontece na fronteira do engine
(`engine/Params.h`, `toEngineDomain`/`fromEngineDomain`), que é a mesma ordem contratada do
projeto-irmão (renumerar quebraria presets salvos). Os dois últimos de modelagem (`body`,
`ring`) são a camada de corpo acústico da Forja e os quatro de molho (`compressor`, `molho`,
`tail`, `quinta`) são a camada de worship, todos acrescentados por **append** — nada antes deles
se move.

| Nome (id) | Knob | Faixa no engine | Default | Nota |
| --- | --- | --- | --- | --- |
| `volume` | Volume | 0–1 | 0,82 | nível de saída, antes do soft-clip |
| `hammer` | Martelo | 0–1 | 0,16 | transiente do ataque (click do feltro + thump da caixa), calibrado por nota; escala `kHammerEscala = 0,50` |
| `velCurve` | Dinâmica | 0–1 | 0,5 | curva de velocity aplicada **antes** do sampler (MOTOR) |
| `sympRes` | Ressonância | 0–1 | 0,45 | ganho do banco de ressonadores simpáticos |
| `lid` | Tampa | 0–1 | 0,52 | lowpass de 2,5 kHz (fechada) a 16 kHz (aberta); é o valor que levanta a tampa do desenho (MOTOR) |
| `bassEQ` | Graves | 0–1 | 0,5 | low shelf 120 Hz (±8 dB) |
| `midEQ` | Médios | 0–1 | 0,74 | peak 1 kHz Q 0,8 (±8 dB; ≈ +3,8 dB no default) |
| `highEQ` | Agudos | 0–1 | 0,5 | high shelf 5 kHz (±8 dB) |
| `chorus` | Chorus | 0–1 | 0,0 | chorus de 2 vozes, LFO 0,8 Hz |
| `reverb` | Reverb | 0–1 | 0,12 | mistura do reverb FDN de concerto |
| `reverbType` | Sala | 0–3 (choice) | 2 (Prato) | passo 0,25 (exceção no estilo da `division`): Sala pequena · Studio · Prato · Catedral (MOTOR) |
| `trimDb` | Trim | −12…+12 dB | −1,9 dB | trim de master (MOTOR) |
| `width` | Largura | 0–2 | 1,54 | largura estéreo (MOTOR) |
| `releaseDamp` | Abafar soltura | 0–1 | 0,53 | amortecimento de release (MOTOR) |
| `hammerTone` | Brilho do martelo | 0–1 | 0,0 | calibração do tom do transiente (MOTOR) |
| `sympTune` | Afin. ressonância | 0–1 | 0,55 | afinação dos ressonadores (MOTOR) |
| `engineTune` | Afin. global | ±100 cents | 0 | detune geral (MOTOR) |
| `body` | Corpo | 0–1 | 0,62 | resposta do tampo: curva de radiação do corpo (unity em 0) |
| `ring` | Madeira | 0–1 | 0,5 | modos do tampo: anel de madeira que continua depois da nota (aditivo) |
| `compressor` | Compressor | 0–1 | 0,0 | compressor original da Forja (`engine/dsp/Compressor.h`) — **o mesmo compressor de canal do mixer do Drumforge** (mesmo algoritmo): detector de pico estéreo-linkado com decaimento (60 ms), joelho suave de 6 dB e a **redução suavizada em dB por dois tempos** (attack ~3 ms / release ~120 ms), com o **threshold na faixa do canal (−40→0 dB)** e teto de 48 dB de redução. O ganho aplicado fica quase constante dentro de um ciclo, então **não degrada o áudio**. **Ratio fixa em 5:1** (cada 1 dB acima do threshold vira 5 dB de redução); o knob **só aprofunda o threshold** (mais squash = threshold mais baixo), e um makeup de laço fechado (seguidor lento de 200 ms, teto ~+30 dB — camada Forja, já que o recompor nível no canal é feito pelo fader/trim) achata a macro-dinâmica segurando o nível — girar não derruba o volume; opt-in (0 = bypass honesto). Gesto invertido na UI: girar para a **esquerda** achata mais, a direita é o off (o knob mostra 1 − valor) (card Molho) |
| `molho` | Molho | 0–1 | 0,0 | shimmer espectral: resonador no domínio da frequência (grãos de Hann com sobreposição 4x, energia por bin com decaimento, reconstrução com **fase aleatória** — dissolve formantes, mata a voz robotizada), com **shift de intervalo fixo**: uma cópia da energia difusa sobe exatamente +1 oitava (k→2k) somada ao seco — **sem recircular, sempre harmônico**; nível/intensidade do halo (0 = off; no talo dosado a 30% do ganho antigo), no barramento depois do reverb; técnica reimplementada do Atlantis (Geraint Luff), sem copiar código (card Molho; opt-in, bypass em 0) |
| `tail` | Cauda | 0–1 | 0,5 | tempo (RT) do resonador espectral por onde o shimmer passa (0 = halo de decay curto, 1 = pad de cauda longa); é o comprimento do halo, não eco que repete a tecla; só age com o molho aberto (MOTOR) |
| `quinta` | Quinta | 0–1 | 0,0 | dose da décima-segunda (oitava + 5ª) somada por cima da oitava no shift (k→3k, além do k→2k); como o shift não é realimentado, a 12ª nunca compõe (nada de 9ª/27ª dissonante): 0 = só oitava (+12 limpo), 1 = 12ª tão presente quanto a oitava; só age com o molho aberto (card Molho; opt-in) |

Mais `bypass` (bool, padrão `false`, crossfade de 10 ms sem parar as vozes). Na aba MOTOR moram
os dez discos de ajuste fino (`lid`, `velCurve`, `reverbType`…`engineTune` e `tail`); `body` e
`ring` moram no card **Corpo**; `compressor`, `molho` e `quinta` no card **Molho**. A tela PIANO ainda
exibe o painel gráfico da Tampa, que levanta com o valor de `lid`.

## DSP

Por baixo das amostras toca o [TinySoundFont](https://github.com/schellingb/TinySoundFont)
de Bernhard Schelling (MIT), vendorizado byte a byte em
`Forja/Source/forja/vendor/soundfont/tsf.h` — o mesmo vendor do CANIVETE, mas compilado num
translation unit **próprio** (`engine/PianoEngine.cpp` com `TSF_IMPLEMENTATION`), para não
contaminar o caminho GM do CANIVETE. Sobre as amostras, a cadeia física portada
(`forja::pianoforge`) roda em ordem fixa:

1. **Martelo** (`dsp/Hammer.h`) — click de feltro + thump da caixa, tabelas `cal_[128]` por
   nota e por `hammerTone`; brilho e secura variam com a velocity (o feltro comprime no
   fortissimo); lowpass 9,5 kHz compartilhado na soma, pan só no click.
2. **Ressonância simpática** (`dsp/Resonance.h`) — banco de ressonadores ligados às teclas
   ativas e ao pedal.
3. **Resposta do tampo** (`dsp/Soundboard.h`, `SoundboardResponse`) — a curva de radiação do
   corpo de um piano de cauda em cascata de biquads: peito +5,5 dB em 92 Hz, cavidade em
   262 Hz, scoop anti-“box” em 900 Hz, articulação do bridge em 2,8 kHz, ar acima e sub
   domado em 55 Hz. Escalada por um único knob (`body`), unity em 0.
4. **Modos da madeira** (`dsp/Soundboard.h`, `SoundboardModes`) — dez ressonadores de alto Q
   nas frequências modais do tampo (70…740 Hz), excitados pela soma mono e reinjetados com
   L/R levemente desencontrados; é o anel que continua vibrando depois da nota. Aditivo puro,
   sem realimentação, bypass em 0 (`ring`).
5. **EQ de corpo** em 3 bandas (`dsp/Biquad.h`) — low shelf / peak / high shelf, trim do
   usuário sobre a curva do tampo.
6. **Tampa** (`dsp/Filters.h`) — lowpass de 2,5 kHz a 16 kHz, transparente em 1,0.
7. **Compressor** (`engine/dsp/Compressor.h`) — camada Forja de um knob, opt-in, compressor
   **original da Forja** (não é o adaptador Dyno/ÍMPETO): é **o mesmo compressor de canal do mixer
   do Drumforge**, mesmo algoritmo — detector de pico estéreo-linkado com decaimento (~60 ms), joelho
   suave de 6 dB e a **redução suavizada em dB por dois tempos** (attack ~3 ms na subida, release
   ~120 ms na descida), com o threshold na faixa do canal (−40→0 dB) e teto de 48 dB de redução. É
   isso que dá o caráter de **compressor musical de mesa, não de limitador**: o ataque é finito, então
   o ganho aplicado fica praticamente constante ao longo de um ciclo audível — nada modula dentro da
   forma de onda e **não degrada** (o teste mede THD de um tom puro < 3%). **Ratio fixa em 5:1**
   (cada 1 dB acima do threshold rende 5 dB de redução): o knob **não toca na razão, só aprofunda
   o threshold** (mais squash =
   threshold mais baixo), e soma um **makeup de laço fechado** (seguidor lento de 200 ms, teto ~+30 dB)
   que **achata a macro-dinâmica segurando o nível percebido** — girar esmaga a dinâmica sem derrubar o
   volume (no canal do Drumforge quem recompõe o nível é o fader/trim; aqui, sendo um knob só, o makeup
   faz esse papel). Em 0% (o default) o `process` não encosta na amostra: saída = entrada, bypass
   honesto. Na UI o gesto é **invertido** — girar para a **esquerda** achata mais, a direita é o off.
8. **Chorus** e **reverb FDN** (`dsp/Space.h`) — 4 salas por `reverbType`.
9. **Molho** (`dsp/Shimmer.h`) — camada Forja de worship, opt-in, no barramento depois do
   reverb: um **shimmer espectral** (não um transpositor no tempo, mas um resonador no domínio
   da frequência). O sinal é cortado em grãos de Hann (sobreposição 4x, `hop = fftSize/4`, graio
   alvo ~150 ms) por uma FFT radix-2 in-place; a **magnitude** de cada bin é acumulada num
   espectro que decai exponencialmente (o RT do halo, controlado por `tail`) com damping extra
   nos agudos; a reconstrução usa **fase aleatória** (com espelho Hermitiano para saída real) —
   é a fase aleatória que dissolve formantes e transientes e mata a voz robotizada que um
   granulador coerente impõe. Sobre esse espectro roda o **shift de intervalo FIXO**: uma cópia
   da energia difusa é reposicionada exatamente uma oitava acima (bin `k → 2k`, +12) e, na dose
   da quinta, uma décima-segunda acima (`k → 3k`, oitava + 5ª). A cópia **não** é realimentada no
   espectro, então o intervalo nunca compõe nem sobe escada — halo sempre harmônico e salto
   previsível (resposta direta a "quantas +12 eu pulo?": exatamente uma, mais a 12ª da Quinta).
   O halo sai por overlap-add, com latência de `fftSize` amostras só nele; um lowpass de 5,4 kHz
   doma o chiado dos bins altos. Três controles: `molho` = nível/intensidade do halo (0 = bypass
   honesto, amostra a amostra; no talo dosado a 30% do ganho antigo), `quinta` = dose da 12ª por
   cima da oitava (0 = só oitava), `tail` = tempo (RT) do resonador (MOTOR). Auto-pan lento abre o halo. **Técnica
   reimplementada do Atlantis Reverb (Geraint Luff), sem copiar código** — a licença do Atlantis
   não é MIT, então nada dele é vendorizado; o DSP é original da Forja, sem paridade de disco. O
   Molho **não** usa mais o reverb ClearCoat (que continua vendorizado só para o futuro plugin da
   suíte, adiado).
10. **Master** (`dsp/Master.h`) — largura, trim, volume, soft-clip.

Estado por instância, `updateBlock(frames)` uma vez por bloco com smoothing **por frames**
(`alpha = 1 − exp(−frames·invTauPerSample)` — o conserto documentado do repo antigo), buffers
pré-dimensionados para 192 kHz, nenhum `static`, nenhuma alocação em
`prepareToPlay`/`processBlock`. O pedal de sustain segue a semântica medida do projeto-irmão:
não furta voz cujo envelope ainda está preso. O bloco é **fatiado** pelos tempos dos eventos
MIDI (`kMinSlice = 16`) para a nota soar no instante certo.

Bancos `.sf2`/`.sf3` e bundles `.pforge` são depositados em `Documentos/Forja/Pianoforge`; a
carga roda em background e é publicada no áudio por token (mesmo contrato do CANIVETE).
Entrada: nenhuma (só MIDI). Saída: estéreo.

**Coexistência de `tsf` no FORJARACK**: no binário do rack, o `PianoEngine` compila o
TinySoundFont com `TSF_STATIC` (ligação interna) para conviver com a `SoundFontEngine` do
CANIVETE sem símbolos duplicados. O `stb_vorbis` (SF3) vive num único TU por binário: o
FORJARACK define `FORJA_PIANOFORGE_SHARES_STB` e a gaveta PIANOFORGE não re-inclui o
decodificador — o SF3 da gaveta fica desligado nesse alvo; o standalone e os testes o têm.

## Interface

`forja::ui::ForjaEditorBase` de 1000 × 540. Tela PIANO: cinco cards — **Dinâmica** (Volume,
Martelo), **Corpo** (Corpo, Madeira), **Equalização** (Graves/Médios/Agudos), **Molho**
(Compressor, Molho) e **Espaço** (Ressonância, Chorus, Reverb) — mais banco e saída. Os cards
**Corpo** e **Molho** ficam **achatados** para ceder palco ao painel gráfico da **Tampa**
(`ui/PianoLidArt.h`): um piano de cauda desenhado em JUCE nativo (sem assets), agora em **VISTA
DE FRENTE** (perspectiva de quem toca) — teclado embaixo, a tampa levantando atrás conforme
`lid`, a brasa do corpo dosada pelo `body`, as cordas acendendo com o `ring` e o **atril com a
partitura mostrando o NOME do banco carregado**. É display, não controle. Aba **MOTOR** com os
dez discos de ajuste fino e o painel **FUNDAMENTOS** em português (`ForjaInfoText.h`). Discos
compactos (98 px) para três por card de meia coluna sem sobreposição. Rótulos longos via
`utf8()`. Redimensionável com aspecto fixo — a arte trabalha em coordenadas fracionárias e
escala junto com o canvas.

## Testes

`forja_pianoforge_engine` (harness estilo `forja_drumforge_engine`) gera um SF2 mínimo e um
`.pforge` no próprio disco e cobre: adoção de banco e troca com reap, pedal de sustain,
fatiamento de eventos, monotonia de `lid`/`hammer`, a escala `kHammerEscala`, o mapeamento
bit-exato dos 23 parâmetros, a prova de energia no peito (banda 80–500 Hz) para `body`/`ring`,
a redução real do compressor e o sentido físico da `tail` no shimmer espectral (cortada a
entrada, a cauda longa segura o halo difuso na janela tardia enquanto a cauda domada já morre —
RT do resonador, não eco que repete a tecla), a rejeição de `.pforge` com CRC errado, a
rampa de bypass, a latência por impulso e a ausência de alocação no áudio. No FORJARACK a **gaveta instrument**
`pianoforge` é testada contra o `ModuleCatalog` (registro append-only, `create()`,
`isInstrument()`, latência 0) e de ponta a ponta (carga assíncrona esperada com orçamento
alto, saindo no primeiro sinal de áudio). O alvo é registrado em `Tests/CMakeLists.txt` e no
ctest.

## Licença e créditos

TinySoundFont, © Bernhard Schelling, MIT — `Forja/Source/forja/vendor/soundfont/tsf.h`
(v0.9) vendorizado verbatim, com `LICENSE` e `PROVENANCE.md` na mesma pasta; `stb_vorbis.c`
(v1.22, domínio público, Sean Barrett / `nothings/stb`) divide a pasta e entra só para o SF3,
no mesmo TU do `tsf.h`. Não se usa FluidSynth: a regra da suíte é manter o binário livre de
GPL. A **cadeia física** (martelo, ressonadores, EQ, tampa, chorus, FDN, master), o formato
`.pforge`, a interface, a medição e a camada de bypass são **portados** do repo-irmão
autônomo `pianoforge/` (referência histórica, fora do build da suíte) e assinados como Forja,
MIT — **sem paridade Airwindows**. O **Molho** é um shimmer espectral **original da Forja**
(`dsp/Shimmer.h`): a *técnica* (resonador de espectro com fase aleatória e shift de intervalo
fixo em +12 / +19)
foi reimplementada a partir do **Atlantis Reverb** de Geraint Luff, **sem copiar uma linha de
código** — a licença do Atlantis não é MIT, por isso nada dele é vendorizado. O **Compressor**
também é **original da Forja** (`engine/dsp/Compressor.h`): é o **mesmo compressor de canal do
mixer do Drumforge** (mesmo detector de pico, joelho 6 dB e redução suavizada em dB por dois
tempos), com ratio fixa em 5:1 e um makeup de laço fechado (camada Forja) que segura o nível —
**sem** reusar o Dyno/ÍMPETO; nenhuma adaptação Airwindows entra no PIANOFORGE.
