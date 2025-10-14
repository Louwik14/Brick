# 🤖 Agent Codex — Brick Firmware

## 🎯 Mission permanente
Tu es l’agent Codex principal du projet **Brick**, une interface de contrôle modulaire Elektron-like (ChibiOS 21.11.x, STM32F429).
Tu aides à :
- maintenir la cohérence architecturale du firmware,
- refactorer proprement (Model/Engine/UI),
- générer du C compilable et documenté,
- et garder le code lisible, modulaire et sans dette.

## 🧩 Lignes directrices
- Respecter la **séparation stricte des couches** :
  - `core/` (engine, scheduler, model)
  - `ui/` (interface utilisateur, rendu OLED/LED)
  - `drivers/` (hardware abstrait)
  - `cart/` (communication avec DSP externes)
- Aucun `#include` croisé UI ↔ core ↔ cart.
- Temps toujours en `systime_t`, jamais de blocage en ISR.
- Tous les fichiers : guards + Doxygen standard.
- Style C clair, `static` par défaut, commentaires anglais.
## 🕒 RTOS & Timing — ChibiOS

- RTOS : **ChibiOS 21.11.x**  
- Cible : **STM32F429** (Cortex-M4F)  
- Pas de heap dynamique non maîtrisée (`malloc`, `free` interdits).  
- **Threads** (`chThdCreateStatic`) uniquement ; priorité gérée explicitement (`NORMALPRIO+X`).  
- **Pas de chThdSleep()** dans les chemins critiques (ISR, player, clock callback).  
- Tous les timings doivent être en **`systime_t`** et comparés avec `chVTGetSystemTimeX()` ou `TIME_MS2I()`.  
- Les **ISR** doivent être très courts : aucun appel bloquant, aucun `printf`.  
- Préférer **mailboxes / queues / semaphores** (`chMBPostTimeout`, `chSemSignal`) pour synchroniser les threads.
- Le code de rendu (OLED/LED) doit être **non-bloquant** : utiliser des buffers + flags, jamais `sleep()`.
## 🧩 Organisation du code (Brick / ChibiOS)

- `core/` : logique temps réel (clock, scheduler, seq_engine, player).  
- `ui/` : rendu et interactions (ui_task, ui_backend, ui_shortcuts, modes customs).  
- `drivers/` : périphériques STM32 (OLED, LEDs, UART cartouches, MIDI USB/DIN).  
- `cart/` : protocole XVA1-like, communication UART vers DSP externes.  
- `apps/` : modes utilisateurs (SEQ, KBD, ARP, MUTE, etc.), avec UI et logique internes.

### 🧠 Règles d’inclusion
- Aucune inclusion croisée (UI ↛ core, core ↛ drivers, etc.).  
- `ui_backend` = **unique point de contact** entre interface et moteur.  
- `cart_link` = **unique point de contact** entre moteur et cartouches.  
- `clock_manager` = **unique source de temps** pour tout l’écosystème.  

## 🧰 Build & Outils

- Compiler avec :  
  ```bash
  make all USE_OPT='-O2 -Wall -Wextra -std=gnu11'

## 🧠 Culture du projet
- Philosophie Elektron-like : menus cyclables, modes actifs persistants, p-locks visuels.
- UI : 4 cadres × jusqu’à 5 pages, bandeau supérieur fixe (num cart inversé, nom cart, label mode actif, BPM, pattern).
- Cartouches : protocoles XVA1-like, mais indépendants du moteur séquenceur.

## ⚙️ Outils
- Compiler avec `make` (ChibiOS/arm-gcc).
- Vérifications : `cppcheck`, `clang-tidy`, `doxygen`.
- Tests sur carte STM32F429 Disco.

---

### 🔒 Contraintes fortes

```markdown
## 🔒 Contraintes RT fortes

- **Aucune allocation dynamique** pendant l’exécution.  
- **Aucune division flottante** dans les callbacks temps réel.  
- **Aucun printf / log bloquant** dans les threads hautes priorités.  
- **Pas de delay arbitraire** (`chThdSleepMilliseconds`) en dehors du thread UI.  
- Tous les chemins critiques doivent se terminer en ≤ 200 µs (analyse via logic analyzer ou STM Studio).
- Pas de dépendance à HAL STM32 directe (tout passe par drivers Brick).

## 🧩 Mode opératoire

- Toujours produire du C compilable sous **ChibiOS 21.11.x**.
- Les threads doivent être créés via `chThdCreateStatic`.
- Toute temporisation doit utiliser `systime_t` et les macros `TIME_MS2I`, `chVTGetSystemTimeX`.
- Utiliser `mailboxes` ou `queues` pour la communication inter-threads.
- Aucune logique UI dans les ISR.
- Respecter les priorités :
  - Clock (24 PPQN) : NORMALPRIO+3
  - CART TX : NORMALPRIO+2
  - Player : NORMALPRIO+1 (≥ UI)
  - USB MIDI TX : NORMALPRIO+1
- Interdire tout appel bloquant (`sleep`, `printf`, `malloc`) dans ces contextes.
- Les fichiers générés doivent inclure Doxygen, guards, et passer la compilation.

---
