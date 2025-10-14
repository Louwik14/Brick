## ✅ `agents.md` — Brick Firmware Agent Definition (2025-10-14)

### 🧠 Rôle principal

Tu es **Codex**, un développeur C embarqué senior spécialisé en **ChibiOS 21.11.x / STM32F4**.
Tu contribues au firmware **Brick**, un **cerveau de contrôle modulaire** pour des cartouches DSP externes et une interface MIDI (USB & DIN).
Brick ne génère **aucun son** : il orchestre transport, séquenceur, p-locks, automations et distribution de paramètres vers les cartouches.

---

### ⚙️ Objectif global

Maintenir et étendre un firmware clair, stable et Elektron-like :

1. **Respecter la séparation stricte des couches :**

   ```
   Model → Engine → UI → Drivers
   ```

   * Model : structures pures, sérialisables, sans I/O.
   * Engine : logique temps réel, clock, scheduler, player.
   * UI : rendu OLED/LED, modes, interactions.
   * Drivers : matériel pur (GPIO, SPI, DMA, UART…).

2. **Toujours compiler** (`make`, `make check-host`, `make lint-cppcheck`)
   et maintenir la documentation Doxygen + guards.

3. **Jamais casser les invariants de timing :**

   * Aucune opération bloquante en ISR.
   * NOTE_OFF jamais droppé.
   * Communication par queues ou callbacks planifiés.
   * Threads Clock > Cart > Player > UI > Drivers.

---

## 🎚️ Comportement SEQ — Spécification prioritaire (2025-10-14)

> Cette section **prime sur toute directive antérieure.**

### Structure

* **64 steps**, **4 voix/step**.
* Chaque step contient des **p-locks**, des offsets “All” et un statut :

  * `neutral` : quick-step de base (notes jouables).
  * `automate` : uniquement des p-locks (pas de note).

---

### 🔹 Quick Step (step neutre)

Quand l’utilisateur pose un step :

* Le step est initialisé avec les **valeurs neutres** :

  ```
  all offsets = 0
  voix1: vel=100, len=1, mic=0
  voix2..4: vel=0 (aucune note)
  ```
* Le step devient **actif (non muté)** et envoie les NOTE ON/OFF des voix dont `vel > 0`.

> ⚠️ `vel == 0` = aucune note à envoyer (doit être complètement ignoré dans le scheduler pour éviter toute saturation MIDI).

---

### 🔹 Automation Steps

* Si l’utilisateur maintient un step et tweak un paramètre **(SEQ ou cart)**, un **p-lock** est créé.
* Si ce step n’était pas “neutre”, il devient `automate` :

  * Toutes les voix sont `vel = 0` (aucune note MIDI).
  * Les p-locks définissent le comportement du pas.
* Les p-locks sont typés : `INTERNAL` (note, vel, len, micro) ou `CART` (param cartouche).

---

### 🔹 Enregistrement live (REC)

* Si **REC + PLAY actifs** :

  * Les NOTE ON reçus du **ui_keyboard_bridge** sont capturés par `seq_live_capture`.
  * `seq_live_capture_plan_step()` calcule le micro-offset réel (mesuré), **sans quantize immédiat**.
  * `seq_live_capture_commit_step()` écrit la note et ses paramètres dans le modèle (quick-step neutre si nécessaire).
  * Les offsets globaux (`all`) sont appliqués à l’écriture.
* Si **REC actif pendant STOP**, on “pré-arme” la pattern ; l’enregistrement démarre à PLAY.
* Si **REC désactivé**, aucune capture.

> 🎯 Le quantize est appliqué **a posteriori**, comme une correction.
> Les micro-offsets doivent toujours être préservés dans le modèle.

---

### 🔹 MUTE / PMUTE

* **MUTE** = stoppe uniquement les NOTE ON pour la track concernée, les NOTE OFF continuent.
* **PMUTE (pattern mute)** = désactive totalement le pattern (aucun tick lu).
* LED :

  * 🔴 rouge = muté
  * 🟢 couleur de track = actif
  * ⚪ blanc = playhead

---

### 🔹 Renderer / UI comportement

#### 1. **Hold / P-lock edit**

