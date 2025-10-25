# Carte de migration P-locks (pool packé 3 o)

## A. Résumé pool-only actuel
- `seq_model_step_t` ne contient plus que `pl_ref {offset,count}` (3 octets packés) vers le pool global PLK2. Les tableaux `plocks[]`/`plock_count` ont disparu et `_Static_assert(SEQ_MAX_PLOCKS_PER_STEP == 24)` verrouille la capacité.【F:core/seq/seq_model.h†L22-L61】【F:core/seq/seq_model.c†L329-L361】
- Le pool (`seq_plock_pool.c`) alloue des triplets `(param_id,value,flags)` contigus, offset 16 bits, taille configurable pour les tests. Aucun flag de feature n’est requis ; firmware et hôte partagent la même implémentation.【F:core/seq/seq_plock_pool.c†L1-L60】
- L’encodeur/décodeur projet sérialise uniquement des chunks `PLK2` : chaque step écrit `count` + payload 3 o, et le décodeur remplit le pool en restaurant `pl_ref`. Aucun codec legacy V1/V2 n’est exposé côté build.【F:core/seq/seq_project.c†L200-L408】

## B. Writers & Reader
- **UI Hold / QuickStep** : collecte ≤24 triplets `{id,val,flags}`, dédup “dernier gagnant” puis commit unique via `seq_model_step_set_plocks_pooled()` ; un OOM restaure le snapshot et journalise un avertissement.【F:apps/seq_led_bridge.c†L104-L535】
- **Live recording** : `seq_live_capture_commit_plan()` réutilise la même bufferisation, assure un seul commit, et restaure le step + voice trackers si le pool refuse l’allocation.【F:core/seq/seq_live_capture.c†L468-L617】
- **Reader / Runner** : l’itérateur `seq_reader_pl_*` parcourt exclusivement le pool et le runner applique les p-locks cart/SEQ avant NOTE_ON ; l’ordre encode→decode est strictement conservé.【F:core/seq/reader/seq_reader.c†L1-L214】【F:apps/seq_engine_runner.c†L130-L353】

## C. État final (check-list)
- [x] Stockage par step legacy supprimé, `pl_ref` packé 3 o partout.
- [x] Pool PLK2 global unique (`seq_plock_pool_*`) utilisé par writers, reader, encodeur/décodeur.
- [x] Sérialisation pattern : chunks `PLK2` uniquement, pas de garde `BRICK_EXPERIMENTAL_PATTERN_CODEC_V2`.
- [x] Writers UI & Live Rec → buffer pool-only + commit unique + rollback sur échec.
- [x] Reader/Runner → itération pool-only (ordre encode==lecture) ; invariants playback maintenus (p-locks avant NOTE_ON, NOTE_OFF jamais droppé, STOP → CC#123).

## D. Avant → Après (mémoire & API)

| Champ | Avant (tableau fixe) | Après (pool PLK2) | Notes |
| --- | --- | --- | --- |
| Stockage step | 24 × `seq_model_plock_t` (8 o) intégrés dans `seq_model_step_t` | `pl_ref {offset,count}` (3 o) + pool global 3 o/entrée | Gain ≈9 o par step actif ; plus de RAM réservée quand il n’y a pas de p-lock.【F:core/seq/seq_model.h†L22-L61】【F:out/plock_mem_budget.md†L7-L18】 |
| Pattern 16×64 | 196 608 o de p-locks réservés même à vide | 64 512 o pour 20 p-locks effectifs, extensible à 101 376 o pour 32 | D’après la projection `plock_mem_budget` ; aucun buffer cold requis.【F:out/plock_mem_budget.md†L7-L18】 |
| API Reader | `seq_reader_plock_iter_*` sur tableau statique | `seq_reader_plock_iter_*` sur pool `{offset,count}` | Ordre encode→decode garanti, mêmes flags (cart/signed/voice).【F:core/seq/reader/seq_reader.c†L1-L214】 |
| Sérialisation | Codecs `track_step_v1/v2` (copie du tableau) | Chunk `PLK2` unique (3 o/entrée) | Plus de branches legacy dans `seq_project.c`.【F:core/seq/seq_project.c†L200-L408】 |
| Writers | Upsert direct dans `step->plocks[]` | Buffer `{id,val,flags}` + commit pool | Rollback propre sur OOM (UI & Live Rec).【F:apps/seq_led_bridge.c†L104-L535】【F:core/seq/seq_live_capture.c†L468-L617】 |

## E. Outils & tests de régression
- `tests/plk2_roundtrip.c` : encode → decode → encode, comparaison stricte du payload pour 0/1/24 p-locks (ordre inclus).【F:tests/plk2_roundtrip.c†L1-L64】
- `tests/plk2_minifuzz.c` : mini-fuzz borné (2 000 itérations) validant clamps du décodeur et absence d’UB sur inputs aléatoires.【F:tests/plk2_minifuzz.c†L1-L92】
- `tests/live_rec_sanity.c` : simulateur Live Rec (last-wins, commit unique, rollback en cas d’échec d’allocation).【F:tests/live_rec_sanity.c†L1-L109】

## F. Points de vigilance
- Le pool est monotone par design ; `seq_plock_pool_reset()` doit être appelé lors des resets projet/track pour éviter la dérive (déjà fait dans les chemins init/tests).【F:core/seq/seq_plock_pool.c†L37-L53】【F:core/seq/seq_project.c†L320-L360】
- Les encodeurs doivent rester alignés sur la cap 24 : tout ajout de paramètres nécessitera d’ajuster `SEQ_MAX_PLOCKS_PER_STEP` et les tests associés.
- Sur host, les bancs PLK2 et Live Rec sont intégrés à `make check-host` pour détecter rapidement toute régression de sérialisation ou de rollback.【F:Makefile†L305-L414】【F:docs/ARCHITECTURE_FR.md†L244-L255】
