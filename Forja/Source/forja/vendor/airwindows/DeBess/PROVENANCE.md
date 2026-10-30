# DeBess — provenance

- Original: Airwindows DeBess, Copyright (c) 2018 Chris Johnson.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-29.
- SHA-256:
  - `DeBess.cpp`: `630656bf5c6f5b99cc9513431f240b61cda441f0cbb39c0aa67549dced340570`
  - `DeBess.h`: `b937710a0db4e3c2f59e464fae46eaef4e73922533055843aba5c3577fa9d1a0`
  - `DeBessProc.cpp`: `88c1d888afb772433a85d065190715d5ae893dec889696a9ebcdf0481fb17de8`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaEscoria.h`.

Deviations in the Forja adapter:

- `processDoubleReplacing` is transcribed literally, including the branch that
  tests the A-side ratios inside the B-side update (line 184 of the original
  `DeBessProc.cpp`, double path); it is preserved rather than "fixed".
- `pow(A, 5)` resolves to the double overload of `pow`, so the intensity is
  computed in double, not rounded in float first.
- Forja adds bypass smoothing, peak metering and APVTS/UI wiring. No heap
  allocation, logging or locks in process.
