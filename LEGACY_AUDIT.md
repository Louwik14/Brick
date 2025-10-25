# Résumé exécutif
- Occurrences legacy **actives en pool** : 0 (toutes les références `plocks/plock_count` sont confinées derrière `#if !SEQ_FEATURE_PLOCK_POOL`).【F:apps/seq_led_bridge.c†L504-L537】【F:core/seq/reader/seq_reader.c†L54-L113】【F:core/seq/seq_live_capture.c†L828-L881】
- Occurrences legacy **confinées sous !POOL** : 28 lignes réparties dans les branches `#else` de `apps\seq_led_bridge.c`, `core\seq\reader\seq_reader.c` et `core\seq\seq_live_capture.c` (hors tests).【F:apps/seq_led_bridge.c†L520-L537】【F:apps/seq_led_bridge.c†L751-L766】【F:apps/seq_led_bridge.c†L1046-L1092】【F:apps/seq_led_bridge.c†L1299-L1378】【F:core/seq/reader/seq_reader.c†L96-L147】【F:core/seq/seq_live_capture.c†L839-L881】
- Fichiers hot path impactés : `apps\seq_led_bridge.c`, `core\seq\reader\seq_reader.c`, `core\seq\seq_live_capture.c`.【F:apps/seq_led_bridge.c†L504-L537】【F:core/seq/reader/seq_reader.c†L54-L147】【F:core/seq/seq_live_capture.c†L828-L881】
- État sérialisation : codec **PLK2-only** en place, sans trace V1/V2 ; le décodeur gère `not found`, troncature, dépassement (`invalid`) et `OOM`, tout en annulant `pl_ref` sur erreur.【F:core/seq/seq_project.c†L141-L235】【F:core/seq/seq_project.c†L272-L315】

# Carte thermique (par fichier)

## apps\seq_led_bridge.c
- **Statut** : Confiné `#if !SEQ_FEATURE_PLOCK_POOL`
- **Occurrences** :
  - L520-L535 : commit legacy en recopiant `step->plocks[]` vers un buffer avant `seq_model_step_set_plocks_pooled`.【F:apps/seq_led_bridge.c†L504-L537】
  - L752-L765 : résolution de note primaire via `step->plocks`/`plock_count`.【F:apps/seq_led_bridge.c†L720-L766】
  - L1047-L1091 : collecte Hold interne depuis `step->plocks[]`.【F:apps/seq_led_bridge.c†L1046-L1092】
  - L1101-L1110 : collecte Hold cart depuis `step->plocks[]`.【F:apps/seq_led_bridge.c†L1099-L1110】
  - L1303-L1331 : upsert interne legacy (`step->plocks[...]`, incrément de `plock_count`).【F:apps/seq_led_bridge.c†L1299-L1333】
  - L1352-L1377 : upsert cart legacy (`step->plocks[...]`).【F:apps/seq_led_bridge.c†L1336-L1378】
  - L1693-L1758 : clear `plock_count` dans les toggles Step/Hold.【F:apps/seq_led_bridge.c†L1687-L1769】
  - L1866-L1904 : buffer Hold multi-step en mode legacy (write-through direct dans `step->plocks`).【F:apps/seq_led_bridge.c†L1833-L1905】
- **Contexte** : Toutes ces sections se trouvent dans des blocs `#else` lorsque la feature pool est activée ; la variante pool lit `pl_ref` et alimente un buffer `seq_led_bridge_plock_buffer_t` via `seq_plock_pool_get`.【F:apps/seq_led_bridge.c†L26-L37】【F:apps/seq_led_bridge.c†L720-L747】【F:apps/seq_led_bridge.c†L1833-L1888】
- **Remède** : Supprimer les branches legacy `#else` et conserver uniquement le chemin bufferisé (`pl_ref` + `seq_plock_pool_get`). Ajouter si besoin un wrapper `#if !SEQ_FEATURE_PLOCK_POOL` minimal autour des prototypes pour builds anciens.

## core\seq\reader\seq_reader.c
- **Statut** : Confiné `#if !SEQ_FEATURE_PLOCK_POOL`
- **Occurrences** :
  - L98-L147 : itérateur legacy initialisant `s_plock_iter_state.plocks = legacy_step->plocks` et utilisant `_legacy_extract_plock_payload`.【F:core/seq/reader/seq_reader.c†L54-L147】
  - L188-L229 : helper `_legacy_extract_plock_payload` encore compilé mais uniquement appelé dans la branche legacy.【F:core/seq/reader/seq_reader.c†L132-L229】
