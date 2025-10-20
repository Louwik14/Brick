# SEQ_BEHAVIOR.md — Spécification comportementale du séquenceur Brick

> **But du document.** Décrire **exhaustivement** le comportement attendu du séquenceur Brick, côté **UI**, **LED/OLED**, **modèle de données** et **exécution temps réel**. Cette spec est la source d’autorité pour le développement et les tests.
> **Cible** : STM32F429 + ChibiOS 21.11.x — sép. stricte Model / Engine / UI (pas de cross-include).

---

## 0) Vue d’ensemble

* **Pattern** : conteneur de 16 tracks.
* **Tracks** : 64 pas (steps) avec par steps **4 voix/step** (V1..V4) + 20 p-lock paramètre cartouches max
* **Événements** : NOTE_ON/OFF MIDI + **P-locks** (internes & cartouche).
* **Lecture** : `Reader → Scheduler → Player`. **NOTE_OFF jamais droppé**.
* **Temps** : tout en `systime_t` (ticks fournis par `clock_manager` v2).
* **Quantize** : **reporté** (non appliqué en live commit, paramètre à venir).
* **Micro-timing (Mic)** : ±12 “micro-ticks” (= ±0,5 step).
* **P-locks** : par step (internes & cart), **automation-only** possible (step sans note).
* **LED/OLED** : non bloquant ; rendu stateless à partir d’instantanés (snapshots).

---

## 1) États et paramètres de base

### 1.1 Step “neutre” (quick-step par défaut)

Quand on **active** un step vide (tap court) en mode SEQ :

* Le modèle instancie un **step neutre** :

  * **Voix 1** :
    `note=C4` (par défaut), `vel=100`, `len=1` (en pas), `mic=0`.
  * **Voix 2..4** : `vel=0` ⇒ **aucune note planifiée**.
* **Offsets “All”** (globaux au step) :
  `Transp=0`, `Vel=0`, `Len=0`, `Mic=0`.

  > Les offsets “All” sont **des décalages** appliqués aux 4 voix à la lecture. Valeurs par défaut à 0 (pas d’effet).

**Garantie** : un quick-step neutre **doit jouer** (NOTE_ON/OFF) au moins la **Voix1** (si non mutée).

### 1.2 Step “automation-only”

Un step peut exister **sans note**, **uniquement** avec des P-locks :

* Flag `automation_only=true`.
* **Toutes** les voix ont `vel=0` ⇒ **aucune note** (MIDI silencieux).
* Les P-locks (internes ou cartouche) sont **exécutés** au timing du step.

**Conversions** :

* Si un step vide est **édité** par P-lock (sans placer de voix) ⇒ devient **automation-only**.
* Si on place ensuite une voix (vel>0) ⇒ `automation_only=false`.

### 1.3 P-locks — types & portée

* **Internes** (séquenceur) : NOTE, VEL, LEN, MIC, offsets All (Transp/Vel/Len/Mic).
* **Cartouche** (UART) : paramètres DSP externes via `cart_link_param_changed()`.
* **Ordonnancement** : P-locks **avant** NOTE_ON : `t_plock = max(now, t_on - tick_st/2)`.


* Les tracks **mutées** ne produisent aucun événement (les offsets ne s’appliquent donc pas).

### 1.4 Transpose & Scale

* `transpose_global` + `transpose_track[t]` (appliqués au Reader).
* **Scale** `(enabled, root, mode)` : clamp des notes **au Reader** si activé.

---

## 2) Interaction utilisateur (mode SEQ)

### 2.1 Accès et navigation

* **SHIFT+BS9** : ouvre **SEQ** (re-entrée = sous-mode **mode**, cyclage vers **setup**).
* **+ / −** : change **la page SEQ** visible (la page reste figée pendant PLAY).
* **BMx** : menus cartouche — **le mode actif (SEQ)** reste actif (BS conservent leur sémantique SEQ).

### 2.2 Quick-step

