# Rapport Q1.6 — LED bridge hot-only

## Résumé des actions
- Suppression de toute dépendance directe à `seq_runtime_*` dans `apps/seq_led_bridge.c` ; le module reçoit désormais le projet via `seq_led_bridge_bind_project()` et laisse la banque/pattern actifs alimenter le cache hot uniquement via `seq_led_bridge_set_active()`.
- Actualisation du pipeline UI : `ui_controller.c` récupère le projet côté cold, renseigne la banque/pattern actives puis rattache le bridge avant de démarrer le runner/recorder.
- Mise à jour des tests host (`seq_hold_runtime`, `ui_track_pmute_regression`, `ui_mode_transition`) pour injecter explicitement la banque/pattern actives et lier le projet au bridge.
- Documentation (`docs/ARCHITECTURE_FR.md`) enrichie pour décrire la nouvelle étape de liaison cold→hot et le rôle de `seq_led_bridge_bind_project()`.

## Justification
La version précédente relisait le runtime (`seq_runtime_access_project_mut()` / `seq_runtime_get_project()`) directement depuis `apps/seq_led_bridge`, y compris dans des chemins déclenchés par les LED ou le runner, ce qui rompait la barrière hot/cold et provoquait des accès interdits en tick. La migration vers un cache hot alimenté par setters cold élimine ces traversées tout en conservant le comportement LED.

## Tests exécutés
- `make check_no_engine_anywhere`
- `make check_no_legacy_includes_apps`
- `make check_no_cold_refs_in_apps`

## Garde-fous vérifiés
- Aucun include RTOS direct ajouté dans `apps/` ; `seq_led_bridge.c` reste Reader-only côté hot.
- Pas de réintroduction de `seq_engine` ni de dépendances cold dans le runner.
- Cache hot limité à banque/pattern actifs + flags de steps, alimenté exclusivement par setters cold.
- Probe RAM et mapping MIDI conservés (aucune modification sur ces chemins).
