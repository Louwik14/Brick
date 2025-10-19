# SEQ_BEHAVIOR.md — Spécification Comportementale du Séquenceur

> Ce document décrit le **comportement attendu** du séquenceur Brick. Il formalise l’architecture
> (Projet → Banque → Pattern → Track → Step), la lecture simultanée des 16 tracks, les règles
> d’ordonnancement, les interactions UI et le mapping MIDI/cart XVA1. Il ne traite **pas** des
> contraintes mémoire ni de leur optimisation.

---

## 0) Vue d’ensemble

* **Pattern** : 64 pas (steps) × **4 voix/step** (V1..V4).
* **Tracks** : **16 tracks par pattern**, organisées en **4 cartouches XVA1 virtuelles** × **4 tracks** chacune (4×4 = 16).
* **Événements** : NOTE_ON/OFF MIDI + **P-locks** (internes & cart).
* **Lecture** : `Reader → Scheduler → Player`. **NOTE_OFF jamais droppé**.
* **Temps** : chronologie basée sur `systime_t` (ticks fournis par `clock_manager` v2).
* **Quantize** : reporté (non appliqué en live commit, paramètre à venir).
* **Micro-timing (Mic)** : ±12 “micro-ticks” (= ±0,5 step).
* **P-locks** : par step (internes & cart), **automation-only** possible (step sans note).
* **LED/OLED** : non bloquant ; rendu stateless à partir d’instantanés (snapshots).

### 0.1) Architecture hiérarchique (Projet → Banque → Pattern → Track)

| Niveau       | Cardinalité                         | Description |
|--------------|-------------------------------------|-------------|
| **Projet**   | **16 banques**, persisté (flash/SD) | Manifest + offsets, gestion de persistance. |
| **Banque**   | **16 patterns**                     | Chargeable “live” (précharge/loader). |
| **Pattern**  | **16 tracks** synchronisées         | 64 steps/track, même BPM, même horloge. |
| **Track**    | **64 steps** × jusqu’à **20 p-locks** | Assignée à une cartouche XVA1. |
| **Step**     | Événement (trig, note, p-locks, mic) | Peut être neutre, actif ou automation-only. |
| **Cart XVA1**| **4 tracks** (voix/parts 1..4)      | 4 cartouches virtuelles → 16 tracks au total. |

**Cartouches virtuelles et canaux MIDI :**
* `XVA1_1` → tracks **1..4** → **MIDI CH 1..4**
* `XVA1_2` → tracks **5..8** → **MIDI CH 5..8**
* `XVA1_3` → tracks **9..12** → **MIDI CH 9..12**
* `XVA1_4` → tracks **13..16** → **MIDI CH 13..16**

**Règle UI :**
* **Mode Track** = **sélection d’édition** uniquement (ne limite pas la lecture).
* **Mode Mute** = coupe l’exécution d’une track (NOTE_ON/OFF & p-locks sautés par défaut, cf. §7).

---

## 1) États et paramètres de base

* **Tempo global** : par pattern (BPM unique pour toutes les tracks).
* **Transport** : `STOP / PLAY / CONT` avec position `bar:step:mic`.
* **Longueur** : 64 steps par track (length variable envisagée en extension).
* **Swing** : global (option), appliqué au Reader (non destructif sur la timeline).
* **Hold/Preview** : l’UI peut prévisualiser sans engager d’écriture modèle (états éphémères).

---

## 2) Modèle temporel

* **Tick de référence** : fourni par `clock_manager` v2.
* **Step** : unité logique (0..63). Chaque step peut recevoir un micro-offset (Mic).
* **Mic (micro-timing)** : ±12 micro-ticks (résolution interne), affecte l’ordonnancement relatif au step.
* **Timestamps** : toutes les décisions d’ordonnancement émettent des événements horodatés (`systime_t`).

---

## 3) Step & P-locks — Sémantique

* **Step neutre** : aucun trig, aucun p-lock → ne produit aucun événement.
* **Step actif** : NOTE (avec vel>0) et/ou p-locks → produit des événements.
* **Automation-only** : step sans NOTE mais avec p-locks → produit uniquement des p-locks au timestamp du step.
* **Ordre d’application** (par timestamp) :
  1. P-locks de paramètres **avant** NOTE_ON,
  2. NOTE_ON,
  3. NOTE_OFF à `gate` (ou durée par défaut),
  4. P-locks tail (si spécifié explicitement).
* **NOTE_OFF** : jamais droppé ; s’il manque un NOTE_ON correspondant, NOTE_OFF est ignoré sans erreur fatale.

---

## 4) UI — Modes et principes

* **Mode Track** : choisit la track **à éditer** (affichage, encodeurs). **N’influence pas** l’exécution.
* **Mode Mute** : coupe ou rétablit l’exécution d’une track ; voir §7.
* **UI/LED/OLED** : rendus **asynchrones** depuis des snapshots du modèle ; aucune section bloquante longue.

---

## 5) Exécution temps réel (Engine)

### 5.1 Reader (abonné clock v2)
Le Reader parcourt **toutes les 16 tracks actives** d’une pattern, indépendamment de la “track sélectionnée” en UI.

