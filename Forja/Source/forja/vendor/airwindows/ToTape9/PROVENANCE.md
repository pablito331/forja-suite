# ToTape9 — provenance

- Original: Airwindows ToTape9, Copyright (c) 2026 Chris Johnson.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-29.
- SHA-256:
  - `ToTape9.cpp`: `5e7ef427595db0eb69268843ac63d5d13531c992e82601052ff7579a9c1661c1`
  - `ToTape9.h`: `cf6fafac1003e0c849055bafcf5662a8b207870aa12e80b1a8f058ce7fb2f502`
  - `ToTape9Proc.cpp`: `38b0853e17ac403593c26c5ce825fa42995e50cf8af4b4628bebc1fc60ff509f`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaLaminador.h`.

Deviations in the Forja adapter:

- `static` locals and the pseudo-random period of the flutter were turned into
  per-instance members; the exchange of `sweepR`/`nextmaxR` between channels is
  preserved exactly as in the original.
- ClipOnly3 is shared with other models through `ClipOnly3Channel` (larger
  ring); the original buffers were already in bounds because `spacing` stops at
  16.
- The 64-bit dither is omitted; only the RNG advances, matching the original
  call order.
- Peak/GR metering, 10 ms bypass crossfade, APVTS, host bypass and the Forja UI
  are new.
