# Q1.8e QuickStep boundary fix

## Résumé
- Ajout d'un cache QuickStep côté hot (`apps/seq_quickstep_cache.{c,h}`) permettant de signaler au runner les pas armés depuis QuickStep avec leurs note/vélocité/longueur.
- Mise à jour de `apps/seq_led_bridge.c` pour initialiser le cache, invalider les entrées lors des clears et marquer un pas QuickStep propre avec des valeurs jouables (vélocité minimale et longueur >= 1).
- Renforcement de `_runner_handle_step` dans `apps/seq_engine_runner.c` pour suivre la dernière note/vélocité par slot, consommer les entrées QuickStep et forcer un retrigger propre au boundary en s'appuyant sur le cache.
- Ajout du test hôte `tests/seq_quickstep_boundary_tests.c` et de la règle `HOST_SEQ_QUICKSTEP_TEST` pour vérifier la séquence de NOTE_ON/NOTE_OFF consécutive en mode QuickStep y compris lorsque la vue retourne une vélocité nulle.

## Tests
- `make check_no_engine_anywhere`
- `make check_no_legacy_includes_apps`
- `make check_no_cold_refs_in_apps`
- `make build/host/seq_quickstep_boundary_tests && ./build/host/seq_quickstep_boundary_tests`
- `make build/host/seq_runner_smoke_tests && ./build/host/seq_runner_smoke_tests`
