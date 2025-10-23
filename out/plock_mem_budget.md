
# Budget mémoire p-locks — baseline vs pool packé

## Hypothèses communes
- Tracks: 16
- Taille actuelle par p-lock: 8 o (structure `seq_model_plock_t`)
- Capacité actuelle par step: 24 p-locks réservés (tableau fixe)
- Pool packé: entrée `(param_id, value, flags)` sur 3 o + index `(offset,count)` par step (3 o)

## Pattern 16×64 (1 024 steps au total)
| Modèle | Capacité p-lock/step | Pool/Entries | Headers `(offset,count)` | Total p-lock RAM |
|---|---|---|---|---|
| Actuel tableau fixe | 24 réservés | 12 288 o par track | — | 196 608 o par pattern |
| Pool packé | 20 effectifs | 61 440 o | 3 072 o | 64 512 o |
| Pool packé | 32 effectifs | 98 304 o | 3 072 o | 101 376 o |

## Pattern 16×1024 (16 384 steps au total)
| Modèle | Capacité p-lock/step | Pool/Entries | Headers `(offset,count)` | Total p-lock RAM |
|---|---|---|---|---|
| Actuel tableau fixe | 24 réservés | 196 608 o par track | — | 3 145 728 o par pattern |
| Pool packé | 20 effectifs | 983 040 o | 49 152 o | 1 032 192 o |
| Pool packé | 32 effectifs | 1 572 864 o | 49 152 o | 1 622 016 o |
