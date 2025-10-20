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
