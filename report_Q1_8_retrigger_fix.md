# Rapport Q1.8 — Same-note retrigger fix (runner Reader-only)

## Résumé des actions
- Sécurisation de `_runner_handle_step()` pour appliquer systématiquement la séquence NOTE_OFF → NOTE_ON par slot, avec lecture des flags `AUTOMATION_ONLY` et respect du mute piste avant tout nouveau déclenchement.
- Mise à jour du smoke test host `seq_runner_smoke_tests` avec un scénario ciblé vérifiant le re-trigger d'une note identique lorsque la note précédente est encore active (longueur > 1 step).

## Justification
Le runner Reader-only perdait le NOTE_ON du deuxième step lorsque la même note était répétée et encore active (longueur > 1). En forçant la phase NOTE_OFF avant chaque NOTE_ON déclenché, on garantit un re-trigger déterministe tout en respectant la barrière hot/cold. Le test host dédié verrouille cette régression sans impacter les autres bancs.

## Tests exécutés
- `make check_no_engine_anywhere`
- `make check_no_legacy_includes_apps`
- `make check_no_cold_refs_in_apps`
- `make build/host/seq_runner_smoke_tests && ./build/host/seq_runner_smoke_tests`
- `make check-host && ./build/host/seq_16tracks_stress_tests`
- `make -j8` (⚠️ échoue ici faute de toolchain `arm-none-eabi-gcc` dans l'environnement host)