Entrées du callback `clock_manager` v2 :
* position courante (`bar:step:mic`),
* tempo et swing,
* signaux de transport.

Pour chaque **track (1..16)** et **step** à jouer :
1. **Snapshot** du model si `gen` (génération) a changé.
2. Pour chaque **voix** (V1..V4) :
   * évaluer `trig`, `note`, `vel`, `gate`, `mic`,
   * collecter les p-locks (internes & cart).
3. Si la track est **mutée**, ne rien émettre.
4. Sinon, enfile `EV_PLOCKS_PRE`, `EV_NOTE_ON`, `EV_NOTE_OFF`, `EV_PLOCKS_POST` avec leurs **timestamps**.

### 5.2 Scheduler
* Fusionne les événements issus des **16 tracks** dans une file temporelle unique.
* Ordonne strictement par timestamp ; **NOTE_OFF jamais droppé**.
* Signale le **Player** via sémaphore/queue lorsque des événements arrivent.

### 5.3 Player
* Défile la file d’événements à `now()`, route vers MIDI / cart.
* Respecte le **mapping de canaux** (cf. §6).
* Applique les p-locks “cart” via l’API cart au timestamp demandé.

---

## 6) Règles MIDI & Cart

### 6.1 MIDI (mapping par track)
* **NOTE_ON/OFF** envoyés seulement pour voix **vel > 0**.
* **Vel=0** → silence (ne pas émettre NOTE vel=0).
* **Clock MIDI** : géré par `midi_clock`, abonné à `clock_manager`.
* **Mapping canaux** :
  - Track **1..16** → **MIDI CH 1..16** (1:CH1, 2:CH2, …, 16:CH16).

### 6.2 Cartouche XVA1 (virtuel ×4)
* 4 cartouches virtuelles, chacune avec 4 tracks :
  - `XVA1_1` (tracks 1..4) → CH1..CH4
  - `XVA1_2` (tracks 5..8) → CH5..CH8
  - `XVA1_3` (tracks 9..12) → CH9..CH12
  - `XVA1_4` (tracks 13..16) → CH13..CH16
* P-locks **CART** ordonnancés via `cart_link_param_changed(...)` au timestamp `t_cc`.
* Le protocole XVA1-like est encapsulé dans `cart_proto` ; la layer `cart_registry` gère l’aiguillage vers l’instance concernée.

---

## 7) Mute / Pré-mute (pmute)

* **Mute (commis)** : la track devient **rouge** ; le Reader **saute** ses NOTE_ON/OFF.  
  Par défaut, les **p-locks** de tracks mutées sont **sautés aussi** (comportement retenu).
* **PMUTE (pré-écoute)** : LEDs reflètent l’intention, mais **aucun état** modèle n’est modifié.
  * **Valider** → applique mute réel (commis).
  * **Annuler** → restaure LEDs + état précédent.

---

## 8) Changement de pattern

* Décision utilisateur (UI) ou chain programmée → **queue** de la prochaine pattern.
* Le moteur bascule **à la fin de la période** (bar/loop) ou immédiatement si le mode direct est activé.
* Le swap consiste à **échanger le pointeur** de pattern active ; la timeline reste continue.

---

## 9) Live capture & Quantize (règles)

* **Live rec** : écrit les notes/p-locks sur la track sélectionnée, à la position courante.
* **Quantize** : paramétrable (grid 1/4..1/64, strength 0..100%), appliqué **à la capture live** et **non destructif**.
* **Overdub** : remplace/merge selon mode ; les NOTE_OFF orphelins sont normalisés.

---

## 10) Invariants & erreurs

* **NOTE_OFF** jamais droppé.
* Aucune section bloquante longue dans le chemin temps réel (Reader/Scheduler/Player).
* Les callbacks cart/MIDI ne doivent pas bloquer ; file d’événements privilégiée.
* Les actions UI (Track/Mute) n’impactent **pas** la cadence du moteur.

---

## 11) Scénarios de test recommandés

1. **Lecture 16 tracks** : une note par step, aucune mute → vérification MIDI CH1..CH16.
2. **Mute sélectif** : mute de tracks impaires → seules les paires jouent, p-locks sautés sur muted.
3. **Automation-only** : steps sans NOTE avec p-locks → uniquement p-locks émis.
4. **Micro-timing** : offsets variés, ordre déterministe (p-locks avant NOTE_ON).
5. **Pattern switch** : bascule en fin de loop puis en direct → continuité et absence de doublons NOTE_ON/OFF.

---

## 12) Extensions prévues

* **Quantize** avancé (grid variable, strength, humanize) appliqué à la capture.
* **Assignation avancée** per-track (MIDI CH custom, cart cible alternative) — tout en conservant par défaut le mapping 1:CH1 … 16:CH16 et `XVA1_1..4` (4×4).
* **Copy/paste** de steps/patterns ; **length** de pattern variable.
* **Retrigger** par step ; **swing** global.
* **UI SEQ setup** : options de poly, comportement multi-hold, vitesses d’auto-repeat.
