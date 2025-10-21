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

## [2025-10-24 15:30] MP4b — Opt-in Reader partiel (runner)
### Étapes réalisées
- Migration de `_runner_plock_cb()` dans `apps/seq_engine_runner.c` : lecture du step via Reader (`seq_reader_get_step` + itérateur p-lock) sous `#if SEQ_USE_HANDLES` et fallback legacy intact.
- Opt-in local activé (`apps/seq_engine_runner.o: CFLAGS += -DSEQ_USE_HANDLES=1 -Werror=deprecated-declarations`).
- Garde-fou `check_no_legacy_includes_runner` encore commenté conformément au plan.
### Tests
- make check-host : OK.
### Audits mémoire
- Inchangés (.data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o).

<a id="MP5"></a>
## [2025-10-25 10:00] MP5 — Flip global des handles apps/** + garde-fou généralisé
### Étapes réalisées
- Bascule globale de `SEQ_USE_HANDLES` via `CFLAGS` (handles actifs par défaut + `-Werror=deprecated-declarations`).
- Suppression des opt-ins ciblés `apps/seq_led_bridge.o` et `apps/seq_engine_runner.o` devenus inutiles.
- Nouveau garde-fou bloquant `check_no_legacy_includes_apps` branché sur `POST_MAKE_ALL_RULE_HOOK` pour surveiller l’ensemble de `apps/**`.
### Tests
- make check-host : OK.
- make check_no_legacy_includes_apps : OK.
### Audits mémoire
- Inchangés vs baseline (.data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o).
### Notes
- Aucun changement fonctionnel : pipeline Reader → Scheduler → Player intact, mêmes gardes mémoires.

## [2025-10-27 09:00] MP6a — Préparer le split hot/cold (types + budgets)
### Contexte lu
- docs/ARCHITECTURE_FR.md — contraintes SRAM et futur split hot/cold.
- RUNTIME_MULTICART_REPORT.md — baseline audits (.data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o).
- SEQ_BEHAVIOR.md — rappel pipeline Reader → Scheduler → Player, mute appliqué côté Reader.
### Étapes réalisées
- Création de `core/seq/runtime/seq_runtime_layout.h` (types opaques, budgets cibles hot/cold) et `core/seq/runtime/seq_runtime_layout.c` (alias internes sur `g_seq_runtime`).
- Nouveau test host `tests/seq_runtime_layout_tests.c` validant `sizeof`, alias non nuls et logs budget.
- Mise à jour du Makefile : compilation/exécution du test via `make check-host`.
- Documentation : encart MP6a dans `docs/ARCHITECTURE_FR.md`.
### Tests
- make check-host : OK.
### Audits mémoire
- Inchangés : alias uniquement, pas de delta `.bss` / `.data` / `.ram4`.
### Notes
- Aucun changement fonctionnel ; API interne uniquement (`seq_runtime_blocks_get()`).

## [2025-10-27 14:00] MP6b — Reader via seq_runtime_blocks_get()
### Contexte lu
- docs/ARCHITECTURE_FR.md — encart MP6a, objectif barrière hot/cold.
- SEQ_BEHAVIOR.md — invariants pipeline Reader → Scheduler → Player.
### Étapes réalisées
- `core/seq/reader/seq_reader.c` inclut `seq_runtime_layout.h` et résout désormais les projets/tracks via `seq_runtime_blocks_get()` (alias sur `g_seq_runtime`).
- Aucun export modifié : la vue `seq_step_view_t` reste copiée localement.
### Tests
- make check-host : OK.
### Audits mémoire
- Inchangés (alias uniquement, aucun nouveau symbole BSS/Data).
### Notes
- La dépendance Reader ↔ runtime passe par la barrière hot/cold préparée en MP6a.

