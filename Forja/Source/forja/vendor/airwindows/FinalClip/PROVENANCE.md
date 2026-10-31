# FinalClip — provenance

- Original: Airwindows FinalClip, Copyright (c) Chris Johnson.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-30.
- SHA-256:
  - `FinalClip.cpp`: `909c5f199e1c1cca2ec57e64ab809a2acf9953018b27067ea6eb3eb24ccfe489`
  - `FinalClip.h`: `6ff92a3a25bbc7bfa15d03a11394b67eb4fe73324793ddfbfd8b2de85d932b23`
  - `FinalClipProc.cpp`: `6a9aabf656947a65a2a3a454957b81c3044fb905d7139a0bf7fb59fd2bf9943b`
  - `LICENSE`: `b4f5719b00ba3fecf8c0ce651e843651bd80f08a10cf4cf1f00e673a5bcaf23b`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaDique.h`.

Deviations in the Forja adapter:

- `processDoubleReplacing` is transcribed literally, including the ±4 pre-clamp,
  the slew limit and the ClipOnly2 stage.
- The original `intermediate[16]` array is written at index `spacing` (up to 16
  at 705.6 kHz), so the adapter uses an 18-slot ring to stay in bounds. At
  sample rates up to 192 kHz (`spacing` 4) the output is unchanged.
- FinalClip has no parameters and no dither state. Forja adds bypass smoothing,
  peak metering and a measured latency of `spacing` samples. No heap allocation,
  logging or locks in process.
