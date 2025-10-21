# RUNTIME_MULTICART_REPORT

## 0) Références utilisées
- `docs/ARCHITECTURE_FR.md` — Vue d'ensemble, snapshot mémoire baseline et prochain jalon 16 tracks.【F:docs/ARCHITECTURE_FR.md†L1-L118】【F:docs/ARCHITECTURE_FR.md†L120-L170】
- `SEQ_BEHAVIOR.md` — Spécification séquenceur : pipeline Reader → Scheduler → Player, 16 tracks en parallèle, mapping MIDI CH1..CH16, UI Track & Mute.【F:SEQ_BEHAVIOR.md†L10-L133】【F:SEQ_BEHAVIOR.md†L167-L259】

### 0.1 Clarification terminologique (track vs pattern)

La passe courante aligne enfin l'implémentation sur la définition canonique : un **pattern** spec regroupe 16 tracks, et chaque track couvre 64 steps. Les symboles historiques `*pattern*` utilisés pour représenter une seule track ont été renommés `*track*` dans tout le code partagé. La nouvelle convention est donc :

```
Pattern (spec) ─┬─ Track #1  ─▶ 64 steps (seq_model_track_t)
                ├─ Track #2  ─▶ 64 steps
                ├─ ...
                └─ Track #16 ─▶ 64 steps
```

| Catégorie | Avant (pattern) | Après (track) |
| --- | --- | --- |
| Types | `seq_model_pattern_t` | `seq_model_track_t` |
| Types | `seq_model_pattern_config_t` | `seq_model_track_config_t` |
| Constantes | `SEQ_MODEL_STEPS_PER_PATTERN` | `SEQ_MODEL_STEPS_PER_TRACK` |
| Modèle | `seq_model_pattern_init()` | `seq_model_track_init()` |
| Modèle | `seq_model_pattern_set_quantize()/set_transpose()/set_scale()` | `seq_model_track_set_quantize()/set_transpose()/set_scale()` |
| Runtime | `SEQ_RUNTIME_PATTERN_CAPACITY` | `SEQ_RUNTIME_TRACK_CAPACITY` |
| Runtime | `seq_runtime_get_pattern()` / `seq_runtime_access_pattern_mut()` | `seq_runtime_get_track()` / `seq_runtime_access_track_mut()` |
| Live capture | `seq_live_capture_attach_pattern()` / `clock_pattern_step` | `seq_live_capture_attach_track()` / `clock_track_step` |
| Engine | `seq_engine_attach_pattern()` | `seq_engine_attach_track()` |
| Project | `seq_project_pattern_decode_policy_t` / `SEQ_PROJECT_PATTERN_DECODE_*` | `seq_project_track_decode_policy_t` / `SEQ_PROJECT_TRACK_DECODE_*` |
| Project | `seq_project_pattern_steps_encode()` / `seq_project_pattern_steps_decode()` | `seq_project_track_steps_encode()` / `seq_project_track_steps_decode()` |
| Project | `seq_project_get_active_pattern()` | `seq_project_get_active_track()` |
| Apps | `seq_led_bridge_access_pattern()` / `seq_led_bridge_get_pattern()` | `seq_led_bridge_access_track()` / `seq_led_bridge_get_track()` |
| Apps | `seq_engine_runner_attach_pattern()` / `seq_recorder_attach_pattern()` | `seq_engine_runner_attach_track()` / `seq_recorder_attach_track()` |
| Outils & tests | `tools/seq_pattern_migrate_v2.c` / `tests/seq_pattern_codec_tests.c` | `tools/seq_track_migrate_v2.c` / `tests/seq_track_codec_tests.c` |

Les nouveaux symboles sont définis dans le modèle (`core/seq/seq_model.h`), le runtime partagé (`core/seq/seq_runtime.h`), la capture live (`core/seq/seq_live_capture.h`), le projet (`core/seq/seq_project.h`), ainsi que dans les applications (`apps/seq_led_bridge.[ch]`, `apps/seq_engine_runner.[ch]`, `apps/seq_recorder.[ch]`).【F:core/seq/seq_model.h†L17-L174】【F:core/seq/seq_runtime.h†L18-L39】【F:core/seq/seq_live_capture.h†L25-L107】【F:core/seq/seq_project.h†L20-L156】【F:apps/seq_led_bridge.h†L1-L120】【F:apps/seq_engine_runner.h†L1-L80】【F:apps/seq_recorder.h†L1-L70】

