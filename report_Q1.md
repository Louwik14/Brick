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
