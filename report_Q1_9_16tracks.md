# Rapport Q1.9 — 16 tracks

## Résumé des actions
- Portée `SEQ_RUNTIME_TRACK_CAPACITY` à 16 pistes et synchronisé le pont LED sur cette capacité via `seq_config.h`, en ajoutant les constantes virtuelles (4 cartouches × 4 pistes).
- Rattaché les 16 modèles de piste au projet runtime et renseigné les métadonnées cartouche virtuelles (UID + slot) pour chaque groupe de 4 pistes.
- Enregistré quatre instances XVA1 dans le registre cart et publié les UID correspondants au boot pour refléter les cartouches virtuelles.
- Aligné le runner Reader-only et le backend UI (sélection via BS, rendu OLED) sur `track_count` dynamique afin d’exposer T01..T16.

## Justification
Ces changements valident la configuration runtime 16 pistes avant la passe suivante : le projet connaît désormais l’ensemble des tracks et leur cartouche virtuelle, le runner Reader-only parcourt 16 handles et l’UI liste dynamiquement les pistes disponibles. On vérifie ainsi la scalabilité hot/cold (cart metadata en cold, accès hot via cache) sans réintroduire `seq_engine`.

## Tests exécutés
- `make check_no_engine_anywhere`
- `make check_no_legacy_includes_apps`
- `make check_no_cold_refs_in_apps`
- `make build/host/seq_runner_smoke_tests && ./build/host/seq_runner_smoke_tests`
- `make check-host`
- `./build/host/seq_16tracks_stress_tests`
- `make -j8`

## Garde-fous
- Aucun `seq_engine` réintroduit ; les vérifications CI dédiées passent.
- Pas d’inclusion froide côté apps (`seq_project.h`/`seq_model.h` absents), confirmé par les checks.
- Runner/LED bridge restent Reader-only, aucune lecture cold en TICK.
