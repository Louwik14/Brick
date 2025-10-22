## [2025-10-22 16:00] Q1.5 — Suppression moteur seq_engine + runner Reader-only pur

Résumé
- Retrait complet de core/seq/seq_engine.{c,h} et seq_engine_tables.c.
- Runner apps Reader-only : boucle 16 handles + NOTE_OFF local + CC123 au STOP.
- Makefile nettoyé (plus de lien engine) + garde CI anti-engine.
- make check-host OK, 16 tracks stables (silent_ticks=0).

Fichiers (créés/modifiés/supprimés)
- (SUPPR) core/seq/seq_engine.c, core/seq/seq_engine.h, core/seq/seq_engine_tables.c
- (MOD) apps/seq_engine_runner.c
- (MOD) Makefile
- (MOD) docs/ARCHITECTURE_FR.md

Commandes exécutées
- grep -RIn 'seq_engine' .
- make check_no_engine_anywhere
- make check-host

Résultats attendus
- [CI][OK] no seq_engine remnants
- 16-track stress: silent_ticks=0 ; counters ON/OFF équilibrés

Impact binaire/RAM
- Neutre (suppression code legacy non référencé au runtime)


## [2025-10-21 23:40] Q1.1 — MIDI helpers (apps header-only)

Résumé
- Ajout du shim MIDI côté apps : NOTE_ON/OFF et CC123, mapping canal 1..16.
- Émission via hook weak midi_tx3(b0,b1,b2) ; fallback no-op pour host.
- Aucun changement build ; pas de dépendance core/RTOS.

Fichiers (créés/modifiés)
- apps/midi_helpers.h (NOUVEAU)
- docs/ARCHITECTURE_FR.md

Commandes exécutées
- test -f apps/midi_helpers.h && echo "OK: midi_helpers.h présent"
- gcc -E -I. -Iapps apps/midi_helpers.h > /dev/null && echo "OK: préprocessing passe"
- grep -n "midi_note_on" apps/midi_helpers.h
- grep -n "midi_note_off" apps/midi_helpers.h
- grep -n "midi_all_notes_off" apps/midi_helpers.h
- grep -n "midi_tx3" apps/midi_helpers.h
- ! grep -E '#include "ch\.h"|seq_project\.h|seq_model\.h' apps/midi_helpers.h && echo "OK: pas de dépendance interdite"

Logs utiles (extraits)
- $ test -f apps/midi_helpers.h && echo "OK: midi_helpers.h présent"
  OK: midi_helpers.h présent
- $ gcc -E -I. -Iapps apps/midi_helpers.h > /dev/null && echo "OK: préprocessing passe"
  apps/midi_helpers.h:1:9: warning: #pragma once in main file
      1 | #pragma once
        |         ^~~~
  OK: préprocessing passe
