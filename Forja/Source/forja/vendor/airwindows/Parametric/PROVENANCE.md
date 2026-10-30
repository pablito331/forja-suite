# Parametric — provenance

- Original: Airwindows Parametric, Copyright (c) 2015 Chris Johnson.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-29.
- SHA-256:
  - `Parametric.cpp`: `598acdf96f653b96c079baf2b21045483a4a92d0545ff0044c82de0fa9a4cdf9`
  - `Parametric.h`: `c51b723cd155360336664fbf30adb9a98f30a9510c57748e32703c865284793b`
  - `ParametricProc.cpp`: `ca08517572235b003f297ff958dff1c763eece322f4d0d13036e428b720c7fd7`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaCinzel.h`.

Deviations in the Forja adapter:

- The per-sample state is per instance; the original already keeps all filter
  memory as members, so the static path is a direct transcription.
- The 32-bit float path of the original generates dither through the same RNG
  that `processDoubleReplacing` advances; Forja only reproduces the double
  path, which is the one validated sample by sample.
- A Forja layer adds a per-band dynamic detector (`dyn`). With `dyn = 0` the
  detector is never touched and the output is bit-identical to Parametric.
  Above zero the band gain is modulated on top of the original filter bank;
  this is a documented Forja deviation, not Airwindows behaviour.
- Input/output/host communication, bypass smoothing, metering, APVTS and the
  Forja UI are new.
