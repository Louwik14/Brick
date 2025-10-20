# RUNTIME_MULTICART_REPORT

## 0) Références utilisées
- `docs/ARCHITECTURE_FR.md` — Vue d'ensemble Model / Engine / UI / Cart / MIDI, organisation mémoire actuelle et statut CCRAM.【F:docs/ARCHITECTURE_FR.md†L3-L118】
- `SEQ_BEHAVIOR.md` — Spécification fonctionnelle du séquenceur (pattern = 16 tracks, Reader/Scheduler/Player, UI Track & Mute, mapping MIDI 1↔16).【F:SEQ_BEHAVIOR.md†L10-L109】

### 0.1 Clarification terminologique (track vs pattern)

Le code existant nomme `seq_model_pattern_t` l'unité sérialisée de 64 steps, alors que la spécification rappelle qu'un **pattern agrège 16 tracks synchronisées**.【F:SEQ_BEHAVIOR.md†L10-L43】 Pour lever l'ambiguïté, on adopte la convention suivante dans tout le rapport :

```
Pattern (spec) ─┬─ Track #1  ─▶ 64 steps (seq_model_pattern_t / track courante)
                ├─ Track #2  ─▶ 64 steps
                ├─ ...
                └─ Track #16 ─▶ 64 steps
```

Chaque `seq_model_pattern_t` porte donc **une track** du point de vue de la spec, et un pattern complet doit composer seize instances (CH1→CH16). Cette clarification est également alignée avec l'organisation mémoire décrite dans l'architecture (runtime `g_seq_runtime` + snapshots UI) qui n'héberge qu'une seule piste active à la fois.【F:docs/ARCHITECTURE_FR.md†L33-L118】

> **TODO** : renommer `seq_model_pattern_t` en `seq_model_track_t` (ou équivalent) afin d'éviter les confusions futures ; ce travail est à planifier dans une passe dédiée, car il impacte l'API Model/UI/Runner.

## 1) Constat sur le code existant

### 1.1 Exécution effective
- Le moteur (`seq_engine_process_step`) n'accède qu'à un unique `seq_model_pattern_t` et ne parcourt que ses 64 steps/4 voix ; aucune itération multi-track n'est prévue, et la fonction de mute reçoit l'indice de voix comme identifiant de track.【F:core/seq/seq_engine.c†L234-L245】【F:core/seq/seq_engine.c†L572-L635】
- Le runner initialisé par l'UI attache ce seul pattern actif au démarrage et lors des changements de piste ; les NOTE_ON/OFF sortent sur un canal MIDI constant (`SEQ_ENGINE_RUNNER_MIDI_CHANNEL`).【F:apps/seq_engine_runner.c†L36-L136】【F:apps/seq_led_bridge.c†L1325-L1351】
- Le bridge LED ne maintient que deux motifs (`SEQ_LED_BRIDGE_TRACK_CAPACITY = 2`) en mémoire SRAM (CCRAM neutralisée) et rebondit entre eux lorsque l'utilisateur change de piste, ce qui confirme l'absence d'exécution simultanée des 16 tracks.【F:apps/seq_led_bridge.c†L68-L103】【F:docs/ARCHITECTURE_FR.md†L77-L118】

### 1.2 Cartouches virtuelles XVA1
- `seq_project_t` expose bien 16 emplacements potentiels, mais seules deux patterns sont instanciées et aucune API ne mappe les voix vers quatre instances XVA1 distinctes ; le runner continue d'interroger `cart_registry_get_active_id()` unique et ne sait pas router vers 4 slots virtuels.【F:apps/seq_engine_runner.c†L90-L155】【F:apps/seq_led_bridge.c†L68-L103】
- En l'état, la sélection de piste via l'UI ne fait qu'échanger le pattern unique alimentant moteur/recorder, sans lancer d'exécution parallèle comme requis par `SEQ_BEHAVIOR.md` (§5 et §6).【F:apps/seq_led_bridge.c†L1325-L1351】【F:SEQ_BEHAVIOR.md†L88-L133】

**Conclusion étape 1** : le firmware joue uniquement la piste active (4 voix) sur un canal MIDI fixe. Les 16 tracks définies par la spécification ne sont ni lues ni multiplexées vers quatre cartouches virtuelles.

## 2) Estimation RAM runtime (1 pattern complète)

### 2.1 Taille des structures
Un audit précédent (profil debug `arm-none-eabi-size`) donnait :

