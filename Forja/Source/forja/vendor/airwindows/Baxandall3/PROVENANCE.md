# Baxandall3 — provenance

- Original: Airwindows Baxandall3, Copyright (c) 2018 Chris Johnson.
- License: MIT (see `LICENSE`).
- Upstream commit: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
  (Airwindows repository, master).
- Downloaded: 2026-10-29.
- SHA-256:
  - `Baxandall3.cpp`: `4c9c97f79c7fe438feb5aba00903f3f6acaabcb37e59654a5d0c94d27c798609`
  - `Baxandall3.h`: `4e960ef21b2e0ba7900ccee3f56efa13c8f6b42404ee280eed1ffc1beb1cd984`
  - `Baxandall3Proc.cpp`: `7143cebaa5642891b0ab38513de96827c323be9e4e0018187a7d1faa4cc19c78`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaTorno.h`.

Deviations in the Forja adapter:

- The original alternates between two interleaved state sets (A/B) per sample
  through the `flip` flag; that alternation is reproduced exactly.
- No coefficient, gain law or Console5 encode/decode is altered. In
  particular, the algorithm is only neutral at Input = Treble = Bass = 0.5;
  that coupling is original behaviour and is kept.
- Input/output/host communication, bypass smoothing, metering, APVTS and the
  Forja UI are new.