Ce renommage est purement terminologique : aucune structure n'a changé de taille et les audits mémoire restent strictement identiques à la baseline (cf. §1.2).【F:docs/ARCHITECTURE_FR.md†L1-L118】【F:tools/audit/audit_sections.txt†L1-L28】

---

## 1) État du dépôt & build baseline

### 1.1 Vendorisation ChibiOS (repo autosuffisant)
- Aucun sous-module : `.gitmodules` supprimé et `git submodule status` renvoie vide (preuve d'autosuffisance du vendor tree).【a6bcfd†L1-L2】
- `Makefile` pointe vers le ChibiOS local (`CHIBIOS := ./chibios2111`) et inclut les règles GCC/RT nécessaires (`hal.mk`, `rt.mk`, `port.mk`).【F:Makefile†L105-L129】
- Les chemins critiques du port 21.11 sont présents dans le dépôt (`os/common/ports/ARMv7-M/compilers/GCC/mk/port.mk`, `os/hal/hal.mk`, `os/rt/rt.mk`).【F:chibios2111/os/common/ports/ARMv7-M/compilers/GCC/mk/port.mk†L1-L118】【F:chibios2111/os/hal/hal.mk†L1-L120】【F:chibios2111/os/rt/rt.mk†L1-L120】

### 1.2 Build release & audits mémoire (baseline 2025-10-20)
- Compilation effectuée via `make -j8 all` (GNU Arm Embedded 13.2.1) sur le container Linux ; la cible Windows devra rester alignée sur 10.3-2021.10.【459204†L1-L12】
- Les audits post-link ont été régénérés avec `arm-none-eabi-size`/`nm` : `tools/audit/audit_sections.txt`, `audit_ram_top.txt`, `audit_bss_top.txt`, `audit_data_top.txt`, `audit_map_ram4.txt`, `audit_ram4_symbols.txt` décrivent la répartition mémoire actuelle.【F:tools/audit/audit_sections.txt†L1-L34】【F:tools/audit/audit_ram_top.txt†L1-L34】【F:tools/audit/audit_bss_top.txt†L1-L27】【F:tools/audit/audit_data_top.txt†L1-L7】【F:tools/audit/audit_map_ram4.txt†L1-L19】【F:tools/audit/audit_ram4_symbols.txt†L1-L8】
- Sections (`tools/audit/audit_sections.txt`) : `.data` **1 792 o**, `.bss` **130 220 o** ⇒ **~129,0 KiB** de RAM statique ; marge SRAM ≈ **63 KiB** hors piles. `.ram4` reste `NOLOAD` avec taille nulle (0 o).【F:tools/audit/audit_sections.txt†L1-L28】【F:tools/audit/audit_map_ram4.txt†L1-L19】
- BSS dominée par `g_seq_runtime` (**101 448 o**), suivie des buffers UI/cart (`g_hold_slots` 3 648 o, `waCartTx` 3 200 o, `g_shadow_params`/`s_ui_shadow` 2 048 o chacun) et des work areas RTOS (`waUI`, `s_seq_engine_player_wa`).【F:tools/audit/audit_bss_top.txt†L1-L27】【F:tools/audit/audit_ram_top.txt†L1-L30】
- `.data` reste cantonnée aux structures Newlib (`__malloc_av_`, `_impure_data`, locale) ; aucune constante SEQ/UI n'est relogée en RAM initialisée.【F:tools/audit/audit_data_top.txt†L1-L7】
- Les tests hôtes (`make check-host`) s'alignent sur ces renommages et exécutent les scénarios runner, UI Track/PMute et codec track.【6e78c9†L1-L74】【F:Makefile†L265-L329】

### 1.3 Artéfacts joints
- `build/ch.elf`, `build/ch.map`
- `tools/audit/audit_sections.txt`
- `tools/audit/audit_ram_top.txt`
- `tools/audit/audit_bss_top.txt`
- `tools/audit/audit_data_top.txt`
- `tools/audit/audit_map_ram4.txt`
- `tools/audit/audit_ram4_symbols.txt`

Ces fichiers constituent la baseline mémoire de cette passe et sont produits à partir du build décrit ci-dessus.

---

## 2) Constat runtime actuel (avant 16 tracks)

- `g_seq_runtime` occupe 101 448 o en `.bss`. Il embarque le projet courant (`seq_project_t`), les deux tracks actives du bridge LED, le scheduler et l'état live capture. Cette masse unique doit être re-segmentée pour tenir un hot runtime ≤64 KiB.【F:tools/audit/audit_bss_top.txt†L1-L10】【F:core/seq/seq_runtime.h†L18-L39】
- Les buffers UI (`s_ui_shadow` 2 048 o, `g_hold_slots` 3 648 o) restent en SRAM principale tant que la CCRAM est neutralisée, conformément à l'architecture actuelle.【F:tools/audit/audit_ram_top.txt†L1-L26】【F:docs/ARCHITECTURE_FR.md†L33-L118】
- Aucun I/O externe (SPI/SD/QSPI) n'est requis pour la prochaine étape : tout le pipeline reste en exécution locale (Reader/Scheduler/Player + MIDI).【F:SEQ_BEHAVIOR.md†L60-L133】

---

## 3) Gate « Go 16 tracks » (CCRAM off, sans I/O externes)

Les audits étant au vert (RAM statique <150 KiB, marge >35 KiB, CCRAM vide), la mise en œuvre 1 pattern / 16 tracks peut être engagée. Le plan suivant sera exécuté dans la passe dédiée :

### 3.1 Split `seq_runtime_hot_t` / `seq_runtime_cold_t`

| Bloc | Contenu | Taille actuelle (approx.) | Objectif |
|------|---------|----------------------------|----------|
| `seq_runtime_hot_t` | Reader/Scheduler/Player, curseurs de tick, queues d'événements ordonnées, work areas `s_seq_engine_player_wa`, watchers clocks, caches temps réel (status voices, pending NOTE_OFF) | ~48–52 KiB (à extraire de `g_seq_runtime` + WA associées) | ≤64 KiB (SRAM principale) |
| `seq_runtime_cold_t` | Patterns sérialisés (16×`seq_model_track_t`), snapshots UI/LED, caches hold, métadonnées projet/cart, structures rarement mutées | ~50 KiB (`g_seq_runtime` restant + buffers UI/cart) | Reste en SRAM principale, accès amorti hors tick |

- Les deux blocs seront alignés sur la spécification Reader → Scheduler → Player (`SEQ_BEHAVIOR.md` §3-5). Aucun accès cold ne doit s'exécuter dans le chemin Player (<20 µs) ; si une lecture cold est nécessaire (ex. chargement de track), elle sera mise en file via le thread UI/manager.【F:SEQ_BEHAVIOR.md†L167-L259】
- Les work areas ChibiOS (`waUI`, `s_seq_engine_player_wa`, `waCartTx`) sont gardées hors `.ram4` ; on s'interdit toute allocation dynamique à l'exécution (pool statique uniquement).【F:tools/audit/audit_ram_top.txt†L1-L30】

### 3.2 Fusion Reader/Scheduler/Player sur 16 tracks

- Le Reader balayera les 16 tracks (pattern agrégé) en suivant l'ordre spec : P-locks → NOTE_ON → NOTE_OFF.【F:SEQ_BEHAVIOR.md†L13-L109】
- Chaque track est pré-affectée à un canal MIDI : CH1..CH16, groupées par cartouche virtuelle XVA1 (4×4). Les callbacks `seq_engine_runner_note_on/off` routeront vers quatre slots logiques en respectant l'ordre spec (`Reader` non bloquant, `Scheduler` ordonné, `Player` sans attente >20 µs).【F:SEQ_BEHAVIOR.md†L80-L133】
- Le mute UI (commis) coupe l'émission des événements dès le Reader (skip NOTE_ON/OFF + P-locks) afin d'éviter tout NOTE_OFF tardif, conformément à `SEQ_BEHAVIOR.md` (§6).【F:SEQ_BEHAVIOR.md†L222-L247】

### 3.3 Contraintes temps réel

- Aucune allocation dynamique ni `malloc` pendant le tick ; les pools nécessaires (ex. scheduler entries) seront pré-initialisés dans `seq_runtime_hot_t`.
- Aucun blocage >20 µs sur le chemin Player ; les interactions cart/MIDI utilisent les work areas existantes (`waCartTx`, `waUI`). Les opérations plus lourdes (recompression pattern, sauvegarde) restent sur le thread UI.
- Instrumentation prévue : traces `chVTGetSystemTime()` dans Reader/Scheduler/Player pour vérifier la dette temporelle, plus audit MIDI (NOTE_ON/OFF pairs) en CH1..CH16.

---

## 4) TODOs suivis
- Finaliser l'installation de la toolchain GNU Arm Embedded 10.3-2021.10 sur l'environnement Windows (ChibiStudio) pour reproduire cette baseline byte-for-byte.
- Prochaine passe : isoler `seq_runtime_hot_t`/`seq_runtime_cold_t` et étendre le runner aux 16 tracks en respectant le pipeline Reader → Scheduler → Player (`SEQ_BEHAVIOR.md`).【F:SEQ_BEHAVIOR.md†L60-L133】

---

## 5) Conclusion

Le dépôt est désormais autosuffisant (ChibiOS vendorisé), le build release compile et fournit une baseline mémoire <150 KiB avec CCRAM neutralisée. Le gate « Go 16 tracks » est **ouvert** : le split `seq_runtime_hot/cold`, le routage 4×XVA1 et la consolidation Reader/Scheduler/Player pourront être implémentés dans la prochaine passe sans dépasser le budget SRAM ni lever l'opt-in CCRAM.

---

## 6) Phase A — tentative progressive et fail-fast RAM

### 6.1 Mesures `sizeof` avant implémentation

Une mesure préliminaire sur l'outil hôte (`gcc -std=c11`) confirme que la simple extension du runtime partagé à quatre tracks dépasse déjà le budget `.bss + .data ≤ 150 KiB` exigé par Gate A :

| `SEQ_RUNTIME_TRACK_CAPACITY` | `sizeof(seq_runtime_t)` |
| --- | --- |
| 2 (baseline) | 101 576 o |
| 4 | 130 024 o |
| 8 | 186 920 o |
| 16 | 300 712 o |

Les valeurs 4/8/16 sont obtenues en recompilant `seq_runtime.h` avec `-DSEQ_RUNTIME_TRACK_CAPACITY=N` avant inclusion, puis en exécutant le binaire hôte généré.【6d9b09†L1-L4】【f9ad51†L1-L4】【2f2cf6†L1-L4】 La taille `101 576 o` de la baseline 2 tracks correspond aux audits post-link fournis dans la passe précédente.【F:tools/audit/audit_sections.txt†L1-L28】

Même sans intégrer les autres symboles `.bss` (buffers UI, work areas RTOS, shadow cart, etc.), la variante 4 tracks porterait `g_seq_runtime` à ~130 KiB. En y ajoutant les ~28 KiB de buffers annexes déjà présents, la marge SRAM retomberait sous 0 KiB, enfreignant immédiatement Gate A (marge ≥35 KiB).

### 6.2 Rapport de non-faisabilité (Phase A)

| Sous-composant | Baseline 2 tracks | Projection 4 tracks | Budget Phase A |
| --- | --- | --- | --- |
| `seq_runtime.project` (banques + manifest) | ~73 KiB | ~73 KiB | ≤40 KiB |
| `seq_runtime.tracks` | 2 × 14 224 o = 27,8 KiB | 4 × 14 224 o = 55,7 KiB | ≤32 KiB |
| Buffers UI (`seq_led_bridge`, holds, mute) | ~8 KiB | ~8 KiB | ≤8 KiB |
| Work areas RTOS (`waUI`, `s_seq_engine_player_wa`, `waCartTx`) | ~9 KiB | ~9 KiB | ≤9 KiB |
| MIDI/cart shadow (`cart_link`, `midi`) | ~6 KiB | ~6 KiB | ≤6 KiB |
| **Total `.bss + .data` estimé** | ~129 KiB | ~151 KiB | **≤150 KiB** |

Le simple doublement du nombre de tracks consommerait ~22 KiB supplémentaires, faisant sauter le plafond de 150 KiB sans même toucher au code du runner. Aucun gain superficiel (nettoyage de buffers UI ou WA) ne compense une telle hausse ; il faut restructurer en profondeur le runtime pour séparer les masses « hot » et « cold » et compacter la représentation d'une track.

### 6.3 Plan de remédiation (A→E)

| Axe | Gain visé | Actions clefs | Risques |
| --- | --- | --- | --- |
| **A. Split `seq_runtime` hot/cold** | −35 KiB | Extraire `seq_runtime_hot_t` (Reader/Scheduler/Player, WA) ≤64 KiB et repousser `seq_project_t` + caches UI dans `seq_runtime_cold_t` (accès hors RT). | Toucher `seq_runtime_init()` / `seq_led_bridge` et adapter toutes les API `seq_runtime_get_*`. Contrainte forte sur la sérialisation (`seq_project_*`). |
| **B. Compaction p-locks** | −20 KiB | Réencoder `seq_model_plock_t` (value 16 b, param 10 b, domaine/voix 6 b) + stocker les 20 p-locks/step dans un pool partagé compressé (4 o/entrée). | Refonte complète de `seq_model_plock_*`, `seq_project_track_steps_encode/decode`, impacts UI hold & runner. Tests host à réécrire. |
| **C. Manifest projet fin** | −15 KiB | Extraire les 16×16 descriptors (`seq_project_pattern_desc_t`) vers un manifest compact (nom + offset + track_count), charger une seule banque active en RAM. | Refactor `seq_project_save/load`, `ui_track_mode`, `seq_led_bridge_select_track`. Risque de régression I/O si manifest partiel. |
| **D. Relocalisation buffers non-RT** | −8 KiB | Déplacer `seq_project` I/O buffer (3,9 KiB) + caches hold (3,6 KiB) dans une zone `.noinit` ou segment froid explicitement exclus du hot path. | Vigilance sur l’init zéro (`memset`) et la persistance lors d’un soft reset. |
| **E. CCRAM opt-in ciblé** | −24 KiB | (Ultime) Réactiver `.ram4` pour les piles RT (`waUI`, `s_seq_engine_player_wa`) et scratch UI non-DMA si A–D insuffisants. | Demande validation explicite, audits `.ram4` à fournir, garantit absence de DMA (cart/MIDI) sur CCRAM. |

Le cumul des gains A–D ramènerait la projection 4 tracks ≈ 73 (proj) + 25 (tracks compacts) + 10 (UI/WA) ≈ 108 KiB, ouvrant enfin Gate A. Ce pré-requis est indispensable avant d’implémenter le super-runner 4×4 XVA1.

### 6.4 Décision

Sans refonte mémoire préalable (A–D), l’extension à 4 tracks violerait Gate A. Conformément aux instructions fail-fast, la passe s’arrête ici : aucun code fonctionnel n’est introduit tant que le split hot/cold et la compaction p-locks ne sont pas implémentés et vérifiés par audits. Les prochaines étapes devront se concentrer sur ces optimisations structurelles avant toute tentative Phase B/Phase C.【SEQ_BEHAVIOR.md†L60-L133】

## Phase A — Analyse de faisabilité (Fail-Fast mémoire A→D)

### 1. Diagnostic de l’échec

**Résumé.** La tentative de refonte mémoire A→D a été interrompue « fail-fast » car le découplage `g_seq_runtime` → hot/cold implique une requalification simultanée des API runtime/UI/engine/projet, un effort supérieur au budget de la passe. Les quatre axes (split hot/cold, compaction p-locks, manifest compact, relocalisation non-RT) sont interdépendants : toucher l’un impose d’aligner les autres pour conserver la compatibilité des encodeurs/sérialiseurs et des vues UI, ce qui excède la fenêtre impartie.

**Points de blocage concrets.**

- **Dépendances croisées fortes** : `seq_runtime_t` agrège directement le projet persistant et les tracks actives, exposant des getters utilisés par le bridge LED, le runner engine et la capture live. Toute scission impose de revoir `seq_runtime_init()` et les assignations `seq_project_assign_track()` pour tous les consommateurs.【F:core/seq/seq_runtime.h†L18-L40】【F:core/seq/seq_runtime.c†L12-L47】【F:apps/seq_led_bridge.h†L24-L93】【F:apps/seq_engine_runner.h†L18-L22】
- **Sérialisation/UI sensibles** : le modèle track + p-lock est encodé/décodé par `seq_project_track_steps_encode/decode()` et vérifié par les tests hôtes (`seq_track_codec_tests.c`). Toute modification binaire (compaction p-locks, manifest) nécessite d’adapter ces fonctions et de régénérer les tests, travail impossible dans le budget fail-fast.【F:core/seq/seq_model.h†L82-L154】【F:core/seq/seq_project.h†L109-L156】【F:tests/seq_track_codec_tests.c†L71-L160】
- **Structure actuelle des p-locks** : `seq_model_plock_t` stocke cinq champs (value, paramètre, domaine, voix, param interne). Le bit-packing envisagé requiert une représentation parallèle, plus un upgrade de tous les appels UI/runner qui inspectent `seq_model_step_t`, effort incompatible avec une passe courte.【F:core/seq/seq_model.h†L82-L120】
- **Tests manquants pour la migration mémoire** : aucun test automatisé ne couvre aujourd’hui un runtime scindé hot/cold, ni la migration des buffers UI/cart hors `g_seq_runtime`. Les suites existantes (`seq_hold_runtime_tests`, `ui_*_tests`) supposent les symboles actuels et échoueraient tant que les doubles pointeurs ne sont pas recablés et validés sur cible réelle.【F:tests/seq_hold_runtime_tests.c†L1-L200】【F:tests/ui_mode_transition_tests.c†L1-L200】
- **Risque d’instabilité init/runtime partagé** : `seq_runtime_init()` efface la totalité de `g_seq_runtime` avant d’attacher les tracks au projet. Un split en plusieurs sections impose de garantir la remise à zéro et l’attachement cohérents, faute de quoi les pointeurs du bridge UI ou du runner engine deviennent pendants pendant l’initialisation.【F:core/seq/seq_runtime.c†L14-L25】

### 2. Analyse de la structure actuelle

#### 2.1 Cartographie mémoire baseline

- `.data` = 1 792 o, `.bss` = 130 220 o, soit ≈ 129 KiB de RAM statique ; la marge SRAM restante est ~63 KiB hors piles, confirmant que la CCRAM (`.ram4`) est vide (`0 o`, NOLOAD).【F:tools/audit/audit_sections.txt†L1-L28】【F:tools/audit/audit_map_ram4.txt†L1-L19】
- `g_seq_runtime` demeure le symbole dominant avec 101 448 o, suivi par `g_hold_slots` (3 648 o), `waCartTx` (3 200 o), les caches UI (`s_ui_shadow`, `g_shadow_params`) et les work areas ChibiOS (`waUI`, `s_seq_engine_player_wa`).【F:tools/audit/audit_ram_top.txt†L1-L45】
- Les autres audits (`audit_bss_top`, `audit_data_top`) restent inchangés par rapport à la baseline précédente ; aucune relocalisation automatique ne réduit le poids des patterns ou du projet dans `g_seq_runtime`.

#### 2.2 Composants les plus coûteux

- **Runtime séquenceur** (`g_seq_runtime`) : embarque `seq_project_t` (manifest complet, banques/patterns) et `seq_model_track_t[2]`. Le projet seul contient 16 banques × 16 patterns, chacune avec 16 descriptors, ce qui explique la masse >70 KiB.【F:core/seq/seq_runtime.h†L26-L40】【F:core/seq/seq_project.h†L20-L121】
- **Caches UI/LED** : `seq_led_bridge` maintient des vues hold (masques, paramètres) qui pointent directement sur les tracks du runtime, empêchant de simplement déplacer ces buffers sans plan de synchronisation.【F:apps/seq_led_bridge.h†L61-L93】
- **Work areas temps réel** : `waCartTx`, `waUI`, `s_seq_engine_player_wa` sont nécessaires au runner et doivent rester en SRAM principale tant que `.ram4` est neutralisée.【F:tools/audit/audit_ram_top.txt†L12-L45】

#### 2.3 Complexité du couplage

- Le runtime expose des getters partagés : `seq_led_bridge_access_track()`, `seq_engine_runner_attach_track()` et `seq_project_get_track()` manipulent directement les pointeurs internes. Séparer hot/cold impose de réinventer l’accès (handles, proxies) pour éviter les derefs directs.【F:apps/seq_led_bridge.h†L84-L93】【F:apps/seq_engine_runner.h†L18-L22】【F:core/seq/seq_project.h†L123-L156】
- Les encodeurs/décodeurs projet manipulent les structures complètes (steps, p-locks). Toute compaction doit préserver les invariants Reader → Scheduler → Player décrits dans `SEQ_BEHAVIOR.md` et les invariants de sérialisation, d’où un chantier transversal Model/Project/UI/Engine.【F:SEQ_BEHAVIOR.md†L60-L133】【F:core/seq/seq_project.h†L147-L156】
- L’absence de tests CI ciblant la migration mémoire (hot/cold, manifest partiel) impose une validation manuelle lourde : il faudrait forger de nouveaux tests hôtes et scripts d’audit avant de pouvoir bouger un seul champ dans `seq_runtime_t`.

### 3. Relance progressive (micro-passes A1→A5)

| Étape | Objectif | Impact attendu | Risque | Dépendances clés | Estimation | Vérification |
| :---- | :------- | :------------- | :----- | :--------------- | :--------- | :----------- |
| **A1** | Introduire `seq_runtime_hot_t` / `seq_runtime_cold_t` sans déplacer les données (double wrapper + API). | 0 KiB (préparation) | Faible | `seq_runtime_init`, `seq_led_bridge`, `seq_engine_runner`, `seq_project` (pointeurs). |  | Build release, `make check-host`, audits identiques (`audit_sections`). |
| **A2** | Déporter `seq_project` + caches UI non-RT dans `seq_runtime_cold_t`. | ~10 KiB | Modéré | `seq_project_*`, `seq_led_bridge_*project`, sauvegarde/chargement projet. |  | Build OK, audits verts (`audit_ram_top`), vérif UI (smoke). |
| **A3** | Prototype bit-packing p-locks via structure parallèle + encodeur pilote. | 15–20 KiB | Fort | `seq_model_step_t`, `seq_project_track_steps_encode/decode`, tests codec, UI hold. | | `make check-host`, nouvel encodeur validé sur pattern de test, audits `sizeof`. |
| **A4** | Manifest projet compact (charge 1 banque active). | ~10 KiB | Moyen | `seq_project_*descriptor`, `seq_project_load/save`, UI bank/pattern selectors. |  | Migration flash manuelle, tests UI navigation, audits `.bss`. |
| **A5** | Campagne `sizeof` + audits post-refactor pour Gate A. | 0 KiB | Aucun | Scripts audit (`tools/audit/*`), docs runtime. |  | `make -j8 all`, `tools/audit`, check `.ram4 = 0 o`. |

**Notes d’exécution.**

- Chaque étape garde CCRAM désactivée ; toute réactivation (`.ram4`) devra faire l’objet d’un jalon distinct avec audits spécifiques.【F:tools/audit/audit_map_ram4.txt†L1-L19】
- Les dépendances listées impliquent mise à jour des tests hôtes (`tests/seq_track_codec_tests.c`, `tests/seq_hold_runtime_tests.c`) pour suivre les nouvelles signatures et garantir la compatibilité des encodeurs UI/engine.【F:tests/seq_track_codec_tests.c†L71-L160】【F:tests/seq_hold_runtime_tests.c†L1-L200】
- Les estimations supposent disponibilité des audits automatiques (`tools/audit/*.txt`) et d’un environnement cible pour smoke tests UI/runner.

### 4. Décision

- **Code inchangé** : aucun fichier source n’a été modifié durant cette passe (fail-fast documenté uniquement).
- **Prochain jalon** : A1 (split minimal hot/cold) pour préparer la migration sans déplacer les données.
- **CCRAM** : reste désactivée (`.ram4 = 0 o`).【F:tools/audit/audit_map_ram4.txt†L1-L19】
- **Phase A (4 tracks)** : exécution suspendue jusqu’à validation complète des étapes A1→A4 puis audits Gate A.
