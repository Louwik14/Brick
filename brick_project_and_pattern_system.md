# Étape 6 — Système de projets & résolveur de patterns

## Vue d'ensemble
La structure `seq_project_t` encapsule désormais l'intégralité du futur séquenceur multi-track :

- **Hiérarchie** : `Project → Bank[16] → Pattern[16] → Track[1..16]`.
- **Métadonnées persistantes** : chaque slot de pattern mémorise `track_count`, les références cart (`cart_id`, `slot_id`, capacités, flags), l'offset dans la flash externe et la taille du blob sérialisé.
- **Runtime** : les pistes actives conservent des pointeurs vers `seq_model_pattern_t` (pour les moteurs/UI) tandis que les banques stockent la cartographie persistante.

Le module `core/seq/seq_project.c` fournit les helpers de lecture/écriture (`seq_project_save/load`, `seq_pattern_save/load`) et gère le remapping automatique des cartouches en s'appuyant sur `cart_registry`.

## Organisation mémoire
- **Flash externe** : 16 MiB mappés par `board_flash.c` (simulateur logiciel par défaut, surcouche future QSPI).
- **Slots de projet** : 1 MiB par projet (`SEQ_PROJECT_FLASH_SLOT_SIZE`), soit 16 projets maximum.
- **Répartition interne** :
  - `sizeof(seq_project_directory_t)` (≈3 Ko) — annuaire du projet (tempo, active bank/pattern, offsets des 256 patterns).
  - `SEQ_PROJECT_PATTERN_STORAGE_MAX` = 3968 octets par pattern, soit `3968 × 256 ≈ 992 Ko` de données utiles.
- **Buffers CCM** : le workspace de sérialisation (`s_pattern_buffer`) et l'instance `seq_project_t` utilisée par le pont LED restent en CCM pour ne pas gonfler `.bss`.

## Sérialisation des patterns
- Chaque piste écrit un blob compact (`pattern_track_header_t` + steps utilisés).
- Les steps ne sont enregistrés que s'ils portent des voix actives, des offsets ou des p-locks (SEQ/cart).
- Les voix sont stockées avec leurs champs bruts (note, vélocité, longueur, micro, état), ce qui évite toute perte d'information à la relecture.
- Les p-locks cart sont filtrés si la cartouche n'est plus disponible ou si un autre modèle occupe le slot.
- `SEQ_PROJECT_PATTERN_STORAGE_MAX` garantit qu'un pattern reste ≤ 3,9 Ko ; la fonction retourne `false` si le blob excède la capacité (cas pathologique à documenter).

## Politique de remapping cartouche
Lors d'un `seq_pattern_load()` :

| Situation détectée | Politique appliquée |
| --- | --- |
| même `cart_id` et même slot | charge intégralement les p-locks et réactive la piste |
| même `cart_id`, slot différent | remap automatique (`slot_id` mis à jour) |
| slot occupé par un autre cart | p-locks cart filtrés, drapeau `SEQ_PROJECT_CART_FLAG_MUTED` positionné |
| cartouche absente | piste rechargée en mémoire mais toutes les voix sont désactivées (`state = DISABLED`, vélocité=0) |

Les identifiants uniques sont fournis par `cart_registry_set_uid/get_uid/find_by_uid`, ce qui permet de résorber les permutations de cartouches.

## API exposée
- `seq_project_set_active_slot()` : sélectionne la banque/pattern courante pour l'édition.
- `seq_project_save()/load()` : sérialise l'annuaire complet d'un projet (tempo, offsets, actifs).
- `seq_pattern_save()/load()` : écrit/relit un pattern individuel, met à jour les métadonnées (`seq_project_pattern_desc_t`).
- `board_flash_{read,write,erase}` : primitives génériques (simulateur RAM + hooks weak pour un backend QSPI).
- `cart_registry_{set_uid,get_uid,find_by_uid}` : gestion des identifiants uniques cartouche nécessaires au remapping.

## Prochaines étapes
1. Ajouter un backend QSPI réel dans `board_flash_hw_*` (lecture/écriture DMA) et activer l'effacement sectoriel paresseux.
2. Instrumenter les appels `seq_project_save/load` côté UI (menu Projet/Banque/Pattern).
3. Étendre le runner/engine pour respecter `SEQ_PROJECT_CART_FLAG_MUTED` et reporter l'état au backend MUTE.
4. Supporter un cache SD → flash externe pour préparer l'import/export de projets.
