# Phase Audit — Sous-système Séquenceur

## 1. Cartographie des structures de données

### 1.1 Couche temps réel (RAM)
- `seq_model_pattern_t` regroupe 64 steps, chacun portant 4 voix, un pool de 24 p-locks et des offsets globaux ; il est partagé par l'engine, la capture live et l'UI via des pointeurs. 【F:core/seq/seq_model.h†L17-L154】
- `seq_engine_t` consomme le pattern actif en lecture seule, alimente un scheduler de 64 événements et publie les callbacks temps réel. 【F:core/seq/seq_engine.h†L22-L128】
- `seq_live_capture_t` maintient un cache de timing/quantize et écrit directement dans le pattern attaché lors des enregistrements live. 【F:core/seq/seq_live_capture.h†L22-L107】

### 1.2 Couche projet & persistance
- `seq_project_t` agrège 16 banques × 16 patterns de métadonnées (`seq_project_pattern_desc_t`) et la table des pistes runtime (`seq_project_track_t`). Chaque descripteur inclut nom, offsets flash et 16 références cartouches. 【F:core/seq/seq_project.h†L20-L107】
- La sérialisation persiste un *directory* de 1 Mo par projet, accompagné d'un tampon CCM unique `s_pattern_buffer[3968]` pour encoder/décoder un pattern. 【F:core/seq/seq_project.c†L14-L131】【F:core/seq/seq_project.c†L98-L131】
- `seq_led_bridge` (UI) instancie aujourd'hui un `seq_project_t` global en CCM ainsi qu'un tableau de 2 patterns `g_project_patterns[]` utilisés comme cache actif et exposés à l'engine/recorder. 【F:apps/seq_led_bridge.c†L53-L117】

### 1.3 Flux d'accès
1. L'UI manipule le pattern courant via `seq_led_bridge_access_pattern()` et publie l'état vers les LEDs.
2. Le moteur (`seq_engine_runner`) attache le même pattern pour la lecture temps réel.
3. `seq_project_assign_track()` lie chaque piste active à un `seq_model_pattern_t` ; les changements de banque/pattern reposent sur la mise à jour des pointeurs par l'UI avant lecture. 【F:core/seq/seq_project.c†L400-L545】

## 2. Analyse mémoire

| Élément | Quantité active | Empreinte unitaire | Empreinte totale |
| --- | --- | --- | --- |
| `seq_model_pattern_t` (cache temps réel) | 2 (UI) | 14 224 B | 28 448 B |
| `seq_project_t` (banques + runtime) | 1 | 73 128 B | 73 128 B |
| Tampon sérialisation pattern | 1 | 3 968 B | 3 968 B |
| `seq_led_bridge_state_t` + hold/preview | 1 | ~4 kB | ~4 kB |
| Scheduler engine (`seq_engine_scheduler_t`) | 1 | 64 × 20 B ≈ 1.3 kB | ~1.3 kB |

Mesures issues d'un binaire host (`sizeof`). 【fd2b74†L1-L13】

Observations clés :
- Les métadonnées projet (72 kB) résident en CCM alors qu'elles ne sont accédées qu'en UI/gestion de fichiers ; elles concurrencent la place nécessaire aux caches temps réel.
- Chaque pattern actif coûte 14 kB ; augmenter la capacité de pistes actives multipliera linéairement cette empreinte.
- Le tampon de 3.9 kB est réservé en permanence bien qu'il ne soit utilisé que lors des sauvegardes/chargements.

## 3. Points faibles identifiés

1. **Métadonnées projet sur-allouées en RAM** : les 16 banques × 16 patterns sont chargés en permanence alors que seule la banque courante est utilisée à chaud. 【F:core/seq/seq_project.h†L95-L107】
2. **Faible capacité de cache pattern** : `SEQ_LED_BRIDGE_TRACK_CAPACITY` vaut 2, limitant artificiellement le nombre de pistes simultanément accessibles sans recharger la RAM. 【F:apps/seq_led_bridge.c†L68-L115】
3. **Dépendance UI ↔ projet** : l'UI possède le projet et expose les pointeurs aux autres couches, ce qui couplant les responsabilités (UI, sérialisation, cache). 【F:apps/seq_led_bridge.c†L53-L117】
4. **Tampon de sérialisation dédié** : `s_pattern_buffer` vit dans le même module que la logique projet et occupe le CCM même hors opérations I/O. 【F:core/seq/seq_project.c†L98-L131】
5. **Descripteurs volumineux** : chaque `seq_project_pattern_desc_t` conserve 16 références cart + padding, soit 284 B par pattern, indépendamment du nombre de pistes réellement stockées. 【F:core/seq/seq_project.h†L72-L88】
6. **Manque de séparation persistant/runtime** : les API `seq_project_*` mélangent la gestion de la topologie (banques/patterns) et la persistence sur flash, ce qui complique l'introduction d'une mémoire externe ou d'un cache sélectif. 【F:core/seq/seq_project.c†L120-L545】

## 4. Proposition de refactor

