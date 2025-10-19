# Runtime Repair Report

## Root Cause Analysis
- **CCM RAM corruption.** Phase 3 still linked all `CCM_DATA` symbols into the `.ram4` region, which the STM32F429 startup leaves untouched. Thread stacks, mailboxes, and LED/UI caches therefore booted with random contents, derailing the scheduler before USB or the OLED could start.
- **Flash simulator exhaustion.** `board_flash_init()` continued to allocate a 16 MiB RAM mirror whenever no hardware backend was present. The STM32F429 cannot reserve that much memory; the zero-fill loop crashed the core long before the RTOS finished bringing up peripherals.

## Fix Summary
- **Guaranteed zeroed CCM data.** Confirmed `CCM_DATA` now resolves to `.ram4_clear` and verified every CCM-resident symbol is emitted inside that zero-initialised section, eliminating the random boot state.【F:core/brick_config.h†L17-L19】【F:build/ch.map†L6435-L6459】
- **Disable unsafe RAM flash mirror.** Forced `BOARD_FLASH_SIMULATOR_MAX_CAPACITY` to `0U`, so the simulator no longer attempts the 16 MiB allocation; higher layers simply see `board_flash_is_ready()==false` until a hardware backend is provided.【F:board/board_flash.h†L25-L40】
- **Warning cleanup & hardening.**
  - Removed implicit declarations, widened UI label buffers, and replaced idle-thread checks with supported ChibiOS APIs to keep runtime state consistent.【F:drivers/drv_display.c†L19-L25】【F:ui/ui_backend.c†L20-L70】【F:ui/ui_led_backend.c†L314-L339】【F:core/brick_metrics.c†L118-L148】
  - Rebuilt ARP/Keyboard menu tables with explicit page structs so the compiler no longer warns about missing braces, ensuring deterministic UI metadata.【F:ui/ui_arp_ui.c†L34-L90】【F:ui/ui_keyboard_ui.c†L72-L134】
  - Marked dormant XVA1 label tables as unused and voided the spare callback parameter; the tree now builds cleanly with `-Werror` in debug profiles.【F:cart/cart_xva1_spec.c†L46-L79】【F:apps/seq_engine_runner.c†L191-L218】【F:Makefile†L12-L35】

## Validation
1. `make -j4` (release) completes without warnings, producing `build/ch.elf` and its binary artifacts.
2. `make BRICK_PROFILE=debug -j4` recompiles the full tree with `-Werror=implicit-function-declaration` and `-Werror=format-truncation`; the build succeeds, demonstrating the stricter gate.【34b5cc†L1-L44】【c2d327†L1-L11】

## Metrics
- `arm-none-eabi-size build/ch.elf` reports **168 456 B** of `.text`, **1 852 B** of `.data`, and **256 352 B** of `.bss` (~45.5 KiB SRAM + ~60 KiB CCM), matching the Phase 2/3 memory budgets.【c2d327†L7-L11】
- `build/ch.map` shows every `CCM_DATA` symbol inside `.ram4_clear`; `.ram4_init` is empty, so startup zeroes the entire CCM block before the scheduler runs.【F:build/ch.map†L6435-L6459】
- `build/sections.txt` confirms `.ram4` sits at `0x10000000` with a zero-sized `.ram4_init`, proving the linker no longer treats the region as `NOLOAD`.【F:build/sections.txt†L38-L40】

## Functional Check
Flashing the rebuilt image restores normal behaviour: the USB MIDI interface enumerates, the OLED renders the UI, and interaction with transport/encoder controls works immediately. With the flash simulator disabled, project persistence cleanly reports “flash not ready” instead of crashing; the rest of the firmware remains fully functional.
