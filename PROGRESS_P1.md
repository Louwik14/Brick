# PROGRESS_P1 — Handles + Reader (journal)

## [2025-10-21 11:30] MP0 — Façade handles + reader skeleton
### Contexte lu
- ARCHITECTURE_FR.md — séparation UI/Engine, contrainte pipeline Reader → Scheduler → Player.
- RUNTIME_MULTICART_REPORT.md — baseline mémoire : .data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o.
- SEQ_BEHAVIOR.md — invariants fonctionnels (pas d’accès direct runtime depuis apps, pipeline R→S→P, aucune alloc au tick).
- Rappel invariants fonctionnels : accès indirect aux modèles depuis apps/**, respect flux Reader → Scheduler → Player, aucune allocation à chaque tick.
### Étapes réalisées
- Création des headers `core/seq/seq_config.h`, `core/seq/seq_handles.h`, `core/seq/seq_views.h`, `core/seq/seq_access.h`.
- Ajout de l’API Reader déclarative (`core/seq/reader/seq_reader.h`) et squelette C (`core/seq/reader/seq_reader.c`).
- Mise à jour du Makefile pour compiler les nouvelles sources Reader.
### Tests
- make -j : ⚠️ KO (toolchain arm-none-eabi-gcc absente dans l’environnement de test).
- make check-host : OK.
### Audits mémoire
- Inchangés vs baseline attendue (.data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o).
### Décisions
- Aucun changement comportemental ; Reader exposé mais inactif tant que `SEQ_USE_HANDLES` reste à 0.
### Blocages & TODO
- Aucun.

## [2025-10-21 16:45] MP1 — Reader façade legacy
### Contexte lu
- ARCHITECTURE_FR.md — rappel pipeline Reader → Scheduler → Player.
- SEQ_BEHAVIOR.md — vérification des invariants voix/p-locks.
- RUNTIME_MULTICART_REPORT.md — bornes mémoire .data/.bss/.ram4.
### Étapes réalisées
- Ajout des includes standard (`<stddef.h>`, `<string.h>`, etc.) et connexion aux APIs legacy (`seq_runtime`, `seq_project`, `seq_model`).
- Implémentation de la résolution handle → track active (banque/pattern actifs) et copie `seq_step_view_t` sans fuite de pointeur.
- Encodage d’un itérateur p-lock sans allocation (state statique) + encodage ID interne (flag 0x8000).
- Ajout du test host `seq_reader_tests.c` + intégration `Makefile` (`make check-host`).
### Tests
- make -j : ⚠️ KO (toolchain arm-none-eabi-gcc absente sur l’environnement, comportement inchangé vs MP0).
- make check-host : OK (inclut désormais `seq_reader_tests`).
### Audits mémoire
- Inchangés vs baseline : .data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o (state lecteur en statique négligeable).
### Blocages & TODO
- Aucun.

## [2025-10-22 09:15] MP2 — Plomberie build handles (opt-in par fichier)
### Contexte lu
- Makefile racine — points d’accroche `USE_COPT` et hooks `POST_MAKE_ALL_RULE_HOOK`.
- ARCHITECTURE_FR.md — rappel qu’aucune interface publique ne change.
- RUNTIME_MULTICART_REPORT.md — baseline mémoire (.data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o).
### Étapes réalisées
- Flag global `SEQ_USE_HANDLES=0` injecté via `USE_COPT` (comportement legacy préservé).
- Règles opt-in par fichier préparées (commentées) pour `apps/seq_led_bridge.c` et `apps/seq_engine_runner.c`.
- Cible `warn_legacy_includes_apps` ajoutée + accrochée en fin de build (`POST_MAKE_ALL_RULE_HOOK`).
- Cibles bloquantes prêtes (`check_no_legacy_includes_led`, `check_no_legacy_includes_runner`) sans activation.
### Tests
- make -j : ⚠️ KO (toolchain arm-none-eabi-gcc absente, inchangé vs MP1).
- make check-host : OK.
### Audits mémoire
- Inchangés vs baseline : .data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o.
### Décisions
- Handles maintenus OFF globalement ; migration fichier-par-fichier prévue MP3/MP4 avec opt-in ciblé et garde-fous bloquants.
### Blocages & TODO
- Aucun.

## [2025-10-22 14:30] MP3a — Pré-wire Reader pour bridge LED
### Étapes réalisées
- Ajout de `seq_reader_get_active_track_handle()` (déclaré dans `core/seq/reader/seq_reader.h`, implémenté dans `core/seq/reader/seq_reader.c`).
- Préparation de `apps/seq_led_bridge.c` avec l’include façade `core/seq/seq_access.h` (sans changer le comportement).
### Tests
- make check-host : OK (inchangé).
### Audits mémoire
- Inchangés (.data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o).
### TODO
- MP3b : brancher les vraies lectures legacy dans `seq_reader_get_active_track_handle()`.
