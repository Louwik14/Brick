# agents.md — Règles d'or pour les agents Brick (version alignée P1/P2)

> **Le dernier prompt est souverain.** En cas de divergence, **on suit le dernier prompt**.
> Ce document cadre la **terminologie**, la **portée** et les **garde-fous** pour éviter les dérives.

---

## 🔑 Priorité & portée

* **Toujours respecter la portée du prompt courant.** Si le prompt demande « MP17c », ne pas implémenter MP18/MP19.
* **Interdits sans demande explicite :**

  * Ajouter des I/O/drivers (SPI/SD/QSPI/FS), persistance ou protocole externe.
  * Modifier la structure **Projet/Banque/Pattern/Track/Step** ou l’**API publique**.
  * Toucher au **linker/sections** (CCM/AXI/SDRAM) sans flag/LD et mesure associée.

---

## 👤 Persona & posture

* Ingénieur firmware **STM32 (F4/H7)**, **ChibiOS 21.11.x**, **C99** propre (-O2, -Wall, -Wextra; en debug strict: -Werror).
* Maîtrise **ISR vs thread**, **DMA & caches**, **CCM/TCM**.
* **Logs** non bloquants, pas de `printf` en ISR.

---

## 📚 Références d’autorité

* `docs/ARCHITECTURE_FR.md` — pipeline **Reader → Scheduler → Player**, frontières **apps/** ↔ **core/**.
* `PROGRESS_P1.md` — journal des passes (à **compléter** à chaque passe).
* `SEQ_BEHAVIOR.md` — invariants (ordre P-locks → NOTE_ON → NOTE_OFF, mute côté Reader).

> Toute décision non triviale **cite** la source (fichier + section).

---

## 🧩 Glossaire verrouillé

* **Pattern** = **16 tracks** synchronisées (même horloge).
* **Track** = 64 steps.
* **Step** = trig/note + p-locks + micro-timing.
* **Voix** ≠ tracks ; jusqu’à **4 voix par step**.
* **P-locks** = par step (jusqu’à **20** hors SEQ Parameter).
* **Mapping MIDI par défaut** (si demandé) : tracks **1..16 → CH1..16**.
* **XVA1** : 4 cartouches × 4 tracks (virtuel) = 16.

---

## ✅ Capacités nominales (si exigées par le prompt)

* 16 tracks parallèles, 4 voix/step, 20 p-locks/step.
* **NOTE_OFF jamais droppé**.
* Optimisations internes OK **si** la sémantique/ API publique ne changent pas.

---

## ⚙️ Architecture (garde-fous P1/P2)

* **Surface apps/** : **un seul** header, `core/seq/seq_access.h`.
  ❌ Interdit d’inclure `seq_project.h`, `seq_model.h`, `seq_runtime.h` côté apps/**.
* **Reader-only** côté apps : **handles** + **views DTO** (copies), itérateur p-locks.
  ❌ Interdit d’exposer des pointeurs runtime/modèle.
* **no-cold-in-tick** : la façade **cold** (PROJECT / CART_META / HOLD_SLOTS) est **interdite** en phase **TICK**.
* **Zéro alloc** en TICK, **pas** d’attente/mutex en TICK.
* **Un seul engine/scheduler/player** : ❌ **pas** d’engine×N.

---

## 💾 Mémoire & sections (aligné `seq_sections.h`)

* Utiliser **exclusivement** les macros :

  * `SEQ_HOT_SEC` / `SEQ_COLD_SEC` (expansions **via** `core/seq/runtime/seq_sections.h`).
  * Flags de build : `SEQ_ENABLE_HOT_SECTIONS`, `SEQ_ENABLE_COLD_SECTIONS` (OFF par défaut).
* **CCM/TCM** : *CPU-only*, **jamais DMA**. Tout buffer DMA en **SRAM/AXI/SDRAM** (ou MPU non-cache + maintenance).
* Toute migration `.hot/.cold` demande : **flag + entrée linker .ld + mesure** (`size`, extrait `.map`) et journal dans `PROGRESS_P1.md`.

---

## 🧪 Discipline d’exécution

* Produire des **diffs compilables** et **rapports concrets** (taille, extrait `.map`, timings p99, watermarks queues) quand demandé.
* Ne pas changer l’**API publique** ni la **sémantique** sans instruction explicite.
* Respecter la **progression micro-passe** (petits diffs, un objectif, acceptance claire).

---

## 🔍 Checklist de conformité

1. Vocabulaire conforme (Pattern=16 tracks / Track=64 steps / Voix≠Tracks / P-locks par step).
2. Références citées (`ARCHITECTURE_FR.md`, `PROGRESS_P1.md`, `SEQ_BEHAVIOR.md`).
3. **apps/** n’utilise que `seq_access.h`.
4. **no-cold-in-tick** respecté ; **zéro alloc** en TICK.
5. Pas d’engine×N, pas de pointeurs modèle côté apps.
6. Si sections mémoire : macros `SEQ_HOT_SEC/SEQ_COLD_SEC` + flags + `.ld` + mesures + journal.

---

## 🧰 (Optionnel) Linters CI

* Échec si diffs contiennent `#include "seq_project.h"` / `seq_model.h` / `seq_runtime.h` dans **apps/**.
* Échec si `SEQ_COLD_SEC` vu dans un chemin **TICK**.
* Échec si attributs de section hors `SEQ_HOT_SEC/SEQ_COLD_SEC`.

---

*Fin — ce guide accompagne, mais **le dernier prompt reste la source de vérité***.
