# ButterComp2 — provenance

- Original: Airwindows ButterComp2, Copyright (c) 2016 Chris Johnson.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-29.
- SHA-256:
  - `ButterComp2.cpp`: `49ba9d4948f939f00b7c8875e11b4140a76bcf7b3f15f6277e6ef8907a05f298`
  - `ButterComp2.h`: `16c788bff2508baf7025e50d8ff8a70216b0bad6ca56172d154d83c3230d6724`
  - `ButterComp2_plug.cpp`: `4eebcabeffd8adebe5f0034e7ea6f3c67136521a1df897d69fa186d389bb4063`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaTempera.h`.

Deviations in the Forja adapter:

- The two `static` noise sources declared inside the processing loop became
  per-instance members, so two instances no longer share state.
- Input/output/host communication, bypass smoothing, metering, APVTS and the
  Forja UI are new; the gain computer and the four parallel compressors are a
  direct transcription.
