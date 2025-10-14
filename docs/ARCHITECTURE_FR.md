# Architecture logicielle du firmware Brick

## 1. Introduction générale

Brick est un **firmware de contrôle modulaire** conçu pour piloter jusqu'à quatre cartouches DSP externes et une interface MIDI (USB et DIN). Il ne génère aucun son en propre : il agit comme un **cerveau Elektron-like** qui orchestre transport, séquenceur, automation (parameter locks) et distribution des contrôles vers les cartouches.

La cible matérielle est un **STM32F429** exécutant **ChibiOS 21.11.x**. L'architecture impose une séparation stricte des responsabilités :

- **Model** : structures de données pures sérialisables.
- **Engine** : logique temps réel (clock, scheduler, player, bus).
- **UI** : rendu OLED/LED, interactions utilisateur, modes customs.

Les communications s'organisent autour de ponts explicites (`ui_backend`, `cart_link`, `seq_led_bridge`), garantissant l'isolation des couches.

## 2. Architecture pyramidale

La plateforme est structurée en couches hiérarchiques, de l'abstraction matérielle vers l'expérience utilisateur.

```
Drivers
  ├── Buttons / Encoders / OLED / LEDs / UART / USB
  │
  ├── Core
  │     ├── Clock manager
  │     ├── Sequencer (Model / Engine / Live Capture)
  │     └── System utilities
  │
  ├── UI
  │     ├── Backend (state manager)
  │     ├── Task (render loop)
  │     ├── Shortcuts (input mapping)
  │     ├── Overlay / OLED Renderer
  │     └── LED backend bridge
  │
  ├── Apps
  │     └── Bridges (seq_led_bridge, ui_keyboard_bridge…)
  │
  ├── Cart
  │     ├── Cart bus (UART)
  │     ├── Cart link (shadow registers)
  │     └── Cart registry
  │
  └── MIDI / USB
        ├── midi_tx / midi_rx
        └── usb_midi
```

Chaque couche ne consomme que les services de la couche immédiatement inférieure. Les flux d'évènements et de données remontent via des files, des structures immuables ou des callbacks planifiés.

## 3. Description hiérarchique complète

### Racine du projet

#### main.c
- **Rôle** : point d'entrée, séquence d'initialisation complète (HAL, drivers, clock, cartouches, UI, backend LED).
- **Structures/API** : `system_init()`, `io_realtime_init()`, `drivers_and_cart_init()`, `ui_init_all()`, `main()`.
- **Dépendances** : appelle `drivers_init_all`, `cart_*`, `ui_task_start`, `ui_led_backend_*`, `midi_*`, `midi_clock_init`, `usb_device_start`.
- **Threads/Callbacks** : lance les threads définis dans les sous-systèmes (clock, UI, MIDI, LED refresh).
- **Exemple** : initialisation d'une cartouche via `cart_registry_register(CART1, &CART_XVA1);` avant de démarrer l'UI.

### core/