* **Tap court** sur un pad (step) :

  * si vide ⇒ **crée step neutre** + **planifie voix1** ; LED passe “actif”.
  * si déjà actif ⇒ **toggle OFF** (= supprime toutes voix et P-locks, step vide).
* **LED/OLED** se mettent à jour immédiatement (via snapshot).

### 2.3 P-lock (Elektron-like)

* **Seuil hold** : maintenir un/plusieurs steps → entre en **mode P-lock**.
* **Tweaker** un paramètre (SEQ **ou** cartouche via BM) pendant le hold :

  * Un paramètre **p-locké** s’affiche **en inversé** (titre) sur l’OLED.
  * Tant que hold actif, l’OLED montre **la valeur p-lock** ; au release, retour **live**.
* **Multi-hold** (plusieurs steps) :

  * OLED affiche uniquement les **p-locks communs** ; si valeurs différentes ⇒ `—`.
  * **Modifier** un paramètre commun ⇒ **écrit la même valeur** sur **tous** les steps maintenus (valeur absolue, pas offset).
* **Automation-only** : si un step devient p-lock-only ⇒ vel=0 sur toutes les voix ⇒ **aucune note**, p-locks seulement.

### 2.4 REC live (Keyboard → Pattern)

* **REC ON + PLAY** : les NOTE_ON/OFF du **mode Keyboard** sont **capturés** :

  * `seq_live_capture_plan_step()` calcule **micro offset** (latence mesurée).
  * `seq_live_capture_commit_step()` :

    * **instancie** un step neutre si besoin,
    * **remplit** les **voix** (par défaut Voix1 ; poly ≤ 4),
    * **conserve** le **micro offset** tel que joué (pas de quantize ici).
* **REC OFF** : aucune capture.
* **REC pendant STOP** : **pré-arm** (prépare la pattern), enregistrement effectif **à PLAY**.

---

## 3) Rendu OLED (UI)

### 3.1 Bandeau commun

* À gauche : **#cart** (inversé), **Nom cart**.
* Dessous : **Label du mode actif** (`SEQ`), **“mode”** ou **“setup”**.
* À droite : **Titre menu actif**, icône de **note**, **BPM**, **Pattern**.

### 3.2 Pages SEQ (4 cadres × jusqu’à 5 pages)

* **Page 1 — “All”** (offsets globaux appliqués aux 4 voix du step) :

  * `Transp : −12..+12` st (0 par défaut)
  * `Vel : −127..+127` (0)
  * `Len : −32..+32` steps (0)
  * `Mic : −12..+12` (0)
* **Pages 2–5 — “Voix1..4”** :

  * `Note` (0..127, affichage B3/C4…), défaut **C4**
  * `Vel` (**0=OFF**..127 ; V1 défaut 100 ; V2..V4 défaut 0)
  * `Len` (1..64 pas, défaut 1)
  * `Mic` (−12..+12, défaut 0)

### 3.3 Affichage P-lock

* **Hold 1 step** :

  * **Sans p-lock** : valeur modèle, **pas d’inversion**.
  * **Avec p-lock** : seuls les params **lockés** ⇒ **inversés** + **valeur p-lock**.
* **Hold multi-steps** :

  * Affiche **p-locks communs** ; si divergents ⇒ `—`.
  * Tweaker ⇒ écrit **même valeur** partout.
* **Release** : retour **instantané** à l’affichage live (aucune inversion).

---

## 4) LEDs

* **Blanc** : **playhead**.
* **Rouge** : **muté** (mute **commis**).
* **Couleur track** : **step actif** (non mute).
* **Automate-only** : **teinte distincte** (ex. cyan atténué).
* **PMUTE** : **prévisualise** (LEDs changent) **sans** écraser l’état de mute tant que non validé ; annuler le PMUTE restaure l’état précédent.

**Performance** :

* Publication LED via `seq_led_bridge_publish()` → `ui_led_backend` (queue non bloquante).
* Aucun `sleep` dans le pipeline LED.

---

## 5) Exécution temps réel (Engine)

### 5.1 Reader (abonné clock v2)

Entrées du callback `clock_manager` v2 :