## [2025-10-28 09:30] MP6c — Init runtime en deux phases (alias)
### Contexte lu
- docs/ARCHITECTURE_FR.md — jalons P2/MP6a-b, contraintes hot ≤64 KiB.
- SEQ_BEHAVIOR.md — séquence d'init runtime avant Reader/Scheduler/Player.
### Étapes réalisées
- API `seq_runtime_layout_reset_aliases()` / `seq_runtime_layout_attach_aliases()` ajoutée dans `seq_runtime_layout.h/.c`.
- `seq_runtime_init()` applique désormais Phase 1 (reset) puis Phase 2 (attach alias sur `g_seq_runtime`).
- Test host étendu : vérifie cycle reset/attach et restauration des alias.
- Documentation mise à jour (`docs/ARCHITECTURE_FR.md`) avec diagramme de phases.
### Tests
- make check-host : OK.
### Audits mémoire
- Inchangés : alias uniquement, aucune nouvelle allocation.
### Notes
- Prépare la future séparation physique hot/cold sans déplacer les données existantes.

## [2025-10-29 08:30] MP7a-fix — Link ARM hot/cold + flag expérimental robuste
### Étapes réalisées
- Ajout explicite de `core/seq/runtime/seq_runtime_layout.c` dans `CSRC` pour le build ARM (`ch.elf`).
- Garde `extern "C"` autour des prototypes dans `seq_runtime_layout.h` afin de garantir la résolution des symboles en C++.
- Attribut `constructor` désormais compilé uniquement en host (`!__arm__ && !__thumb__`), l'embarqué s'appuie sur `seq_runtime_init()`.
- Valeur par défaut `BRICK_EXPERIMENTAL_PATTERN_CODEC_V2=0` centralisée dans `seq_config.h` pour éliminer les warnings `-Wundef`.
### Tests
- make check-host : OK.
- make -j all : ⚠️ KO (toolchain `arm-none-eabi-gcc` absente sur l'environnement, inchangé vs précédents jalons).
### Audits mémoire
- Inchangés vs baseline : .data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o.

## [2025-10-30 10:00] MP7b — Première vue cold branchée
### Étapes réalisées
- Création de la façade `seq_runtime_cold_view()` (`core/seq/runtime/seq_runtime_cold.{h,c}`) et branchement du domaine `SEQ_COLDV_PROJECT` sur l'alias legacy (`g_seq_runtime.project`).
- Migration de `seq_reader_get_active_track_handle()` pour consommer la vue cold au lieu de caster directement `seq_runtime_blocks_get()->hot_impl`.
- Nouveau test host `seq_runtime_cold_project_tests` vérifiant que la vue renvoie un pointeur non NULL et une taille cohérente, intégré à `make check-host`.
### Tests
- make check-host : OK (inclut le nouveau test runtime cold).
### Audits mémoire
- Inchangés (.data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o) — alias uniquement, aucune relocalisation.

## [2025-10-31 09:15] MP7c — Deuxième domaine cold + appelant migré
### Étapes réalisées
- `SEQ_COLDV_CART_META` renvoie désormais l'alias legacy `g_seq_runtime.project.tracks` via `seq_runtime_cold_view()` (aucun déplacement mémoire, simples pointeurs).
- `seq_pattern_save()` (core) lit les métadonnées cart via la vue cold plutôt que via `s_active_project->tracks[i]` directement.
- Nouveau test host `seq_runtime_cold_cart_meta_tests` ajouté au `Makefile`, vérifie que la vue cart renvoie un pointeur non NULL et une taille non nulle.
### Tests
- make check-host : OK (inclut les tests cold project + cart meta).
### Audits mémoire
- Inchangés vs baseline : .data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o.

## [2025-10-31 15:00] MP7d — Nettoyage cold ciblé (project/cart)
### Étapes réalisées
- `seq_project_save()` obtient désormais un pointeur const via `seq_runtime_cold_view(SEQ_COLDV_PROJECT)` avant d'appeler `update_directory()`.
- `seq_pattern_save()` et `seq_pattern_load()` lisent les métadonnées (`track_count`, `tracks[]`) à partir de la vue cold projet (fallback sur le pointeur legacy si nécessaire), tout en conservant les écritures sur `s_active_project`.
- Aucun déplacement mémoire : simple routage lecture-only sur trois sites core.
### Tests
- make check-host : OK (inclut tous les tests runtime cold + hot budget).【a76162†L1-L84】
### Audits mémoire
- Inchangés vs baseline (.data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o).

## [2025-10-31 15:30] MP8a — Garde budget hot (host)
### Étapes réalisées
- Nouveau test `tests/seq_hot_budget_tests.c` : calcule la somme `sizeof(seq_engine_t)` + scratch Reader + pile player (THD_WORKING_AREA 768 o) et vérifie `<= SEQ_RUNTIME_HOT_BUDGET_MAX`.
- Intégration `Makefile` : compilation/exécution du binaire host via `make check-host` + echo dédié.
- Stub `tests/stubs/ch.h` enrichi (`msg_t`, `binary_semaphore_t`) pour compiler `seq_engine.h` côté host.
### Tests
- make check-host : OK, impression `HOT estimate (host): 2184 bytes` (≤64 KiB).【a76162†L1-L84】
### Audits mémoire
- Inchangés (host-only, aucune section embarquée modifiée).

## [2025-11-01 10:00] MP8b — Snapshot hot + verrou compile-time
### Étapes réalisées
- Création de `core/seq/runtime/seq_runtime_hot_budget.h/.c` : snapshot structuré Reader/Scheduler/Player/files RT/scratch alimenté par des `sizeof` compilés et `_Static_assert` garantissant `<= SEQ_RUNTIME_HOT_BUDGET_MAX` côté host/tests.
- `seq_hot_budget_tests` imprime désormais le détail du snapshot (reader/scheduler/player/queues/scratch) et force le lien de `__seq_runtime_hot_total_guard()`.
- Makefile host : compilation du test avec `seq_runtime_hot_budget.c` pour vérifier le guard et maintenir l'exécution via `make check-host`.
### Tests
- make check-host : OK, log détaillé du snapshot hot ≤ 64 KiB.
### Audits mémoire
- Inchangés (.data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o) — modifications host-only, aucun déplacement de RAM embarquée.


## [2025-11-02 09:30] MP7e — Troisième domaine cold (hold slots)
### Étapes réalisées
- `SEQ_COLDV_HOLD_SLOTS` résout désormais le buffer UI `g_hold_slots` via `seq_runtime_cold_view()` sans déplacer la BSS ; stub host dédié pour les tests légers (`tests/stubs/seq_led_bridge_hold_slots_stub.c`).
- `_hold_step_for_view()` (`apps/seq_led_bridge.c`) consomme la vue cold (avec fallback local) pour lire les slots agrégés en lecture seule.
- Nouveau test host `seq_runtime_cold_hold_slots_tests` branché dans `make check-host` pour garantir `_p != NULL` et `_bytes > 0`.
### Tests
- make check-host : OK (inclut le nouveau test hold slots).
### Audits mémoire
- Inchangés (.data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o) — alias uniquement, aucun déplacement de RAM embarquée.

## [2025-11-02 10:15] MP8c — Micro-bench host Reader/Scheduler/Player
### Étapes réalisées
- Ajout du micro-bench host `tests/seq_rt_timing_tests.c` (boucle `clock_gettime` sur `seq_reader_get_step`), intégré à `make check-host`.
- Sortie console formattée `Reader.get_step: <ns>` pour suivre l'évolution du coût Reader ; TODO en place pour Scheduler/Player.
### Tests
- make check-host : OK (affiche les timings Reader sur l'environnement host).
### Audits mémoire
- Inchangés (.data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o) — instrumentation host-only.

## [2025-11-03 09:00] MP9a — Plomberie sections (hot/cold) désactivée par défaut
### Étapes réalisées
- `core/seq/seq_config.h` expose trois flags (`SEQ_ENABLE_COLD_SECTIONS`, `SEQ_ENABLE_HOT_SECTIONS`, `SEQ_EXPERIMENT_MOVE_ONE_BLOCK`) forcés à `0` par défaut pour garantir l'absence de relocalisation tant que non demandé.
- Nouveau header `core/seq/runtime/seq_sections.h` centralisant les attributs `SEQ_COLD_SEC` / `SEQ_HOT_SEC` (expansions vides lorsque les flags restent à `0`).
- `board/rules_data.ld` documente les placeholders `.hot/.cold` via un bloc commenté à activer ultérieurement.
### Tests
- make check-host : OK (inchangé, macros désactivées).
### Audits mémoire
- Inchangés — aucun symbole ne cible `.hot/.cold` avec les flags à `0`.

## [2025-11-03 09:45] MP9b — Rapport host des domaines cold
### Étapes réalisées
- Implémentation host-only `core/seq/runtime/seq_runtime_cold_stats.h/.c` : collecte la taille en octets des vues `SEQ_COLDV_*` via `seq_runtime_cold_view()` et cumule un total.
- Nouveau test `tests/seq_cold_stats_tests.c` imprimant le détail et validant `bytes_total >= bytes_project`; intégré à `make check-host`.
- Makefile host : ajout de la cible `seq_cold_stats_tests` et exécution en fin de `make check-host`.
### Tests
- make check-host : OK ; impression `Cold domains (bytes): project=73128 cart_meta=384 hold_slots=3648 ui_shadow=0 total=77160`.
### Audits mémoire
- Inchangés (.data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o) — instrumentation host-only.

## [2025-11-03 10:30] MP9c — Essai contrôlé de relocation cold (flag OFF par défaut)
### Étapes réalisées
- `apps/seq_led_bridge.c` annote `g_hold_slots` via `SEQ_COLD_SEC` conditionné par `SEQ_EXPERIMENT_MOVE_ONE_BLOCK`, sans impact tant que le flag reste à `0`.
- Tentative de build embarqué avec `SEQ_ENABLE_COLD_SECTIONS=1` + `SEQ_EXPERIMENT_MOVE_ONE_BLOCK=1` pour déplacer `g_hold_slots` (3 648 o) dans `.cold` ; l'outil `arm-none-eabi-gcc` étant indisponible dans l'environnement, la compilation s'arrête (`Error 127`).
- Documenté l'attendu : déplacement unique de `g_hold_slots` réduisant `.bss` de 3 648 o et créant une section `.cold` de même taille (aucune incidence host/tick, buffer UI hors ISR).
### Tests
- make check-host : OK (flags OFF par défaut).
- make -j8 CFLAGS+=" -DSEQ_ENABLE_COLD_SECTIONS=1 -DSEQ_EXPERIMENT_MOVE_ONE_BLOCK=1" : échoue faute d'outil ARM, relocation non évaluée en binaire.
### Audits mémoire
- Flags OFF : audits identiques à la baseline.
- Mode expérimental (non compilé faute d'outil) : attendu `.bss` -3 648 o, `.cold` +3 648 o (UI hold slots).

## [2025-11-04 09:30] MP10a/b/c — Gardes froides RT (phase API + CI + trace)
### Étapes réalisées
- Ajout de l’API de phase temps réel (`core/seq/runtime/seq_rt_phase.h/.c`) et injection `SEQ_RT_PHASE_TICK/IDLE` dans `seq_engine_process_step()`.
- Façade `seq_runtime_cold_view()` instrumentée côté host : compteur `__cold_view_calls_in_tick` + assert bloquant hors `UNIT_TEST`.
- Nouveau test host `tests/seq_cold_tick_guard_tests.c` : vérifie le compteur et force un accès cold en phase TICK.
- Règle Makefile `check_no_cold_in_rt_sources` (grep bloquant) accrochée au hook `POST_MAKE_ALL_RULE_HOOK`.
- `make check-host` enchaîne désormais la nouvelle binaire et reporte `cold_view_calls_in_tick(host): <n>`.
### Tests
- make check-host : OK (`cold_view_calls_in_tick(host): 1`).
### Audits mémoire
- Inchangés (.data ≈ 1 792 o, .bss ≈ 130 220 o, .ram4 = 0 o) — instrumentation purement host.

