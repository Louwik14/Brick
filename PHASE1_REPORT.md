# Phase 1 — Rapport

## Modifs effectuées
- Makefile : chemin `CHIBIOS` local, fallback `ARMCMx/ARMv7-M` pour `port.mk`, variable `BRICK_ENABLE_INSTRUMENTATION`, lien LD sur `board/STM32F429xI.ld`.
- cfg/chconf.h : activation conditionnelle de `CH_DBG_FILL_THREADS` sous instrumentation.
- Nouvelle API `core/brick_metrics.[ch]` pour collecter piles/queues et reset global.
- Instrumentation runtime : MIDI (fill/high-water reset), cart bus (fill/reset), boutons (compteurs fill/high-water/drops), backend LED (fill/high-water/drops), entêtes associés.
- Documentation : README mise à jour (ChibiOS vendored, section instrumentation).

## Build & métriques
- Commande build : `make` (toolchain `arm-none-eabi-gcc`).
- Sorties : `build/ch.elf`, `build/ch.map`, `build/ch.dmp`, `build/ch.list`, `build/ch.bin`, `build/ch.hex`.
- Résumé tailles (text/data/bss) : 168360 / 1864 / 256256 octets.
- Top symboles (≥ 20 KiB) :
  - `g_project` (.bss, ~71.3 KiB)
  - `g_project_patterns` (.bss, ~27.4 KiB)
  - `s_tracks` (.bss, ~22.0 KiB)
  - `waCartTx` (.bss, ~9.1 KiB par pool) — visible dans `build/ch.map`.

## Instrumentation activée
- Stack watermark : `CH_DBG_FILL_THREADS` actif quand `BRICK_ENABLE_INSTRUMENTATION=1`; collecte via `brick_metrics_collect_stacks()`.
- High-water queues (MIDI/cart/UI LED/buttons) :
  - MIDI : `midi_usb_queue_high_watermark()`, `midi_usb_queue_fill_level()`, reset `midi_usb_queue_reset_stats()`.
  - Cart : `cart_bus_get_mailbox_high_water()`, `cart_bus_get_mailbox_fill()`, reset `cart_bus_reset_mailbox_stats()`.
  - Boutons : `drv_buttons_queue_high_water()`, `drv_buttons_queue_fill()`, `drv_buttons_queue_drop_count()`.
  - Backend LED : `ui_led_backend_queue_high_water()`, `ui_led_backend_queue_fill()`, `ui_led_backend_queue_drop_count()`.
- Où lire les compteurs :
  - Agrégateur `core/brick_metrics.h` (`brick_metrics_collect_stacks/queues`, `brick_metrics_reset_queue_counters`).
  - Compteurs unitaires dans leurs modules respectifs (voir ci-dessus).

## Notes compatibilité ChibiOS 21.11
- Makefile détecte `port.mk` dans `ARMCMx` et retombe sur `ARMv7-M` si nécessaire.
- Chaine `CHIBIOS` pointant vers le dossier vendored `./ChibiOS` (plus de dépendance externe).
- `CH_DBG_FILL_THREADS` conditionné par `BRICK_ENABLE_INSTRUMENTATION` pour garder un build release sans surcharge.

## Prochaines étapes (préparation Phase 2)
- Exploiter `brick_metrics_collect_stacks/queues` pour profiler les piles avant réduction CCM.
- Exporter un snapshot des compteurs (UART/log) pour suivre `g_project`/`patterns` lors des futures migrations SRAM → flash.
- Outiller l’analyse de `build/ch.map` (script parsing) afin d’automatiser la comparaison text/data/bss après chaque optimisation.
