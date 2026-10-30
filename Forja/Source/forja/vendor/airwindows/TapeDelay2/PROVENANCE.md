# TapeDelay2 — provenance

- Original: Airwindows TapeDelay2, Copyright (c) 2016 Chris Johnson.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-29.
- SHA-256:
  - `TapeDelay2.cpp`: `c9a8b6bf4f10b7eb9a2f1378d71b40f4a09b2a583ab51731ba346f0ade31d9e2`
  - `TapeDelay2.h`: `f86a116b61ca2792cd5f17498e09cdafc1f21b0b6cf1d0bfb08eb6b855609f25`
  - `TapeDelay2Proc.cpp`: `a822cba0eb1c6c9e3f8eb835ad220699d132cc9c0facfbba800d7739605c6e`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaRepique.h`.

Deviations in the Forja adapter:

- The 88211-sample rings (`dL`/`dR`) live on the heap (std::vector) because the
  DSP type is also created on the stack by the parity harness. Allocation only
  happens on construction; `process()` never allocates.
- The 64-bit dither is omitted; only the RNG advances, matching the original
  call order. `fpdL`/`fpdR` are seeded through the documented test hook so the
  harness can reproduce Airwindows' `rand()` seed.
- The ring never participates in PDC: `latencySamples()` is 0 and the host must
  not compensate the musical delay.
- Peak metering, 10 ms bypass crossfade, APVTS, host bypass and the Forja UI are
  new.
