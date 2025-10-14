## ✅ `agents.md` — Brick Firmware Agent Definition (2025-10-14, mise à jour compilation fiable)

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

2. **Toujours compiler avant commit :**

   * `make -j4 all USE_WARNINGS=yes`
   * ou si ChibiOS absent : `make check-host`
   * puis `make lint-cppcheck` (optionnel)

   ⚠️ Aucun commit n’est autorisé si :

   * Une erreur `unknown type name`, `undefined reference`, `incomplete type` ou `missing include` apparaît.
   * Un type utilisé dans un header n’a pas de définition visible.
   * Le build host échoue.

3. **Jamais casser les invariants de timing :**

   * Aucune opération bloquante en ISR.
   * NOTE_OFF jamais droppé.
   * Communication par queues ou callbacks planifiés.
   * Threads : Clock > Cart > Player > UI > Drivers.

---

### 🔒 Règles headers & dépendances

* Chaque `.h` doit **déclarer complètement** tous les types publics utilisés dans ses prototypes.
* Si un prototype expose un `typedef`, `struct` ou `enum`, sa définition doit être visible dans un header inclus.
* Aucun `.c` ne doit dépendre d’un type interne d’un autre module sans include explicite.
* Interdiction d’utiliser des types anonymes dans les prototypes publics.
* Si un prototype change, **tous les appels dépendants doivent être mis à jour**.
* Ne jamais laisser un type ou une fonction référencé sans définition.

---

## 🎚️ Comportement SEQ — Spécification prioritaire (2025-10-14)

> Cette section **prime sur toute directive antérieure.**

### Structure

* 64 steps, 4 voix/step.
* Chaque step contient des p-locks, offsets “All” et un statut :

  * `neutral` : quick-step de base (notes jouables).
  * `automate` : uniquement des p-locks (pas de note).

### 🔹 Quick Step (step neutre)

Quand l’utilisateur pose un step :

```text
all offsets = 0
voix1: vel=100, len=1, mic=0
voix2..4: vel=0
```

Le step devient actif et envoie NOTE ON/OFF pour les voix où `vel > 0`.
`vel = 0` = aucune note (doit être ignorée par le scheduler pour éviter la saturation MIDI).

### 🔹 Automation Steps

Maintien + tweak paramètre → p-lock créé ; si le step n’était pas neutre, il devient `automate` :

* toutes les voix `vel = 0`; seules les automations sont jouées.

### 🔹 Enregistrement live (REC)

* **REC + PLAY** : capture des NOTE ON via `ui_keyboard_bridge` → `seq_live_capture`.

  * `plan_step()` mesure le micro-offset (non quantisé).
  * `commit_step()` écrit les notes dans le modèle.
  * offsets All appliqués à l’écriture.
* **REC + STOP** : pattern armée (enregistrement commence à PLAY).
* **REC off** : aucune capture.
* Quantize = correction a posteriori ; micro-offsets toujours conservés.

### 🔹 MUTE / PMUTE

* MUTE : stoppe NOTE ON seulement (NOTE OFF continue).
* PMUTE : désactive totalement la pattern.
* LEDs : rouge = muté, couleur track = actif, blanc = playhead.

### 🔹 Renderer / UI

* **Hold + tweak** : nom du param inversé, crée/modifie p-lock.
* **Relâchement** : retour au live view.
* **Multi-hold** :

  * mêmes p-locks → affiche valeurs ;
  * différentes → affiche “—” ou neutre ;
  * nouveau tweak → valeur absolue pour tous.

---

### 🔹 Engine / Player

* Reader → Scheduler → Player.
* Steps silencieux (toutes voix vel = 0) ignorés.
* p-locks à `t_on–tick/2` ; NOTE ON/OFF à `t_on` / `t_off`.
* NOTE OFF jamais droppé. STOP = All Notes Off + purge.

---

## 🧩 UI / Backend règles d’intégration

* `ui_backend` = pont unique UI ↔ core.
* `ui_task` = rendu stateless.
* `ui_shortcuts` = mapping pur.
* `ui_led_backend` = file d’évènements non bloquante.
* `ui_keyboard_bridge` = passerelle MIDI → seq_live_capture.
* Aucune dépendance directe UI ↔ engine / drivers / cart.

---

## 🧠 Bonnes pratiques d’implémentation

| Thread      | Priorité     | Rôle                 |
| ----------- | ------------ | -------------------- |
| Clock (GPT) | NORMALPRIO+3 | Diffusion ticks      |
| Cart TX     | +2           | UART cart            |
| Player      | +1           | Événements MIDI/cart |
| UI          | NORMALPRIO   | Rendu                |
| Drivers     | LOWPRIO      | ADC/DMA LED          |

* Doxygen en tête de fichier.
* Include guards `#ifndef BRICK_<PATH>_H`.
* Pas de `printf()` en temps réel.
* `systime_t` pour tous les timestamps.
* Alignement mémoire DMA.

---

## 🧰 Build & Tests

* **Compiler** : `make -j4 all USE_WARNINGS=yes`
* **Tester (host)** : `make check-host`
* **Lint / style** : `make lint-cppcheck`
* **Jamais pousser** si une erreur de build ou de type apparaît.

---

## 🧩 Docs de référence

* `docs/ARCHITECTURE_FR.md` : vue pyramidale complète.
* `docs/SEQ_BEHAVIOR.md` : spécification normative SEQ.
* `docs/seq_refactor_plan.md` : plan et statut.

---

## 🔒 Directives globales Codex

1. Respect strict Model↛UI, Engine↛UI.
2. Pas de logique métier dans le renderer.
3. Chaque étape doit compiler.
4. Conserver logs, guards, structures.
5. Référer à `ARCHITECTURE_FR.md`, SEQ_BEHAVIOR.md et à ce document en cas de doute.

---

### ✅ TL;DR Codex

* Brick = cerveau MIDI + cartouches DSP.
* Architecture en couches.
* SEQ = 64 steps × 4 voix, p-locks, automate, quantize différé.
* REC live = micro-offsets, sans quantize immédiat.
* Renderer = texte inversé sur hold (p-lock).
* `vel = 0` = step silencieux.
* NOTE OFF jamais perdu.
* STOP = All Notes Off + purge.
* Ce document prime sur toutes versions antérieures.

