# Architecture logicielle actuelle du firmware Brick

## 1. Vue d'ensemble
Brick est un firmware séquenceur pour **STM32F429 + ChibiOS 21.11.x**. Il pilote des cartouches DSP externes et diffuse des événements MIDI sans générer de son propre. L'organisation respecte la séparation **Model / Engine / UI / Cart / Drivers / MIDI**. Les interactions temps réel sont coordonnées par un thread UI unique (`ui_task.c`) et par le gestionnaire d'horloge (`core/clock_manager.c`).

Principes structurants :

* Le **modèle de séquenceur** (`core/seq/seq_model.c`) contient l'état sérialisable : 64 pas, 4 voix par pas, p-locks internes (note, vélocité, longueur, micro, offsets "All") et p-locks cart.
* Le **moteur** (`core/seq/seq_engine.c`) lit le modèle à chaque tick 1/16, ordonne note on/off et p-locks via `seq_engine_runner.c`, et ne change jamais le modèle directement.
* L'**UI** (répartition `ui/` + ponts `apps/`) capte boutons/encodeurs/clavier, applique les modifications via `ui_backend.c`, tient à jour les LED via `seq_led_bridge.c` et `ui_led_backend.c`, et publie les événements MIDI en direct pour le mode clavier.
* Les **cartouches** (`cart/`) reçoivent leurs p-locks via `cart_link.c` qui manipule un shadow de paramètres et sérialise les trames UART.
* La couche **MIDI** (`midi/midi.c`) fournit les primitives note on/off/CC utilisées par l'UI, le runner et la clock.

## 2. Arborescence commentée

### Racine
* `main.c` : point d'entrée. Initialise HAL/RTOS, les piles USB/MIDI/clock, les drivers, les cartouches, l'UI, puis boucle en rafraîchissant `ui_led_backend_refresh()`.
* `Makefile` : inclut les règles ChibiOS, définit les cibles embarquées, le lint (`lint-cppcheck`) et les tests host (`check-host`).
* `README.md`, `SEQ_BEHAVIOR.md` : documentation fonctionnelle historique.
* `Brick4_labelstab_uistab_phase4.zip`, `drivers/drivers.zip`, `log.txt`, `tableaudebord.txt` : artefacts non utilisés par le build (candidats au nettoyage).

### `core/`
* `clock_manager.c` : convertit les ticks MIDI (24 PPQN) en steps 1/16, publie `clock_step_info_t` via callback.
* `midi_clock.c` : pilote la GPT interne, relaye Start/Stop/Song Position.
* `cart_link.c` : shadow des paramètres cart, notifications vers UART.
* `usb_device.c` : démarrage USB Device / MIDI.
* `seq/seq_model.c` : modèle du pattern + helpers (`seq_model_step_make_neutral`, `seq_model_step_recompute_flags`, etc.).
* `seq/seq_engine.c` : moteur Reader/Scheduler/Player, callbacks `note_on`, `note_off`, `plock`.
* `seq/seq_live_capture.c` : planifie les événements live (note on/off) en utilisant la clock et enregistre note, vélocité, longueur et micro sous forme de p-locks internes.
* `arp/arp_engine.c` : moteur d'arpégiateur temps réel (pattern, swing, strum, repeat, LFO) piloté par le mode clavier. // --- ARP: nouveau moteur ---

### `apps/`
* `seq_engine_runner.c` : instancie `seq_engine`, traduit les callbacks en messages MIDI (`midi_note_on/off`) ou cart (`cart_link_param_changed`).
* `seq_led_bridge.c` : conserve un snapshot `seq_model_pattern_t` pour le rendu LED, gère le mode hold, applique les p-locks SEQ/cart sur les steps maintenus, et recalcule les drapeaux.
* `seq_recorder.c` : relie `ui_keyboard_bridge` au live capture, maintient les voix actives pour mesurer les longueurs de note.
* `ui_keyboard_bridge.c` : convertit l'état UI keyboard vers des notes MIDI en direct ou via `arp_engine` (quand activé) tout en relayant les événements vers `seq_recorder`. // --- ARP: intégration moteur ---
* `kbd_*` : dictionnaire d'accords et mapper clavier.