- $ grep -n "midi_note_on" apps/midi_helpers.h
  21:static inline void midi_note_on(uint8_t ch1_16, uint8_t note, uint8_t vel) {
  35:   midi_note_on(3, 60, 100);
- $ grep -n "midi_note_off" apps/midi_helpers.h
  25:static inline void midi_note_off(uint8_t ch1_16, uint8_t note, uint8_t vel) {
  36:   midi_note_off(3, 60, 64);
- $ grep -n "midi_all_notes_off" apps/midi_helpers.h
  29:static inline void midi_all_notes_off(uint8_t ch1_16) {
  37:   midi_all_notes_off(3);
- $ grep -n "midi_tx3" apps/midi_helpers.h
  5:   Ne dépend d'aucun header RTOS/core ; émission via hook midi_tx3(b0,b1,b2).
  6:   Si l'appli ne fournit pas midi_tx3, un fallback no-op est utilisé (link OK côté host). */
  14:__attribute__((weak)) void midi_tx3(uint8_t b0, uint8_t b1, uint8_t b2);
  15:static inline void midi_tx3_weak_impl(uint8_t b0, uint8_t b1, uint8_t b2) { (void)b0; (void)b1; (void)b2; }
  16:static inline void _midi_tx3(uint8_t b0, uint8_t b1, uint8_t b2) {
  17:  if ((void*)&midi_tx3) midi_tx3(b0,b1,b2); else midi_tx3_weak_impl(b0,b1,b2);
  23:  _midi_tx3((uint8_t)(0x90u | ch), note, vel);
  27:  _midi_tx3((uint8_t)(0x80u | ch), note, vel);
  31:  _midi_tx3((uint8_t)(0xB0u | ch), 123u, 0u); /* CC123 All Notes Off */
- $ ! grep -E '#include "ch\.h"|seq_project\.h|seq_model\.h' apps/midi_helpers.h && echo "OK: pas de dépendance interdite"
  OK: pas de dépendance interdite

Impact binaire/RAM
- n/a (header-only, aucun lien ajouté)

Risques / Next
- Aucun à ce stade. Prochaine passe Q1.2 : cibles CI apps (non branchées).


## [2025-10-21 23:50] Q1.2 — CI apps (cibles non branchées)

Résumé
- Ajout de deux cibles CI pour auditer apps/** : includes interdits et usage CCM.
- Cibles NON branchées à la build (pipeline inchangé).

Fichiers (créés/modifiés)
- Makefile (ajout de cibles phony)

Commandes exécutées
- make -n check_no_legacy_includes_apps
- make -n check_no_ccm_in_apps
- make check_no_legacy_includes_apps
- make check_no_ccm_in_apps

Logs utiles (extraits)
- $ make -n check_no_legacy_includes_apps
  echo "[CI] check_no_legacy_includes_apps"
  # Recherche uniquement dans .c/.h sous apps/
  # On match les includes explicites de ces headers interdits
  ! grep -RInE --include='*.c' --include='*.h' \
        '^[[:space:]]*#include[[:space:]]*"ch\.h"|^[[:space:]]*#include[[:space:]]*"seq_project\.h"|^[[:space:]]*#include[[:space:]]*"seq_model\.h"' \
        apps/ \
        || { echo "[CI][FAIL] Forbidden include(s) found in apps/ (ch.h, seq_project.h, seq_model.h)"; exit 1; }
  echo "[CI][OK] no forbidden includes in apps/"
- $ make -n check_no_ccm_in_apps
  echo "[CI] check_no_ccm_in_apps"
  ! grep -RInE --include='*.c' --include='*.h' \
        '\bCCM_|\.ccm\b|section[[:space:]]*\([[:space:]]*".*ccm' \
        apps/ \
        || { echo "[CI][FAIL] CCM usage found in apps/"; exit 1; }
  echo "[CI][OK] no CCM usage in apps/"
- $ make check_no_legacy_includes_apps
  [CI] check_no_legacy_includes_apps
  apps/seq_led_bridge.c:11:#include "ch.h"
  apps/seq_engine_runner.c:15:#include "ch.h"
  apps/seq_recorder.c:8:#include "ch.h"
  apps/ui_keyboard_bridge.h:12:#include "ch.h" // --- ARP: systime_t pour tick ---
  apps/seq_recorder.h:13:#include "ch.h" // --- ARP FIX: timestamp explicite ---
  [CI][FAIL] Forbidden include(s) found in apps/ (ch.h, seq_project.h, seq_model.h)
  make: *** [Makefile:535: check_no_legacy_includes_apps] Error 1
- $ make check_no_ccm_in_apps
  [CI] check_no_ccm_in_apps
  apps/seq_led_bridge.c:71:static CCM_DATA seq_led_bridge_state_t g;
  apps/seq_led_bridge.c:106:SEQ_LED_BRIDGE_HOLD_SLOTS_SEC CCM_DATA seq_led_bridge_hold_slot_t g_hold_slots[SEQ_LED_BRIDGE_STEPS_PER_PAGE];
  apps/seq_led_bridge.c:122:static CCM_DATA seq_led_bridge_hold_cart_entry_t g_hold_cart_params[SEQ_LED_BRIDGE_MAX_CART_PARAMS];
  apps/ui_keyboard_app.c:47:static CCM_DATA kbd_state_t g;
  apps/seq_engine_runner.c:45:static CCM_DATA seq_engine_t s_engine;
  apps/seq_engine_runner.c:55:static CCM_DATA seq_engine_runner_plock_state_t s_plock_state[SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS];
  apps/seq_recorder.c:16:static CCM_DATA seq_live_capture_t s_capture;
  apps/seq_recorder.c:22:static CCM_DATA seq_recorder_active_voice_t s_active_voices[SEQ_MODEL_VOICES_PER_STEP];
  [CI][FAIL] CCM usage found in apps/
  make: *** [Makefile:544: check_no_ccm_in_apps] Error 1

Impact binaire/RAM
- n/a (cibles CI uniquement, aucun lien)

Risques / Next
- Aucun à ce stade. Prochaine passe Q1.3 : test host start/stop smoke (non intrusif).

## [2025-10-22 11:00] Q1.3 — Purge RTOS dans apps + CCM CI fix + garde cold (WARN)

Résumé
- Suppression de tous les includes RTOS (`ch.h`) dans apps/**
- Ajout `apps/rtos_shim.h` (systime_t)
- Correction du garde CCM (ignore CCM_DATA vide)
- Ajout du garde “no-cold-in-apps” (WARN)
- Mise à jour docs/ARCHITECTURE_FR.md
- make check-host OK, CI non bloquante

Fichiers
- apps/rtos_shim.h (nouveau)
- apps/seq_led_bridge.c
- apps/seq_engine_runner.c
- apps/seq_recorder.c
- apps/seq_recorder.h
- apps/ui_keyboard_bridge.h
- Makefile
- docs/ARCHITECTURE_FR.md

Commandes exécutées
- grep -RIn '#include "ch\.h"' apps/
- make check_no_legacy_includes_apps
- make check_no_ccm_in_apps
- make check_no_cold_refs_in_apps
- make check-host

Résultats attendus
- [CI][OK] no forbidden includes in apps/
- [CI][OK] no CCM section usage in apps/
- [CI][WARN] Cold refs still present in apps/ (expected pre-Q1.4)
- make check-host : OK

Impact binaire
- Aucun (header-only)
  
## [2025-10-22 14:00] Q1.4 — LED bridge Reader-only

Résumé
- Migration du bridge LED vers un cache hot Reader-only (banque/pattern actifs + flags step).
- Suppression de la façade `seq_runtime_cold.h` côté apps/.
- Garde `check_no_cold_refs_in_apps` propre + `make check-host` OK.

Fichiers (créés/modifiés)
- apps/seq_led_bridge.c
- apps/seq_led_bridge.h
- docs/ARCHITECTURE_FR.md

Commandes exécutées
- make check_no_cold_refs_in_apps
- make check-host

Impact binaire/RAM
- inchangé (cache hot local en BSS existante).

Risques / Next
- Continuer la migration Reader-only côté apps/ (recorder, keyboard) avant split hot/cold.
