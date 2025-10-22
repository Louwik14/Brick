# Q1.8e — QuickStep boundary fix

## TL;DR
- QuickStep step toggles now stamp a hot cache entry (note/vel/length) when arming a step, and keep the bridge generation in sync.
- The reader-only runner consults this cache at step boundaries to force ON events when the view omits a playable voice (e.g. vel=0), preserving note retriggers.
- Added a dedicated host regression exercising consecutive QuickStep hits (nominal, edge lengths, long bursts, silent-view fallback).

## Details
- Introduced `apps/quickstep_cache.{c,h}` (hot CCM storage, 16×64×4 slots) with `init/set_active/mark/fetch/disarm` helpers.
- `apps/seq_led_bridge.c` now initialises and drives the QuickStep cache when toggling steps and switching pattern focus.
- `apps/seq_engine_runner.c`:
  - Tracks OFF events per slot per tick, fetches QuickStep cache entries, and injects NOTE_ONs when the step view is non-playable but a recent QuickStep edit exists.
  - Always disarms cache entries after processing a step; runner init resets cache.
- Added `tests/seq_quickstep_boundary_tests.c` (standalone host binary) + Makefile wiring / check-host execution.
  - Verifies consecutive identical notes (1/1, 2→1), 512-tick bursts, and silent-view fallback.
- Updated host stubs (`seq_runner_smoke_tests`, UI hold/mode/pmute) and make rules to link the new cache everywhere `seq_led_bridge.c` is compiled.

## Tests
- `make check_no_engine_anywhere`
- `make check_no_legacy_includes_apps`
- `make check_no_cold_refs_in_apps`
- `make check-host`
- `./build/host/seq_runner_smoke_tests`
- `./build/host/seq_quickstep_boundary_tests`
- `make -j8` *(fails: cross toolchain absent in container — arm-none-eabi-gcc missing)*