### `ui/`
* `ui_task.c` : thread principal. Initialise `clock_manager`, enregistre `_on_clock_step`, démarre backend/clavier/runner, boucle sur `ui_backend_process_input()` puis `ui_led_backend_refresh()` et `ui_render()`.
* `ui_backend.c` : coeur de traitement des entrées. Route les encoders/boutons vers cart (`cart_link_param_changed`), UI, ou MIDI. Pendant un hold (`s_mode_ctx.seq.held_mask`), appelle `seq_led_bridge_apply_plock_param` ou `seq_led_bridge_apply_cart_param` pour stocker des p-locks sur les steps maintenus.
* `ui_controller.c` : état UI (menus/pages), initialisation des cycles BM, activation du mode LED SEQ et appel à `seq_led_bridge_init()`.
* `ui_led_backend.c` : file d'événements LED (mute, clock, mode). Diffuse sur `drv_leds_addr_render()`.
* `ui_led_seq.c` : renderer SEQ; applique le playhead absolu, distingue active/automation/muted.
* `ui_renderer.c`, `ui_model.c`, `ui_input.c`, `ui_widgets.c`, `ui_overlay.c`, etc. : pipeline OLED et modèle UI.

### `cart/`
* `cart_registry.c` : enregistre les specs de cartouche (XVA1). Fournit `cart_registry_get_ui_spec()`.
* `cart_xva1_spec.c` : description complète de la cartouche (menus, cycles BM, ID de paramètres).
* `cart_bus.c`, `cart_proto.c` : couche UART et protocole.

### `drivers/`
* Pilotes matériels (boutons, encodeurs, LEDs adressables, OLED, potentiomètres). `drv_leds_addr.c` est consommé par `ui_led_backend`.

### `midi/`
* `midi.c` / `midi.h` : threads TX/RX, primitives `midi_note_on/off`, `midi_cc`, `midi_start/stop/clock`.

### `tests/`
* `seq_model_tests.c` : tests unitaires du modèle.
* `seq_hold_runtime_tests.c` : tests host intégrant `seq_led_bridge`, `seq_live_capture` et les chemins hold/preview.
* `tests/stubs/ch.h` : stubs ChibiOS pour la compilation host.

## 3. Dépendances clés et interfaces

* `ui_task.c` dépend de `clock_manager.h`, `seq_engine_runner.h`, `seq_recorder.h`, `ui_led_backend.h`, `seq_led_bridge.h` et assure la synchronisation temps réel.
* `seq_engine_runner.c` dépend de `seq_engine.h`, `midi.h`, `cart_link.h`, `cart_registry.h`, `ui_mute_backend.h` pour router les callbacks.
* `seq_led_bridge.c` inclut `core/seq/seq_model.h` pour manipuler le pattern local, et `ui_led_seq.h` pour pousser le snapshot vers le renderer.
* `ui_backend.c` s'appuie sur `ui_model.h` (shadow UI), `ui_led_backend.h` (changement de mode), `seq_led_bridge.h` (hold) et `cart_link.h`.
* `seq_recorder.c` et `ui_keyboard_bridge.c` partagent `seq_live_capture.h` et les APIs directes de `ui_backend` (`ui_backend_note_on/off`).
* `clock_manager.c` invoque `midi_clock.h` et `midi.h`; le callback step est enregistré par `ui_task.c`.

## 4. Flux runtime principal

### 4.1 Transport → Engine
1. `clock_manager_init()` configure la source interne et enregistre `on_midi_tick()` auprès de `midi_clock`.
2. `midi_clock` déclenche `on_midi_tick()` à chaque F8. Après 6 ticks, `clock_manager` appelle `_on_clock_step()` (dans `ui_task.c`) avec un `clock_step_info_t` complet (index absolu, durée de step/tick, BPM).
3. `_on_clock_step()` alimente :
   * `ui_led_backend_post_event_i(UI_LED_EVENT_CLOCK_TICK, step_abs, true)` ⇒ `ui_led_seq_on_clock_tick()` (via la file) pour déplacer le playhead.
   * `seq_recorder_on_clock_step(info)` ⇒ `seq_live_capture_update_clock()` maintient les timestamps pour mesurer les longueurs de note.
   * `seq_engine_runner_on_clock_step(info)` ⇒ `seq_engine_process_step()` lit le modèle, planifie note on/off et p-locks, puis les callbacks runner envoient `midi_note_on/off` ou `cart_link_param_changed`.
4. `seq_engine_process_step()` trie les évènements planifiés par `scheduled_time` afin de déclencher toutes les voix d'un step simultanément quand leurs micro-offsets sont identiques.
5. Lors d'un STOP, `seq_engine_stop()` vide la scheduler avant de rejoindre le thread joueur et appelle `_seq_engine_all_notes_off()` pour couper immédiatement toutes les voix encore actives.

