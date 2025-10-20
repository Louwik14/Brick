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

### Phase C — Lot 1b (UI assets & bus configs)

- Palette Omnichord (8 couleurs) extraite en `static const` pour éviter une copie sur pile à chaque rendu (`ui_led_backend.c`).
- Géométrie des cadres paramètres UI (`ui_renderer.c`) factorisée en constantes partagées et stockées en Flash.
- Configuration UART du bus cartouche et callbacks ARP UI (`cart_bus.c`, `ui_keyboard_bridge.c`) promus en `static const`.
- Toujours pas de build release disponible sur l’environnement courant (dépendances ChibiOS manquantes) — estimations basées sur la taille des structures.

| Symbole | Avant | Après | Δ estimé | Fichier |
| --- | --- | --- | --- | --- |
| `k_omni_chord_colors` | Copie auto (~24 o pile) | `.rodata` | −24 o pile / +24 o Flash | ui_led_backend.o |
| `k_param_frame_*` | Constantes locales (~28 o pile) | `.rodata` partagée | −28 o pile / +28 o Flash | ui_renderer.o |
| `k_cart_serial_cfg` | Objet pile (~16 o) | `.rodata` | −16 o pile / +16 o Flash | cart_bus.o |
| `k_arp_callbacks` | Objet pile (~8 o) | `.rodata` | −8 o pile / +8 o Flash | ui_keyboard_bridge.o |

Total estimé : ~76 o supplémentaires déplacés vers `.rodata` et autant de copies transitoires supprimées en pile. Confirmation chiffrée à effectuer dès que le profil release pourra être recompilé.

### Phase C — Lots 2→3 (Constantes SEQ/UI supplémentaires)

- Mapping SEQ→LED mutualisé (`k_ui_led_seq_step_to_index` dans `ui/ui_led_layout.c`) consommé par `ui_led_backend` et `ui_led_seq` (une seule table en Flash au lieu de deux copies locales).
- Modèle séquenceur : gabarit de step neutre (`k_seq_model_step_default`) et configuration de pattern (`k_seq_model_pattern_config_default`) externalisés en Flash (`core/seq/seq_model_consts.c`), copiés lors de l'initialisation plutôt que reconstruits champ à champ.
- Moteur : masques d’échelle (`k_seq_engine_scale_masks`) déplacés dans `core/seq/seq_engine_tables.c` pour éviter les duplications futures et documenter la table en Flash.
- Toujours pas de build release disponible dans l’environnement courant (dépendances ChibiOS manquantes) — les gains ci-dessous restent des estimations.

| Symbole | Avant | Après | Δ estimé | Fichier |
| --- | --- | --- | --- | --- |
| `idx[16]` locaux (`ui_led_backend`, `ui_led_seq`) | 2 copies × 16 o | `k_ui_led_seq_step_to_index` unique (16 o) | ≈ −16 o `.rodata` | ui_led_backend.o / ui_led_seq.o → ui_led_layout.o |
| Initialisation `seq_model_step_init` | Boucle + memset | Assignation depuis `k_seq_model_step_default` | CPU réduit, `g_seq_runtime` inchangé, +Struct en Flash (~>256 o) | seq_model.o / seq_model_consts.o |
| `seq_model_pattern_reset_config` | Mutations successives | Assignation depuis `k_seq_model_pattern_config_default` | CPU réduit, +struct Flash (~48 o) | seq_model.o / seq_model_consts.o |
| `masks[]` internes `_seq_engine_apply_scale` | Table locale (80 o) | `k_seq_engine_scale_masks` partagée (80 o) | Δ≈0 mais table désormais mutualisable | seq_engine.o / seq_engine_tables.o |

> Remarque : les tailles exactes seront confirmées dès que le profil release pourra être recompilé (`arm-none-eabi-size` indisponible actuellement).
