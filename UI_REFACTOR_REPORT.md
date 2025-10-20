# UI Refactor Report — Phase « UI/Moteur »

## Phase A — Instrumentation & Audit mémoire

- Instrumentation `UI_RAM_AUDIT(sym)` ajoutée sur les buffers clés (`seq_led_bridge`, `ui_led_backend`, `drv_leds_addr`, `drv_display`, `seq_project`).
- Script d'audit : `tools/ui_ram_audit.py` (cross-compilation `arm-none-eabi-gcc`, extraction via `arm-none-eabi-nm`).
- Commande d'audit : `tools/ui_ram_audit.py`.

### Empreinte RAM UI/LED/OLED (avant refactor)

| Symbole | Taille (octets) | Fichier |
| --- | ---: | --- |
| g_project | 73000 | seq_led_bridge.o |
| g_project_patterns | 28448 | seq_led_bridge.o |
| s_pattern_buffer | 3968 | seq_project.o |
| g_hold_slots | 3648 | seq_led_bridge.o |
| buffer | 1024 | drv_display.o |
| g_hold_cart_params | 512 | seq_led_bridge.o |
| g | 320 | seq_led_bridge.o |
| s_evt_queue | 192 | ui_led_backend.o |
| drv_leds_addr_state | 68 | drv_leds_addr.o |
| led_buffer | 51 | drv_leds_addr.o |
| s_track_muted | 16 | ui_led_backend.o |
| s_track_pmutes | 16 | ui_led_backend.o |
| s_track_present | 16 | ui_led_backend.o |
| s_cart_tracks | 4 | ui_led_backend.o |

### Observations

- `g_project` (73 kB) et `g_project_patterns` (27,8 kB) constituent l’essentiel de l’empreinte UI.
- `s_pattern_buffer` (3,9 kB) persiste côté `seq_project`, utilisé pour la sérialisation.
- Buffers LED/OLED : `buffer` (framebuffer OLED 1 kB), `drv_leds_addr_state` + `led_buffer` (119 octets au total).
- Files et caches UI (hold slots, file d’événements) restent <4 kB chacun.