### 4.2 Édition SEQ hold & p-locks
1. `ui_backend_process_input()` détecte un appui sur un pad SEQ, met à jour `s_mode_ctx.seq.held_mask` et appelle `seq_led_bridge_begin_plock_preview()`.
2. Pendant le hold, les mouvements d'encodeur :
   * Pages SEQ (`UI_DEST_UI`) ⇒ `_resolve_seq_param()` résout `seq_hold_param_id_t`, `seq_led_bridge_apply_plock_param()` écrit les offsets ou voix dans les steps maintenus, crée les p-locks internes via `_ensure_internal_plock_value()`, recalcule les flags (`seq_model_step_recompute_flags`) et publie.
   * Paramètres cart (`UI_DEST_CART`) ⇒ `seq_led_bridge_apply_cart_param()` enregistre les p-locks cart dans les steps maintenus.
3. À la release, `seq_led_bridge_end_plock_preview()` et `_hold_sync_mask()` committent les steps stagés, recalculent le `seq_runtime_t` et forcent `seq_led_bridge_publish()` pour rafraîchir les LED.
4. Les steps verts (actifs ou contenant des p-locks SEQ) conservent leur note et leur vélocité (`seq_model_step_make_neutral()`), les steps bleus (automation pure) ont vélocité voix1 = 0.
5. Le “quick toggle” ne joue plus de pré-écoute MIDI : les notes ne sont émises qu'en playback.

### 4.3 Lecture et classification
* `seq_led_bridge_publish()` agrège le pattern et renseigne `seq_runtime_t.steps[]` : `active`, `automation`, `muted`.
* `ui_led_seq_render()` colore : vert = `active`, bleu = `automation`, rouge = mute.
* `seq_model_step_recompute_flags()` pose `flags.automation = (!has_voice) && has_cart_plock && !has_seq_plock`, garantissant que toute présence de p-lock SEQ garde le step vert.

### 4.4 Mode clavier & capture live
1. `ui_keyboard_bridge_init()` lit le shadow UI (`ui_backend_shadow_get()`), configure `ui_keyboard_app`, `kbd_input_mapper` **et** `arp_engine` avec les callbacks MIDI/recorder puis laisse le mode LED actif (SEQ) inchangé. // --- ARP: initialisation bridge ---
2. `ui_keyboard_bridge_update_from_model()` synchronise Root/Scale/Omnichord, mais aussi les 20 paramètres du sous-menu Arpégiateur (`apps/ui_arp_menu.c`). Toute modification déclenche `arp_set_config()` ; le flag legacy `KBD_UI_LOCAL_ARP` est tenu à jour pour compatibilité. // --- ARP: synchro paramètres ---
3. `sink_note_on()` route soit directement vers `seq_recorder_handle_note_on()` + `ui_backend_note_on()` (ARP OFF), soit vers `arp_note_input()` (ARP ON). Le moteur planifie swing/strum/repeat et renvoie les notes générées via ses callbacks, qui continuent d'alimenter le recorder avant l'émission MIDI.
4. `sink_note_off()` applique la même logique (direct MIDI ou `arp_note_input(..., pressed=false)`), garantissant que l'arrêt clavier provoque immédiatement les NOTE_OFF ou un `arp_stop_all()` si nécessaire. // --- ARP: gestion release ---
5. `ui_keyboard_bridge_tick()` est appelé à chaque boucle UI (`systime_t now`) pour faire avancer `arp_engine` à haute résolution (BPM courant) et désenfiler les note-on/off planifiés.

### 4.5 Modes MUTE / PMUTE
1. Entrer en PMUTE (`UI_SHORTCUT_ACTION_ENTER_MUTE_PMUTE`) bascule le backend LED en mode MUTE, republie l'état courant via `ui_mute_backend_publish_state()` et garantit que les LED reflètent immédiatement les tracks préparées ou commitées.
2. `ui_mute_backend_toggle_prepare()` émet `UI_LED_EVENT_PMUTE_STATE` pour chaque track préparée ; `ui_mute_backend_commit()` / `ui_mute_backend_cancel()` synchronisent respectivement l'état réel (`UI_LED_EVENT_MUTE_STATE`) ou nettoient l'aperçu.

