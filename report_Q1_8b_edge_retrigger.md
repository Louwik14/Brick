# Rapport Q1.8b — Same-note edge retrigger (boundary-safe)

## Résumé des actions
- Instrumentation du runner pour mémoriser les NOTE_OFF déclenchés sur le tick courant et préserver l'ordre OFF→ON par slot.
- Ajout d'un forçage de re-trigger lorsque le Reader reste silencieux mais que la même note est relancée exactement au tick frontière (OFF et ON simultanés).
- Enrichissement du smoke test host `seq_runner_smoke_tests` avec trois scénarios : nominal, edge explicite et edge sans "hit" Reader.

## Détails clés
- Nouveau state par slot : `last_note` / `last_velocity` pour décider du re-trigger implicite après un NOTE_OFF planifié.
- Étend `seq_reader_get_step_voice()` pour retourner la note/vel brute même si la voix est désactivée (lecture Reader-only).
- Politique de re-trigger "boundary-safe" : on ne renvoie pas de NOTE_OFF doublon et on force un NOTE_ON unique si `off_fired_this_tick` + même note + vélocité nulle côté Reader.

## Tests exécutés
- `make check_no_engine_anywhere`
- `make check_no_legacy_includes_apps`
- `make check_no_cold_refs_in_apps`
- `make build/host/seq_runner_smoke_tests && ./build/host/seq_runner_smoke_tests`
- `make check-host`
- `./build/host/seq_16tracks_stress_tests`
- `make -j8` (⚠️ échoue : toolchain `arm-none-eabi-gcc` absente sur l'environnement host)

