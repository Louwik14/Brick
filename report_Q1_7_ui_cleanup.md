# Rapport Q1.7 — UI cleanup

## Résumé des actions
- Suppression des traces résiduelles de `was_active` lors de la sortie du mode track (`ui_backend.c`) et mise à jour du log pour refléter l'état clavier/overlay/shift actuel.
- Ajout d'un wrapper local circonscrit pour accéder au projet via l'API dépréciée (`ui_controller.c`) afin de neutraliser l'avertissement côté cold sans masquer d'autres diagnostics.
- Vérification de l'ordre d'initialisation UI : setters cold (`seq_led_bridge_set_active()` + `seq_led_bridge_bind_project()`) exécutés avant le démarrage des chemins hot.

## Justification
Le résidu `was_active` bloquait `make check-host`. Le wrapper local maintient l'appel temporaire à `seq_runtime_access_project_mut()` tout en limitant le `#pragma` à cet usage précis en attendant une API non dépréciée. L'ordre d'initialisation cold→hot reste conforme à la passe Q1.6.

## Tests exécutés
- `make check_no_engine_anywhere`
- `make check_no_legacy_includes_apps`
- `make check_no_cold_refs_in_apps`
- `make check-host`
