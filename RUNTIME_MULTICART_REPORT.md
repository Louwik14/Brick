# RUNTIME_MULTICART_REPORT

## 0) Références utilisées
- `docs/ARCHITECTURE_FR.md` — Vue d'ensemble, snapshot mémoire baseline et prochain jalon 16 tracks.【F:docs/ARCHITECTURE_FR.md†L1-L118】【F:docs/ARCHITECTURE_FR.md†L120-L170】
- `SEQ_BEHAVIOR.md` — Spécification séquenceur : pipeline Reader → Scheduler → Player, 16 tracks en parallèle, mapping MIDI CH1..CH16, UI Track & Mute.【F:SEQ_BEHAVIOR.md†L10-L133】【F:SEQ_BEHAVIOR.md†L167-L259】

### 0.1 Clarification terminologique (track vs pattern)

Le code historique conserve `seq_model_pattern_t` pour désigner l'unité sérialisée de 64 steps. La spec impose qu'un **pattern complet agrège 16 tracks synchronisées**. La convention utilisée dans ce rapport est donc :

```
Pattern (spec) ─┬─ Track #1  ─▶ 64 steps (seq_model_pattern_t)
                ├─ Track #2  ─▶ 64 steps
                ├─ ...
                └─ Track #16 ─▶ 64 steps
```

Chaque `seq_model_pattern_t` incarne **une track** de 64 steps. Le renommage attendu (`seq_model_track_t`) reste un TODO explicitement planifié afin d'éviter toute ambiguïté future sans casser l'API publique dans cette passe.【F:SEQ_BEHAVIOR.md†L10-L43】

---

## 1) État du dépôt & build baseline

### 1.1 Vendorisation ChibiOS (repo autosuffisant)
- Aucun sous-module : `.gitmodules` supprimé et `git submodule status` renvoie vide (preuve d'autosuffisance du vendor tree).【a6bcfd†L1-L2】
- `Makefile` pointe vers le ChibiOS local (`CHIBIOS := ./chibios2111`) et inclut les règles GCC/RT nécessaires (`hal.mk`, `rt.mk`, `port.mk`).【F:Makefile†L105-L129】
- Les chemins critiques du port 21.11 sont présents dans le dépôt (`os/common/ports/ARMv7-M/compilers/GCC/mk/port.mk`, `os/hal/hal.mk`, `os/rt/rt.mk`).【F:chibios2111/os/common/ports/ARMv7-M/compilers/GCC/mk/port.mk†L1-L118】【F:chibios2111/os/hal/hal.mk†L1-L120】【F:chibios2111/os/rt/rt.mk†L1-L120】

### 1.2 Build release & audits mémoire (baseline 2025-10-20)
- Compilation effectuée via `make -j8 all` (GNU Arm Embedded 13.2.1 dans le container ; alignement 10.3-2021.10 requis sur l'environnement Windows final).【556150†L1-L9】【2b76ba†L1-L6】
- Post-link, les commandes de l'outil `tools\audit_all.bat` ont été rejouées sous Linux pour générer `tools/audit/*.txt` (size/nm triées, snapshots .ram4) à partir de `build/ch.elf`.【3fff42†L1-L23】【a6b7db†L1-L3】
- Résultat sections (`tools/audit/audit_sections.txt`) : `.data` **1 792 o**, `.bss` **130 220 o** ⇒ **~129,0 KiB** de RAM statique. Marge SRAM restante ≈ **63 KiB** avant piles (budget ≥35 KiB respecté).【F:tools/audit/audit_sections.txt†L1-L28】
- `.ram4` neutralisée : taille 0 o, VMA `0x1000_0000`, attribut `NOLOAD` et aucun symbole applicatif (`audit_map_ram4.txt`, `audit_ram4_symbols.txt`).【F:tools/audit/audit_map_ram4.txt†L1-L19】【F:tools/audit/audit_ram4_symbols.txt†L1-L8】
- BSS dominée par `g_seq_runtime` (**101 448 o**), suivie des buffers UI/cart (`g_hold_slots` 3 648 o, `waCartTx` 3 200 o, `g_shadow_params`/`s_ui_shadow` 2 048 o chacun) et des work areas RTOS (`waUI`, `s_seq_engine_player_wa`).【F:tools/audit/audit_bss_top.txt†L1-L10】【F:tools/audit/audit_ram_top.txt†L1-L30】
- `.data` quasiment limitée aux structures Newlib (`__malloc_av_`, `_impure_data`, locale) — aucune constante SEQ/UI ne retombe en RAM initialisée.【F:tools/audit/audit_data_top.txt†L1-L7】

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

- `g_seq_runtime` occupe 101 448 o en `.bss`. Il embarque le projet courant (`seq_project_t`), les deux patterns actifs du bridge LED, le scheduler et l'état live capture. Cette masse unique doit être re-segmentée pour tenir un hot runtime ≤64 KiB.【F:tools/audit/audit_bss_top.txt†L1-L10】
- Les buffers UI (`s_ui_shadow` 2 048 o, `g_hold_slots` 3 648 o) restent en SRAM principale tant que la CCRAM est neutralisée, conformément à l'architecture actuelle.【F:tools/audit/audit_ram_top.txt†L1-L26】【F:docs/ARCHITECTURE_FR.md†L33-L118】
- Aucun I/O externe (SPI/SD/QSPI) n'est requis pour la prochaine étape : tout le pipeline reste en exécution locale (Reader/Scheduler/Player + MIDI).【F:SEQ_BEHAVIOR.md†L60-L133】

---

## 3) Gate « Go 16 tracks » (CCRAM off, sans I/O externes)

Les audits étant au vert (RAM statique <150 KiB, marge >35 KiB, CCRAM vide), la mise en œuvre 1 pattern / 16 tracks peut être engagée. Le plan suivant sera exécuté dans la passe dédiée :

### 3.1 Split `seq_runtime_hot_t` / `seq_runtime_cold_t`

| Bloc | Contenu | Taille actuelle (approx.) | Objectif |
|------|---------|----------------------------|----------|
| `seq_runtime_hot_t` | Reader/Scheduler/Player, curseurs de tick, queues d'événements ordonnées, work areas `s_seq_engine_player_wa`, watchers clocks, caches temps réel (status voices, pending NOTE_OFF) | ~48–52 KiB (à extraire de `g_seq_runtime` + WA associées) | ≤64 KiB (SRAM principale) |
| `seq_runtime_cold_t` | Patterns sérialisés (16×`seq_model_pattern_t`), snapshots UI/LED, caches hold, métadonnées projet/cart, structures rarement mutées | ~50 KiB (`g_seq_runtime` restant + buffers UI/cart) | Reste en SRAM principale, accès amorti hors tick |

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
- Renommer `seq_model_pattern_t` → `seq_model_track_t` (et dérivés) dans une passe dédiée, en alignant la terminologie avec la spec sans casser l'API existante pour l'instant.【F:SEQ_BEHAVIOR.md†L10-L43】
- Finaliser l'installation de la toolchain GNU Arm Embedded 10.3-2021.10 sur l'environnement Windows (ChibiStudio) pour reproduire cette baseline byte-for-byte.

---

## 5) Conclusion

Le dépôt est désormais autosuffisant (ChibiOS vendorisé), le build release compile et fournit une baseline mémoire <150 KiB avec CCRAM neutralisée. Le gate « Go 16 tracks » est **ouvert** : le split `seq_runtime_hot/cold`, le routage 4×XVA1 et la consolidation Reader/Scheduler/Player pourront être implémentés dans la prochaine passe sans dépasser le budget SRAM ni lever l'opt-in CCRAM.
