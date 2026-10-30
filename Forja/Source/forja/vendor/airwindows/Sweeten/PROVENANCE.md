# Sweeten — provenance

- Original: Airwindows Sweeten, Copyright (c) Airwindows.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-30.
- SHA-256:
  - `Sweeten.cpp`: `d7ee42e318f20bf4424a54ea50c228ac6fed4827bdb1f15eea1b04fd4739bf60`
  - `Sweeten.h`: `acd958baa57aefbfcf3b131d8a4a3ffbe7a950626233f8da63551ff7c68b246f`
  - `SweetenProc.cpp`: `fa6ab2d18c500b307e82c35ea97601d1f83ff0435fc2a89031329af46ad62197`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaMel.h`.

Deviations in the Forja adapter:

- The 64-bit dither is omitted; only the RNG advances. `fpdL`/`fpdR` are seeded
  through the documented test hook so the harness can reproduce Airwindows'
  `rand()` seed.
- Peak metering, 10 ms bypass crossfade, APVTS, host bypass and the Forja UI are
  new.
