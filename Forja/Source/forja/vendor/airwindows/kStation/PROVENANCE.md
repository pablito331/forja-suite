# kStation — provenance

- Original: Airwindows kStation, Copyright (c) Airwindows (created 2011).
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-29.
- SHA-256:
  - `kStation.cpp`: `173c460dad1898e4ca01d6d96eb155c65c7062c2b7f272c845390e096becb303`
  - `kStation.h`: `8fab8093ca3d455f843f2656903f25a74fa8aa46b6602fa742822f1c84c91fcf`
  - `kStationProc.cpp`: `936ebeedf00ec108beae9d598210df7c9a5295366adcea8b7c93a0c2d9bf4bb9`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaFolego.h`.

Deviations in the Forja adapter:

- The 88 delay lines (3×3 early reflections and the six 6×6 stages, per
  channel) live on the heap (std::vector) because the DSP type is also created
  on the stack by the parity harness. Allocation only happens on construction;
  `process()` never allocates.
- The 64-bit dither is omitted; only the RNG advances. `fpdL`/`fpdR` are seeded
  through the documented test hook so the harness can reproduce Airwindows'
  `rand()` seed.
- The internal space, the early reflections and both Bézier stages never
  participate in PDC: `latencySamples()` is 0.
- Peak metering, 10 ms bypass crossfade, APVTS, host bypass and the Forja UI are
  new.
