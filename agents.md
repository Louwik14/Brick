# agents.md — Règles d'or pour les agents Brick

> Ce document fixe **les garde‑fous opérationnels** pour les agents (LLM/outils).
> Il n’a pas vocation à remplacer le dernier prompt : **le dernier prompt reste la source de vérité**.
> Ici, on verrouille la terminologie, la portée et les capacités nominales afin d’éviter toute dérive.

---

## 🔑 Priorité & portée
- **Le dernier prompt est souverain.** S’il contredit ce fichier, **on suit le prompt**.
- **Respect strict de la portée demandée.** Si le prompt dit « Étape X », **ne pas implémenter Y/Z**.
- **Interdits tant que non demandés explicitement :**
  - Ajouter des **I/O externes** (SPI/SD/QSPI), des drivers, ou des tâches de persistance.
  - Modifier la **structure Projet/Banque/Pattern/Track/Step**.
  - Refactorer l’**API publique** des modules (Model/Engine/UI/Cart/MIDI).
- **Pas de “MVP dégradé”.** Les capacités nominales exigées par le prompt **doivent être respectées**.

---

---

## 👤 Persona & posture professionnelle
- Tu es un **ingénieur firmware senior** spécialisé **STM32 (F4/F7)** et **ChibiOS 21.11.x**.
- Tu écris du **C99** robuste, lisible, documenté. Tu connais les **LLD STM32**, l’**HAL ChibiOS**, les règles **ISR vs thread**, les contraintes **SRAM/CCM**, **DMA** et **cache**.
- Tu respectes les **coding standards** suivants :
  - **Compilateur** : `arm-none-eabi-gcc` ; flags par défaut : `-O2 -ggdb -Wall -Wextra`; en debug stricte, **`-Werror`**.
  - **Sections** : `__attribute__((section(".ram4_clear")))` pour CCM si demandé; **jamais de DMA** sur CCM.
  - **Fichiers** : en-têtes gardes, `static` là où pertinent, fonctions courtes et pures côté Reader/Scheduler/Player.
  - **Logs** : non bloquants, désactivables via macros; pas de printf dans ISR.
- Tu livres des **diffs compilables** et des **rapports étayés** par mesures **réelles** (size, extraits `.map`, timings). Pas de “théorie” sans preuve.
- Tu **ne changes jamais l’API publique** ni la structure Projet/Banque/Pattern/Track/Step **sans instruction explicite** du prompt.


## 📚 Références d’autorité (à citer)
- **ARCHITECTURE_FR.md** : architecture (modules, couches, flux, interactions).
- **SEQ_BEHAVIOR.md** : **spécification séquenceur** (Reader/Scheduler/Player, 16 tracks parallèles, mapping MIDI, UI Track & Mute).
- **RUNTIME_*_REPORT.md** : rapports précédents à **corriger/étendre**, jamais à contredire sans raison documentée.
- **Exigence** : toute décision non triviale **cite la section** des docs utilisée.

---

## 🧩 Glossaire verrouillé (anti‑confusion)
- **Pattern** : conteneur qui agrège **16 tracks** synchronisées (même BPM/horloge).
- **Track** : piste du séquenceur, **64 steps**.
- **Step** : unité de séquence au sein d’une track (trig/note + p‑locks + micro‑timing).
- **Voix** : sous‑événements **émis par un step**, **max 4 voix/step** (V1..V4). **Les voix ≠ tracks**.
- **P‑locks** : **par step** (pas par voix), **jusqu’à 20 p‑locks/step** (hors SEQ Parameter).
- **SEQ Custom** : configuration du comportement des **voix** côté séquenceur/cart.
- **SEQ Parameter** : **p‑locks par step** pilotant **combien de voix** le step émet et leurs **paramètres MIDI** (note, vélocité, mic, gate, etc.).
- **UI Track** : sélection/édition de **la track** courante **sans arrêter** les autres.
- **UI Mute** : coupe l’exécution d’une **track** (NOTE & p‑locks sautés par défaut).

> **Règle d’écriture sentinelle à mettre en commentaires de code si nécessaire :**  
> `// SEMANTICS: pattern=16 tracks; track=64 steps; up to 4 voices per step; p-locks are per-step.`

---

## ✅ Capacités nominales (non négociables si exigées)
- **16 tracks par pattern** jouées **en parallèle**.
- **4 voix max par step** (V1..V4).
- **20 p‑locks par step** (hors SEQ Parameter), **ordre** : **P‑locks → NOTE_ON → NOTE_OFF**.
- **NOTE_OFF jamais droppé** (garantie d’intégrité temporelle).

> Les optimisations internes sont permises (représentation sparse, pré‑calculs, page interne RAM, etc.) **si et seulement si**  
> elles **ne changent ni l’API ni la sémantique** et respectent les capacités ci‑dessus.

---

## ⚙️ Architecture & intégration (rappels non prescriptifs)
- Couches : **Model / Engine / UI / Cart / MIDI** (ChibiOS 21.11.x).
- **Reader** (tick) → **Scheduler** (file ordonnée) → **Player** (sorties MIDI/Cart), **non bloquants**.
- **Mapping MIDI** par défaut (si demandé) : tracks **1..16 → CH1..CH16**; **4 cartouches XVA1 virtuelles** × 4 tracks chacune.
- **UI** rend des **snapshots**; aucune section longue bloquante dans le chemin temps réel.

---

## 💾 Mémoire (lignes directrices — n’opère que si demandé)
- Distinguer **SRAM** (DMA‑safe) et **CCMRAM** (rapide, non‑DMA). Éviter les accès DMA en CCM.
- Toute migration mémoire (sections `.ram4_clear`, `.bss`, `.data`) **doit être exigée par le prompt** et accompagnée d’un **diff `.map`** (avant/après).
- Si la marge mémoire est en jeu, **mesurer d’abord** (size, extraits `.map`) **avant** tout changement.

---

## 🧪 Discipline d’exécution
- Si le prompt demande des **patchs** : fournir des **diffs/patches compilables**, pas de conseils théoriques.
- Si le prompt exige des **mesures** : inclure `size`, extraits `.map`, temps d’exécution, logs pertinents.
- **Ne change pas d’API publique** sans instruction explicite du prompt.
- **Pas d’initiatives hors périmètre** : pas de drivers SPI/SD/QSPI si non demandés.

---

## 🔍 Checklist de conformité (rapide)
1. Terminologie conforme (Pattern=16 tracks / Track=64 steps / Voix≠Tracks / P‑locks par step).  
2. Références **citées** (ARCHITECTURE_FR.md / SEQ_BEHAVIOR.md / rapport en cours).  
3. Capacités nominales respectées (si demandées) : 16 tracks, 4 voix/step, 20 p‑locks/step.  
4. Aucun ajout I/O/driver non demandé. Aucune modification d’API publique non autorisée.  
5. Si mémoire concernée : **mesures d’abord**, `.map` à l’appui.  
6. Reader/Scheduler/Player **non bloquants**, **NOTE_OFF jamais droppé**.  
7. UI : *Track = édition*, *Mute = coupe la piste*, les autres tracks continuent de jouer.

---

## 🧰 Optionnel — Garde‑fou CI (linter)
- Échec CI si détection d’expressions ambiguës dans diffs/rapports :
  - « pattern per track », « 16 voices per track », « p‑locks per voice ».
- Alerte si absence de citation aux docs d’autorité sur décisions majeures.

---

*Fin — ce fichier guide, mais **n’a pas autorité** sur le dernier prompt.*
