# DeRez5 — provenance

- Original: Airwindows DeRez5, Copyright (c) Airwindows (created 2011).
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-30.
- SHA-256:
  - `DeRez5.cpp`: `5d33932182b40952f73da55396c87b8bacd78279c65f99529c270c3552dd6703`
  - `DeRez5.h`: `fe24efcd9ba61b7e4ddf187a5577a2664f2b0ada1a11bdb9f6eb01616eaab752`
  - `DeRez5Proc.cpp`: `4892f51bbb64a621fe949b5961ddc9739faa97b41c28d645682cdf92bea282b1`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaGranalha.h`.

Deviations in the Forja adapter:

- The 64-bit dither is omitted; only the RNG advances, including the asymmetric
  extra step the original runs on `fpdR`. `fpdL`/`fpdR` are seeded through the
  documented test hook so the harness can reproduce Airwindows' `rand()` seed.
- Peak metering, 10 ms bypass crossfade, APVTS, host bypass and the Forja UI are
  new.
