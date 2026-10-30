# Mojo — provenance

- Original: Airwindows Mojo, Copyright (c) Airwindows.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-30.
- SHA-256:
  - `Mojo.cpp`: `6c17a1eb47a0ac2d67a55026388b541fdc4ab25bb830b33a810a06530d80d156`
  - `Mojo.h`: `480db13a898fb036649c3dd987ba9fcd2c2bd906c14e9d1274f2698e3e891eca`
  - `MojoProc.cpp`: `f5af4031dc709d0f30723f5b08889f3f021ca98329158083d5145b79bfbfeaad`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaCorpo.h`.

Deviations in the Forja adapter:

- The 64-bit dither is omitted; only the RNG advances. `fpdL`/`fpdR` are seeded
  through the documented test hook so the harness can reproduce Airwindows'
  `rand()` seed.
- Peak metering, 10 ms bypass crossfade, APVTS, host bypass and the Forja UI are
  new.
