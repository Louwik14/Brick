# seq_led_bridge.c – Nettoyage legacy p-lock

## Changements réalisés
- suppression totale des branches `#if !SEQ_FEATURE_PLOCK_POOL` et du chemin d'accès direct `step->plocks[]` / `plock_count`.
- adoption exclusive du buffer pool `{ids, values, flags}` et du commit via `seq_model_step_set_plocks_pooled()`.
- harmonisation des helpers UI (hold/apply, cart, preview) pour ne manipuler que les entrées pool et la déduplication "dernier gagnant".
- sécurisation via `#pragma GCC poison plocks plock_count` pour éviter toute régression future.

## Effets attendus
- build cold/UI désormais strictement PLK2 pool-only.
- invariants UI (Hold, multi-hold, preview, Live Rec) conservés tout en évitant le stockage legacy.

---

# seq_live_capture.c – Purge Live Rec legacy

## Changements réalisés
- suppression de toutes les branches `#if !SEQ_FEATURE_PLOCK_POOL` et des accès directs `step->plocks[]` / `plock_count`.
- bufferisation des triplets `{param_id, value, flags}` (cap 24) avec déduplication “dernier gagnant” et flush unique via `seq_model_step_set_plocks_pooled()`.
- gestion des erreurs (collect/commit) via `_seq_live_capture_flush_buffer()` : snapshot du step, rollback en cas d’OOM et warning UI existant.

## Effets attendus
- capture Live désormais 100 % pool-only, alignée sur la migration PLK2.
- rollback propre en cas d’erreur d’allocation tout en conservant le feedback UI et les invariants NOTE_OFF/STOP.

---

# seq_model.{h,c} – purge storage per-step

## Changements réalisés
- suppression des champs `plocks[]` / `plock_count` dans `seq_model_step_t` au profit du seul `pl_ref` packé (3 octets).
- bascule de l’API modèle vers des triplets pool-only (`plk2_t`) avec helper `seq_model_step_get_plock()` et `seq_model_step_set_plocks_pooled()` unique.
- nettoyage des helpers (clear/clone/reset) pour ne toucher qu’au `pl_ref` et ajout des verrous compile (`_Static_assert`, `#pragma GCC poison`).

## Effets attendus
- modèle séquenceur aligné sur la référence `pl_ref` partagée par Reader/Writer.
- suppression définitive de tout stockage legacy par step, compilation bloquée sur `plocks`/`plock_count`.

---

# seq_reader.c – Purge legacy Reader hot

## Changements réalisés
- suppression des branches duales `seq_reader_plock_iter_*` / `seq_reader_pl_*` dépendantes de `step->plocks[]` et `plock_count`.
- itération unique sur `pl_ref {offset,count}` via `seq_plock_pool_get()` avec empaquetage `{param_id,value,flags}` partagé.
- verrouillage compile via `_Static_assert` et `#pragma GCC poison` pour empêcher toute réintroduction du chemin legacy.

## Effets attendus
- Reader hot strictement pool-only tout en conservant l’ordre encode→read.
- Protection compile-time contre les régressions legacy et alignement avec la migration PLK2 (Passe 2).

---

# Passe 6 – Flatten total pool-only

## Changements réalisés
- suppression des macros de features `SEQ_FEATURE_PLOCK_POOL{,_STORAGE}` et `SEQ_PROJECT_LEGACY_CODEC`, Makefile neutre (plus d’`EXTRA_CFLAGS`).
- réécriture des modules (`seq_project`, `seq_model`, `seq_plock_pool`, LED bridge, live capture, reader) pour ne conserver que le chemin pool et supprimer les prototypes/fichiers legacy.
- mise à jour des tests host pour utiliser systématiquement les triplets `{param_id,value,flags}` (`plk2_t`) et la piscine (`seq_model_step_set_plocks_pooled`).
- codec track : encodeur/décodeur PLK2 unique, plus aucune trace des variantes V1/V2.
- documentation (`ARCHITECTURE_FR.md`) alignée sur le mode pool-only inconditionnel.

## Effets attendus
- build monolithique pool-only, sans flags conditionnels ni chemins legacy cachés.
- Makefile `fw-pooled` aligné sur `all`, sérialisation/Reader/tests couvrant uniquement PLK2.
- future régression sur `plocks`/`plock_count` bloquée par les `#pragma GCC poison` restants.

---

# Passe 7 — Polish final

## Changements réalisés
- nettoyage des includes hérités (`apps/seq_led_bridge.c` n’importe plus `seq_engine_runner.h` inutile sur le chemin pool-only).【F:apps/seq_led_bridge.c†L1-L40】
- ajout des tests host légers `plk2_roundtrip`, `plk2_minifuzz` et `live_rec_sanity` + intégration `Makefile` (`check-host`).【F:tests/plk2_roundtrip.c†L1-L64】【F:tests/plk2_minifuzz.c†L1-L92】【F:tests/live_rec_sanity.c†L1-L109】【F:Makefile†L305-L414】
- script utilitaire `tools/size_sanity.sh` (audit `text/data/bss`, seuil `.bss` 80 KiB) documenté dans `ARCHITECTURE_FR.md`.【F:tools/size_sanity.sh†L1-L16】【F:docs/ARCHITECTURE_FR.md†L244-L255】
- documentation (`ARCHITECTURE_FR.md`, `PLOCK_MIGRATION_MAP.md`) mise à jour pour refléter l’état final pool-only (pas de flags, check-list PLK2 + tableau avant/après).【F:docs/ARCHITECTURE_FR.md†L17-L205】【F:docs/PLOCK_MIGRATION_MAP.md†L1-L62】

## Effets attendus
- `make check-host` couvre la sérialisation PLK2 (round-trip + fuzz) et le rollback Live Rec, évitant toute régression silencieuse.
- audit RAM rapide via `tools/size_sanity.sh`, détection immédiate d’un retour des buffers legacy.
- docs alignées sur la situation finale : plus de branches conditionnelles, pool PLK2 unique, invariants runner rappelés (STOP→CC123, NOTE_OFF garantis).
