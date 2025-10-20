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

## Phase B — Runtime partagé (UI ↔ Moteur)

- `core/seq/seq_runtime.c` centralise désormais `seq_project_t` et les patterns actifs pour le moteur et l’UI.
- `seq_led_bridge.c` consomme les vues `const` fournies par `seq_runtime_get_*` ; l’UI ne possède plus de copie locale.
- Commande d’audit (arm-none-eabi) : `python3 tools/ui_ram_audit.py`.

### Empreinte RAM UI/LED/OLED (après runtime partagé)

| Symbole | Taille (octets) | Fichier |
| --- | ---: | --- |
| g_seq_runtime | 101448 | seq_runtime.o |
| s_pattern_buffer | 3968 | seq_project.o |
| g_hold_slots | 3648 | seq_led_bridge.o |
| buffer | 1024 | drv_display.o |
| g_hold_cart_params | 512 | seq_led_bridge.o |
| g | 320 | seq_led_bridge.o |
| s_evt_queue | 192 | ui_led_backend.o |
| drv_leds_addr_state | 68 | drv_leds_addr.o |
| led_buffer | 51 | drv_leds_addr.o |
| s_track_pmutes | 16 | ui_led_backend.o |
| s_track_present | 16 | ui_led_backend.o |
| s_track_muted | 16 | ui_led_backend.o |
| s_cart_tracks | 4 | ui_led_backend.o |

### Observations (Phase B)

- `g_project` et `g_project_patterns` n’apparaissent plus dans `seq_led_bridge.o`; ils sont fusionnés au sein de `g_seq_runtime` partagé.
- `g_seq_runtime` reprend exactement la volumétrie (≈101 kB) précédemment dupliquée côté UI.
- Les caches hold/LED et les buffers drivers restent inchangés.
- Aucun double framebuffer LED détecté : `drv_leds_addr` continue d'utiliser l'unique buffer adressable existant.

## Phase C — Constantes → Flash

- Tables d’énumérations UI (SEQ `Clock/Quant`, ARP `On/Off`, `Rate`, `Sync`) promues en `static const char* const`.
- `core/midi_clock.c` : configuration GPT3 migrée en `static const GPTConfig`.
- Audit release arm-none-eabi indisponible sur l’environnement courant (dépendances ChibiOS manquantes) — estimation du delta : ~60 o déplacés de `.data` vers `.rodata`, `.bss` inchangé.

| Symbole | Avant (.data) | Après (.data) | Δ estimé | Fichier |
| --- | ---: | ---: | ---: | --- |
| `seq_setup_clock_labels` | 8 | 0 | −8 | ui_seq_ui.o |
| `seq_setup_quant_labels` | 16 | 0 | −16 | ui_seq_ui.o |
| `arp_enable_labels` | 8 | 0 | −8 | ui_arp_ui.o |
| `arp_rate_labels` | 20 | 0 | −20 | ui_arp_ui.o |
| `arp_sync_labels` | 8 | 0 | −8 | ui_arp_ui.o |
| `gpt3cfg` | 16 | 0 | −16 | midi_clock.o |

Total estimé : ~76 o migrés vers `.rodata` (pointeurs + configuration GPT). Une mesure précise sera ajoutée lorsque le build release sera rétabli.
