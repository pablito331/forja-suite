# Pop2 — provenance

- Original: Airwindows Pop2, Copyright (c) 2016 Chris Johnson.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-29.
- SHA-256:
  - `Pop2.cpp`: `8a0a53f88c025fb6538746da27656f741429d2e1b2a3f833129a93fb3ec03ca5`
  - `Pop2.h`: `a7bae152a732099c1a7c3cb3301106f0ba2b0d0dd6579332480c9af35f4d4234`
  - `Pop2_plug.cpp`: `e88f8a55cd7407a47a30c768ddd939f9fe8c9e139508360f7c1a93e2c1e3c682`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaMalho.h`.

Deviations in the Forja adapter:

- `intermediateL`/`intermediateR` are sized 17 so `spacing == 16` no longer
  writes out of bounds at very high sample rates.
- The reported latency is 1 sample. The original reports 0, but its descending
  shift delays the signal by one sample.
- Bypass smoothing, metering, APVTS and the Forja UI are new. The dual
  envelopes, attack/release mapping, drive gain and ClipOnly2 are a direct
  transcription.