### 4.5 Modes MUTE / PMUTE
1. Entrer en PMUTE (`UI_SHORTCUT_ACTION_ENTER_MUTE_PMUTE`) bascule le backend LED en mode MUTE, republie l'état courant via `ui_mute_backend_publish_state()` et garantit que les LED reflètent immédiatement les tracks préparées ou commitées.
2. `ui_mute_backend_toggle_prepare()` émet `UI_LED_EVENT_PMUTE_STATE` pour chaque track préparée ; `ui_mute_backend_commit()` / `ui_mute_backend_cancel()` synchronisent respectivement l'état réel (`UI_LED_EVENT_MUTE_STATE`) ou nettoient l'aperçu.

## 5. Politique MIDI et cartouches

* `seq_engine_runner_on_transport_stop()` diffuse désormais un CC123 “All Notes Off” sur les 16 canaux avant de relayer les NOTE_OFF individuels via `_runner_note_off_cb()` et de restaurer les paramètres cart p-lockés ; hors STOP, aucune commande globale n'est émise.
* `midi.c` centralise l'émission des Channel Mode Messages (CC#120-127) via `midi_all_notes_off()`, `midi_all_sound_off()`, etc., évitant les stubs dispersés.
* Les p-locks cart sont appliqués via `cart_link_param_changed()` et restaurés lorsque leur profondeur (`slot->depth`) retombe à zéro.
* `cart_registry_get_active_id()` et `cart_link_shadow_get/set()` fournissent les valeurs courantes aux autres modules.

## 6. Tests & outillage

* `make -j8 all` : build complet embarqué via les règles ChibiOS.
* `make clean` : nettoyage du répertoire `build/`.
* `make lint-cppcheck` : exécute `cppcheck` sur `core/` et `ui/`.
* `make check-host` : compile et lance `tests/seq_model_tests` puis `tests/seq_hold_runtime_tests` avec `gcc -std=c11 -Wall -Wextra -Wpedantic`.
* Les stubs `tests/stubs/ch.h` fournissent les symboles ChibiOS manquants pour les tests host.

## 7. Points d'extension identifiés

* **Patterns multiples / banques** : `seq_led_bridge.c` expose `seq_led_bridge_access_pattern()` pour partager le pattern avec d'autres modules ; l'initialisation se fait dans `ui_task.c`.
* **Nouveaux modes UI** : `ui_backend.c` gère les overlays via `ui_overlay.h` et `ui_shortcuts`. Ajouter un mode implique de fournir un `ui_cart_spec_t` et de mettre à jour les cycles BM dans `ui_controller.c`.
* **Cartouches supplémentaires** : enregistrer une nouvelle spec via `cart_registry_register()` et fournir les mappings `ui_spec`/`cart_link`.
* **Tests runtime** : `tests/seq_hold_runtime_tests.c` montre comment instrumenter `seq_led_bridge_apply_plock_param()` et `seq_live_capture_commit_plan()` sans RTOS.

## 8. Éléments obsolètes ou redondants

* Archives et logs (`Brick4_labelstab_uistab_phase4.zip`, `drivers/drivers.zip`, `log.txt`, `tableaudebord.txt`) ne participent pas au build.
* `docs/ARCHITECTURE_FR.md` (ancienne version) décrivait des fonctions inexistantes (`seq_led_bridge_tick()`, `seq_led_bridge_bind_pattern()`). Cette version reflète les noms et flux actuels.
* Aucun double des structures principales (`seq_model_step_t`, `seq_runtime_t`). Les entêtes `*_v2` ou backups n'existent plus, mais vérifier régulièrement les répertoires pour éviter leur réapparition.

## 9. Résumé des invariants comportementaux

* Aucun `All Notes Off` implicite : seules `ui_backend_all_notes_off()` (action utilisateur) et le bouton STOP (via `seq_engine_runner_on_transport_stop()`) diffusent CC123 global, tandis que le moteur continue d'émettre des NOTE_OFF individuels.
* Le STOP invoque désormais `ui_keyboard_bridge_on_transport_stop()` pour purger `arp_engine` avant le CC123 global, garantissant qu'aucune voix arpégiateur ne reste suspendue. // --- ARP: flush STOP ---
* Un step contenant au moins un p-lock SEQ reste musical (LED verte, vélocité de la voix 1 conservée).
* `seq_live_capture` enregistre note, vélocité, longueur et micro-timing à partir des timestamps `clock_manager`.
* `seq_led_bridge` garantit que les steps maintenus reçoivent les p-locks au moment des mouvements d'encodeur, avec commit à la release.
* Le playhead LED utilise l'index absolu (`ui_led_seq_on_clock_tick`) pour rester aligné avec le moteur, sans réinitialisation lors du changement de page.

Cette documentation reflète l'état réel du dépôt actuel et sert de base à toute refactorisation progressive.
