# Sequencer Integration Architecture Plan

This note captures refactors that keep the current layering intact while giving the
future sequencer room to plug in cleanly. Each proposal is scoped to minimise churn
outside the SEQ feature set.

## 1. Stabilise the step-clock contract

* **Problem today** – `clock_manager_register_step_callback2()` stores a single global
  function pointer, so the sequencer would monopolise the callback and prevent other
  subsystems (metronome, diagnostics) from subscribing later.
  (`core/clock_manager.c` keeps only `s_step_cb_v2`.)
* **Refactor** – promote the V2 callback to a tiny observer list (fixed array of 4) and
  hand out opaque handles for removal. This keeps the `clock_manager` API core-only and
  avoids cascading `#include`s into UI code.
* **Bonus** – add a `clock_manager_step_source_t` (internal/external) to the notification
  payload so the sequencer can decide whether to advance or just chase the transport.

## 2. Carve a dedicated `seq_service`

* **Problem today** – there is no neutral place to bootstrap the trio `seq_model`
  (state), `seq_engine` (player) and `seq_runtime` (snapshot). `main.c` currently jumps
  from drivers/cart initialisation straight to `ui_init_all()`.
* **Refactor** – insert a `seq_service_init()` call alongside `drivers_and_cart_init()`
  inside `main.c`, keeping the new module inside `core/` so it can reach the clock,
  MIDI and CartLink layers without touching UI headers (`main.c` already hosts the boot
  order just before `ui_task_start()`). The service would:
  1. Boot the pure model (pattern defaults, offsets).
  2. Launch the sequencer worker thread (priority N+1, between UI and MIDI).
  3. Register the step callback with the enhanced `clock_manager`.
  4. Publish the initial snapshot via `seq_runtime_commit()`.
* **Rationale** – by centralising these responsibilities the UI stays consumer-only,
  piping edits through `ui_backend` (which already routes SEQ parameters as `UI_DEST_UI`
  entries). The service can also bridge CartLink safely when P-Locks mutate.

## 3. Publish runtime snapshots through a lock-free bridge

* **Problem today** – the LED renderer pulls state from a mutable struct owned by UI
  (`ui_led_seq.c` keeps a global `seq_renderer_t` snapshot). When the engine goes live,
  sharing raw buffers would risk tearing.
* **Refactor** – replace the ad-hoc struct with the future `seq_led_bridge` API that
  hands back copies of the immutable `seq_runtime_t`. The renderer simply requests the
  `visible_page`/`held_mask` pair and colours LEDS without knowing about threads.
* **Follow-up** – expose a `seq_led_bridge_set_visible_page(uint8_t)` called from
  `ui_shortcuts` when pages change, so the engine dictates nothing about presentation.

## 4. Route SEQ UI writes through typed adapters

* **Problem today** – SEQ menu entries are declared as generic UI params
  (`ui/ui_seq_ui.c` maps All/Voice offsets to `UI_DEST_UI` ids). Without helpers the
  controller must keep hand-written switch statements to translate each UI change.
* **Refactor** – add a tiny adapter layer (in `ui_backend_seq.c`) that translates UI
  deltas into high-level calls:
  * Offsets → `seq_engine_set_global_offset(voice, param, value)`.
  * Per-voice params → `seq_engine_set_step_value(...)` using the current page + held mask.
* **Outcome** – existing UI routing stays untouched, but SEQ-specific logic is confined to
  one translation unit that can be unit-tested without the full UI thread.

## 5. Prepare CartLink hooks for parameter locks

* **Problem today** – `cart_link_param_changed()` immediately pushes to the hardware bus
  after updating the shadow register. P-Locks will need to alter the shadow without
  spamming the UART during pattern preparation.
* **Refactor** – extend CartLink with `cart_link_defer_param()` / `cart_link_flush_pending()`
  so the sequencer can queue parameter changes atomically at step boundaries while the
  UI continues to read the shadow through `ui_backend_shadow_get()`.

## 6. Definition-of-done checklist alignment

* Write the above service & adapters with full Doxygen coverage so they slot naturally
  into the project-wide DoD.
* Add unit hooks for the pure model (`seq_model`) to the host test harness to guarantee
  set/get/toggle semantics before firmware integration.