#### brick_config.h
- **Rôle** : configuration compile-time (flags globaux, tailles de buffers, macros d'alignement).
- **API** : définitions statiques, macros `BRICK_USE_*`.
- **Dépendances** : inclus par modules nécessitant les constantes (drivers, ui, core).

#### cart_link.c / cart_link.h
- **Rôle** : pont unique vers les cartouches (shadow registers, notifications de paramètres, handshake specs).
- **API** : `cart_link_init()`, `cart_link_param_changed()`, `cart_link_tick()`, `cart_link_request_snapshot()`.
- **Dépendances** : consomme `cart_bus` pour l'I/O UART et `cart_registry` pour les spécifications ; appelé par `ui_backend` et `seq_engine`.
- **Threads** : fonctions invoquées depuis thread UI ou callbacks clock (pour synchronisation playhead/cart).
- **Usage** : mise à jour d'un paramètre cart via `ui_backend_param_changed()` qui se propage à `cart_link_param_changed()`.

#### clock_manager.c / clock_manager.h
- **Rôle** : gestion centrale de la clock 24 PPQN, abonnement des listeners et diffusion de ticks.
- **API** : `clock_manager_init()`, `clock_manager_subscribe()`, `clock_manager_unsubscribe()`, `clock_manager_set_rate()`.
- **Dépendances** : utilise `chibios` (threads GPT) ; servi par `seq_engine`, `midi_clock`, `ui_backend` (transport) ; interface unique vers GPT timer driver.
- **Threads** : thread haute priorité `NORMALPRIO+3` + callbacks dispatchés dans `clock_manager_notify()`.
- **Usage** : `seq_engine_start()` enregistre son reader pour recevoir les ticks planifiés.

#### midi_clock.c / midi_clock.h
- **Rôle** : conversion des événements clock internes en messages MIDI Clock/Start/Stop.
- **API** : `midi_clock_init()`, `midi_clock_set_source()`, `midi_clock_handle_tick()`.
- **Dépendances** : souscrit à `clock_manager`; publie vers `midi.c`.
- **Threads** : thread `NORMALPRIO+3` (aligné sur GPT) ; callbacks non bloquants.

#### spec/cart_spec_types.h
- **Rôle** : types partagés pour décrire les cartouches (paramètres, plages, cycles de menus).
- **API** : structures `cart_param_spec_t`, `cart_menu_cycle_t`, etc.
- **Dépendances** : inclus par `cart_registry`, `cart_xva1_spec`, `ui_spec`.

#### seq/seq_model.c / seq_model.h
- **Rôle** : modèle séquenceur pur (patterns, steps, voices, parameter locks, offsets globaux).
- **API** : `seq_model_pattern_init()`, `seq_model_step_get_voice()`, `seq_model_p_lock_add()`, `seq_model_gen_next()`.
- **Dépendances** : consommé par `seq_engine`, `seq_live_capture`, `seq_led_bridge`, `ui_seq_ui`.
- **Structures** : `seq_model_pattern_t`, `seq_model_step_t`, `seq_model_voice_t`, `seq_model_p_lock_t`, `seq_model_gen_t`.
- **Usage** : UI SEQ modifie une voix via `seq_model_step_set_note()` puis incrémente la génération pour notifier le LED bridge.

#### seq/seq_engine.c / seq_engine.h
- **Rôle** : squelette Reader/Scheduler/Player reliant la clock aux évènements MIDI/cart.
- **API** : `seq_engine_init()`, `seq_engine_start()`, `seq_engine_stop()`, `seq_engine_bind_pattern()`, callbacks `seq_engine_set_output()`.
- **Structures** : `seq_engine_reader_t`, `seq_engine_scheduler_t`, `seq_engine_player_t`, `seq_engine_event_t`.
- **Dépendances** : consomme `clock_manager`, `seq_model`; exporte hooks pour `midi`, `cart_link`.
- **Threads** : player thread `NORMALPRIO+1` ; queues internes lock-free.

#### seq/seq_live_capture.c / seq_live_capture.h
- **Rôle** : capture d'évènements live (Keyboard/ARP) avec quantize, micro-timing et p-lock runtime.
- **API** : `seq_live_capture_init()`, `seq_live_capture_begin_recording()`, `seq_live_capture_plan_step()`, `seq_live_capture_commit_step()`.
- **Structures** : `seq_live_capture_context_t`, `seq_live_capture_plan_t`, `seq_live_capture_quantize_t`.
- **Dépendances** : utilise `seq_model` pour planifier les steps ; alimenté par `ui_keyboard_bridge` ou `arp`.

#### usb_device.c / usb_device.h
- **Rôle** : initialisation de la pile USB Device (CDC/MIDI), ré-énumération et callbacks d'attachement.
- **API** : `usb_device_start()`, `usb_device_stop()`, `usb_device_suspended_cb()`.
- **Dépendances** : `ChibiOS HAL`, `usb/usbcfg`.
- **Threads** : none ; gère callbacks HAL.

### cart/

#### cart_bus.c / cart_bus.h
- **Rôle** : abstraction UART dédiée aux cartouches (DMA TX/RX, framing, CRC éventuel).
- **API** : `cart_bus_init()`, `cart_bus_send_frame()`, `cart_bus_receive()`.
- **Dépendances** : drivers STM32 (USART, DMA via HAL) ; utilisé par `cart_link`.

#### cart_proto.c / cart_proto.h
- **Rôle** : encode/décode des messages XVA1-like (structures de commande, ack, errors).
- **API** : `cart_proto_build_param()`, `cart_proto_parse_status()`.
- **Dépendances** : `cart_bus`, `cart_spec_types`.

#### cart_registry.c / cart_registry.h
- **Rôle** : référentiel des cartouches disponibles, instanciation des specs, suivi de la cart active.
- **API** : `cart_registry_init()`, `cart_registry_register()`, `cart_registry_select()`, `cart_registry_get_active()`.
- **Dépendances** : `cart_spec_types`, `cart_xva1_spec`.

#### cart_xva1_spec.c / cart_xva1_spec.h
- **Rôle** : description complète de la cartouche XVA1 (paramètres, menus, cycles BM, mapping UI).
- **API** : tables constantes `CART_XVA1`, `CART_XVA1_MENUS`.
- **Dépendances** : `cart_spec_types`, `ui_spec`.

### drivers/

#### drivers.c / drivers.h
- **Rôle** : façade unique pour initialiser tous les drivers matériels.
- **API** : `drivers_init_all()`, `drivers_update()`.
- **Dépendances** : appelle `drv_buttons`, `drv_encoders`, `drv_display`, `drv_leds_addr`, `drv_pots`.

#### drv_buttons.c / drv_buttons.h
- **Rôle** : lecture des matrices de boutons, debouncing, génération d'évènements.
- **API** : `drv_buttons_init()`, `drv_buttons_poll()`, `drv_buttons_get_state()`.
- **Threads** : appelé depuis thread UI (polling 1 kHz max) ; IRQ pour GPIO si configuré.

#### drv_buttons_map.h
- **Rôle** : mapping statique des broches vers les identifiants logiques (BS1..BS16, SHIFT, transport).
- **Dépendances** : utilisé par `drv_buttons` et `ui_input`.

#### drv_display.c / drv_display.h
- **Rôle** : pilote OLED (SPI/I2C) avec double buffer.
- **API** : `drv_display_init()`, `drv_display_start_frame()`, `drv_display_end_frame()`.
- **Dépendances** : `ui_renderer`.

#### drv_encoders.c / drv_encoders.h
- **Rôle** : lecture des encodeurs (quadrature), filtrage de bruit, accélération.
- **API** : `drv_encoders_init()`, `drv_encoders_poll()`, `drv_encoders_get_delta()`.

#### drv_leds_addr.c / drv_leds_addr.h
- **Rôle** : pilotage des LED adressables (WS2812/SK6812) via DMA/SPI.
- **API** : `drv_leds_addr_init()`, `drv_leds_addr_render()`.
- **Dépendances** : `ui_led_backend`.

#### drv_pots.c / drv_pots.h
- **Rôle** : acquisition des potentiomètres analogiques (ADC DMA + calibration).
- **API** : `drv_pots_init()`, `drv_pots_read()`.

#### drivers.zip
- **Rôle** : archive historique (non utilisée dans le build).

### midi/

#### midi.c / midi.h
- **Rôle** : pile MIDI DIN + USB (mailboxes, threads TX/RX, mapping messages vers engine/cart).
- **API** : `midi_init()`, `midi_send_event()`, `midi_post_usb()`, `midi_process_rx()`.
- **Threads** : thread TX `NORMALPRIO+1`, thread RX `NORMALPRIO`.
- **Dépendances** : `usb_device`, `clock_manager`, `seq_engine`.

### usb/

#### usbcfg.c / usbcfg.h
- **Rôle** : configuration statique USB (descriptors, endpoints, callback `usb_event`).
- **API** : structures `usbcfg`, `vcom_config`, callbacks `usb_event()`.
- **Dépendances** : `usb_device.c`.

### apps/

#### kbd_chords_dict.c / kbd_chords_dict.h
- **Rôle** : dictionnaire d'accords pour le mode Keyboard (voicings, intervalles).
- **API** : `kbd_chord_lookup()`, tables `KBD_CHORDS`.
- **Dépendances** : `ui_keyboard_app`.

#### kbd_input_mapper.c / kbd_input_mapper.h
- **Rôle** : traduction des touches clavier physiques vers notes/modes (scales, transpose).
- **API** : `kbd_input_mapper_process()`, `kbd_input_mapper_set_scale()`.
- **Dépendances** : `ui_keyboard_bridge`, `seq_live_capture`.

#### seq_led_bridge.c / seq_led_bridge.h
- **Rôle** : pont modèle → backend LED pour afficher playhead, steps, p-locks.
- **API** : `seq_led_bridge_init()`, `seq_led_bridge_bind_pattern()`, `seq_led_bridge_publish()`.
- **Structures** : `seq_led_snapshot_t`, `seq_led_page_state_t`.
- **Dépendances** : `seq_model`, `ui_led_backend`.
- **Threads** : consommé par thread UI (rafraîchissement LED ~50 Hz).

#### ui_keyboard_app.c / ui_keyboard_app.h
- **Rôle** : logique haute-niveau du mode Keyboard (gestion overlays MODE/SETUP, interactions encodeurs/pads).
- **API** : `ui_keyboard_app_init()`, `ui_keyboard_app_process_event()`, `ui_keyboard_app_render()`.
- **Dépendances** : `ui_backend`, `kbd_input_mapper`, `seq_live_capture`.

#### ui_keyboard_bridge.c / ui_keyboard_bridge.h
- **Rôle** : glue entre `ui_backend` et `seq_live_capture` pour le mode Keyboard ; gère latche du mode actif.
- **API** : `ui_keyboard_bridge_attach()`, `ui_keyboard_bridge_handle_event()`.

#### ui_backend_midi_ids.h
- **Rôle** : mapping des paramètres UI vers identifiants MIDI (constantes `UI_MIDI_PARAM_*`).
- **Dépendances** : inclus par `ui_backend` et modules UI.

### ui/

#### ui_backend.c / ui_backend.h
- **Rôle** : gestion centrale de l'état UI (`ui_mode_context_t`, mute, transport, keyboard, pages SEQ).
- **API** : `ui_backend_init_runtime()`, `ui_backend_process_input()`, `ui_backend_param_changed()`, `ui_backend_get_mode_label()`.
- **Dépendances** : consomme `ui_shortcuts` (mapping), `ui_overlay`, `cart_link`, `seq_led_bridge`, `ui_led_backend`, `midi`.
- **Threads** : exécuté dans le thread UI.
- **Usage** : SHIFT+BS9 déclenche `UI_SHORTCUT_ACTION_OPEN_SEQ_OVERLAY` traité ici (mise à jour `custom_mode` + label).

#### ui_task.c / ui_task.h
- **Rôle** : boucle de rendu périodique (poll inputs → backend → render OLED → drain LED queue).
- **API** : `ui_task_start()`, thread `ui_task_thread()`.
- **Dépendances** : `ui_backend`, `ui_renderer`, `ui_led_backend`, `drv_buttons`, `drv_encoders` via `ui_input`.
- **Threads** : thread UI dédié (priorité `NORMALPRIO`), unique endroit où `chThdSleepMilliseconds` est autorisé.

#### ui_shortcuts.c / ui_shortcuts.h
- **Rôle** : mapping pur des événements bruts vers actions `ui_shortcut_action_t` (SHIFT combos, pages, holds).
- **API** : `ui_shortcut_map_process()`, `ui_shortcut_map_init()`, `ui_shortcut_map_reset()`.
- **Dépendances** : `ui_backend` (contexte), `ui_input`.

#### ui_input.c / ui_input.h
- **Rôle** : abstraction des entrées physiques (boutons, encodeurs) en événements `ui_input_event_t`.
- **API** : `ui_input_poll()`, `ui_input_decode_button()`, `ui_input_decode_encoder()`.
- **Dépendances** : `drv_buttons`, `drv_encoders`.

#### ui_renderer.c / ui_renderer.h
- **Rôle** : moteur de rendu OLED (bandeau, 4 cadres × 5 pages, widgets).
- **API** : `ui_renderer_init()`, `ui_renderer_begin()`, `ui_renderer_draw_frame()`, `ui_renderer_present()`.
- **Dépendances** : `ui_backend` (labels), `ui_model`, `ui_widgets`, `drv_display`.

#### ui_overlay.c / ui_overlay.h
- **Rôle** : définition des overlays/modes customs (SEQ, ARP, KBD, etc.), spécifications d'écran MODE/SETUP.
- **API** : `ui_overlay_init()`, `ui_overlay_get_spec()`, `ui_overlay_render()`.
- **Dépendances** : `ui_labels_common`, `ui_widgets`.

#### ui_led_backend.c / ui_led_backend.h
- **Rôle** : backend LED non bloquant (queue d'évènements, mapping palette, synchronisation).
- **API** : `ui_led_backend_init()`, `ui_led_backend_post_event()`, `ui_led_backend_refresh()`, `ui_led_backend_acquire_canvas()`.
- **Dépendances** : `drv_leds_addr`, `seq_led_bridge`, `ui_led_palette`.

#### ui_led_seq.c / ui_led_seq.h
- **Rôle** : helpers spécifiques au rendu LED du séquenceur (playhead, steps, p-lock hints).
- **API** : `ui_led_seq_render_page()`, `ui_led_seq_get_palette()`.
- **Dépendances** : `ui_led_backend`, `seq_led_bridge`.

#### ui_led_palette.h
- **Rôle** : palette de couleurs LED (constantes RGB pour états UI).

#### ui_model.c / ui_model.h
- **Rôle** : modèle de données UI (valeurs des paramètres, caches, structures partagées avec renderer).
- **API** : `ui_model_init()`, `ui_model_bind_cart()`, `ui_model_get_param()`.
- **Dépendances** : `ui_backend`, `cart_registry`.

#### ui_controller.c / ui_controller.h
- **Rôle** : orchestration des interactions UI (navigation BM, binding pages/params vers backend).
- **API** : `ui_controller_init()`, `ui_controller_handle_event()`.
- **Dépendances** : `ui_backend`, `ui_model`, `ui_seq_ui`, `ui_arp_ui`, `ui_keyboard_ui`.

#### ui_seq_ui.c / ui_seq_ui.h
- **Rôle** : logique UI spécifique au mode séquenceur (pages All/V1..V4, encodeurs, quick step, p-lock preview).
- **API** : `ui_seq_ui_init()`, `ui_seq_ui_process_event()`, `ui_seq_ui_render()`.
- **Dépendances** : `seq_model`, `seq_led_bridge`, `ui_backend`.

#### ui_arp_ui.c / ui_arp_ui.h
- **Rôle** : gestion du mode arpeggiateur (pattern, modes, assignation encodeurs).
- **API** : `ui_arp_ui_init()`, `ui_arp_ui_process_event()`, `ui_arp_ui_render()`.
- **Dépendances** : `ui_backend`, `seq_live_capture`, `midi`.

#### ui_keyboard_ui.c / ui_keyboard_ui.h
- **Rôle** : affichage du mode Keyboard (overlay, octave, chords).
- **API** : `ui_keyboard_ui_init()`, `ui_keyboard_ui_render()`, `ui_keyboard_ui_process_event()`.
- **Dépendances** : `ui_backend`, `ui_keyboard_app`.

#### ui_labels_common.c / ui_labels_common.h
- **Rôle** : générateurs de labels (notes, BPM, pattern, bandeau commun).
- **API** : `ui_labels_render_note()`, `ui_labels_render_bpm()`, `ui_labels_get_cart_name()`.

#### ui_knob.c / ui_knob.h
- **Rôle** : widget knob unipolaire/bipolaire (dessin + conversion valeur → angle).
- **API** : `ui_knob_draw()`, `ui_knob_compute_ticks()`.

#### ui_widgets.c / ui_widgets.h
- **Rôle** : collection de widgets (texte, icônes, bargraphs) utilisés par le renderer.
- **API** : `ui_widget_draw_text()`, `ui_widget_draw_icon()`, `ui_widget_draw_value_box()`.

#### ui_icons.c / ui_icons.h
- **Rôle** : tables d'icônes bitmap (note, transport, overlays).

#### ui_primitives.h
- **Rôle** : primitives graphiques bas niveau (rectangles, inversion de zone) partagées par renderer/widgets.

#### ui_types.h
- **Rôle** : types communs UI (identifiants de pages, structures d'évènements custom).

#### Font assets (font.c/h, font4x6.*, font5x7.*, font5x8_elektron.*)
- **Rôle** : polices bitmap multiples (hauteur 4x6, 5x7, Elektron).
- **API** : `font_get_glyph()`, tables de glyphes.

### docs/

#### README.md
- **Rôle** : documentation utilisateur générale.

#### seq_refactor_plan.md
- **Rôle** : feuille de route refactor (mise à jour régulière, inclut statut des étapes).

#### ARCHITECTURE_FR.md (ce document)
- **Rôle** : référence d'architecture pyramidale détaillée.

### tests/

#### seq_model_tests.c
- **Rôle** : tests host-only du modèle séquenceur (génération, defaults, limites p-lock).
- **API** : `main()` d'exécution host, assertions `BRICK_TEST_ASSERT_*`.
- **Dépendances** : `seq_model`.

## 4. Flux de données

### Exemple 1 — Quick step séquenceur
1. **Entrée physique** : un pad est pressé → `drv_buttons` capture l'état → `ui_input_poll()` émet un `ui_input_event_t`.
2. **Mapping** : `ui_shortcut_map_process()` détecte un `UI_SHORTCUT_ACTION_SEQ_STEP_HOLD` et met à jour le `held_mask` dans `ui_mode_context_t`.
3. **Backend** : `ui_backend_process_input()` active le step (toggle dans `seq_model`) et notifie `seq_led_bridge`.
4. **Modèle LED** : `seq_led_bridge_publish()` prépare un `seq_led_snapshot_t` pour la page courante.
5. **Rendu** : `ui_led_backend_refresh()` consomme le snapshot, `drv_leds_addr_render()` l'affiche. Le renderer OLED affiche l'état du step via `ui_seq_ui_render()` → `ui_renderer_draw_frame()`.

### Exemple 2 — Enregistrement live avec quantize et p-lock
1. **Entrée MIDI/Keyboard** : `ui_keyboard_bridge` reçoit un event -> `seq_live_capture_plan_step()` calcule quantize + micro-offset.
2. **Commit** : `seq_live_capture_commit_step()` applique note/velocity/locks au `seq_model_pattern_t`, incrémente la génération.
3. **Scheduler** : `seq_engine_reader` consomme la génération lors du prochain tick, planifie `seq_engine_event_t` (NOTE ON + p-lock) dans le scheduler.
4. **Player** : thread player dépile l'évènement, envoie NOTE ON via `midi_send_event()`, propage les p-locks à `cart_link_param_changed()` au timestamp planifié.
5. **Retour UI/LED** : `seq_led_bridge` voit la nouvelle génération, rafraîchit l'état LED ; `ui_renderer` met à jour la valeur affichée.

## 5. Threads & timing

| Thread / Contexte           | Source                       | Priorité           | Rôle principal | Communication |
|-----------------------------|------------------------------|--------------------|----------------|---------------|
| Clock 24 PPQN               | `clock_manager` (GPT)        | `NORMALPRIO+3`     | Publie les ticks vers engine/MIDI | Mailbox interne, callbacks rapides |
| Cart TX                     | `cart_link`                  | `NORMALPRIO+2`     | Sérialisation UART vers cartouches | Files circulaires DMA |
| Player séquenceur           | `seq_engine_player`          | `NORMALPRIO+1`     | Dépile `seq_engine_event_t` et déclenche NOTE ON/OFF | Queue lock-free |
| USB MIDI TX                 | `midi`                       | `NORMALPRIO+1`     | Envoi paquet USB | Mailbox |
| UI                          | `ui_task`                    | `NORMALPRIO`       | Poll I/O, déléguation backend, rendu OLED/LED | File d'évènements internes |
| Threads drivers auxiliaires | `drv_pots`/`drv_leds_addr`   | `LOWPRIO`          | Conversion ADC / DMA LED | Buffers partagés |

Contraintes : aucune opération bloquante dans les callbacks (ISR et GPT). Les temps sont exprimés en `systime_t`. Les communications utilisent des mailboxes/queues pour éviter le busy-wait. `ui_led_backend_post_event_i()` est safe en ISR.

## 6. Règles d’ingénierie et conventions

- **Documentation** : chaque fichier C/h possède un bloc Doxygen standard + include guard (`#ifndef BRICK_<PATH>_H`).
- **Compilation** : `make all USE_WARNINGS=yes` active `-Wall -Wextra`; les builds host utilisent `make check-host`.
- **Erreurs** : interfaces ChibiOS (`msg_t`, `eventmask_t`) respectent `MSG_OK`/`MSG_RESET`; côté Brick, `CH_SUCCESS` est préféré.
- **Mémoire** : allocations statiques uniquement ; buffers DMA alignés.
- **Temps réel** : pas de `printf` ni de `chThdSleep()` hors thread UI ; `systime_t` pour tous les timestamps.
- **Séparation des couches** :
  - UI ↛ drivers/cart : seul `ui_backend` parle au reste.
  - Engine ↛ UI : `seq_led_bridge` publie des snapshots, pas d'appel direct.
  - Cart ↛ UI : interactions via `cart_link` + `ui_backend_param_changed()`.
- **Tests** : préférer tests host (`tests/seq_model_tests.c`) pour le modèle ; intégrer `cppcheck` (`make lint-cppcheck`).

## 7. Annexes

### 7.1 Tableau des fichiers

| Module | Fichier(s) | Description synthétique |
|--------|------------|-------------------------|
| Racine | `main.c` | Boot, initialisations, boucle LED |
| Core   | `brick_config.h` | Config compile-time |
| Core   | `cart_link.c/h` | Pont UI ↔ cart, shadow registers |
| Core   | `clock_manager.c/h` | Gestion GPT + distribution ticks |
| Core   | `midi_clock.c/h` | Export clock → MIDI DIN/USB |
| Core   | `spec/cart_spec_types.h` | Types de specs cart |
| Core   | `seq/seq_model.c/h` | Modèle pattern 64×4 voix |
| Core   | `seq/seq_engine.c/h` | Reader/Scheduler/Player |
| Core   | `seq/seq_live_capture.c/h` | Capture live + quantize |
| Core   | `usb_device.c/h` | Wrapper pile USB ChibiOS |
| Cart   | `cart_bus.c/h` | Driver UART cartouches |
| Cart   | `cart_proto.c/h` | Encodage protocole XVA1 |
| Cart   | `cart_registry.c/h` | Gestion cart actives |
| Cart   | `cart_xva1_spec.c/h` | Spécification cartouche XVA1 |
| Drivers| `drivers.c/h` | Initialisation globale |
| Drivers| `drv_buttons.c/h` | Matrice boutons |
| Drivers| `drv_buttons_map.h` | Mapping BS/BM ↔ GPIO |
| Drivers| `drv_display.c/h` | OLED bufferisé |
| Drivers| `drv_encoders.c/h` | Encodeurs quadrature |
| Drivers| `drv_leds_addr.c/h` | LEDs adressables |
| Drivers| `drv_pots.c/h` | ADC potentiomètres |
| MIDI   | `midi.c/h` | Pile MIDI + threads |
| USB    | `usbcfg.c/h` | Descripteurs USB |
| Apps   | `kbd_chords_dict.c/h` | Voicings clavier |
| Apps   | `kbd_input_mapper.c/h` | Mapping touches → notes |
| Apps   | `seq_led_bridge.c/h` | Snapshot LED pattern |
| Apps   | `ui_keyboard_app.c/h` | Mode Keyboard high-level |
| Apps   | `ui_keyboard_bridge.c/h` | Liaison backend ↔ capture |
| Apps   | `ui_backend_midi_ids.h` | IDs MIDI internes |
| UI     | `ui_backend.c/h` | Gestion contexte UI |
| UI     | `ui_task.c/h` | Thread UI + render loop |
| UI     | `ui_shortcuts.c/h` | Mapping entrées → actions |
| UI     | `ui_input.c/h` | Décodage boutons/encodeurs |
| UI     | `ui_renderer.c/h` | Rendu OLED |
| UI     | `ui_overlay.c/h` | Overlays MODE/SETUP |
| UI     | `ui_led_backend.c/h` | Backend LED non bloquant |
| UI     | `ui_led_seq.c/h` | Helpers LED séquenceur |
| UI     | `ui_led_palette.h` | Palette LED |
| UI     | `ui_model.c/h` | Modèle UI |
| UI     | `ui_controller.c/h` | Orchestration UI |
| UI     | `ui_seq_ui.c/h` | Mode SEQ |
| UI     | `ui_arp_ui.c/h` | Mode ARP |
| UI     | `ui_keyboard_ui.c/h` | Overlay Keyboard |
| UI     | `ui_labels_common.c/h` | Labels dynamiques |
| UI     | `ui_knob.c/h` | Widget knob |
| UI     | `ui_widgets.c/h` | Widgets génériques |
| UI     | `ui_icons.c/h` | Icônes bitmap |
| UI     | `ui_primitives.h` | Primitives graphiques |
| UI     | `ui_types.h` | Types partagés UI |
| UI     | `font*.c/h` | Polices bitmap |
| Docs   | `README.md` | Vue d'ensemble utilisateur |
| Docs   | `seq_refactor_plan.md` | Roadmap refactor |
| Docs   | `ARCHITECTURE_FR.md` | Architecture pyramidale |
| Tests  | `tests/seq_model_tests.c` | Tests host modèle SEQ |

### 7.2 Diagramme pyramidal détaillé

```
                                 +---------------------+
                                 |     Applications    |
                                 | (SEQ LED, Keyboard) |
                                 +----------+----------+
                                            |
                          +-----------------v------------------+
                          |               UI                   |
                          | Backend ← Shortcuts ← Input        |
                          | Renderer ↔ Model ↔ Widgets         |
                          | LED Backend ↔ LED Seq helpers      |
                          +-----------------+------------------+
                                            |
                +---------------------------v-----------------------------+
                |                         Core                            |
                | Clock mgr ←→ MIDI clock   Seq Engine ↔ Live Capture     |
                | Cart Link ↔ Cart Registry ↔ Cart Spec                   |
                | USB Device wrapper                                        |
                +---------------------------+-----------------------------+
                                            |
          +-------------------------------  v  ------------------------------+
          |                               Drivers                           |
          | Buttons / Encoders / Display / LEDs / Pots / UART / USB         |
          +-------------------------------+--------------------------------+
                                            |
                               +------------v------------+
                               |    STM32F429 + HAL      |
                               +-------------------------+
```

### 7.3 Notes pour l’extension du firmware
- **Ajouter un module UI** : définir les widgets dans `ui_overlay`, implémenter la logique dans un nouveau `ui_<mode>_ui.c` en s'appuyant sur `ui_backend` pour l'état, et brancher via `ui_controller`.
- **Ajouter une cartouche** : créer `<cart>_spec.c/h` décrivant menus + paramètres, l'enregistrer via `cart_registry_register()` et fournir les handlers `cart_link` appropriés.
- **Étendre le séquenceur** : ajouter des attributs au `seq_model` (augmenter les tailles compile-time), puis propager dans `seq_engine` et `seq_led_bridge`; mettre à jour les tests host.
- **Nouveau périphérique** : créer un driver dans `drivers/`, exposer `*_init()` / `*_poll()`, puis l'enregistrer via `drivers_init_all()`.

