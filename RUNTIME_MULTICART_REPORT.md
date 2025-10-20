# RUNTIME_MULTICART_REPORT

## 0) Références utilisées
- `docs/ARCHITECTURE_FR.md` — Vue d'ensemble Model / Engine / UI / Cart / MIDI et interactions threadées.【F:docs/ARCHITECTURE_FR.md†L3-L45】
- `SEQ_BEHAVIOR.md` — Spécification fonctionnelle du séquenceur (16 tracks, règles Reader/Scheduler/Player, mapping MIDI).【F:SEQ_BEHAVIOR.md†L10-L132】

## 1) Constat sur le code existant

### 1.1 Exécution effective
- Le moteur (`seq_engine_process_step`) n'accède qu'à un unique `seq_model_pattern_t` et ne parcourt que ses 64 steps/4 voix ; aucune itération multi-track n'est prévue, et la fonction de mute reçoit l'indice de voix comme identifiant de track.【F:core/seq/seq_engine.c†L234-L245】【F:core/seq/seq_engine.c†L572-L635】
- Le runner initialisé par l'UI attache ce seul pattern actif au démarrage et lors des changements de piste ; les NOTE_ON/OFF sortent sur un canal MIDI constant (`SEQ_ENGINE_RUNNER_MIDI_CHANNEL`).【F:apps/seq_engine_runner.c†L36-L136】【F:apps/seq_led_bridge.c†L1325-L1351】
- Le bridge LED ne maintient que deux motifs (`SEQ_LED_BRIDGE_TRACK_CAPACITY = 2`) en mémoire CCM et rebondit entre eux lorsque l'utilisateur change de piste, ce qui confirme l'absence d'exécution simultanée des 16 tracks.【F:apps/seq_led_bridge.c†L68-L103】【F:apps/seq_led_bridge.c†L1325-L1351】

### 1.2 Cartouches virtuelles XVA1
- `seq_project_t` expose bien 16 emplacements potentiels, mais seules deux patterns sont instanciées et aucune API ne mappe les voix vers quatre instances XVA1 distinctes ; le runner continue d'interroger `cart_registry_get_active_id()` unique et ne sait pas router vers 4 slots virtuels.【F:apps/seq_engine_runner.c†L90-L155】【F:apps/seq_led_bridge.c†L68-L103】
- En l'état, la sélection de piste via l'UI ne fait qu'échanger le pattern unique alimentant moteur/recorder, sans lancer d'exécution parallèle comme requis par `SEQ_BEHAVIOR.md` (§5 et §6).【F:apps/seq_led_bridge.c†L1325-L1351】【F:SEQ_BEHAVIOR.md†L88-L133】

**Conclusion étape 1** : le firmware joue uniquement la piste active (4 voix) sur un canal MIDI fixe. Les 16 tracks définies par la spécification ne sont ni lues ni multiplexées vers quatre cartouches virtuelles.

## 2) Estimation RAM runtime (1 pattern complète)

### 2.1 Taille des structures
Un build hôte affiche :

| Structure | Taille |
|-----------|--------|
| `seq_model_step_t` | 222 octets |
| `seq_model_pattern_t` | 14 224 octets |

【315c7c†L1-L6】

Avec 16 tracks × 64 steps × 20 p-locks/step (limite actuelle de 24), la mémoire requise pour les seules patterns atteint :

- `16 × 14 224 = 227 584` octets ≈ **222,3 KiB**.

Cette valeur excède la SRAM principale STM32F429 (192 KB) et dépasse très largement la CCM (64 KB), alors même que la `.bss` actuelle occupe déjà 163 832 B après migration CCM/const.【F:brick_memory_audit.md†L93-L120】

### 2.2 Autres postes à considérer
- L'état UI (`seq_led_bridge_state_t`, caches hold, overlays) reste volumineux malgré la délocalisation en CCM et ne peut être libéré sans refonte plus profonde.【F:brick_memory_audit.md†L3-L41】
- Les threads ChibiOS et buffers MIDI/cart utilisent la marge restante, limitant drastiquement la possibilité d'ajouter >220 KB supplémentaires sans revoir l'organisation mémoire globale.【F:brick_memory_audit.md†L3-L33】

### 2.3 Conclusion d'estimation
La charge mémoire nécessaire à une pattern complète 16 tracks dépasse ~115 % de la SRAM disponible et consommerait aussi l'intégralité de la CCM. Conformément aux garde-fous du prompt, l'implémentation est **non viable dans l'état actuel**.

## 3) Décision & pistes recommandées

### 3.1 Décision
> Implémentation multi-cart runtime **non engagée** : estimation > 80 % de la RAM totale.

### 3.2 Pistes alternatives
1. **Compresser davantage `seq_model_step_t`** : réduire le nombre de p-locks préalloués (20→12), mutualiser la liste par banque ou basculer vers un pool dynamique indexé par track afin d'abaisser la taille d'un pattern sous 8 KB.
2. **Déporter une partie du modèle en flash externe + streaming** : maintenir un cache circulaire de steps actifs (p.ex. 4 pages × 16 steps) et charger à la volée les p-locks, ce qui limiterait la mémoire vive au strict nécessaire pour la fenêtre courante.
3. **Segmenter l'exécution** : partitionner les 16 tracks en 4 groupes et n'en garder qu'un en RAM simultanément, avec prélecture avant le prochain tick et duplication minimale des offsets pour respecter l'ordre P-lock/NOTE (référence SEQ_BEHAVIOR §3-5).【F:SEQ_BEHAVIOR.md†L64-L109】
4. **Réévaluer l'empreinte UI/hold** : fusionner les buffers `g_hold_slots`/`seq_led_runtime_t` avec le modèle pour supprimer les copies complètes lors du hold, comme déjà anticipé par le plan d'action mémoire (Étapes 3-4).【F:brick_memory_audit.md†L34-L60】

Une combinaison de ces stratégies est indispensable pour dégager >230 KB supplémentaires (ou éviter de les consommer) avant toute tentative de lecture simultanée des 16 tracks conformément à la spécification.

---
**Statut final** : analyse complétée, implémentation suspendue faute de budget RAM. Aucun changement fonctionnel n'a été appliqué.
