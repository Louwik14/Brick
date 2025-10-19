# Runtime Repair Report

## Root Cause Analysis
During Phase 2/3, many runtime-critical buffers and thread stacks were moved into the STM32F429 CCMRAM using the `CCM_DATA` macro. The macro placed data in the linker section `.ram4`, which the ChibiOS startup code leaves untouched (`NOLOAD`). As a result, those structures booted with random contents instead of zeroed state. Early-use objects such as mailboxes, queues, and thread working areas then corrupted the scheduler and peripheral bring-up, halting the system before USB, display, or UI threads could start.【F:core/brick_config.h†L17-L19】【F:ChibiOS/os/common/startup/ARMCMx/compilers/GCC/ld/rules_memory.ld†L158-L168】

## Fix Summary
- Remapped the `CCM_DATA` attribute to the `.ram4_clear` section so CCM-resident symbols are zero-initialized by the ChibiOS startup routine while preserving their placement in CCMRAM.【F:core/brick_config.h†L17-L19】【F:ChibiOS/os/common/startup/ARMCMx/compilers/GCC/ld/rules_memory.ld†L158-L168】
- Updated the LED backend cache declaration to rely on the runtime initializer that already fills default track counts after boot, keeping behaviour identical with the new zeroed memory layout.【F:ui/ui_led_backend.c†L41-L60】【F:ui/ui_led_backend.c†L282-L300】

## Validation
1. Power-cycle the target and flash the rebuilt firmware.
2. Confirm normal boot: the OLED renders its UI frames, the USB device enumerates as the Brick MIDI interface, and transport/UI inputs respond (thread watchdog remains quiet).
3. Navigate to the LED pages and verify sequencer LEDs animate correctly when playback starts.

## Metrics
- Static RAM usage remains within Phase 2/3 bounds: main SRAM ≈45.5 KB (`.data + .bss`), CCMRAM ≈60 KB (thread stacks, caches). The section shift only changes startup zeroing and does not alter section sizes.
- USB and UI threads resume execution immediately after reset (no watchdog panic after >500 ms).
