# Slew3 — provenance

- Original: Airwindows Slew3, Copyright (c) Airwindows.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-30.
- SHA-256:
  - `Slew3.cpp`: `0d201b5304320c57001bc09049c01d9f4287e05bc9fbf0016efe0bd79c333486`
  - `Slew3.h`: `00027490af23d3d03b8922a32a4ed025688d4201f8c52fc7996cc5456593acd0`
  - `Slew3Proc.cpp`: `2e4b21e77a67029f5437f5c753bcdc8936ebf74af933d8f3ed52a2000c7ab295`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaRebarba.h`.

Deviations in the Forja adapter:

- The 64-bit dither is omitted; only the RNG advances. `fpdL`/`fpdR` are seeded
  through the documented test hook so the harness can reproduce Airwindows'
  `rand()` seed.
- Peak metering, 10 ms bypass crossfade, APVTS, host bypass and the Forja UI are
  new.
