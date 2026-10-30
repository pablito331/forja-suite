# X2Buss — provenance

- Original: Airwindows X2Buss, Copyright (c) 2021 Chris Johnson.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-29.
- SHA-256:
  - `X2Buss.cpp`: `3e31c8ac80552df3f6671340d3fbc68902c67a03fee01464d3814d0471fa2106`
  - `X2Buss.h`: `84495a401ff532cad1b8f3f5d10120b19d86a3ac68dcaf3941c4ad252fa645d5`
  - `X2BussProc.cpp`: `97fbcd9c0f5fce8607fb9bad2967009dd63547fe08de58bea13e4da5c43ee403`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaCadinho.h`.

Deviations in the Forja adapter:

- `processDoubleReplacing` is transcribed literally: the golden-ratio
  exponential "Console" pad at the input, three interleaved crossover stages,
  the exponential IIR stage, SmoothEQ2/Dynamics2, the smoothed fader and
  ClipOnly3.
- The original's ClipOnly3 arrays are declared with 18/34 slots for spacing up
  to 17, which goes out of bounds above ~372 kHz (`spacing = 16` already uses
  index 32 in a 34-slot array; the constructor only clears 17/33). The port
  uses 33/65-slot buffers so it stays defined at any sample rate; parity is
  verified sample by sample up to 352.8 kHz, where the original is in bounds.
- ClipOnly3's interpolation ring collapses to a one-sample hold as written, so
  the measured impulse latency is 1 at every sample rate; `latencySamples()`
  reports 1.
- The bezier control voltages start at 1.0, matching the X2Buss constructor.
- Forja adds bypass smoothing, peak metering and a gain-reduction readout from
  Dynamics2. The 64-bit dither is commented out in the original too; only the
  RNG advances. No heap allocation, logging or locks in process.
