# Étape 5 — Projet SEQ multi-piste & cohérence UI

## Résumé des changements
- Ajout de `seq_project_t` pour encapsuler jusqu’à 16 pistes logiques et fournir des helpers d’affectation/sélection de motifs sans casser les API existantes du moteur.【F:core/seq/seq_project.h†L19-L45】【F:core/seq/seq_project.c†L10-L104】
- Le pont LED (`seq_led_bridge`) s’appuie désormais sur le projet partagé : la piste active est synchronisée avec le moteur et le recorder, les caches hold sont remis à zéro lors d’un changement de piste et l’état runtime expose le nombre total de pistes.【F:apps/seq_led_bridge.c†L25-L111】【F:apps/seq_led_bridge.c†L728-L819】【F:apps/seq_led_bridge.c†L1248-L1336】
- Le backend UI garde la page/piste courante dans `ui_mode_context_t`, remet à zéro les holds lors d’un changement de piste et republie l’état MUTE lorsque l’on entre dans le mode rapide.【F:ui/ui_backend.h†L62-L69】【F:ui/ui_backend.c†L21-L35】【F:ui/ui_backend.c†L127-L151】【F:ui/ui_backend.c†L326-L351】
- La pile clavier ne force plus le mode LED au boot, évitant l’affichage « Keyboard » lors du démarrage SEQ, tout en conservant l’activation au moment d’afficher l’overlay.【F:apps/ui_keyboard_app.c†L1-L36】【F:apps/ui_keyboard_app.c†L213-L236】
- Correction du planificateur du moteur SEQ : les événements sont insérés triés pour que les NOTE ON du pas suivant ne restent plus bloqués derrière des NOTE OFF longs.【F:core/seq/seq_engine.c†L211-L241】
- Déplacement en CCM des états volumineux (runner, recorder, backend LED/UI, clavier) pour réduire la pression `.bss` et préparer les allocations multi-pistes.【F:apps/seq_engine_runner.c†L1-L40】【F:apps/seq_recorder.c†L1-L40】【F:ui/ui_led_backend.c†L1-L64】【F:ui/ui_backend.c†L1-L35】【F:apps/ui_keyboard_app.c†L1-L38】
- Ajout d’un stub runner côté host et mise à jour du test `seq_hold_runtime_tests` pour refléter le nouveau comportement LED clavier.【F:tests/stubs/seq_engine_runner_stub.c†L1-L16】【F:tests/seq_hold_runtime_tests.c†L19-L37】【F:tests/seq_hold_runtime_tests.c†L332-L364】

## Mémoire
La compilation cible `make -j4` échoue dans cet environnement (ChibiOS incomplet), et l’outil `arm-none-eabi-size` est absent (`arm-none-eabi-size: command not found`).【b6952a†L1-L3】【b6fd19†L1-L2】 Les déplacements vers la CCM couvrent :

| Bloc | Avant | Après |
| --- | --- | --- |
| `seq_led_bridge` pattern + état | Motif unique en `.bss` | Projet multi-pistes + état runtime en CCM (`g_project`, `g_project_patterns`, `g`) |
| Runner/Recorder | `s_engine`, `s_plock_state`, `s_capture`, voix actives en SRAM | mêmes structures marquées `CCM_DATA` |
| Backend UI/LED | Contextes & files en SRAM | `ui_mode_context_t`, `s_track_muted`, `s_evt_queue`, état clavier en CCM |

> Sur une toolchain complète, relancer `make -j4` puis `arm-none-eabi-size build/ch.elf` permettrait de mesurer la baisse nette de `.bss` grâce au basculement en CCM.

## Correctifs fonctionnels
- **Boot SEQ** : plus de LEDs clavier au démarrage, le mode reste `SEQ` tant que l’overlay clavier n’est pas affiché.【F:apps/ui_keyboard_app.c†L213-L236】【F:ui/ui_backend.c†L56-L135】
- **Mute/PMute** : entrée dans le mode rapide republie l’état courant, ce qui évite les LEDs rouges fantômes lorsqu’on revient sur `+` sans SHIFT.【F:ui/ui_backend.c†L318-L342】
- **Timing pas long** : le scheduler trie désormais chaque insertion, de sorte que les NOTE ON du pas suivant ne sont plus retardés par les NOTE OFF étendus.【F:core/seq/seq_engine.c†L211-L241】

## Tests
- `make check-host` (OK) — compile les binaires host avec le stub runner et vérifie `seq_hold_runtime_tests`.【abcd3b†L1-L4】【e201e5†L1-L2】
- `make -j4` (échoue) — dépendances ChibiOS manquantes dans le workspace courant.【b6952a†L1-L3】

## Actions recommandées
- Réexécuter `make -j4` + `arm-none-eabi-size` sur une machine disposant de ChibiOS complet pour consigner la baisse de `.bss`.
- Étendre les stubs host si d’autres modules ciblent `seq_engine_runner` après l’introduction de `seq_project_t`.