- **Contexte** : la variante active (`#if SEQ_FEATURE_PLOCK_POOL`) lit `step->pl_ref` et empaquette via `reader_pack_from_pool`, sans accès direct au tableau legacy. Les `#pragma GCC poison` protègent la branche active.【F:core/seq/reader/seq_reader.c†L14-L37】【F:core/seq/reader/seq_reader.c†L70-L131】
- **Remède** : Supprimer le bloc `#if !SEQ_FEATURE_PLOCK_POOL` et retirer `_legacy_extract_plock_payload`. Si un fallback est nécessaire hors pool, implémenter un wrapper autour du Reader pool (ex. encoder localement `{id,val,flags}` depuis le pool).

## core\seq\seq_live_capture.c
- **Statut** : Confiné `#if !SEQ_FEATURE_PLOCK_POOL`
- **Occurrences** :
  - L841-L878 : `step->plocks`/`plock_count` pour l’upsert live des p-locks internes/cart.【F:core/seq/seq_live_capture.c†L828-L881】
- **Contexte** : la version pool bufferise dans `seq_live_capture_plock_buffer_t` et commit via `seq_model_step_set_plocks_pooled`, les `#pragma GCC poison` interdisant l’accès direct au tableau lorsque le pool est actif.【F:core/seq/seq_live_capture.c†L13-L60】【F:core/seq/seq_live_capture.c†L828-L835】
- **Remède** : Retirer la branche legacy et s’assurer que tous les appels UI/Live passent par le buffer pool (déjà en place).

## tests\*
- **Statut** : Tests unitaires (tolérés mais à moderniser)
- **Occurrences** :
  - `tests\seq_track_codec_tests.c` : détection de cart p-locks via `step->plocks`.【F:tests/seq_track_codec_tests.c†L117-L129】
  - `tests\seq_runner_plock_router_tests.c` : fixtures legacy pour voice/automation (construction directe de `step->plocks`).【F:tests/seq_runner_plock_router_tests.c†L215-L290】
  - `tests\seq_hold_runtime_tests.c` : vérification d’un p-lock longueur en fallback.【F:tests/seq_hold_runtime_tests.c†L324-L347】
  - `tests\seq_model_tests.c` : asserts sur `seq_model_step_plock_count`, `step.plock_count` sous `#if !POOL`.【F:tests/seq_model_tests.c†L34-L150】
  - `tests\test_load_plk2_*` : helpers générant des headers V2 (champs `plock_count`).【F:tests/test_load_plk2_missing_fallback_legacy.c†L8-L62】
- **Contexte** : chaque test garde une branche `#if !SEQ_FEATURE_PLOCK_POOL` ou construit un flux legacy pour vérifier les compatibilités. Aucun n’est compilé en mode pool-only lorsqu’on supprime ces sections.
- **Remède** : Convertir les fixtures vers des écritures/lectures `pl_ref` via `seq_plock_pool_alloc/get` et supprimer les branches `#if !POOL` restantes dans les tests.

# Détails détections

## A. Reliquats legacy
- `->plocks` :
  - `apps\seq_led_bridge.c` (8 occurrences dans les chemins UI legacy).【F:apps/seq_led_bridge.c†L520-L537】【F:apps/seq_led_bridge.c†L720-L766】【F:apps/seq_led_bridge.c†L1046-L1110】【F:apps/seq_led_bridge.c†L1299-L1378】
  - `core\seq\reader\seq_reader.c` (4 occurrences dans l’itérateur legacy).【F:core/seq/reader/seq_reader.c†L96-L147】
  - `core\seq\seq_live_capture.c` (2 occurrences dans l’upsert live legacy).【F:core/seq/seq_live_capture.c†L839-L878】
  - `tests` (fixtures/assers legacy).【F:tests/seq_runner_plock_router_tests.c†L244-L290】【F:tests/seq_hold_runtime_tests.c†L337-L347】
