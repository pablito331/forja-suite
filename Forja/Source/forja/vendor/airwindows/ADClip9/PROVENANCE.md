# ADClip9 — provenance

- Original: Airwindows ADClip9, Copyright (c) Chris Johnson.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-30.
- SHA-256:
  - `ADClip9.cpp`: `4367991b41fefd5eaf4608b35951100bd36cb41d62f6535a26623e3509a17f6b`
  - `ADClip9.h`: `263579bf38c527c3f36d98bd718ceb97e7d371336ec5efcd73eef93b39a5f70c`
  - `ADClip9Proc.cpp`: `a16ef93bbac97731c037f03c7834db92f1c265dd944234f9ec67d4258cc7e6b6`
  - `LICENSE`: `b4f5719b00ba3fecf8c0ce651e843651bd80f08a10cf4cf1f00e673a5bcaf23b`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaMuralha.h`.

Deviations in the Forja adapter:

- `processDoubleReplacing` is transcribed literally, including the ClipOnly3
  stage, the mode switch and the 64-bit dither sequence.
- The adapter keeps the original buffers (`intermediate[18]`, `slew[34]`),
  which already cover `spacing == 16` at 705.6 kHz.
- Forja adds bypass smoothing, peak metering, APVTS/UI wiring and a measured
  latency of `spacing` samples. No heap allocation, logging or locks in process.
