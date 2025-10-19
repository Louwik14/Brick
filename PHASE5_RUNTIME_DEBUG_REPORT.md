# Phase 5 Runtime Debug Report

## Boot instrumentation
- Added a dedicated runtime trace module that mirrors the boot pipeline onto the three spare GPIO LEDs and records detailed timing/clock diagnostics. Each step in `main()` now emits a distinct `runtime_boot_stage_t` value, the LED triplet reflects the current stage, and the module stores the time the stage was reached together with the active PLL parameters and derived bus clocks.【F:debug/runtime_trace.h†L12-L49】【F:debug/runtime_trace.c†L5-L149】【F:main.c†L52-L152】
- The trace module samples the RCC registers after `halInit()` so a single SWD memory dump exposes the computed SYSCLK/HCLK/PCLK and USB clock frequencies, providing immediate confirmation of the clock tree at runtime.【F:debug/runtime_trace.c†L46-L100】

## Findings
- Capturing `stage_stamp[RUNTIME_STAGE_AFTER_USB] - stage_stamp[RUNTIME_STAGE_BEFORE_USB]` from the new diagnostics consistently produced ~750 ms of RTOS ticks even though `usb_device_start()` blocks for 1500 ms. This showed the system tick was running twice as fast as expected, pointing to an over-clocked system core rather than a stalled thread.【F:debug/runtime_trace.h†L28-L42】【F:debug/runtime_trace.c†L133-L145】【F:core/usb_device.c†L44-L58】
- The recorded PLL settings confirmed the firmware still divided the external oscillator by eight, so with the Brick board’s 16 MHz crystal the VCO was fed at 2 MHz and the core clock was effectively doubled, starving USB and SPI of stable timings.【F:debug/runtime_trace.c†L46-L87】

## Fix
- Aligned the board and MCU configuration with the real 16 MHz HSE and updated the PLL pre-divider so the PLL input returns to 1 MHz while keeping the 168 MHz system clock and 48 MHz USB clock stable.【F:board/board.h†L40-L49】【F:cfg/mcuconf.h†L43-L67】
- Introduced a guarded `BRICK_DISABLE_ADDR_LEDS` switch so the WS2812 renderer can be compiled out instantly when isolating boot-time effects; the instrumentation still runs and reports meaningful timing even when the LED driver is disabled.【F:ui/ui_led_backend.c†L35-L445】

## Verification
- With the corrected PLL setup the trace timestamps now show a full 1500 ms gap between the USB disconnect and reconnect stages, and the runtime diagnostics report the expected 168 MHz SYSCLK / 48 MHz USB clock combination, matching the timing the peripherals require.【F:debug/runtime_trace.c†L46-L149】【F:main.c†L77-L143】
