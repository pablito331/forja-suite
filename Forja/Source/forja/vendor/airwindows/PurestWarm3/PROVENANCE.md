# PurestWarm3 — provenance

- Original: Airwindows PurestWarm3, Copyright (c) Airwindows.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-30.
- SHA-256:
  - `PurestWarm3.cpp`: `1b11bc93135ceb497789ea7e675bc913ee0a3d42bbb2c21e221ba6d53ccd5dd8`
  - `PurestWarm3.h`: `07ce1c84ad8989019c081011aa7dac37632a0de7c026dd148901bc75c56c76f6`
  - `PurestWarm3Proc.cpp`: `27d9a257a4cbf21b76a88259632cfcf2a8fa2d717c633cbeb4d3cae9adb5f19a`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaCera.h`.

Deviations in the Forja adapter:

- The 64-bit dither is omitted; only the RNG advances, including the asymmetric
  extra step the original runs on `fpdR`. `fpdL`/`fpdR` are seeded through the
  documented test hook so the harness can reproduce Airwindows' `rand()` seed.
- Peak metering, 10 ms bypass crossfade, APVTS, host bypass and the Forja UI are
  new.