- `plock_count` :
  - `apps\seq_led_bridge.c` (15 lignes dont 14 dans les branches legacy).【F:apps/seq_led_bridge.c†L520-L537】【F:apps/seq_led_bridge.c†L751-L766】【F:apps/seq_led_bridge.c†L1299-L1378】【F:apps/seq_led_bridge.c†L1687-L1769】
  - `core\seq\reader\seq_reader.c` (3 lignes, toutes dans `#if !POOL`).【F:core/seq/reader/seq_reader.c†L96-L147】
  - `core\seq\seq_live_capture.c` (4 lignes legacy).【F:core/seq/seq_live_capture.c†L839-L878】
  - `tests` (génération de flux et asserts).【F:tests/seq_model_tests.c†L34-L150】【F:tests/test_load_plk2_missing_fallback_legacy.c†L8-L62】
- `SEQ_MODEL_MAX_PLOCKS_PER_STEP` :
  - Défini uniquement quand `!SEQ_FEATURE_PLOCK_POOL`.【F:core/seq/seq_model.h†L31-L35】
  - Utilisé dans les branches legacy (LED bridge, live capture, tests).【F:apps/seq_led_bridge.c†L526-L528】【F:core/seq/seq_live_capture.c†L863-L877】【F:tests/seq_model_tests.c†L134-L149】
- `seq_model_step_plock_count` et helpers legacy : toujours empoisonnés quand `POOL=1`, mais référencés dans les tests fallback.【F:core/seq/seq_model.h†L135-L140】【F:tests/seq_model_tests.c†L34-L233】
- `_legacy_extract_plock_payload` : helper encore présent mais uniquement invoqué côté legacy.【F:core/seq/reader/seq_reader.c†L132-L229】

## B. Chemins pool attendus
- `pl_ref` / `seq_plock_pool_get` :
  - LED bridge : wrappers `_pool_count`/`_pool_entry` et tous les parcours actifs s’appuient sur le pool.【F:apps/seq_led_bridge.c†L26-L37】【F:apps/seq_led_bridge.c†L720-L748】
  - Reader : itérateurs hot-path `seq_reader_plock_iter_*` et `seq_reader_pl_*` ne lisent que `pl_ref`.【F:core/seq/reader/seq_reader.c†L54-L131】【F:core/seq/reader/seq_reader.c†L213-L247】
  - Live capture : buffer `seq_live_capture_plock_buffer_t` + commit via pool.【F:core/seq/seq_live_capture.c†L34-L60】【F:core/seq/seq_live_capture.c†L828-L835】
  - Project codec : allocation/lecture pool dans `decode_plk2_chunk` / `encode_plk2_chunk`.【F:core/seq/seq_project.c†L141-L235】【F:core/seq/seq_project.c†L272-L315】
- `seq_plock_pool_alloc/reset` : utilisés pour décoder et commit dans `seq_model_step_set_plocks_pooled` et les writers/tests pool-only.【F:core/seq/seq_model.c†L361-L410】【F:core/seq/seq_project.c†L201-L234】
- `seq_model_step_set_plocks_pooled` : appelé par LED bridge et live capture sur les chemins actifs.【F:apps/seq_led_bridge.c†L221-L535】【F:core/seq/seq_live_capture.c†L200-L210】

## C. Codec / sérialisation
- Aucun symbole `encode/decode_track_steps_v1/v2` dans `core\seq`.【b3c28a†L1-L1】【912c64†L1-L1】
- `decode_plk2_chunk` : gère les cas `NOT_FOUND`, troncatures, dépassement de borne (`count > SEQ_MAX_PLOCKS_PER_STEP`), accès pool invalide et `OOM`, en resetant `pl_ref` avant retour d’erreur.【F:core/seq/seq_project.c†L141-L235】
- `encode_plk2_chunk` : sérialise uniquement quand `pl_ref.count > 0` et garantit l’ordre lecture=écriture via `seq_plock_pool_get` séquentiel.【F:core/seq/seq_project.c†L272-L315】

## D. Garde-fous
- `apps\*` : aucun `#include "ch.h"` (scan vide).【70061a†L1-L2】
- Pas de sections `.ram4`/`CCMRAM` référencées dans `apps\` ou `core\`.【d05904†L1-L2】
- Poisons actifs côté pool :
  - LED bridge : `#pragma GCC poison plocks/plock_count/SEQ_MODEL_MAX_PLOCKS_PER_STEP`.【F:apps/seq_led_bridge.c†L26-L37】
  - Reader, Live capture, Model : poisons pour interdire les API legacy quand le pool est actif.【F:core/seq/reader/seq_reader.c†L14-L37】【F:core/seq/seq_live_capture.c†L13-L60】【F:core/seq/seq_model.h†L135-L148】

