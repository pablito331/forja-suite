# PROVENANCE — TinySoundFont (tsf.h)

Vendido byte-exato para o plugin **CANIVETE SUÍÇO** (base `Minis/`). Não é
Airwindows: vem do TinySoundFont, um sintetizador SoundFont2/3 de um único
header, licença **MIT**.

- **Upstream:** https://github.com/schellingb/TinySoundFont
- **Biblioteca / versão:** TinySoundFont **v0.9** (cabeçalho declara
  `TinySoundFont - v0.9 - SoundFont2 synthesizer`; copyright 2017-2025
  Bernhard Schelling; baseado em SFZero de Steve Folta).
- **Licença:** MIT (ver `LICENSE` nesta pasta; texto idêntico ao bloco de
  licença no topo do `tsf.h`).
- **Arquivo:** `tsf.h`
  - Tamanho: 105101 bytes
  - SHA-256: `c70cd9ea2983ac1c5026b29424c084dfba37378060985bfbdc7d978bbc1daeaa`
- **Aquisição:** cópia byte-exata do header já vendorizado no projeto irmão
  `pianoforge/src/core/tsf.h` (mesmo upstream v0.9), tomada em 2026-11-01. O
  commit exato do upstream não é registrado aqui porque a cópia local não traz
  o hash; a versão v0.9 e o SHA-256 acima são a identidade verificável.

## Como é usado

Compilado em **um único** translation unit (`canivete/SoundFontEngine.cpp`) com
`#define TSF_IMPLEMENTATION` antes do `#include "tsf.h"`. O `tsf.h` nunca é
modificado. O plugin só usa a API pública (load, canais, render).

## SF3 (amostras Ogg/Vorbis)

O `tsf.h` já traz o decodificador SF3 (`tsf_decode_sf3_samples` /
`tsf_decode_ogg`), mas ele fica atrás de `#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H`
— ou seja, só compila com **stb_vorbis** presente. Enquanto o
`Forja/Source/forja/vendor/soundfont/` não contiver `stb_vorbis.c`/`.h`, o build
é **SF2**. Ao soltar os dois arquivos do stb (domínio público, repo
`nothings/stb`) nesta pasta, o CMake detecta, define `FORJA_CANIVETE_SF3`, inclui
o `stb_vorbis.c` em modo header-only no TU da engine e compila a implementação
uma vez — acendendo o SF3 sem tocar no código da engine.

**Nunca** usar FluidSynth (LGPL): a regra da suíte é manter o binário livre de
GPL. TinySoundFont (MIT) + stb_vorbis (public domain) cobrem SF2 e SF3.

## Caminho de áudio (RT)

Estado tsf (voz/canal) vive no pool pré-alocado por `tsf_set_max_voices` e nos
16 canais pré-criados na carga (thread de mensagens). No `processBlock` só há
nota/on-off, CC, program-change (escreve `channel->presetIndex`, sem realloc) e
render no pool — nenhuma alocação. Ver `Tests/Source/canivete.cpp` (alloc_guard).
