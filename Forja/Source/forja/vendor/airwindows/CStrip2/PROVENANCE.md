# CStrip2 — provenance

- Original: Airwindows CStrip2, Copyright (c) 2020 Chris Johnson.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-29.
- SHA-256:
  - `CStrip2.cpp`: `59d62ecd5913664aa5be0da61e2adb9f7684f5c9df0c8527b3193e38e9f2db4f`
  - `CStrip2.h`: `19cd087e9a8f11ec0ad7f5570e1be5c717f8c990d793f26163bd3f237fce9409`
  - `CStrip2Proc.cpp`: `1de4a2e55211c0ec469432aea9c411d6eeef603599be15b18f5f4254f72ffeb3`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaBigorna.h`.

Deviations in the Forja adapter:

- `processDoubleReplacing` is transcribed stage by stage: gearbox capacitor,
  ButterComp, the triple interleaved EQ with its drive section, and Spiral.
- `pow(float, int)` promotes to the double overload; all four powers (low cap,
  high cap, compres, comp speed) are computed in double exactly like the
  original. Rounding them in float first causes audible parity drift.
- The original keeps two different scale variables (`overallscale` = raw sample
  rate at the IIR stage, `compscale` = sr/44100). The adapter keeps both.
- `flipthree` starts at 0, matching the original constructor, not at 1.
- Forja adds bypass smoothing, peak metering and a gain-reduction readout from
  the compressor's `totalmultiplier`. The 64-bit dither is commented out in the
  original too; only the RNG advances. No heap allocation, logging or locks in
  process.
