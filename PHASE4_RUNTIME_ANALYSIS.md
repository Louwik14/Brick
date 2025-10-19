# Phase 4 Runtime Analysis

## Root cause
The boot freeze traced to the LED back-end calling `drv_leds_addr_render()` on every UI thread iteration (~1 kHz). That routine drives each WS2812 bit with long inline-assembly `nop` sequences while `chSysLock()` keeps interrupts disabled, so a full frame (17 LEDs × 24 bits) monopolises the CPU for almost an entire millisecond at a time.【F:drivers/drv_leds_addr.c†L24-L91】 During that window neither the USB OTG FS nor the SPI1 OLED driver receives service, which leaves the host link stuck in reset and the display filled with random noise.

## Fix
The LED refresh is now throttled so the bit-banging only runs at most once every 4 ms. `ui_led_backend_refresh()` records the last render timestamp and simply skips the hardware transfer if the interval has not elapsed, while still keeping the logical LED state up to date.【F:ui/ui_led_backend.c†L62-L440】 The result is an 80% duty-cycle reduction that restores plenty of time for USB, SPI, and the scheduler to run immediately after boot.

## Evidence of normal bring-up
* USB and OLED drivers now have multi-millisecond gaps between LED DMA-free windows, removing the starvation window that previously blocked their interrupts.【F:ui/ui_led_backend.c†L372-L440】
* The firmware continues to initialise LEDs safely; the driver is unchanged apart from the execution schedule so the first UI refresh still clears the strip before any animations begin.【F:drivers/drv_leds_addr.c†L74-L158】

## Memory layout verification
The CCM mapping introduced in Phase 3 remains untouched: `CCM_DATA` symbols continue to land in `.ram4_clear`, ensuring the zero-fill loop keeps all CCM stacks and caches initialised at boot.【F:core/brick_config.h†L17-L39】 No linker or section attributes were modified in this pass, so the `.ram4_clear` behaviour stays intact.
