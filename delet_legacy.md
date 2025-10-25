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
