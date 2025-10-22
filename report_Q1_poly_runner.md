# Rapport Q1 — Runner polyphonique

## Ce qui a été fait
- Ajout d'une vue voix slotisée (`seq_step_voice_view_t`) et des APIs Reader associées pour accéder aux voix actives par slot.
- Mise à jour du runner Reader-only pour suivre plusieurs voix par piste (tableau `[track][slot]`), planifier NOTE_OFF par slot et déclencher NOTE_ON via `seq_reader_get_step_voice()`.
- Revue de la capture live : confirmation du slot-picking (réutilisation, premier libre, round-robin) sans effacer les autres voix, aucun changement nécessaire.
- Documentation architecture synchronisée pour décrire l'itération par slot et rappeler l'absence d'accès cold/UI en tick.

## Pourquoi
- La vue mono `seq_step_view_t` ne portait qu'une voix primaire, ce qui limitait le runner à une seule NOTE_ON par step alors que le modèle stocke jusqu'à 4 voix simultanées.
- La nouvelle vue slotisée permet au runner de rester Reader-only tout en jouant les accords capturés ou programmés, sans réintroduire `seq_engine` ni accéder au modèle cold.

## Tests & métriques
- `make check_no_engine_anywhere` ✅
- `make check_no_legacy_includes_apps` ✅
- `make check_no_cold_refs_in_apps` ✅
- `make build/host/seq_runner_smoke_tests` ✅
- `./build/host/seq_runner_smoke_tests` → `events=127`, `silent_ticks=0`, `on=64`, `off=63` ✅
- `make check-host` ✅ (stress: `silent_ticks=0`, ON/OFF équilibrés sur 16 pistes)
- `./build/host/seq_16tracks_stress_tests` → `silent_ticks=0`, `total_on=2048`, `total_off=2048` ✅
- `make -j8` ⚠️ (toolchain ARM absente dans l'environnement : `arm-none-eabi-gcc: No such file or directory`).

## Garde-fous vérifiés
- Runner strictement Reader-only : aucun accès `seq_model.*` ni UI/cold dans `_runner_handle_step()`.
- Pas d'includes RTOS ajoutés dans `apps/` ; surface Reader unique (`core/seq/seq_access.h`).
- Probe MIDI intacte, NOTE_OFF planifiés par slot, STOP conserve l'envoi CC#123.
- `seq_live_capture` conserve la logique de slots sans écraser les autres voix.
- Tests CI host passent, aucune modification des règles/guards existants.
