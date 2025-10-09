# Brick — Firmware Documentation

**Brick** est une plateforme matérielle modulaire de contrôle.  
Elle ne produit aucun son : le firmware gère l’interface utilisateur (UI), la communication temps réel vers des **cartouches DSP externes** et, à venir, un **séquenceur polyphonique** (p-locks, micro-timing).

Les cartouches (ex. **XVA1**) sont pilotées via un bus série. Brick agit comme un **cerveau de contrôle** : il lit les entrées (boutons, encodeurs), affiche l’état sur l’OLED et envoie les paramètres mis à jour **sans couplage direct** entre UI et bus (grâce à `ui_backend`).

> Cette page est la documentation générale. Les recommandations de robustesse, l’audit structurel et la feuille de route sont détaillés dans la section **[Points à surveiller & évolutions](#points-à-surveiller--évolutions)**.

---

## Architecture générale

### Vue en couches (cible)

```
[ Application / Modes customs (futur) ]
│
▼
[ UI Layer (task, input, controller, renderer, widgets) ]
│           ▲
│           └─────── ui_backend (pont neutre)
▼
[ Cart Link & Bus (shadow, UART, proto, registry) ]
│
▼
[ Drivers matériels (buttons, encoders, display, leds, pots) ]
│
▼
[ ChibiOS HAL 21.11.x ]
```

**Règle d’or :** chaque couche ne dépend que de **celle du dessous**.

- L’UI **n’importe pas** de headers bus/UART — elle passe par `ui_backend`.
- Les **headers UI ne dépendent pas des drivers** (`drv_*`) ; le mapping matériel → UI est **confiné à `ui_input.c`**.

### Arborescence (projet actuel)

| Dossier / Module      | Rôle principal                                                                    | Fichiers clés (exemples)                                                                                                   |
|-----------------------|------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------|
| `main.c`              | Entrée programme, séquence d’init, boucle principale                              | `main.c`                                                                                                                    |
| `drivers/`            | Accès matériel bas niveau (GPIO, UART init, OLED, LEDs, boutons, encodeurs, pots) | `drivers.c/.h`, `drv_display.*`, `drv_leds_addr.*`, `drv_buttons.*`, `drv_encoders.*`, `drv_pots.*`                         |
| `cart/`               | Bus & protocole cartouches + registre                                              | `cart_bus.*`, `cart_proto.*`, `cart_registry.*`, `cart_xva1_spec.*`                                                         |
| `core/`               | Services transverses (link, clock, USB/MIDI)                                       | `cart_link.*`, `midi_clock.*`, `clock_manager.*` (à venir), `usb_device.*`                                                  |
| `ui/`                 | Logique et rendu UI, thread, widgets, specs UI                                     | `ui_task.*`, `ui_input.*`, `ui_controller.*`, `ui_renderer.*`, `ui_backend.*`, `ui_widgets.*`, `ui_model.*`, `ui_spec.h`    |
| `midi/`               | Pile MIDI (USB class-compliant + DIN UART)                                         | `midi.*`                                                                                                                    |
| `apps/`               | Apps utilitaires / démos                                                           | `metronome.*`                                                                                                               |
| `usb/`                | Config USB                                                                         | `usbcfg.*`                                                                                                                  |
| `core/brick_config.h` | Paramètres globaux (debug, fréquence UI, etc.)                                     | `brick_config.h`                                                                                                            |

#### Nouveaux ponts / abstractions (post-refactor)

- `ui_backend.[ch]` : **interface neutre** entre l’UI et le lien cartouche (`cart_link`).
- `cart_registry.[ch]` : gestion **de la cartouche active** et des specs associées (forward-decl de `struct ui_cart_spec_t`).
- `cart_link.[ch]` : **shadow register** par cartouche + sérialisation via `cart_bus` (**header public sans dépendre du registry**).

#### Types UI — précision importante

Dans `ui_spec.h`, les bornes de plage des paramètres **continus** sont en **`int16_t`** (`ui_param_range_t.min/max`) afin d’éviter tout overflow (ex. 255) et de permettre des bornes négatives.  
Cela corrige le comportement des encodeurs sur les paramètres non discrets.

---

## Pipeline d’exécution

```
[ Entrées physiques : boutons / encodeurs / pots ]
│
▼
ui_input
│ évènements
▼
ui_controller ──→ (ui_backend) → cart_link → cart_bus (UART)
│ état UI (ui_model)
▼
ui_renderer → drv_display
│
▼
OLED
```

- **Aucun accès bus/UART depuis l’UI** : tout passe par `ui_backend`.
- Les valeurs envoyées aux cartouches sont maintenues côté firmware dans un **shadow register** (lecture possible dans l’UI pour le rendu).
- `ui_input.h` expose des **événements UI neutres** (`btn_id`, `btn_pressed`, `encoder`, `enc_delta`) ; la dépendance aux `drv_*` est confinée à `ui_input.c`.
- `ui_task.c` utilise `ui_input_shift_is_pressed()` pour les combinaisons (ex. SHIFT).

---

## Séquence d’initialisation & boucle principale

Ordre d’initialisation recommandé (implémenté dans `main.c`) :

```c
static void system_init(void) {
    halInit();
    chSysInit();
}

/* I/O temps réel : USB device + MIDI (USB & DIN) + clock 24 PPQN */
static void io_realtime_init(void) {
    usb_device_start();   // re-énumération USB, endpoints MIDI FS EP1 OUT / EP2 IN (64B)
    midi_init();          // UART DIN @31250 + mailbox USB + thread TX (prio ≥ UI)
    midi_clock_init();    // GPT + thread clock NORMALPRIO+3
}

/* Drivers & bus cartouche */
static void drivers_and_cart_init(void) {
    drivers_init_all();
    cart_bus_init();         // UART des ports CART1..CART4
    cart_registry_init();    // état global des cartouches (active, specs)
    cart_link_init();        // initialisation du shadow/cart link (avant l’UI)
}

int main(void) {
    system_init();
    io_realtime_init();      // prêt I/O temps réel avant usage
    drivers_and_cart_init(); // bus + registre + link
    ui_init_all();
    ui_task_start();         // thread UI (cadence ~60 Hz)

    while (true) {
        drv_leds_addr_render();         // rafraîchissement LEDs
        chThdSleepMilliseconds(20);
    }
}
```

- **USB/MIDI/Clock** sont prêts **avant** l’UI.
- `cart_bus_init()` configure les UART cartouches (cf. mapping hardware dans `cart_bus.h`).
- `cart_registry_init()` réinitialise la cartouche active et ses specs UI.
- `cart_link_init()` prépare les **shadows** et l’API de pont.

---

## UI : task, input, controller, renderer, widgets

### Thread UI (`ui_task.c`)

- Lecture non bloquante des entrées (`ui_input`).
- Application des actions via `ui_controller`.
- Rendu conditionnel via `ui_renderer`.
- **Changement de cartouche** via `cart_registry` (types neutres; headers sans drivers).

#### Mode **UI interne** : *SEQ overlay* (nouveau)

- **Accès** : `SHIFT + UI_BTN_SEQ9` (ou autre SEQx mappé).
- **Principe** : **overlay** (on n’active pas une cartouche « SEQ »).  
  L’UI affiche la spec SEQ mais **garde le bandeau/titre** de la cart réelle.
- **Sortie** : le **premier BM** pressé (avec ou sans SHIFT) **quitte SEQ** puis exécute le BM côté cart réelle.  
  Idem avec `SHIFT + UI_BTN_SEQ9` (toggle).
- **État** : pas de reset agressif de l’état cart ; la navigation cart est conservée.

> Ce choix garantit un aller/retour propre entre la vue SEQ et l’UI cartouche, sans “menus fantômes” ni altération du titre.

### Contrôleur UI (`ui_controller.c`)

- Maintient l’état courant (`ui_state_t`), gère les menus/pages/encodeurs.
- **Ne touche pas au bus** : propage via `ui_backend`.
- **Menus cycliques (BM)** : désormais **déclaratifs** via `ui_spec.h` :
  - chaque cartouche peut déclarer pour BM1..BM8 un **cycle** d’indices de menus,
  - le contrôleur **charge automatiquement** ces cycles à chaque `ui_init()` / `ui_switch_cart()`,
  - option `resume` par BM pour reprendre le dernier choix lors d’un retour sur le bouton.

**Exemple (cart XVA1)** : BM8 → FX1→FX2→FX3→FX4
```c
.cycles = {
  [7] = { .count=4, .idxs={11,12,13,14}, .resume=true },
}
```

> L’ancien appel `ui_cycles_set_options()` dans `main.c` n’est plus nécessaire.

### Rendu (`ui_renderer.c`)

- Transforme l’état logique en pixels via `drv_display_*`.
- **Source de vérité** : `ui_get_state()` et `ui_resolve_menu()` (si cycle BM actif, le menu résolu est celui du cycle).

### Widgets & primitives (`ui_widgets.c/.h`, `ui_primitives.h`, `ui_icons.*`, `font*`)

- Composants réutilisables : bargraph, switch, knob, labels.
- Rendu strictement via `drv_display`.

### Backend neutre (`ui_backend.c/.h`)

Point d’entrée unique de l’UI vers la couche inférieure :

```c
void    ui_backend_param_changed(uint16_t id, uint8_t val, bool bitwise, uint8_t mask);
uint8_t ui_backend_shadow_get(uint16_t id);
void    ui_backend_shadow_set(uint16_t id, uint8_t val);
```

- Implémentation côté bas (redirige vers `cart_link` + `cart_registry`).
- Permet de mocker l’UI hors hardware (tests).

---

## Cartouches : bus, link, proto, registry

- **`cart_bus.[ch]`** : configuration UART (CART1..CART4), **file d’envoi asynchrone** (mailbox + pool), **un thread TX par cartouche** (`cart_tx_thread`) à **`NORMALPRIO+2`** (macro configurée).
- **`cart_proto.[ch]`** : sérialisation **compacte et déterministe** :
  - **Format générique** : `[CMD][PARAM_H][PARAM_L][VALUE]`
  - **Profil XVA1** (actuel) : commandes ASCII `'s'/'g'` + extension `255` pour `param >= 256`
- **`cart_link.[ch]`** : **API haut niveau** : applique le `shadow` (set/get par param), évite les envois redondants, fournit les primitives appelées par `ui_backend`. **Header public minimal** : ne dépend que de `cart_bus.h`.
- **`cart_registry.[ch]`** : **cartouche active** + accès à sa spécification UI (labels, menus, pages) pour piloter l’UI à chaud. **Header public sans UI** : forward-decl `struct ui_cart_spec_t`.

---

## Drivers matériels

- `drv_buttons.*` : scan 74HC165 + anti-rebond + mailbox d’événements.
- `drv_encoders.*` : lecture quadrature HW (timers) + accélération EMA.
- `drv_pots.*` : ADC circulaire + moyennage.
- `drv_leds_addr.*` : LEDs WS2812, rafraîchies dans la boucle principale.
- `drv_display.*` : SSD1309 ; thread d’auto-refresh optionnel.

Façade unique : `drivers_init_all()` et `drivers_update_all()` dans `drivers.c/.h`.

---

## Horloge / MIDI / Clock

- `midi_clock.[ch]` : générateur **24 PPQN** (GPT3 @ 1 MHz), ISR courte (signal), thread **`NORMALPRIO+3`**, émission F8 et callbacks précis.
- `midi.[ch]` : pile MIDI **class-compliant** (EP1 OUT / EP2 IN, **64 B**), **mailbox non bloquante** pour TX, **chemin rapide** pour Realtime (F8/FA/FB/FC/FE/FF).
  - Thread TX USB avec **priorité ≥ UI** (macro `MIDI_USB_TX_PRIO`, défaut `NORMALPRIO+1`).
  - Sémaphore d’EP IN avec **timeout court** (anti-blocage) avant `usbStartTransmitI()`.
  - DIN MIDI sur **UART 31250** (SD2), séparé du bus cartouche.
- `clock_manager.[ch]` : orchestration/bridging (métronome & futur SEQ).

---

## Configuration globale (`core/brick_config.h`)

Exemples (actuels) :

```c
/* Debug désactivé (pas de flux UART pour éviter les collisions avec MIDI DIN) */
#define DEBUG_ENABLE 0
#define debug_log(...) ((void)0)

/* Cadence UI */
#define UI_FRAME_INTERVAL_MS 16

/* Priorité TX USB MIDI (≥ UI) */
#ifndef MIDI_USB_TX_PRIO
#define MIDI_USB_TX_PRIO (NORMALPRIO + 1)
#endif
```

> Optionnel ultérieur : réactiver un **debug via USB CDC** au lieu d’UART.

---

## Extrait `main.c`

```c
int main(void) {
    system_init();
    io_realtime_init();      // USB/MIDI/Clock prêts
    drivers_and_cart_init(); // drivers + bus + registre + link
    ui_init_all();
    ui_task_start();
    for (;;) {
        drv_leds_addr_render();
        chThdSleepMilliseconds(20);
    }
}
```

---

## Points à surveiller & évolutions

### Audit structurel — synthèse (état du code actuel)

**Objectif :** garantir que chaque couche ne dépend que de la couche inférieure, et que l’UI est découplée du bus.

#### ✅ Points conformes

- UI → **plus de dépendances directes** vers `cart_*` dans les headers (usage de `ui_backend`).
- **Headers UI sans `drv_*`** : mapping matériel→UI **confiné à `ui_input.c`**.
- `ui_renderer` → ne parle qu’à `drv_display` et à l’état UI / shadow (pas de bus).
- UART cartouche **confinée** à `cart_bus` / `cart_link` (threads TX dédiés).
- Pile USB MIDI isolée du bus cartouche ; endpoints FS 64 B ; callbacks ISR **courts**.
- Threading ChibiOS propre et nommé : `UIThread`, `cart_tx_thread[*]`, `ButtonsThread`, `displayThread`, `potReaderThread`, `thMidiClk`, `thdMidiUsbTx`, `metronomeThread`.
- **Include guards uniformisés** (pas de `#pragma once`).
- **`cart_link.h`** : header public **minimal** (dépend seulement de `cart_bus.h`).
- **`cart_registry.h`** : **forward-decl** `struct ui_cart_spec_t`.

#### ✅ Correctifs appliqués (Phase 4)

- **Paramètres continus UI** : `int16_t` sur `ui_param_range_t.min/max`.
- **Initialisation** : insertion explicite de `cart_link_init()` après `cart_registry_init()`.
- **USB/MIDI** : ajout de `usb_device_start()`, `midi_init()`, `midi_clock_init()` dans l’ordre d’init avant l’UI.
- **MIDI TX USB** : priorité dédiée `MIDI_USB_TX_PRIO (NORMALPRIO+1)` + **timeout** sur sémaphore d’EP IN (anti-deadlock).
- **Debug UART** : **désactivé** (`DEBUG_ENABLE 0`) pour éviter toute collision avec **MIDI DIN (SD2)**.
- **Menus cycliques (BM)** : passés en **data-driven** via `ui_spec.h::cycles[]` (chargés automatiquement par `ui_controller`).  
  *Ex. XVA1 : BM8 → {FX1, FX2, FX3, FX4}, `resume=true`.*
- **Mode SEQ overlay** : UI interne activée par `SHIFT+SEQ9`, bandeau cart **conservé**, sortie SEQ au **premier BM** (avec ou sans SHIFT).

#### ⚠️ Points à garder en vigilance (court terme)

- Homogénéiser la doc Doxygen sur tous les headers (`@defgroup`, `@ingroup`, `@file`, `@brief`, `@details`).
- Garder `ui_backend.c` côté **bas** (cart/core) pour matérialiser clairement le rôle de pont (header côté `ui/`, implémentation côté `core/` ou `cart/`).

#### ℹ️ Tolérances actuelles (temporaire)

- `ui_task` appelle `cart_registry` pour le **changement de cartouche**. Toléré tant que les **types** côté headers restent neutres (forward-decl).

---

## Invariants de timing et de séparation (rappels)

- **Temps réel :**
  - Horodatage absolu (`systime_t`) pour les événements (futur SEQ).
  - Threads critiques (Clock/MIDI, cart bus) **≥ priorité UI**.
  - **Aucune** opération bloquante en ISR ni dans les threads critiques.

- **Séparation stricte :**
  - UI → *modèle/UI_backend uniquement* (pas de bus, pas d’UART).
  - Cart Link/Bus → *jamais de logique UI* (pas de rendu, pas de boutons).
  - Drivers → *bas niveau*, pas de logique applicative.

---

## Priorités et threads ChibiOS (recommandé)

| Domaine           | Thread                       | Priorité suggérée |
|-------------------|------------------------------|-------------------|
| MIDI / Clock      | `thMidiClk`                  | `NORMALPRIO + 3`  |
| Cart Bus (TX)     | `cart_tx_thread[*]`          | `NORMALPRIO + 2`  |
| MIDI USB (TX)     | `thdMidiUsbTx`               | `NORMALPRIO + 1`  |
| UI                | `UIThread`                   | `NORMALPRIO`      |
| Drivers “polling” | `ButtonsThread`, `potReader` | `NORMALPRIO`      |
| Affichage auto    | `displayThread`              | `NORMALPRIO`      |
| Arrière-plan      | divers                       | `LOWPRIO`         |

> Les callbacks ISR ne doivent **jamais** contenir de logique lourde : utiliser `chBSemSignalI()` / `chEvtSignalI()` puis traiter en thread.

---

## UART cartouche — règles de communication

- Cadence stable (baud configurable, cf. `cart_bus.h`), envoi **DMA + callbacks**.
- Sérialisation compacte :
  - **Format générique** : `[CMD][PARAM_H][PARAM_L][VALUE]`
  - **Profil XVA1** (actuel) : `'s'/'g'` + extension `255` pour `param >= 256`
- **Confinement** : seuls `cart_bus` / `cart_link` peuvent appeler `sdWrite()/sdRead()` pour le **bus cartouche**.
- La pile MIDI USB (`midi.c`) utilise un flux **distinct** (OK).

---

## Préparation SEQ (Model + Engine) — à venir

- `seq_model.[ch]` : modèle **pur** (64 steps × 4 voix, p-locks, micro).
- `seq_engine.[ch]` : Reader → Scheduler → Player (file triée, timestamps absolus).
- **Live record** : capture temps réel (clavier/arp) → écriture atomique sur step courant.
- **API** : le moteur consomme une queue d’événements, pas d’appel direct depuis l’UI.

---

## Feuille de route (séquenceur et UI)

1. **Isoler les types UI cartouche** dans `spec_types.h` (neutre), et ajuster `cart_registry` (déjà amorcé via forward-decl).
2. Implémenter `seq_model.[ch]` (structures, helpers purs).
3. Implémenter `seq_engine.[ch]` (queues, planif, player temps réel).
4. UI séquenceur (`seq_ui.[ch]`) + rendu minimal dédié.
5. Intégration destinations : `SEQ_DEST_INTERNAL` (MIDI) / `SEQ_DEST_CART` (UART).
6. Runtime par voix + STOP propre (OFF garantis, vidage file).
7. Outils de debug : page “SEQ DEBUG”, compteurs d’overflow, scénarios retrigger.

---

## Doxygen & conventions

- `PROJECT_NAME = Brick` ; `USE_MDFILE_AS_MAINPAGE = README.md`
- En-tête Doxygen **dans chaque fichier** :
  ```c
  /**
   * @file ui_controller.c
   * @brief Gestion logique de l'interface utilisateur Brick.
   * @details Traduit les entrées (boutons, encodeurs) en modifications d'état UI
   *          et transmet les changements via ui_backend.
   */
  ```
- Groupes :
  - `@defgroup ui`    (UI Layer)
  - `@defgroup cart`  (Cart Link / Bus / Registry)
  - `@defgroup drv`   (Drivers)
  - `@defgroup core`  (Services transverses)
- **Include guards** : obligatoires, forme `BRICK_<DIRS>_<FILE>_H` (remplace `#pragma once`).
- Préfixes normalisés : `ui_`, `cart_`, `drv_`, `seq_`.

---

## Perspectives

| Domaine            | Objectif                                       | Statut        |
|--------------------|------------------------------------------------|---------------|
| Découplage Cart/UI | Forward-decl & header public minimal côté link | **Fait**      |
| Ranges UI          | `int16_t` sur CONT (overflow évité)            | **Fait**      |
| Guards             | Uniformisation guards explicites               | **Fait**      |
| MIDI USB TX        | Prio dédiée + timeout sém. (anti-blocage)      | **Fait**      |
| Debug UART         | Désactivé (USB CDC à envisager plus tard)      | **Fait**      |
| SEQ Model/Engine   | Implémentation temps réel déterministe         | À implémenter |
| Live record        | Enregistrement temps réel (quantize, overdub)  | À concevoir   |
| UI perf            | Draw batching + cache `dirty`                  | En cours      |
| Sauvegarde         | Dump patterns → flash ou SysEx                 | À définir     |


---

## Phase 5 — Overlays & Modes customs (SEQ / ARP)

Cette phase introduit un **nouveau module** UI ainsi que des règles de navigation/rendu associées, sans modifier l’architecture en couches.

### 1. Nouveau module : `ui_overlay.[ch]`

**Rôle :** centraliser la gestion des **overlays UI** (ex. SEQ, ARP).  
**Invariants :** module UI pur, aucune dépendance bus/UART/driver.

**API principale :**
- `void ui_overlay_enter(ui_overlay_id_t id, const ui_cart_spec_t* spec);`  
  Active un overlay : **sauvegarde** la cartouche/état **réels** et bascule sur `spec`.
- `void ui_overlay_switch_subspec(const ui_cart_spec_t* spec);`  
  Bascule de **MODE ↔ SETUP** sans quitter l’overlay.
- `void ui_overlay_exit(void);`  
  Ferme l’overlay et **restaure** cartouche/état réels.
- `bool ui_overlay_is_active(void);` — `const ui_cart_spec_t* ui_overlay_get_spec(void);`
- `void ui_overlay_set_custom_mode(ui_custom_mode_t mode);` — `ui_custom_mode_t ui_overlay_get_custom_mode(void);`  
  **Flag persistant** indiquant le **dernier mode custom actif** (utilisable par les règles de pas et le rendu).
- `void ui_overlay_prepare_banner(const ui_cart_spec_t* src_mode, const ui_cart_spec_t* src_setup, ui_cart_spec_t* dst_mode, ui_cart_spec_t* dst_setup, const ui_cart_spec_t* prev_cart, const char* mode_tag);`  
  Utilitaire : prépare deux **bannières** d’overlay (MODE/SETUP) en injectant le **nom de la cartouche réelle** et un **tag** (`overlay_tag`, p.ex. `"SEQ"`, `"ARP"`).

**Remarques d’implémentation :**
- L’overlay est **exclusif** : activer un overlay **ferme** le précédent (avec restauration), puis entre dans le nouveau.
- `ui_overlay_exit()` **ne réinitialise pas** le flag persistant du mode custom — il reste disponible pour le rendu et la logique des steps.

### 2. `ui_task` — Raccourcis overlay

- `SHIFT + BS9` → **SEQ overlay**  
  1er appui : MODE ; appuis suivants : **MODE ↔ SETUP**.
- `SHIFT + BS10` → **ARP overlay**  
  1er appui : MODE ; appuis suivants : **MODE ↔ SETUP**.
- **Sortie overlay** : **premier appui** sur **BM1..BM8** (avec/sans SHIFT) → **ferme** l’overlay puis exécute le BM sur la cartouche **réelle**.
- **Changement de cartouche réelle** (`SHIFT+BM1..BM4`) : **ferme** tout overlay actif avant bascule.

Ces comportements remplacent les essais précédents et **stabilisent** la navigation.

### 3. `ui_controller` — Cycles BM déclaratifs

- Les cycles sont définis dans `ui_spec.h::cycles[]` (par cartouche).  
- Chargement/activation : `ui_init()` et `ui_switch_cart()`.  
- `resume=true` : retour au **dernier menu** du cycle ; `resume=false` : **premier menu** du cycle (utile pour overlays).

**Cas standards utilisés :**
- **BM6** : cycle des enveloppes (Filter/Amp/Pitch env).
- **BM7** : cycle des Mods (ex. LFO1/LFO2/MIDI Mod) — bug de delta **corrigé** (les paramètres couvrent leur plage complète).
- **BM8** : FX1→FX2→FX3→FX4 (`resume=true`).

### 4. `ui_spec.h` — Champ optionnel `overlay_tag`

La structure `ui_cart_spec_t` inclut désormais un champ optionnel :
```c
const char* overlay_tag; /* Tag visuel du mode custom actif, ex: "SEQ" */
```
- Valeur `NULL` par défaut — les specs existantes restent **compatibles**.
- Lors de l’utilisation d’un overlay, `ui_overlay_prepare_banner` **renseigne** ce champ pour que le renderer puisse afficher un **label** (ex. “SEQ”) **à droite du nom de cartouche**.

### 5. Rendu (`ui_renderer`) — à venir

- Affichage du `overlay_tag` à droite du `cart_name` lorsque présent dans la spec active.  
- L’API et les contrats **ne changent pas** ; seule la composition visuelle sera ajustée.

---

**Statut de la phase :**  
- Navigation overlay **stabilisée** (SEQ/ARP).  
- Cycles BM **fiabilisés** (BM6, BM7, BM8).  
- **Préparation** du rendu (tag persistant) et des règles de pas en fonction du **dernier mode custom actif**.
