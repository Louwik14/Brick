# Brick Sequencer & UI Refactor — Audit and Roadmap

## 1. Repository overview
- **Target MCU / OS**: STM32F429 + ChibiOS 21.11.x (confirmed by `agents.md`).
- **Current build options**: `Makefile` still defaults to legacy `USE_OPT=-O2`; warnings such as `-Wall/-Wextra` are not enforced.
- **Subsystem folders**: `core/`, `apps/`, `ui/`, `drivers/`, `cart/`, `midi/`, `usb/`. The split generally matches the README guidance, but several files cross boundaries through helper bridges (e.g. `seq_led_bridge`, `ui_keyboard_bridge`).

## 2. Dependency map (high level)
```
ui_task -> ui_input -> drivers (buttons)
        -> ui_shortcuts -> ui_overlay/ui_model/ui_led_backend
        -> clock_manager (start/stop, callback)
        -> seq_led_bridge (playhead + UI quick-step)
        -> ui_keyboard_* (bridge & app)

ui_shortcuts -> ui_overlay/ui_model/ui_led_backend
             -> cart_registry (cart specs for overlays)
             -> seq_led_bridge (page / p-lock preview)
             -> clock_manager (transport)
             -> ui_keyboard_app (octave state)
             -> ui_mute_backend
```
- `ui_shortcuts` owns **runtime state** for mute, overlays, keyboard tag, and step hold masks. This state is then queried by `ui_task` (e.g. `ui_shortcuts_is_keys_active`) and by LED backends.
- `ui_task` duplicates responsibilities (transport REC toggle, overlay naming for keyboard, etc.) that should live in a UI state manager (`ui_backend`).
- `clock_manager` is used directly in UI for transport; the sequencer engine still lives elsewhere (`core/seq_*` is missing).

## 3. Problems & technical debt
1. **Mixed responsibilities** in `ui_shortcuts.c`:
   - Input decoding, runtime mode tracking, transport control, and LED/UI state updates are interleaved.
   - Static globals (`s_mute`, `s_keys_active`, `s_seq_btn_down`) prevent inspection from outside and make testing difficult.
   - Overlay banners are cloned locally with manual tag patching (`s_seq_mode_spec_banner`, …), leading to hidden coupling with `ui_overlay`.
2. **`ui_task.c` handles state transitions** (record toggle, keyboard label updates, page routing) instead of delegating to a backend/state manager. This makes the render loop non-declarative.
3. **Sequencer core missing**: there is no `seq_model`/`seq_engine` layer. The UI bridges (`seq_led_bridge`) assume immediate LED reactions but there is no data model to back p-locks, quantize, etc.
4. **Tools & hygiene**:
   - `Makefile` does not enforce `-Wall -Wextra`.
   - No cppcheck/clang-tidy integration or reports.
   - Several headers miss uniform include guards and Doxygen preamble.
5. **Potential dead/duplicate code**:
   - `ui_task` keeps a commented include `// #include "seq_led_bridge.h"` suggesting unused code paths.
   - `ui_shortcuts` re-implements keyboard tag management duplicated from `ui_task` (`_update_keyboard_overlay_label_from_shift`).
   - Need to inspect `ui_led_seq.c` vs `seq_led_bridge.c` to remove any redundant state machines.

## 4. Proposed incremental plan (each step == separate PR)
1. **Foundational tooling pass** ✅
   - `Makefile` now enables `-Wall -Wextra` via an opt-out flag `USE_WARNINGS=no` for legacy builds.
   - Added `make lint-cppcheck` target invoking `cppcheck --enable=warning,style,performance --std=c11` on `core/` and `ui/`.
   - Ensure headers touched receive unified Doxygen block + `#ifndef/#define/#endif` guard pattern.
2. **UI state extraction (shortcuts/backend)** ✅
   - Added `ui_mode_context_t` to `ui_backend.h` and exposed `ui_backend_init_runtime/ui_backend_process_input` for the UI thread.
   - Replaced `ui_shortcuts` with a pure mapper that emits `ui_shortcut_action_t` events processed by the backend.
   - Migrated mute flow, keyboard octave/tag management, and overlay banner cloning into `ui_backend.c` with persistent context.
   - Simplified `ui_task` to poll inputs, delegate to the backend, and keep the render loop/LED refresh.
3. **Sequencer model scaffolding** ✅
   - `core/seq/seq_model.{h,c}` now provides the immutable pattern container with steps, voices, per-step parameter locks, and global quantize/transpose/scale configuration helpers.
   - Added the `seq_model_gen_t` helper for generation tracking alongside convenience setters/getters for voices, offsets, and pattern configuration.
4. **Sequencer engine skeleton** ✅
   - `seq_engine.{h,c}` now expose reader, scheduler, and player contexts with ring-buffer utilities, configuration helpers, and stubbed callbacks for NOTE ON/OFF and parameter locks.
   - The engine registers/unregisters to `clock_manager` on start/stop while keeping dispatch bodies as TODO placeholders so the firmware stays compilable.
5. **Live capture façade** ✅
   - Introduced `seq_live_capture.{h,c}` to translate keyboard/ARP inputs into sequencer plans with per-step micro offsets.
   - Implemented a quantize placeholder that computes timing windows, strength blending, and reports the resulting micro-timing without mutating the pattern.
6. **UI overlay rework** ✅
   - The overlay specs now rely on the shared `ui_mode_context_t` via `ui_backend_get_mode_label()`, which feeds both overlay banners and the renderer (dynamic labels such as `KEY+1`, `MUTE`, etc.).
   - `ui_task` remains a pure render/input loop—the backend owns keyboard label updates and exposes the mode label helper for the renderer.
7. **LED backend alignment**
   - Expose non-blocking LED update queue from backend; ensure sequencer playhead/p-lock overlays feed from model/gen data instead of immediate toggles.
8. **Testing & validation**
   - After each major step, run `make`, `cppcheck`, and add short summary of warnings. Provide unit-style tests where possible (e.g. host-compiled `seq_model` tests via `make check-host`).

## 5. Immediate follow-up items
- Document the dependencies discovered above in Doxygen diagrams (sequence + module) once the refactor stabilizes.
- Prepare fixtures for demonstrating Reader → Scheduler → Player execution with micro-timed steps (target for Step 4/5).
- Align all new headers with include guards in the form `#ifndef BRICK_<PATH>_<NAME>_H_`.

*This document will evolve as each milestone lands. Step 2 targets the "ui_shortcuts/ui_task" split requested in the super-prompt; Step 3+4 lay the sequencer foundations before plugging UI pages.*
