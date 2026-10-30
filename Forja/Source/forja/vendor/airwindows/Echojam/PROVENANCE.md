# Echojam — provenance

- Original: Airwindows Echojam, Copyright (c) Airwindows (created 2011).
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-29.
- SHA-256:
  - `Echojam.cpp`: `114b9215f8ba8847fd6db7e880a5f9007e24ce634cc7df9dbd31a91d80e54b57`
  - `Echojam.h`: `b5ac8baf47e9a3d844d21fa67f70a35f566d7cc52ab2767d7f22c391b5a74f0a`
  - `EchojamProc.cpp`: `c3611b266cdf187b258bb50920e84b08979082837a0a38deca7fb045ff93eb4b`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaEco.h`.

Deviations in the Forja adapter:

- The 32 delay lines (`d4A`…`d4P` per channel) live on the heap (std::vector)
  because the DSP type is also created on the stack by the parity harness.
  Allocation only happens on construction; `process()` never allocates.
- The 64-bit dither is omitted; only the RNG advances, including the asymmetric
  extra step the original runs on `fpdR`. `fpdL`/`fpdR` are seeded through the
  documented test hook so the harness can reproduce Airwindows' `rand()` seed.
- The internal delay never participates in PDC: `latencySamples()` is 0 and the
  host must not compensate the musical space.
- Peak metering, 10 ms bypass crossfade, APVTS, host bypass and the Forja UI are
  new.