* `now`, `step_idx_abs`, `tick_st` (24 PPQN), `step_st` (1/16), `bpm`, `ext_clock`.

Pour chaque **track** et **step** à jouer :

1. **Snapshot** du model si `gen` changé.
2. Pour chaque **voix** :

   * Calcule `note_eff = note + transpose_global + transpose_track` (clamp 0..127).
   * **Scale** si activée (clamp au `(root, mode)`).
   * Calcule `t_on`, `t_off` à partir de `len`, `mic` et `step_st`.
   * **P-locks CART** : `t_cc = max(now, t_on - tick_st/2)`.
3. **Scheduling** :

   * Si **vel == 0** ⇒ **pas** de NOTE_ON/NOTE_OFF pour cette voix.
   * Si **automation_only** ⇒ **pas** de NOTE_ON/OFF, **P-locks seulement**.
   * Sinon, enfile `EV_NOTE_ON`, `EV_PLOCK`, `EV_NOTE_OFF` aux **timestamps**.

### 5.2 Scheduler

* File **triée** par timestamp (`systime_t`) + sémaphore `Player`.
* **Drop policy** : NOTE_OFF **jamais droppé**.
* **STOP** :

  * **All Notes Off** immédiat,
  * **annule** P-locks futurs,
  * **flush** de la queue.

### 5.3 Player (thread `NORMALPRIO+1`)

* `sleep_until(e.t)` ; pour NOTE_ON/OFF → `midi_send_event()` ; P-lock CART → `cart_link_param_changed(...)`.
* **Retrigger/poly** : identifiant `on_id` par voix ; OFF n’agit que si `on_id` matche.

---

## 6) Règles MIDI & Cart

### 6.1 MIDI

* **NOTE_ON/OFF** envoyés seulement pour voix **vel>0**.
* **0 = OFF** : **aucune NOTE_ON/OFF** (silence), **ne pas** “jouer vel=0”.
* **Clock MIDI** : géré par `midi_clock` abonné à `clock_manager`.

### 6.2 Cartouche (UART)

* P-locks **CART** ordonnancés via `cart_link_param_changed(...)` au `t_cc`.
* Le protocole XVA1-like est encapsulé dans `cart_proto`.

---

## 7) États mute / pmute

* **Mute** (commis) : la track devient **rouge** ; le Reader **saute** ses NOTE_ON/OFF et ne peut pas **exécuter** des P-locks si on le souhaite 
* **PMUTE** (pré-écoute) : LEDs reflètent l’intention, mais **aucun état** modèle n’est modifié.

  * **Valider** → applique mute réel (commis).
  * **Annuler** → restaure LEDs + état précédent.

---

## 8) Cas limites & règles de cohérence

* **Multi-hold** : si sélectionne des steps de pages différentes, l’UI SEQ reste sur la **page courante**, mais l’édition s’applique à **tous** les steps maintenus.
* **Toggle OFF** d’un step : supprime **voix + P-locks** (redevient vide).
* **Len court** : clamp `t_off` ≥ `t_on + min_len` (≥ 1/64 de step) pour éviter NOTE_OFF “immédiat” nul.
* **Micro** : `mic ∈ [-12..+12]`, conversion `micro_to_offset(m) = m * step_st / 24`.
* **Page lock** : pendant PLAY, la **page UI** affichée reste fixe ; `+ / −` change de page, le **playhead** peut être hors-page.
* **Transport** :

  * **PLAY** : démarre Reader + Player.
  * **STOP** : All Notes Off + flush.
  * **REC** : bras la capture live.

---

## 9) Tests de validation (manuels)

### 9.1 Quick-step neutre

1. Effacer le pattern.
2. TAP step 1 : la LED devient “actif (couleur track)”, un NOTE_ON/OFF est émis sur V1 (C4, vel=100, len=1).
3. TAP step 1 de nouveau : step OFF, silence MIDI.

### 9.2 Automation-only

1. Step vide ; **hold** + tweaker `Transp` en page All.
2. LED = automate (cyan) ; aucun NOTE_ON en lecture ; le paramètre cart/All est exécuté à t_cc.

