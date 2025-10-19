# Phase "Efficience & Design" — Rapport d'étape

## 1. Baseline

### 1.1 Empreinte mémoire runtime
- Deux patterns temps réel (`seq_model_pattern_t`) en cache UI/engine occupent 2 × 14 224 B = 28 448 B en CCM, auxquels s'ajoute le tampon de sérialisation `s_pattern_buffer[3968]` alloué en permanence. 【F:PHASE_AUDIT_SEQ.md†L10-L31】【F:core/seq/seq_project.c†L102-L131】
- Les métadonnées projet (`seq_project_t`) résident aussi en CCM et consomment 73 128 B, ce qui laisse peu de marge pour des caches supplémentaires sans optimisation. 【F:PHASE_AUDIT_SEQ.md†L10-L31】

### 1.2 Codec pattern v1 (référence)
- Charge utile piste (payload) : 610 B pour un pattern de test (voix multiples + p-locks). 【339abb†L1-L4】
- Taille blob BPAT complète (en-tête + piste) : 630 B. 【339abb†L1-L4】
- Temps moyen d'encodage : 0,95 µs (10 000 itérations, `CLOCK_MONOTONIC`). 【339abb†L1-L4】

## 2. Designs envisagés

### 2.1 Design A — Codec clairsemé avec saut de steps (implémenté)
- Regroupe les métadonnées step dans un en-tête compact `pattern_step_v2_header_t` et encode uniquement les voix/p-locks modifiés ; les steps neutres sont sautés via un champ `skip`. 【F:core/seq/seq_project.c†L293-L360】
- Les voix conservent leur densité (AoS) mais ajoutent un masque `payload_mask` pour éviter de sérialiser les valeurs par défaut. 【F:core/seq/seq_project.c†L308-L348】
- Compatible rétro (décode v1 & v2) ; l'encodage v1/v2 est sélectionné à la compilation via `BRICK_EXPERIMENTAL_PATTERN_CODEC_V2`. 【F:core/seq/seq_project.c†L208-L289】【F:core/seq/seq_project.c†L292-L360】【F:core/seq/seq_project.c†L674-L739】

### 2.2 Design B — Pages de 8 steps + dictionnaire partagé (à prototyper)
- Grouper les steps par "page" de 8 avec un en-tête commun (flags actifs, nombre de p-locks) et un dictionnaire partagé pour les IDs de paramètres cart ; seul l'index de dictionnaire serait stocké dans les steps.
- Avantages attendus : suppression des répétitions d'IDs cart pour les longues séquences automatisées ; chargement partiel de page pour préparer un switch de pattern.
- Risques : complexité accrue du décodeur (gestion des pages vides), besoin d'une table transitoire en RAM lors du chargement.

### 2.3 Design C — Pool de steps copy-on-write (futur)
- Maintenir un pool global de "steps neutres" immuables ; les pistes référencent ces steps tant qu'ils ne sont pas édités. Une écriture effectue un clone dans un pool mutualisé.
- Gains visés : réduire la RAM runtime en supprimant les 64×4 voix clonées par pattern lorsque seules quelques steps sont actives.
- Risques : complexité sur le live-rec (synchronisation UI/engine), nécessité d'un garbage collector pour libérer les steps redevenus neutres.

## 3. Prototype implémenté (Design A)

### 3.1 Modifications clés
- Encodage clairsemé conditionné par `BRICK_EXPERIMENTAL_PATTERN_CODEC_V2`, avec `compute_voice_payload_mask` et un champ `skip` pour compresser les runs de steps neutres. 【F:core/seq/seq_project.c†L183-L360】
- Sélection conditionnelle des encodeurs v1/v2 pour éviter les symboles inutilisés selon la configuration (supprime les warnings). 【F:core/seq/seq_project.c†L208-L360】
- Tests hôtes `seq_pattern_codec_tests` couvrant round-trip, suppression des p-locks cart et mode "cart absent", maintenant alignés sur les prototypes `cart_registry`. 【F:tests/seq_pattern_codec_tests.c†L7-L163】
- CLI de migration `seq_pattern_migrate_v2` pour ré-encoder les blobs BPAT existants vers le nouveau format, tout en conservant la lecture des versions 1 ou 2. 【F:tools/seq_pattern_migrate_v2.c†L1-L155】

### 3.2 Mesures après prototype
- Charge utile piste (payload) : 450 B (‑26 % vs v1). 【949a8c†L1-L4】
- Taille blob BPAT : 470 B (‑25 % vs v1). 【949a8c†L1-L4】
- Temps moyen d'encodage : 1,01 µs (+0,06 µs absolu, +6 % relatif). 【949a8c†L1-L4】

### 3.3 Analyse CPU / RT
- L'encodage reste <1,1 µs en moyenne (10 000 itérations) et n'est jamais exécuté dans les ISR critiques ; impact négligeable sur la latence globale.
- Le coût additionnel vient des branches conditionnelles (`payload_mask`) mais s'échange contre une réduction significative des écritures mémoire/flash.

## 4. Comparatif synthétique

| Mesure | Codec v1 | Codec v2 | Gain |
| --- | --- | --- | --- |
| Payload piste | 610 B | 450 B | ‑26 % |
| Taille blob | 630 B | 470 B | ‑25 % |
| Temps encode | 0,95 µs | 1,01 µs | +0,06 µs |

Données issues des mesures `measure_pattern_codec` (10 000 encodages). 【339abb†L1-L4】【949a8c†L1-L4】

## 5. Migration & compatibilité
- Le décodeur accepte toujours les versions 1 et 2, quel que soit le flag expérimental. 【F:core/seq/seq_project.c†L708-L739】
- `seq_pattern_migrate_v2` effectue la conversion offline en décodant puis ré-encodant chaque piste avec le codec actif, ce qui garantit la compatibilité future. 【F:tools/seq_pattern_migrate_v2.c†L46-L97】
- Plan recommandé :
  1. Intégrer la CLI dans la toolchain de production ; exiger un passage migration lors du bump de version firmware.
  2. Conserver `BRICK_EXPERIMENTAL_PATTERN_CODEC_V2` à 0 par défaut tant que les tests hardware (sauvegarde/chargement live) ne sont pas validés.
  3. Ajouter un test d'intégration ciblant la migration (pattern v1 → CLI → chargement firmware) avant activation par défaut.

## 6. Pistes suivantes
- Prototyper le design B (pages + dictionnaire) pour les projets riches en automation cart, en veillant à instrumenter le coût de lookup.
- Étudier la faisabilité du design C (pool copy-on-write) côté runtime pour réduire la RAM CCM occupée par les patterns neutres.
- Déplacer `s_pattern_buffer` vers une zone SRAM DMA-safe partagée (cf. plan Phase Audit) pour libérer 3,9 kB de CCM, tout en garantissant l'accès hors ISR. 【F:core/seq/seq_project.c†L102-L131】