### 4.1 Hiérarchie mémoire cible
- **Manifest global léger (SRAM)** : charger au boot un `seq_project_manifest_t` contenant uniquement l'en-tête projet + table des banques (index, offset flash, taille, nom). Chaque entrée de banque ne garderait que `name`, `tempo`, `pattern_dir_offset` et un compteur de patterns valides.
- **Cache de banque active (CCMRAM)** : réserver un `seq_project_bank_cache_t` avec les 16 `seq_project_pattern_desc_t` de la banque courante et les `seq_model_pattern_t` actuellement utilisés. Une opération `seq_project_activate_bank(bank_id)` remplirait ce cache depuis la flash/SD.
- **Pool de patterns** : gérer un `seq_pattern_pool_t` (ex. 4 slots) contenant les `seq_model_pattern_t` en CCM. Chaque piste runtime pointe vers un slot ; un LRU permettrait de charger une nouvelle pattern dans un slot libre (ou swap contrôlé) sans casser la latence.
- **Tampon I/O partagé (SRAM)** : déplacer `s_pattern_buffer` dans une zone SRAM lente partagée avec le module de stockage (hors CCM) car la sérialisation n'est pas critique temps réel.

### 4.2 Structs prototypes
```c
typedef struct {
    uint32_t storage_offset;
    uint32_t storage_length;
    uint8_t track_count;
    char name[SEQ_PROJECT_PATTERN_NAME_MAX];
} seq_project_pattern_meta_t;

typedef struct {
    uint8_t bank_index;
    seq_project_pattern_meta_t patterns[SEQ_PROJECT_PATTERNS_PER_BANK];
} seq_project_bank_cache_t;

typedef struct {
    seq_model_pattern_t *slots[SEQ_PATTERN_POOL_SLOTS];
    uint8_t track_to_slot[SEQ_PROJECT_MAX_TRACKS];
} seq_pattern_pool_t;
```
Ces structures extraient la persistance (metas sérialisables) du runtime (pointeurs vers patterns vivants) tout en minimisant les doublons cart/flags.

### 4.3 Répartition mémoire
- **CCMRAM** : `seq_pattern_pool_t` et les `seq_model_pattern_t` actifs ; `seq_led_bridge_state_t` (déjà CCM) pour garantir la réactivité UI.
- **SRAM principale** : `seq_project_manifest_t`, caches de banques inactives, tampon de sérialisation, routines de stockage. Cette mémoire reste DMA-safe si, ultérieurement, la flash externe/SPI requiert des transferts.

### 4.4 API & dépendances
- Introduire un service `seq_project_service` exposant :
  - `seq_project_service_open(index)` → charge le manifest.
  - `seq_project_service_load_bank(bank_id, cache*)` → rafraîchit le cache actif.
  - `seq_project_service_bind_track(track_id, pattern_handle)` → met à jour le pool et retourne un pointeur stable vers `seq_model_pattern_t`.
- L'UI (`seq_led_bridge`) ne possède plus directement `seq_project_t` mais consomme ce service pour :
  - connaitre les noms de patterns (manifest),
  - sélectionner une pattern → `seq_project_service_request_pattern(bank, pattern)` qui renvoie un slot actif instantanément.
- L'engine et le live capture continuent d'utiliser `seq_model_pattern_t *` sans changement ; seul le fournisseur (service) gère les déplacements mémoire.

### 4.5 Compatibilité future
- La séparation manifest/cache permet de remplacer la flash interne par une SD : seules les fonctions `*_load_bank` et `*_save_bank` changent de backend.
- Un champ `storage_location` dans `seq_project_pattern_meta_t` pourra distinguer flash interne / externe / SD.
- Préparer un `seq_project_pool_prefetch(bank_id)` pour anticiper un changement de banque sans latence (préchargement dans un slot libre).

## 5. Plan d'intégration progressif

1. **Extraction du manifest** : créer `seq_project_manifest_t` et déplacer la lecture du directory flash dans un module dédié, tout en conservant l'API actuelle (`seq_project_load`).
2. **Service de cache** : implémenter `seq_project_bank_cache_t` + API `seq_project_load_bank_meta()` ; adapter `seq_project_t` pour ne garder qu'une banque en RAM.
3. **Pool de patterns** : introduire `seq_pattern_pool_t` (avec 2 slots initiaux) et rétrofitter `seq_project_assign_track()` pour mapper les pistes vers les slots.
4. **Refactor UI** : modifier `seq_led_bridge` afin qu'il consomme le service (`seq_project_service_*`) au lieu de posséder `g_project`; déplacer les allocations globales dans le pool.
5. **Migration du tampon I/O** : déplacer `s_pattern_buffer` vers SRAM, exposer un getter `seq_project_io_buffer()` pour les routines de sauvegarde.
6. **Extension progressive des slots** : augmenter `SEQ_PATTERN_POOL_SLOTS` (4→8) une fois la consommation CCM validée ; instrumenter `seq_led_bridge` pour relâcher un slot lorsqu'on quitte une piste.
7. **Préchargement de banque** : ajouter une étape optionnelle de préfetch dans le pool pour permettre un changement de banque instantané, en s'appuyant sur le manifest pour détecter les patterns réellement utilisées.

Chaque étape conserve les pointeurs `seq_model_pattern_t *` attendus par le moteur et la capture live, garantissant l'absence de régression musicale tout en préparant le support multi-projet et stockage externe.