### 9.3 P-lock display

1. Step actif sans p-lock : hold ⇒ **aucune inversion**.
2. P-locker `Len` : hold ⇒ `Len` en **inversé** avec la valeur p-lock.
3. Multi-hold (deux steps avec `Vel` différents) : affiche `Vel = —`.
4. Tweaker `Vel` pendant multi-hold ⇒ même valeur appliquée aux deux steps.

### 9.4 REC live

1. REC ON + PLAY.
2. Jouer le clavier (1–3 notes).
3. Les voix sont écrites (V1..Vn), micro offsets conservés.
4. STOP ; PLAY : on ré-entend ce qu’on a joué (sans quantize).

### 9.5 Mute / PMUTE

1. Mute track ⇒ LEDs **rouges**, pas de NOTE_ON/OFF planifiés.
2. PMUTE ⇒ LEDs changent, mais annuler restaure l’état ; valider = mute commis.

---

## 10) API modèle — attendu (extraits)

> Les signatures exactes peuvent varier, mais le **sens** doit rester.

```c
// Création neutre / automation
void seq_model_step_make_neutral(seq_model_step_t *s);      // V1 vel=100, len=1, mic=0 ; V2..4 vel=0
void seq_model_step_make_automation_only(seq_model_step_t *s);

// Voix & locks
bool seq_model_voice_is_playable(const seq_model_voice_t *v);   // vel>0
bool seq_model_step_has_playable_voice(const seq_model_step_t *s);
bool seq_model_step_has_any_plock(const seq_model_step_t *s);

// P-lock accessors
bool seq_model_plock_set(seq_model_step_t *s, seq_plock_dest_t dest, int32_t value);
bool seq_model_plock_get(const seq_model_step_t *s, seq_plock_dest_t dest, int32_t *out);

// Multi-edit helpers
void seq_model_steps_apply_abs_value(seq_model_step_t **steps, size_t n, seq_plock_dest_t dest, int32_t value);

// Live capture
void seq_live_capture_commit_step(seq_model_track_t *p, const live_note_t *ev, const capture_ctx_t *ctx);
```

---

## 11) Performance & RT

* **Aucun `sleep`** ni I/O bloquant dans Reader/Scheduler/Player/ISR.
* **Player** ≥ priorité UI (`NORMALPRIO+1`), **Clock** ≥ Player.
* **NOTE_OFF jamais droppé** ; OFF anticipé si retrigger (avec `on_id`).
* **LED/OLED** : buffers + queues ; **stateless** côté renderer.

---

## 12) Extensions prévues

* **Quantize** paramétrable (grid 1/4..1/64, strength 0..100%) appliqué **à la capture live** (non destructif, stocké séparément).
* **Tracks multiples** (jusqu’à 16) + assignation per-track (MIDI ch., cart cible).
* **Copy/paste** de steps/patterns ; **length de pattern** variable.
* **Retrigger** par step ; **swing** global (option).
* **UI SEQ setup** : options de poly, comportement multi-hold avancé, vitesses d’auto-repeat, etc.

---

## 13) Checklist d’acceptation (à chaque PR)

* ✅ `make check-host`
* ✅ `make -j8 all` (avec ChibiOS)
* ✅ Quick-step neutre **joue V1** (C4 vel=100 len=1 mic=0).
* ✅ Automation-only **n’émet pas** de NOTE_ON/OFF, **exécute** ses P-locks.
* ✅ REC ON + PLAY : capture **écrit** dans le pattern (voix + micro).
* ✅ OLED : inversion **uniquement** pour params **p-lockés** ; multi-hold = `—` si divergents ; tweak = valeur appliquée à tous.
* ✅ LEDs : **blanc** playhead, **rouge** mute, **couleur track** actif, **cyan** automate.
* ✅ STOP : **All Notes Off** + **flush** queue.
* ✅ Pas de dépendances croisées UI↔core↔cart ; pas de sleeps en temps réel.

---

**Fin de la spec.**

