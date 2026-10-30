# kRockstar2 — provenance

- Original: Airwindows kRockstar2, Copyright (c) Airwindows (created 2011).
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-29.
- SHA-256:
  - `kRockstar2.cpp`: `3ff245e6b613055248dd61cbd4e31cf06bdee43446c8e0642494bac7d13c68ba`
  - `kRockstar2.h`: `41aa4b63ceafbe2f58c8f712a8138788fbd751efbd0cc041859ca5f99e788613`
  - `kRockstar2Proc.cpp`: `4d09f408cc9d9fffdab6006c6a30891d314763c9515530b55a13dc25481af76d`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaSalao.h`.

Deviations in the Forja adapter:

- The 90 delay lines (3×3 early reflections and the six 6×6 stages, per
  channel) live on the heap (std::vector) because the DSP type is also created
  on the stack by the parity harness. Allocation only happens on construction;
  `process()` never allocates.
- The 64-bit dither is omitted; only the RNG advances, including the asymmetric
  extra step the original runs on `fpdR`. `fpdL`/`fpdR` are seeded through the
  documented test hook so the harness can reproduce Airwindows' `rand()` seed.
- The internal space, the early reflections and both Bézier stages never
  participate in PDC: `latencySamples()` is 0.
- Peak metering, 10 ms bypass crossfade, APVTS, host bypass and the Forja UI are
  new.
