# ClearCoat — provenance

- Original: Airwindows ClearCoat, Copyright (c) Airwindows.
- License: MIT (see `LICENSE`).
- Upstream: `master` branch of github.com/airwindows/airwindows
  (path `plugins/WinVST/ClearCoat/`).
- Downloaded: 2026-11-03.
- SHA-256:
  - `ClearCoat.cpp`: `732767a6a81d370f32a2aeb83c3ecd0e94f6bad0fbc185524ca1216a4fe7619f`
  - `ClearCoat.h`: `cdada686cccf797b12f61021555a30b749ce8bef6abd89ab7448ce9e7a0d0b2d`
  - `ClearCoatProc.cpp`: `4299738153acbaf59d81eb0ee380754e1d241c7e110a7319eb77b400f5c4ce64`

These files are reference material only. They are not compiled into the Forja
plugin. The shipping algorithm lives in `Forja/Source/forja/dsp/ForjaClearCoat.h`.

Deviations in the Forja adapter:

- The 64-bit dither is omitted; only the RNG advances.
- Peak metering, 10 ms bypass crossfade, APVTS, host bypass and the Forja UI are
  new.
- The 16x2 delay-line buffers are heap-allocated (std::vector) in prepare() to
  avoid stack overflow in the test harness.
