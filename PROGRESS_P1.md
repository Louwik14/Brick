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
- Aucun.

## [2025-10-22 18:00] MP3b — Opt-in Reader partiel (LED bridge)
### Étapes réalisées
- `seq_reader_get_active_track_handle()` vérifie le projet actif et renvoie les indices legacy (bank/pattern/track).
- Opt-in local activé (`apps/seq_led_bridge.o: CFLAGS += -DSEQ_USE_HANDLES=1 -Werror=deprecated-declarations`).
- Migration d’un site dans `_rebuild_runtime_from_track()` : lecture Reader sous `#if SEQ_USE_HANDLES`, fallback legacy intact.
### Tests
- make check-host : OK.
### Audits mémoire
- Inchangés (.data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o).
### Décisions
- Garde-fou legacy (`check_no_legacy_includes_led`) laissé commenté pour MP3b conformément au plan.

## [2025-10-23 11:00] MP3c — Reader complet + garde-fou actif
### Étapes réalisées
- Flags publics `SEQ_STEPF_*` exposés dans `seq_views.h` et alimentés par `seq_reader_get_step()` (voix, p-locks seq/cart, automation, mute placeholder) pour supprimer les heuristiques côté apps.
- Tous les accès lecture/flags du bridge migrés vers Reader dans `_rebuild_runtime_from_track()` (usage exclusif de `seq_access.h`).
- Garde-fou bloquant `check_no_legacy_includes_led` activé sur la cible `apps/seq_led_bridge.o`.
### Tests
- make check-host : OK.
- make check_no_legacy_includes_led : OK.
### Audits mémoire
- Inchangés (.data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o).
### Notes
- Sous-étape A effectuée : exposition des flags Reader pour couvrir les besoins LED sans heuristiques locales.

## [2025-10-24 09:00] MP4a — Pré-wire runner avec façade Reader
### Étapes réalisées
- Ajout de l’include `core/seq/seq_access.h` dans `apps/seq_engine_runner.c` (commenté MP4a, sans modifier les includes legacy existants).
- Fourniture d’un helper inline public `seq_reader_make_handle()` dans `core/seq/reader/seq_reader.h` pour faciliter la création de handles côté apps.
### Impact
- Aucun changement de comportement ni de dépendance effective au Reader pour le runner.
### Tests
- make check-host : OK (inchangé).
### Audits mémoire
- Inchangés (.data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o).
### Notes
- Prêt pour MP4b : opt-in ciblé + migration d’un premier site de lecture.