| Structure | Taille |
|-----------|--------|
| `seq_model_step_t` | 222 octets |
| `seq_model_pattern_t` | 14 224 octets |

【315c7c†L1-L6】

Avec 16 tracks × 64 steps × 20 p-locks/step (limite spec), la mémoire requise pour les seules tracks atteindrait :

- `16 × 14 224 = 227 584` octets ≈ **222,3 KiB**.

Cette valeur excède la SRAM principale STM32F429 (192 KiB utiles) et dépasse la CCM (64 KiB) alors que la `.bss` actuelle oscille autour de 130 184 o (audit `tools/Audit/audit_sections.txt`).【F:tools/Audit/audit_sections.txt†L1-L33】

### 2.2 Autres postes à considérer
- L'état UI (`seq_led_bridge_state_t`, caches hold, overlays) reste volumineux et doit cohabiter avec `g_seq_runtime` dans la SRAM principale tant que la CCM est neutralisée, conformément à l'architecture en vigueur.【F:docs/ARCHITECTURE_FR.md†L33-L118】
- Les threads ChibiOS et buffers MIDI/cart consomment la marge restant après `.bss + .data`, ce qui laisse ~60 KiB de marge théorique (192 KiB - 130 184 o - 1 788 o) à partager entre piles et allocations statiques supplémentaires.【F:tools/Audit/audit_sections.txt†L1-L33】

### 2.3 Conclusion d'estimation
L'empreinte d'un pattern complet 16 tracks dépasse ~115 % de la SRAM disponible et consommerait aussi la CCM si elle était réactivée. Sans refonte structurelle (split hot/cold, compaction p-locks), l'implémentation brute est **non viable**.

### 2.4 Audit mémoire — état actuel (blocage build)
L'exécution de `make -j8 all` échoue car la dépendance `chibios2111` n'est pas fournie dans le dépôt (`.gitmodules` sans URL).【da5dc4†L1-L4】【3b42f1†L1-L2】 Faute de `build/ch.elf`, il est impossible de régénérer `tools/audit/*.txt` ni `build/ch.map`. Les valeurs citées ci-dessus proviennent donc du dernier audit disponible sous `tools/Audit/`. Une action de remise en place des sous-modules est requise avant toute validation sur cible.

## 3) Décision & pistes recommandées

### 3) Plan d'attaque (préparation split hot/cold)

1. **Remise en état de build** : restaurer le sous-module `chibios2111` ou introduire un mirroir interne permettant de compiler `build/ch.elf`. Sans cette étape, aucune mesure ni validation ne peut être exécutée.
2. **Split `seq_runtime` hot/cold** : isoler `seq_runtime_hot_t` (≤64 KiB) contenant curseurs Reader/Scheduler/Player, queues en cours et buffers d'émission immédiats, et `seq_runtime_cold_t` hébergeant patterns, caches UI, paramètres rarement touchés. Les structures doivent respecter l'ordre d'exécution décrit dans `SEQ_BEHAVIOR` (P-locks → NOTE_ON → NOTE_OFF) pour préserver la sémantique lors du routage multi-track.【F:SEQ_BEHAVIOR.md†L60-L109】
3. **Pooling p-locks** : remplacer l'allocation dense (20 slots/step) par un pool compacté indexé par track+step, afin de ramener la taille d'une track <8 KiB. Cette étape conditionne la possibilité de tenir 16 tracks simultanées dans la SRAM + marge piles.
4. **Router 4×XVA1 virtuels** : une fois la mémoire dégagée, étendre `seq_engine_runner` pour publier 4 slots XVA1 (CH1-4, CH5-8, CH9-12, CH13-16) tout en conservant l'API existante (`seq_engine_runner_note_on/off_cb`). Le mute devra court-circuiter la génération d'événements au niveau Reader pour éviter tout NOTE_OFF perdu.
5. **Validation worst-case** : après compilation, rejouer le pattern 16×64×20 p-locks, collecter jitter Reader/Scheduler/Player (<500 µs), vérifier NOTE_OFF = NOTE_ON et fournir la capture MIDI (CH1..CH16). Cette phase dépend de la disponibilité de la cible STM32F429.

> **Statut** : bloqué sur dépendance `chibios2111`. Aucun changement fonctionnel n'a été appliqué dans le firmware. Une passe d'implémentation sera replanifiée une fois le build rétabli et le split hot/cold chiffré.