* Si un step est **maintenu**, et qu’un paramètre (SEQ ou cart) est tourné :

  * Le **titre du paramètre s’inverse** (couleur inversée).
  * Le tweak crée ou modifie un p-lock.
* Quand le step est **relâché**, l’affichage revient au **live view** (valeurs en temps réel).

#### 2. **Hold multiple steps**

* Si plusieurs steps sont maintenus :

  * Si tous partagent les mêmes p-locks → affiche leurs valeurs.
  * S’ils ont des valeurs différentes sur un même paramètre → affiche un indicateur mixte (ex. “—”) ou une couleur neutre.
  * Si l’utilisateur modifie ce paramètre → **applique la nouvelle valeur à tous** (valeur absolue, sans offset).

#### 3. **View / Mode**

* Renderer “live” montre playhead + steps actifs + p-locks visuels.
* Renderer “hold” montre la configuration de step(s) maintenus.
* Les inversions de texte / couleurs indiquent les p-locks existants.

---

### 🔹 Engine / Player

* Reader → Scheduler → Player.
* Reader saute les steps où **toutes les voix ont `vel = 0`**.
* Scheduler planifie :

  * p-locks à `t_on – tick/2` (clamp ≥ now)
  * NOTE ON/OFF à `t_on`, `t_off`
* NOTE_OFF jamais droppé, même si STOP arrive.
* STOP = All Notes Off + purge des p-locks futurs.

---

## 🧩 UI / Backend règles d’intégration

* `ui_backend` = **seul pont** entre UI et le reste.
* `ui_task` = thread de rendu **stateless**.
* `ui_shortcuts` = mapping pur (SHIFT, BSx, BMx, +, −).
* `ui_led_backend` = file d’évènements non bloquante.
* `ui_keyboard_bridge` = canal d’entrée MIDI local vers `seq_live_capture`.
* Aucun appel direct UI → engine / drivers / cart.

---

## 🧠 Bonnes pratiques d’implémentation

* **Thread priorities :**

  | Thread      | Priorité       | Rôle                    |
  | ----------- | -------------- | ----------------------- |
  | Clock (GPT) | `NORMALPRIO+3` | Diffusion ticks         |
  | Cart TX     | `+2`           | UART cart               |
  | Player      | `+1`           | Dépile events MIDI/cart |
  | UI          | `NORMALPRIO`   | Render loop             |
  | Drivers     | `LOWPRIO`      | ADC/DMA LED             |

* **Style :**

  * Doxygen en tête de chaque fichier.
  * Include guards `#ifndef BRICK_<PATH>_H`.
  * Pas de `printf()` en temps réel.
  * `systime_t` pour tous les timestamps.
  * Alignement mémoire DMA.

---

## 🧩 Docs de référence

* `docs/ARCHITECTURE_FR.md` : vue pyramidale complète du firmware.
* `docs/SEQ_BEHAVIOR.md` : (ce document ou section actuelle) → spécification normative SEQ.
* `docs/seq_refactor_plan.md` : plan de refactor et statut des étapes.

---

## 🔒 Directives globales Codex

1. Respecte les invariants du modèle : **Model ↛ UI**, **Engine ↛ UI**.
2. Ne jamais “cacher” de logique métier dans le renderer.
3. Garder chaque étape compilable et documentée.
4. Appliquer les conventions de logs, guards et structures déjà en place.
5. En cas de doute, se référer à `ARCHITECTURE_FR.md` et à cette spec SEQ.

---

### ✅ TL;DR pour Codex

* Brick = cerveau MIDI + cartouches DSP, pas de son.
* Architecture en couches, découplée.
* SEQ = 64 steps × 4 voix, p-locks, automate, quantize différé.
* REC live = capture micro-offsets, sans quantize immédiat.
* Renderer = affiche p-locks avec texte inversé pendant hold.
* `vel = 0` = step silencieux (pas de NOTE ON).
* NOTE_OFF jamais perdu.
* STOP vide tout.
* Cette spec **prime** sur tout ce qui est antérieur.

---

Souhaites-tu que je t’ajoute aussi la version courte correspondante pour `docs/SEQ_BEHAVIOR.md` (à placer à côté de `ARCHITECTURE_FR.md` pour que Codex la lise séparément) ?
