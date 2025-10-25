# seq_led_bridge.c – Nettoyage legacy p-lock

## Changements réalisés
- suppression totale des branches `#if !SEQ_FEATURE_PLOCK_POOL` et du chemin d'accès direct `step->plocks[]` / `plock_count`.
- adoption exclusive du buffer pool `{ids, values, flags}` et du commit via `seq_model_step_set_plocks_pooled()`.
- harmonisation des helpers UI (hold/apply, cart, preview) pour ne manipuler que les entrées pool et la déduplication "dernier gagnant".
- sécurisation via `#pragma GCC poison plocks plock_count` pour éviter toute régression future.

## Effets attendus
- build cold/UI désormais strictement PLK2 pool-only.
- invariants UI (Hold, multi-hold, preview, Live Rec) conservés tout en évitant le stockage legacy.
