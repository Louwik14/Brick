# Host vs Target Runtime Hooks (avril 2025)

Ce mémo compare le flux exercé par `make check-host` et celui exécuté sur la cible STM32/ChibiOS afin d’écarter toute divergence de câblage.

## 1. Edition hold / p-locks
- **Cible** : `ui_backend_param_changed()` (fichier `ui/ui_backend.c`) détecte un step tenu (`s_mode_ctx.seq.held_mask != 0`). Les rotations d’encodeur appellent directement `seq_led_bridge_apply_plock_param()` ou `seq_led_bridge_apply_cart_param()`. À la release (`ui_backend_seq_hold_release()`), l’UI déclenche `seq_led_bridge_end_plock_preview()`.
- **Host** : le test `tests/seq_hold_runtime_tests.c` invoque les mêmes fonctions (`seq_led_bridge_begin_plock_preview()`, `seq_led_bridge_apply_*`, `seq_led_bridge_end_plock_preview()`). Aucune branche `#ifdef` ne masque ces appels : la logique compilée est identique.

## 2. Classification LED / publish runtime
- **Cible** : `seq_led_bridge_publish()` renvoie les états vers `ui_led_seq_update_from_app()` (dans `ui/ui_led_seq.c`) via `ui_led_seq_set_total_span()` et `ui_led_seq_set_running()`. La classification vert/bleu repose sur `seq_model_step_recompute_flags()`.
- **Host** : les stubs de `tests/seq_hold_runtime_tests.c` capturent les appels à `ui_led_seq_update_from_app()` et vérifient les flags `active` / `automation`. Les mêmes fonctions de `seq_model` sont utilisées, sans garde conditionnelle.

## 3. Pont clavier et NOTE OFF ciblé
- **Cible** : `ui_keyboard_bridge.c` enregistre un sink dont `sink_note_off()` appelle `seq_recorder_handle_note_off()` puis `ui_backend_note_off()`. `sink_all_notes_off()` est volontairement vide (pas de MIDI All Notes Off hors STOP).
- **Host** : `make check-host` compile désormais `apps/seq_recorder.c` et `apps/seq_led_bridge.c`. Les tests traversent `ui_keyboard_app.c` avec un `ui_keyboard_note_sink_t` factice, comptent les `note_off` et confirment que `all_notes_off` reste à zéro. Même implémentation que sur cible.

## 4. Horloge / capture live
- **Cible** : `clock_manager` pousse `clock_step_info_t` vers `seq_live_capture_update_clock()` (dans `core/seq/seq_live_capture.c`).
- **Host** : la suite `seq_hold_runtime_tests` alimente le même code (`core/seq/seq_live_capture.c` + `apps/seq_recorder.c`) avec des timestamps synthétiques et vérifie que la longueur de note capturée devient un p-lock SEQ et que les LED restent vertes.

## 5. Points de vigilance restants
- Les seules différences host/target résident dans les stubs ChibiOS (`tests/stubs/ch.h`) et l’absence de drivers physiques. Aucun hook “manquant” n’a été détecté après suppression des API orphelines `seq_led_bridge_set_plock_mask()` / `seq_led_bridge_plock_clear()`.
- Sur STM32, vérifier que les handlers UI appellent bien `seq_led_bridge_plock_add/remove` autour du hold (déjà couvert par `ui/ui_backend.c`). Aucun `#ifdef` ChibiOS ne contourne ces appels.

Conclusion : le runtime exercé par les tests host correspond au chemin exécuté sur la cible. Les corrections appliquées (suppression des All Notes Off globaux, classification SEQ vs cart) partagent le même code entre les deux environnements.