# Top 5 actions recommandées (ordonnées)
1. Purger définitivement les branches `#if !SEQ_FEATURE_PLOCK_POOL` dans `apps\seq_led_bridge.c` (L504-L537, L720-L766, L1046-L1110, L1299-L1378, L1687-L1905) et ne garder que le chemin `pl_ref` + buffers pool.【F:apps/seq_led_bridge.c†L504-L537】【F:apps/seq_led_bridge.c†L720-L766】【F:apps/seq_led_bridge.c†L1046-L1110】【F:apps/seq_led_bridge.c†L1299-L1378】【F:apps/seq_led_bridge.c†L1687-L1905】
2. Supprimer l’implémentation `_legacy_extract_plock_payload` et les itérateurs legacy correspondants dans `core\seq\reader\seq_reader.c` (L96-L229).【F:core/seq/reader/seq_reader.c†L54-L229】
3. Retirer la branche legacy live-recording dans `core\seq\seq_live_capture.c` (L839-L878) pour s’assurer d’un chemin pool-only. 【F:core/seq/seq_live_capture.c†L828-L881】
4. Nettoyer les tests pour qu’ils n’utilisent plus `step->plocks`/`plock_count`, en générant leurs fixtures via le pool (`seq_plock_pool_alloc/get`).【F:tests/seq_runner_plock_router_tests.c†L215-L290】【F:tests/seq_model_tests.c†L34-L233】
5. Après purge, retirer les définitions conditionnelles `SEQ_MODEL_MAX_PLOCKS_PER_STEP` et les `#pragma GCC poison` redondants, en ne gardant que `SEQ_MAX_PLOCKS_PER_STEP` et le path pool. 【F:core/seq/seq_model.h†L31-L148】

# Annexe — Scripts de repro (ne pas exécuter, juste fournis)
- **Greps** Windows (findstr) :
  ```powershell
  findstr /S /N /I /C:"->plocks" /C:".plocks" /C:"plock_count" apps\* core\* cart\*
  findstr /S /N /I /C:"seq_model_step_plock_count" apps\* core\* cart\*
  findstr /S /N /I /C:"SEQ_MODEL_MAX_PLOCKS_PER_STEP" apps\* core\*
  findstr /S /N /I /C:"pl_ref" /C:"seq_plock_pool_get" apps\* core\*
  findstr /S /N /I /C:"seq_model_step_set_plocks_pooled" apps\* core\*
  findstr /S /N /I /C:"PLK2" core\seq\*.c
  findstr /S /N /I /C:"decode_track_steps_v1" /C:"decode_track_steps_v2" /C:"encode_track_steps_v1" /C:"encode_track_steps_v2" core\seq\*.c
  findstr /S /N /I /C:"#include \"ch.h\"" apps\*
  findstr /S /N /I /C:".ram4" /C:"CCMRAM" apps\* core\*
  ```
- **Simu préprocesseur POOL=1** (pseudo-code PowerShell 5.1) :
  ```powershell
  Get-ChildItem -Recurse -Include *.c,*.h -Path apps,core,cart | ForEach-Object {
      $stack = @( @{ active = $true } )
      $lines = Get-Content $_.FullName
      for ($i = 0; $i -lt $lines.Length; $i++) {
          $line = $lines[$i]
          if ($line -match '#if\s+SEQ_FEATURE_PLOCK_POOL') { $stack += @{ active = $true } }
          elseif ($line -match '#if\s+!SEQ_FEATURE_PLOCK_POOL') { $stack += @{ active = $false } }
          elseif ($line -match '#else') { $stack[-1].active = -not $stack[-1].active }
          elseif ($line -match '#endif') { $stack = $stack[0..($stack.Count-2)] }
          elseif ($stack[-1].active -and ($line -match 'plocks|plock_count|SEQ_MODEL_MAX_PLOCKS_PER_STEP')) {
              Write-Output "${($_.FullName)}:$($i+1): $line"
          }
      }
  }
  ```
