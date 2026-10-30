# IronOxide5 — provenance

- Original: Airwindows IronOxide5, Copyright (c) 2026 Chris Johnson.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-29.
- SHA-256:
  - `IronOxide5.cpp`: `646ca4741282b72e6b0cfa8d7278b253e2b16962a5034bcbed4ab6e31b7ce91e`
  - `IronOxide5.h`: `b501e8ac3f22b8ce46997146f1ab7378bfd5d5ff599d4f8c75c93690c681b3c9`
  - `IronOxide5Proc.cpp`: `f47f52dad6c1cc235a3e769607e527c3210736c940ecc30da8ed6522568362d4`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaOxido.h`.

Deviations in the Forja adapter:

- `static` locals and the pseudo-random flutter/noise draws were turned into
  per-instance members; call order is unchanged.
- The 64-bit dither is omitted; only the RNG advances.
- Unused members of the original (`iirSamplehA/B`, `fastIIH*`, `slowIIH*`) were
  dropped; they never touch the signal.
- Peak metering, 10 ms bypass crossfade, APVTS, host bypass and the Forja UI are
  new.
