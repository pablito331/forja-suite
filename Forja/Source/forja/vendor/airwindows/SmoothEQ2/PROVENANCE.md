# SmoothEQ2 — provenance

- Original: Airwindows SmoothEQ2, Copyright (c) 2019 Chris Johnson.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-29.
- SHA-256:
  - `SmoothEQ2.cpp`: `c51040f082e2e4df6858f1248c17995560ab940cc9f327ace01887d9fdcd0352`
  - `SmoothEQ2.h`: `25d7653a82d0cca689d21715a5a25eccc8699e07d8a8a69a0e45ec488beca5e1`
  - `SmoothEQ2Proc.cpp`: `d8c514a7e58b5889fc25dc9eba68193f753e4c67a01220d7d6e98b22ab1e7c3a`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaLima.h`.

Deviations in the Forja adapter:

- The three cascaded crossover stages, the exponential stage and the four
  band gains are a direct transcription of `processDoubleReplacing`.
- The band gain law `1 + x·|x|·|x|` with `x = (A − 0.5)·2` is reproduced
  exactly; treating the raw 0–1 value as a gain is a known trap and is not
  done here.
- Input/output/host communication, bypass smoothing, metering, APVTS and the
  Forja UI are new.
