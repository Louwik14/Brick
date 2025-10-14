# Brick — Firmware Documentation

**Brick** est une plateforme matérielle modulaire de contrôle.  
Elle ne produit aucun son : le firmware gère l’interface utilisateur (UI), la communication temps réel vers des **cartouches DSP externes** et, à venir, un **séquenceur polyphonique** (p-locks, micro-timing).

Les cartouches (ex. **XVA1**) sont pilotées via un bus série. Brick agit comme un **cerveau de contrôle** : il lit les entrées (boutons, encodeurs), affiche l’état sur l’OLED et envoie les paramètres mis à jour **sans couplage direct** entre UI et bus (grâce à `ui_backend`).

> Cette page est la documentation générale. Les recommandations de robustesse, l’audit structurel et la feuille de route sont détaillés dans la section **[Points à surveiller & évolutions](#points-à-surveiller--évolutions)**.

---

## 📘 Documentation en ligne

La documentation complète du firmware (générée automatiquement avec **Doxygen**) est disponible ici :  
👉 [https://Louwik14.github.io/Brick/](https://Louwik14.github.io/Brick/)

---

---

## Architecture générale

### Vue en couches (cible)

```[ Application / Modes customs (futur) — **KEY** runtime actif ]
│   (Overlay KEYBOARD via SHIFT+SEQ11 ; label dynamique **KEY ±N** ; contexte persistant dans `ui_mode_context_t`)
│   (Options Page 2 : Note order Natural/Fifths, Chord override ; Omni ON/Off harmonisé avec OFF)
▼
[ UI Layer (task, input, controller, renderer, widgets) ]
│     ├─ ui_task       (thread UI : poll → `ui_backend_process_input` + rendu/refresh LEDs)
│     ├─ ui_shortcuts  (mapping pur → actions raccourcis, aucun side-effect)
│     ├─ ui_keyboard_app (quantization commune OFF/ON ; Chord override ; Note order ; clamp [0..127] ; base C4 ; octave shift)
│     ├─ ui_keyboard_ui  (menu Keyboard p2 : Note order, Chord override)
│     ├─ kbd_input_mapper (SEQ1..16 → notes/chords app)
│     ├─ ui_model     (tag overlay courant : **KEY**/SEQ/ARP ; label dynamique conservé)
│     ├─ ui_spec      (specs des menus/cart ; overlay_tag=NULL pour KEY → tag côté modèle)
│     └─ ui_backend   (pont neutre vers cart/MIDI ; leds via ui_led_backend)
│
▼
[ core/spec/cart_spec_types.h ]   ← types neutres (menus/pages/params, bornes)
│
...
▼
[ ChibiOS HAL 21.11.x ]```

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
| `apps/`               | Apps utilitaires / démos                                                           | `metronome.*`, `ui_keyboard_app.*`, `kbd_chords_dict.*`, `kbd_input_mapper.*`, `ui_keyboard_bridge.*`                       |
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


                 [ core/spec/cart_spec_types.h ]  ← types neutres (specs Cart)
                               │
                               └──(activation cart / build des menus via ui_spec)──┐
                                                                                    │
[ Entrées physiques : boutons / encodeurs / pots ]                                  │
│                                                                                   │
▼                                                                                   │
ui_task  (tick: scan entrées + logique périodique + 60 FPS rendu)                   │
│                         ├─ (mode KEYBOARD) SEQ1..16 → kbd_input_mapper → ui_keyboard_app → ui_backend (MIDI)
│                                                                                   │
├─ ui_input  ── events ──►  ui_controller  ──►  (ui_backend)  ──►  cart_link/shadow/registry ──► cart_bus (UART)
│                         │                    (routage Cart/MIDI/interne)
│                         └── écrit l’état UI (ui_model : menu/page/vals, tag custom persistant "SEQ"/"ARP")
│
└─ ui_renderer  ──►  drv_display  ──►  OLED
      (lit ui_model + ui_spec ; bandeau : #ID inversé à gauche,
       cart name + mode 4×6 empilés, titre centré dans le cadre à coins ouverts)

```

- **Aucun accès bus/UART depuis l’UI** : tout passe par `ui_backend`.
- Les valeurs envoyées aux cartouches sont maintenues côté firmware dans un **shadow register** (lecture possible dans l’UI pour le rendu).
- `ui_input.h` expose des **événements UI neutres** (`btn_id`, `btn_pressed`, `encoder`, `enc_delta`) ; la dépendance aux `drv_*` est confinée à `ui_input.c`.
- `ui_task.c` utilise `ui_input_shift_is_pressed()` pour les combinaisons (ex. SHIFT).

---

## Séquence d’initialisation & boucle principale

Ordre d’initialisation recommandé (implémenté dans `main.c`) :

> Remarque : `ui_backend_init_runtime()` initialise le label de mode à « SEQ ». En cas d'appel anticipé du renderer, `ui_backend_get_mode_label()` renvoie toujours un libellé valide (fail-safe « SEQ »).

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

#### État global du **mode custom actif** (tag persistant)

- Le backend expose `ui_backend_get_mode_label()` pour récupérer le label affiché dans le bandeau ("SEQ", "ARP", "KEY±n", "MUTE", etc.).
- Le modèle conserve un **tag texte persistant** du dernier mode custom actif pour compatibilité (synchronisé par le backend) :
  ```c
  void        ui_model_set_active_overlay_tag(const char *tag);
  const char* ui_model_get_active_overlay_tag(void);
  ```
- **Valeur par défaut** : "SEQ". `ui_backend_get_mode_label()` applique un fail-safe identique lors du tout premier rendu.


#### Mode **KEYBOARD** (runtime musical, Phase 6½ — *Orchid-inspired*)

- **Vitrine** : `ui_keyboard_ui.c/.h` (déjà en place).
- **App runtime** : `ui_keyboard_app.c/.h` — empile les notes/accords selon *Root/Gamme* ; envoi via `ui_backend` (MIDI) ; vélocité par défaut **100** ; canal **1** (0-based).
- **Mapper d’entrées** : `kbd_input_mapper.c/.h` — traduit `SEQ1..16` en **note/chord actions**, détecte les combinaisons **Chord+Note** (ordre libre), applique le split **Omnichord** (ON/OFF).
- **Bridge UI** : `ui_keyboard_bridge.c/.h` — lit `Root/Gamme/Omnichord` via `ui_backend_shadow_get(UI_DEST_UI|idLocal)`, pousse immédiatemment dans l’app + mapper + LEDs, et route les notes via **chemin direct** `ui_backend_note_on/off()`.
- **Règles d’Omnichord** (LEDs & jeu) :
  **OFF** — *layout scalaire* :
  • SEQ1..8 = **octave haute** de la gamme (7 notes) + **SEQ8** = octave haute de la **root** ;
  • SEQ9..16 = **octave basse** (même mapping).
  **ON** — *split Orchid-like* :
  • **Chords area** = SEQ1..4 & SEQ9..12 (8 qualités/qualif. d’accords) ;
  • **Notes area** = SEQ5..8 & SEQ13..16 = 7 notes de la gamme + **SEQ16** = octave haute de la **root**.
  > *Pas de strum/arp ici* : ces fonctions seront gérées par le **mode ARP** futur.



**Mises à jour (2025‑10)**  
- **Octave Shift (+/−)** : ±12 demi‑tons, bornes `[-4..+4]`, point zéro **C4 (60)** ; actif si **KEY** est le contexte (overlay visible ou non). Label bandeau : **`KEY`**, **`KEY +N`**, **`KEY -N`**.  
- **Quantisation commune** : en **Omnichord ON**, accords/arpèges passent par la **même quantisation d’échelle** que OFF (sauf si *Chord override* = ON). **Clamp [0..127]** systématique.  
- **Page 2 (Keyboard)** : `Note order = Natural / Circle of Fifths` (cycle root, +7, +14… ; 12 pas, dernier pad = **+12**). `Chord override` permet d’autoriser les **accidentals** en Omni ON.  
- **Mapping** : rangée basse = 0, rangée haute = **+12** ; ordre *Fifths* appliqué **avant** transpose/quantize.
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

#### Cadre du **titre de menu** (coins ouverts, centrage interne)

- Le titre de menu peut être affiché **dans un cadre** à **coins ouverts** (esthétique : **pas de pixel aux 4 coins**).  
- Le **centrage du titre** se fait **à l’intérieur du cadre** (et non plus entre cart/tag et la zone note).
- La position et la taille du cadre sont **pilotées par 4 constantes** pour faciliter le tâtonnement :
  ```c
  #define MENU_FRAME_X   34   /* position X du cadre titre */
  #define MENU_FRAME_Y    0   /* position Y du cadre titre */
  #define MENU_FRAME_W   60   /* largeur du cadre titre    */
  #define MENU_FRAME_H   12   /* hauteur du cadre titre    */
  ```
- Rendu typique (extrait) :
  ```c
  /* 1) Cadre à coins ouverts */
  draw_rect_open_corners(MENU_FRAME_X, MENU_FRAME_Y, MENU_FRAME_W, MENU_FRAME_H);

  /* 2) Centrage du texte **dans** le cadre */
  int tw = text_width_px(&FONT_5X7, title);
  int x  = MENU_FRAME_X + (MENU_FRAME_W - tw) / 2;
  int y  = MENU_FRAME_Y + (MENU_FRAME_H - FONT_5X7.height) / 2;
  drv_display_draw_text_with_font(&FONT_5X7, (uint8_t)x, (uint8_t)y, title);
  ```
- **Invariant respecté** : ce comportement reste purement **rendu**, sans logique d’état dans le renderer.


- Transforme l’état logique en pixels via `drv_display_*`.
- **Source de vérité** : `ui_get_state()` et `ui_resolve_menu()` (si cycle BM actif, le menu résolu est celui du cycle).
- **Bandeau supérieur — mis à jour** :
  - Le **nom de cartouche** est suivi d’un **label capsule** pour le **mode custom actif**.
  - L’ordre d’affichage est : `CartName` + *(éventuel)* `overlay_tag` → **Titre du menu centré** → Zone note/BPM/pattern.
  - Le **centrage du titre** se fait **entre la fin du bloc** `CartName + overlay_tag` et le **début de la zone note** (fenêtrage dynamique, sans chevauchement).
  - Le renderer **ne contient aucune logique d’état** : il lit `cart->overlay_tag` **ou**, à défaut, `ui_model_get_active_overlay_tag()` pour afficher le mode actif persistant (ex.: "SEQ").

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

- **Shadow UI local** : `ui_backend.c` maintient désormais un **shadow** pour les IDs `UI_DEST_UI` (en plus du shadow cartouches). Ainsi, les changements **Omnichord/Gamme/Root** de la vitrine sont lisibles instantanément par le bridge (`ui_backend_shadow_get(UI_DEST_UI|idLocal)`).
- **APIs directes de note** (chemin court, latence minimale) :
  `void ui_backend_note_on(uint8_t note, uint8_t vel);`
  `void ui_backend_note_off(uint8_t note);`
  `void ui_backend_all_notes_off(void);`
  → implémentées dans `ui_backend.c` ; routent vers `midi.c` (**MIDI_DEST_BOTH**, canal par défaut **0**). Le **panic** utilise **CC#123**.


---

## Cartouches : bus, link, proto, registry

### Types neutres de spécification (`core/spec/cart_spec_types.h`)

- Fourni par `core/spec/cart_spec_types.h`, ce header définit des **types purement descriptifs** et **neutres** pour les cartouches :  
  `cart_param_spec_t`, `cart_page_spec_t`, `cart_menu_spec_t`, `cart_spec_t` ainsi que les bornes (`CART_MAX_*`).  
- **Objectif** : servir de **pont de types** entre la couche **Cart** (registry/link) et la couche **UI** (ui_spec/ui_renderer) **sans dépendance circulaire** ni logique embarquée.
- **Contenu** : uniquement des **structures** et **constantes** ; **aucune logique** ni dépendance fonctionnelle.
- **Utilisation attendue** :
  - côté Cart : déclare les **specs runtime** (menus, pages, paramètres) remontées par `cart_registry` ;
  - côté UI : `ui_spec.h` peut **mapper/convertir** ces types neutres vers les structures UI si nécessaire, ou les consommer directement pour construire les menus/pages.
- **Doxygen** : le fichier appartient au groupe `@ingroup cart` et expose le sous-groupe `@defgroup cart_spec_types`.


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
- `drv_leds_addr.*` : LEDs WS2812/SK6812, **rendu atomique** depuis `ui_led_backend_refresh()` → `drv_leds_addr_render()`.
- `drv_display.*` : SSD1309 ; thread d’auto-refresh optionnel.

Façade unique : `drivers_init_all()` et `drivers_update_all()` dans `drivers.c/.h`.  
> ⚠️ **Les LEDs sont rendues depuis le thread UI** via `ui_led_backend_refresh()` → `drv_leds_addr_render()` ;
> **ne pas** appeler `drv_leds_addr_update()` depuis `drivers_update_all()` (évite les races).

---

## Horloge / MIDI / Clock
- `clock_manager.[ch]` : publie un **index de pas absolu** (0..∞). `ui_task` le forwarde au backend via `UI_LED_EVENT_CLOCK_TICK` (sans modulo 16).  
- `ui_led_backend` relaie cet index au renderer **SEQ** (`ui_led_seq_on_clock_tick()`), qui applique le modulo sur `pages×16` et rend le **pas courant** stable (LED pleine).

**Nota (2025‑10‑13)** — Le renderer SEQ met en œuvre un **latch `has_tick`** : le playhead n’est affiché qu’à partir du **premier tick** après PLAY, évitant tout effet de double allumage au redémarrage.


- `midi_clock.[ch]` : générateur **24 PPQN** (GPT3 @ 1 MHz), ISR courte (signal), thread **`NORMALPRIO+3`**, émission F8 et callbacks précis.
- `midi.[ch]` : pile MIDI **class-compliant** (EP1 OUT / EP2 IN, **64 B**), **mailbox non bloquante** pour TX, **chemin rapide** pour Realtime (F8/FA/FB/FC/FE/FF).
  - **Chemin Keyboard** : `ui_backend_note_on/off()` → `midi_note_on/off()` avec `MIDI_DEST_BOTH`, canal **0**. Vélocité par défaut **100**.
  - **All Notes Off** : émis via **CC#123** (`midi_cc(..., 123, 0)`).
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

> **Latence UI** : réglée par `ui_task.c` (poll 2 ms + yield 1 ms). Éviter d'augmenter la priorité UI pour ne pas affamer le scan boutons.
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

## Fondations SEQ (Model + Engine + Capture)

- `seq_model.[ch]` : modèle **pur** (64 steps × 4 voix, p-locks, micro).
- `seq_engine.[ch]` : Reader → Scheduler → Player (file triée, timestamps absolus).
- `seq_live_capture.[ch]` : façade live record → calcule quantize/strength, micro-offset et planifie la mutation sans toucher au modèle.
- **Live record** : capture temps réel (clavier/arp) → mutation pattern à implémenter (placeholder de planification prêt).
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

#### Correctif navigation overlay (2025‑10‑13)

- **Rebuild déterministe** des overlays : à chaque `enter/exit/switch_subspec`, l’overlay **réinitialise** `cur_menu=0` / `cur_page=0`, **publie** le `overlay_tag` si présent, et **force** un `ui_mark_dirty()` pour éviter tout état « fantôme ».
- **BM1..BM8 en sortie d’overlay** : le contrôleur (`ui_controller`) **ferme d’abord** tout overlay actif (`ui_overlay_exit()`), **puis** traite le bouton menu sur la **cart réelle** restaurée.  
  → Plus de menus vides ni de cycles MODE/SETUP inattendus après un mode custom.


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
- `ui_overlay_exit()` **ne réinitialise pas** le flag persistant du mode custom — il reste disponible pour le rendu et la logique des steps (le **label reste affiché** dans le bandeau, même hors écran du mode).

### 2. `ui_task` — Raccourcis overlay

- `SHIFT + BS9` → **SEQ overlay**  
  1er appui : MODE ; appuis suivants : **MODE ↔ SETUP**.
- `SHIFT + BS10` → **ARP overlay**  
  1er appui : MODE ; appuis suivants : **MODE ↔ SETUP**.
- **Sortie overlay** : **premier appui** sur **BM1..BM8** (avec/sans SHIFT) → **ferme** l’overlay puis exécute le BM sur la cartouche **réelle**.
- **Changement de cartouche réelle** (`SHIFT+BM1..BM4`) : **ferme** tout overlay actif avant bascule.

Ces comportements remplacent les essais précédents et **stabilisent** la navigation.




- **SHIFT+SEQ11 (KEYBOARD)** : si **KEY** est déjà affiché, l’action **quitte puis rouvre**
  la bannière pour **préserver `cart_name`** et reconstruire le **label** (`KEY ±N`).
- **Contexte persistant** : un flag runtime `s_keys_active` conserve l’état **KEY actif**
  hors overlay ; à la **sortie de MUTE/PMUTE**, **LEDs KEYBOARD + label** sont restaurés si ce flag est vrai.
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

### 5. Rendu (`ui_renderer`) — **implémenté**

- Affichage du **mode custom actif** (*overlay_tag*) **en 4×6 non inversé**, sous le **nom de cartouche** (4×6 non inversé).
- Si la spec active ne fournit pas de `overlay_tag`, le renderer utilise `ui_backend_get_mode_label()` (dernière valeur gérée par le backend, **par défaut « SEQ »** au démarrage).
- Le **titre du menu** est **centré dans un cadre** à coins ouverts (voir *Rendu (`ui_renderer.c`)*). 
- Invariants respectés : aucune logique d’état dans le renderer ; pas d’accès bus/driver hors `drv_display`/primitives.

---

**Statut de la phase :**  
- Navigation overlay **stabilisée** (SEQ/ARP).  
- Cycles BM **fiabilisés** (BM6, BM7, BM8).  
- **Préparation** du rendu (tag persistant) et des règles de pas en fonction du **dernier mode custom actif**.


---
## 📘 ANNEXE : Mise à jour Phase 5

- `ui_shortcuts.c` : Couche de mapping pure (évènement → `ui_shortcut_action_t`), sans effets secondaires.
- `ui_backend.c` : Conserve le contexte `ui_mode_context_t`, applique les actions (mute, overlays, transport) et publie les tags.
- `ui_task.c` : Simplifié — délègue désormais tous les événements à `ui_backend_process_input()` et se concentre sur le rendu.
- `ui_overlay.c` : Conserve la logique d’ouverture/fermeture et de bannière, appelée depuis le backend.
- `ui_controller.c` / `ui_model.c` : Inchangés, découplés et stables.
- `ui_renderer.c` : Rendu prioritaire par `overlay_tag` > `model_tag`, permettant un affichage correct des états MUTE/PMUTE.
- `cart_registry.c` : Sert de registre déclaratif de specs pour les “apps custom” (SEQ, ARP, FX, etc.).

✅ Architecture validée sans dépendance circulaire.

📦 Prochaine étape : création des futures UIs custom (`ui_fx_ui`, `ui_drum_ui`, etc.) sur le modèle SEQ/ARP.

---

## ✅ Mise à jour — **Phase 6**

### 🔧 Mise à jour (2025‑10‑13) — Cohérence **SEQ UI/LED** (Elektron‑like)

- **Suppression totale** du focus violet (*ancien « P‑Lock hold visuel »*).  
  Les steps maintenus **ne changent plus de couleur**.
- **Priorité des états LED SEQ** (par step) :  
  **Playhead (blanc)** ▶ **Param‑only (bleu)** ▶ **Active/Recorded (vert)** ▶ **Off**.
- **Param‑only = bleu** : un step **P‑Lock sans note** (toutes vélocités = 0, au moins un param locké) s’affiche **bleu**.
- **Hold / Preview P‑Lock** : le maintien d’un ou plusieurs steps sert **uniquement** à éditer les P‑Locks au(x) step(s) sélectionné(s) ;  
  **aucune couleur spécifique** n’est rendue pendant le maintien. Le masque UI de preview est posé **à l’appui** et retiré **au relâchement**.
- **Quick Step / Quick Clear** : tap court **toggle** immédiatement l’état du step (**on/off**) — comportement inchangé.
- **Playhead stable** : latch anti‑double (premier tick post‑PLAY) pour éviter l’allumage simultané *playhead+step précédent* lors d’un redémarrage.
- **Threading / découplage** : `ui_led_seq` reste mis à jour **uniquement** via `ui_led_backend` (aucune dépendance à `clock_manager` dans le renderer).
 (UIs custom + LEDs adressables)

**Ajouts Phase 6½ (runtime Keyboard)**

- `apps/ui_keyboard_app.c/.h` : moteur notes/accords (inversions/extensions inspirées Orchid), API claire (`*_note_on/off`, `*_chord_on/off`, `*_all_notes_off`).
- `apps/kbd_chords_dict.c/.h` : dictionnaire d’accords (intervalles relatifs root) + utilitaires de transposition par *Gamme/Root*.
- `apps/kbd_input_mapper.c/.h` : mapping `SEQ1..16` → actions note/accord + détection de combinaisons **Chord+Note** (ordre libre).
- `apps/ui_keyboard_bridge.c/.h` : lecture **shadow UI** (Root/Gamme/Omni) → app+mapper+LEDs ; **émission directe** via `ui_backend_note_on/off()`.
- `ui/ui_backend.c` : ajout **shadow UI** (espace `UI_DEST_UI`) + APIs **NoteOn/Off/AllOff** + PANIC via **CC#123** ; routage `MIDI_DEST_BOTH`, canal par défaut **0**.
- `ui/ui_task.c` : latence entrée **réduite** (poll 2 ms, priorité **NORMALPRIO**, yield 1 ms), synchro **à chaque itération** vers le bridge ; routing **SEQ1..16** vers `kbd_input_mapper_process(...)`.


Cette section récapitule les ajouts réalisés en Phase 6, sans modifier l’architecture de la Phase 5.

### Nouveaux modules
- `ui/led/`
  - `ui_led_backend.c/.h` : **observateur passif** de l’UI (aucune logique LED dans `ui_task` / `ui_controller` / `ui_shortcuts`). Pilote `drv_leds_addr` (format **GRB**).
  - `ui_led_palette.h` : palette centralisée des couleurs (C1..C4, REC, Playhead, Keyboard/Omnichord).

  - `ui_led_seq.c/.h` : **renderer SEQ** (playhead absolu, pages, priorités d’état, sans dépendre de `clock_manager`).
- `ui/seq/`
  - `seq_led_bridge.c/.h` : **pont SEQ ↔ renderer** (pages, P-Lock mask, publication snapshot, total_span `pages×16`).
- `ui/customs/`
  - `ui_keyboard_ui.c/.h` : **vitrine UI KEYBOARD** (menu unique **Mode** avec 4 paramètres : *Gamme*, *Root*, *Arp On/Off*, *Omnichord On/Off*).

- `apps/`
  - `ui_keyboard_app.c/.h`, `kbd_chords_dict.c/.h`, `kbd_input_mapper.c/.h`, `ui_keyboard_bridge.c/.h` : **app Keyboard** (runtime), dictionnaire d’accords, mapper SEQ→actions, bridge UI↔app↔backend.

### Raccourcis & overlays
- **SHIFT + SEQ11** → **KEYBOARD** (overlay vitrine).  
  - Mise en place d’un **banner clone** : le nom affiché reste celui de la **cart active** (ex. *XVA1*), et le tag court `"KEY"` apparaît à droite (comme `"SEQ"` / `"ARP"`).
- **MUTE actif** (QUICK ou PMUTE) : **tous les overlays sont bloqués** (aucun `SHIFT+SEQx` ne s’active).

### Comportement LEDs (unifié par `ui_led_backend`)
- **REC** : OFF par défaut, **ROUGE** quand actif.
- **MUTE** : les 16 steps ne s’allument **que** en mode MUTE.  
  - Track **mutée** → **rouge** (MUTE/PMUTE sans distinction visuelle).  
  - Track **active** → **couleur de sa cartouche** (C1=bleu, C2=jaune, C3=violet, C4=cyan).  
  - **Aucun chenillard** en MUTE (pas d’accent tick).
- **KEYBOARD** (bleu froid) :  
  - **Omnichord OFF** : **layout scalaire** ; SEQ1..8 = octave **haute** (bleu fort), SEQ9..16 = octave **basse** (bleu atténué).  
  - **Omnichord ON** :  
    - **Chords area** : SEQ1..4 & SEQ9..12 → **8 couleurs distinctes** (palette dédiée).  
    - **Notes area**  : SEQ5..8 & SEQ13..16 → **bleu** (7 notes de la gamme + **SEQ16** = octave haute de la root).
- **SEQ** (séquenceur) :  
  - **Param‑only = bleu**, **Active = vert**, **Playhead = blanc**, **Off = éteint**.

  - **Playhead absolu** qui avance sur **toutes les pages** (`pages × 16`), **sans auto-changer** la page visible.  
  - **Affichage stable** : le pas courant est **allumé plein** (pas de pulse).  
  - **Pages** : `+`/`−` (sans SHIFT) changent la **page visible** ; `SHIFT + (+/−)` = **MUTE/PMUTE** (prioritaire).  
  - **Longueur** : défaut **4 pages** (64 pas) ; ajustable via `seq_led_bridge_set_max_pages(N)`.



### Hook encodeur → LEDs (mise à jour immédiate)
- Dans `ui_controller.c`, un hook met à jour **instantanément** le rendu LEDs lorsque le paramètre **Omnichord (Off/On)** de la vitrine **Keyboard** change :  
  `ui_led_backend_set_mode(UI_LED_MODE_KEYBOARD);`  
  `ui_led_backend_set_keyboard_omnichord(on_off);`

### Arborescence — compléments
- Ajouts par rapport à la table existante :
  - `ui/led/` → `ui_led_backend.*`, `ui_led_palette.h`
  - `ui/customs/` → `ui_keyboard_ui.*`

> ℹ️ L’**ordre d’initialisation** reste identique ; la **boucle principale** continue d’appeler `drv_leds_addr_render()` pour rafraîchir les LEDs. `ui_led_backend` ne bloque pas le flux principal.



**Depuis 2025‑10 :** capture des boutons **PLUS/MINUS** pour piloter l’**octave shift**
lorsque **KEY** est le contexte actif (overlay visible ou non) ; mise à jour du
**label** bandeau en conséquence.
- `ui/customs/` → `ui_keyboard_ui.*` (menus Keyboard, page 2)
- `apps/` → `ui_keyboard_app.*`, `kbd_input_mapper.*`, `kbd_chords_dict.*`
- `ui/` → `ui_shortcuts.*` (mapping neutre → actions), `ui_backend.*` (contexte UI + effets secondaires)
