# Dyno — provenance

- Original: Airwindows Dyno, Copyright (c) Airwindows.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-30.
- SHA-256:
  - `Dyno.cpp`: `c470c473b65915c84a7136344c622ee9889e3133794fb4af96f571eb3202ed13`
  - `Dyno.h`: `6845f837b8af7924df82195041329243f957dbe044af8de24687255293c770f2`
  - `DynoProc.cpp`: `5e014caeb6f8d48daf751563a5ab88c4d1bbe9e2881b50ed800d0b535fa72b80`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaImpeto.h`.

Deviations in the Forja adapter:

- The 64-bit dither is omitted; only the RNG advances. `fpdL`/`fpdR` are seeded
  through the documented test hook so the harness can reproduce Airwindows'
  `rand()` seed.
- Peak metering, 10 ms bypass crossfade, APVTS, host bypass and the Forja UI are
  new.
