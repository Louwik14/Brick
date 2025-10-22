# Q1.8c — Runner edge retrigger hardening

## TL;DR
- Ajout de `apps/runner_trace.{h,c}` pour tracer en RAM les événements OFF/ON du runner (anneau 256 entrées hot-safe).【F:apps/runner_trace.c†L1-L40】【F:apps/runner_trace.h†L1-L30】
- Réécriture de `_runner_handle_step()` : OFF planifiés loggés (`OFF_PLAN`), OFF/ON séquencés déterministes et retrigger implicite (type `EDGE_FORCED_ON`) limité aux pas `vel==0` partageant la note précédente.【F:apps/seq_engine_runner.c†L107-L211】
- Tests host enrichis (`seq_runner_smoke_tests`) pour couvrir nominal, re-hit explicite, re-hit implicite (vel=0) et rafale 512 ticks sans silent tick, tout en validant la trace RAM (types 1..4).【F:tests/seq_runner_smoke_tests.c†L347-L511】

## Automate durci
Le nouveau pipeline `_runner_handle_step()` suit quatre phases :
1. **OFF planifiés** — détection stricte `step_abs == off_step` → log `OFF_PLAN`, puis `>=` → émission NOTE_OFF + log `OFF_SENT` et reset d’état.【F:apps/seq_engine_runner.c†L122-L143】
2. **Mute** — garde existante qui flush l’état et court-circuite le tick si la piste est mutée.【F:apps/seq_engine_runner.c†L145-L162】
3. **Lecture Reader** — `seq_reader_get_step()` et test automation-only pour éviter toute dépendance cold/engine.【F:apps/seq_engine_runner.c†L164-L179】
4. **Par slot** —
   - Si la voix est jouable : OFF immédiat (log `OFF_SENT`) puis ON (log `ON_SENT`), calcul `off_step = step_abs + max(length,1)` et mémorisation note/vélocité pour le tick suivant.【F:apps/seq_engine_runner.c†L186-L212】
   - Si la voix n’est pas jouable mais qu’un OFF vient de tirer et que `vel==0` avec même note qu’au tick précédent : retrigger implicite (log `EDGE_FORCED_ON`) avec vélocité mémorisée.【F:apps/seq_engine_runner.c†L214-L221】

Cette politique garantit OFF→ON même lorsque deux steps consécutifs rejouent la même note ou que la vue Reader n’émet pas de hit (gate clamp/legato). Le filtre `vel==0` évite de relancer des steps neutres (gabarit par défaut Reader) observés durant les tests.【F:apps/seq_engine_runner.c†L214-L221】

## Extrait de trace
`tests/seq_runner_smoke_tests.c` valide les séquences typiques enregistrées par `runner_trace` :
- Nominal : `ON_SENT` step 0 → `OFF_PLAN`/`OFF_SENT`/`ON_SENT` step 1 → `OFF_PLAN`/`OFF_SENT` step 2.【F:tests/seq_runner_smoke_tests.c†L371-L386】
- Edge explicite (len=2 puis len=1) : `ON_SENT` step 0, `OFF_SENT`+`ON_SENT` step 1, `OFF_PLAN`/`OFF_SENT` step 2.【F:tests/seq_runner_smoke_tests.c†L395-L409】
- Edge implicite (vel=0) : `OFF_PLAN`/`OFF_SENT` step 1 suivi de `EDGE_FORCED_ON`, puis OFF planifié step 2.【F:tests/seq_runner_smoke_tests.c†L412-L426】
- Rafale 512 ticks : anneau saturé (256 entrées) contenant à la fois `ON_SENT` (type 3) et `EDGE_FORCED_ON` (type 4).【F:tests/seq_runner_smoke_tests.c†L432-L462】

## Tests
- `make check_no_engine_anywhere`
- `make check_no_legacy_includes_apps`
- `make check_no_cold_refs_in_apps`
- `make build/host/seq_runner_smoke_tests && ./build/host/seq_runner_smoke_tests`
- `make check-host`
- `./build/host/seq_16tracks_stress_tests`
- `make -j8` (⚠️ KO attendu : toolchain ARM absente)

Résultats détaillés dans les sorties de la section “Testing”.
