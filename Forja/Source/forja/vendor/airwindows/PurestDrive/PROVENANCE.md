# PurestDrive — provenance

- Original: Airwindows PurestDrive, Copyright (c) Airwindows (created 2016).
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-30.
- SHA-256:
  - `PurestDrive.cpp`: `6a26336d66aef7a7b4d21eef4278c3f6ca9d421ca040f5ddbb67ab3fad3066a9`
  - `PurestDrive.h`: `0ab062e61fd26d37dc9741667711fbcc33a8defc1e97a8bf51ffe53429951e2b`
  - `PurestDriveProc.cpp`: `9a3c397e64bd64ee36e109c7feb7d044b4e6c44087420bfb582e240fcec47920`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaLixa.h`.

Deviations in the Forja adapter:

- The 64-bit dither is omitted; only the RNG advances. `fpdL`/`fpdR` are seeded
  through the documented test hook so the harness can reproduce Airwindows'
  `rand()` seed.
- Peak metering, 10 ms bypass crossfade, APVTS, host bypass and the Forja UI are
  new.
