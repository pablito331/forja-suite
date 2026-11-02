# CANIVETE SUÍÇO — player SoundFont SF2/SF3 (Forja)

Mini da base `Minis/`. Formato compacto (560 × 260), instrumento multi-timbral, e funciona
como gaveta de instrumento dentro do FORJARACK.

## Estado

| Item | Estado |
| --- | --- |
| Paridade com vendor | Não se aplica: o timbre vem do TinySoundFont, não há adapter Airwindows |
| Teste próprio | `forja_canivete` (gera um SF2 mínimo no harness) |
| Guarda de alocação | Zero alocação no caminho de áudio |
| Latência | 0 — o `tsf` entrega no mesmo bloco; a cauda depende do bank |
| SF3 (amostras Ogg) | Ligado por `FORJA_CANIVETE_SF3` quando `stb_vorbis.c` está na pasta de vendor (hoje presente: o build lê `.sf3` de fábrica) |
| Build | VST3 (Windows, macOS, Linux) |

## Parâmetros

| Nome | Tipo | Faixa | Default | Nota |
| --- | --- | --- | --- | --- |
| `bank` | choice | banks varridos da pasta | índice 0 | a lista é varrida antes do APVTS nascer |
| `volume` | float | 0–1, passo 0,01 | 0,50 | trim geral de saída |
| `bypass` | bool | — | `false` | crossfade de 10 ms, sem resetar o bank carregado |

O seletor INSTRUMENTO da interface é camada Forja: ele expõe o banco de instrumentos do
bank no canal melódico 0. Program change do host continua mandando no canal — o seletor não
substitui o MIDI, só dá acesso visual ao mesmo mapa.

## DSP

O DSP **não é nosso**: é o [TinySoundFont](https://github.com/schellingb/TinySoundFont) de
Bernhard Schelling (MIT), vendorizado byte a byte em
`Forja/Source/forja/vendor/soundfont/tsf.h`. A Forja acrescenta só o wrapper em tempo real
(`SoundFontEngine`):

- carga assíncrona do bank, para o parse não travar o áudio nem a UI;
- adoção por token, sem alocar no caminho de áudio;
- reap do bank substituído (timer de 8 Hz na thread de mensagens);
- medidor por bloco e a camada de bypass de 10 ms.

O estado do `tsf` vive no pool pré-alocado por `tsf_set_max_voices` e nos 16 canais criados
na carga; no `processBlock` só há nota/on-off, CC, program change e render no pool.

Bancos `.sf2` e `.sf3` são depositados em `Documentos/Forja/Canivete`; a varredura roda na
abertura e quando a pasta muda. 16 canais MIDI, polifonia e cura de envelopes do próprio
`tsf`. Entrada: nenhuma. Saída: estéreo.

## Interface

Seletor de BANK, seletor de INSTRUMENTO, knob de VOLUME e medidor de saída com CLIP, no
chrome Forja de sempre. Redimensionável com aspecto fixo.

## Testes

`forja_canivete` gera um SF2 mínimo válido no harness e cobre silêncio sem bank, carga e
adoção, trim de volume, latência 0 por impulso, resposta à velocidade, rampa de bypass,
medidor, troca de bank com reap, bank inválido, CC e program change multi-timbral, escolha
de instrumento pelo canal 0 e ausência de alocação no áudio. No FORJARACK a gaveta
`canivete` é testada de ponta a ponta (`forjarack_processor` e `forjarack_vst3_midi`): a
lista de instrumentos aparece na gaveta e a troca pelo seletor muda o preset em uso. Como a
carga vai ao background, um bank grande demora — o `MuseScore_General.sf3` (38 MB de amostras
Ogg) leva ~8 s para assentar nesta máquina, e o seletor da gaveta só existe depois disso; os
testes esperam a carga em vez de assumir latência zero.

## Licença e créditos

TinySoundFont, © Bernhard Schelling, MIT — `Forja/Source/forja/vendor/soundfont/tsf.h`
(v0.9) vendorizado verbatim, com `LICENSE` e `PROVENANCE.md` na mesma pasta. `stb_vorbis.c`
(v1.22, domínio público, Sean Barrett / `nothings/stb`) divide a pasta e entra só para o
SF3, incluído antes do `tsf.h` no mesmo translation unit. Não se usa FluidSynth: a regra da
suíte é manter o binário livre de GPL. Wrapper, interface, medição e camada de bypass:
Forja, MIT.
