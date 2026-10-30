# Pressure5 — provenance

- Original: Airwindows Pressure5, Copyright (c) 2016 Chris Johnson.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-29.
- SHA-256:
  - `Pressure5.cpp`: `687c274910628cd33f9d7a9b66b2f2b998fd6a9204d7a49db0776beb1a60eb6e`
  - `Pressure5.h`: `cbf03f9c473c0ffdd8e0e2943a637f4b137861624855e057a82d9816f3af3c2f`
  - `Pressure5_plug.cpp`: `34c37209a3fdbce6ccc21ab892b1d62195245ffed08c54c3d5640651f02922d9`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaPrensa.h`.

Deviations in the Forja adapter:

- `muNewSpeed` and `muAttack` are initialized to 0 in every instance; the
  original constructor leaves them uninitialized.
- `intermediateL`/`intermediateR` are sized 17 so `spacing == 16` no longer
  writes out of bounds at very high sample rates.
- The reported latency is 1 sample. The original reports 0, but its descending
  shift delays the signal by one sample.
- Bypass smoothing, metering, APVTS and the Forja UI are new. The detector,
  vari-mu gain computer, fixed ultrasonic filters and ClipOnly2 are a direct
  transcription.
