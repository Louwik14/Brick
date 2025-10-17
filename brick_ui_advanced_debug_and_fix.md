# Debug avancé UI : P-Mute & Track Mode

## 1. Cause racine – LEDs rouges en PMute

* Les préparations PMute (`s_pmute_prepare[]`) n'étaient pas purgées lors du retour vers SEQ.
* `ui_led_refresh_state_on_mode_change()` se contentait de republier l'état courant, reproduisant les flags obsolètes.
* Le label Track restait vide car la transition n'écrasait pas `s_mode_ctx.overlay_*` ni le banner override.

## 2. Correctifs appliqués

* Ajout de `ui_mute_backend_clear()` pour forcer la remise à zéro des previews sans toucher aux mute réels. 【F:ui/ui_mute_backend.c†L74-L95】
* Nouvelle API `ui_mode_reset_context()` centralisée (appelée depuis toutes les transitions) : purge des holds SEQ, clear PMute, reset track/overlay selon le mode suivant. 【F:ui/ui_mode_transition.c†L44-L78】
* Instrumentation `ui_mode_transition_t` + macro `UI_MODE_TRACE()` : snapshot des transitions et chainage UI → LED → séquenceur. 【F:ui/ui_mode_transition.h†L23-L54】【F:ui/ui_mode_transition.c†L5-L42】
* `ui_led_refresh_state_on_mode_change()` orchestre désormais : begin transition → reset → set LED mode → publish bridge → commit. 【F:ui/ui_backend.c†L169-L208】
* `ui_track_mode_enter()` force un label `TRACK`, désactive les overlays et s'appuie sur le reset central. 【F:ui/ui_backend.c†L214-L224】
* Les overlays/clavier appellent `ui_mode_reset_context()` avant toute bascule pour garantir la sortie du Track mode. 【F:ui/ui_backend.c†L332-L355】

## 3. Nouvelle machine d'état UI

```
SEQ (default)
  ├─ Overlay SEQ / ARP (ui_mode_reset_context → SEQ)
  ├─ Keyboard (overlay tag conservé)
  ├─ Track Select (SHIFT+BS11 → ui_led_refresh_state_on_mode_change(SEQ_MODE_TRACK))
  │     └─ Sortie via reset (overlay/custom) ou SHIFT+BS11
  └─ PMute / Quick Mute (SHIFT+PLUS → SEQ_MODE_PMUTE)
        └─ Commit / Cancel → reset contexte + clear previews
```

* `ui_mode_transition_begin()` capture l'origine et la destination ; chaque couche (`reset`, `led`, `seq`) marque son sync avant `commit`.
* Toute sortie vers SEQ/Track déclenche `ui_mute_backend_clear()` afin d'éviter la propagation de LEDs rouges.

## 4. Hooks de transition

| Étape                     | API                       | Effet principal |
|--------------------------|---------------------------|-----------------|
| Reset contexte           | `ui_mode_reset_context()` | Purge PMute, steps hold, états track/overlay. |
| LED mode & bridge        | `ui_led_refresh_state_on_mode_change()` | Définit `UI_LED_MODE_*`, publie `seq_led_bridge`, rafraîchit LEDs. |
| Transition snapshot      | `ui_mode_transition_*()`  | Logging debug + accès test. |
| Track entry              | `ui_track_mode_enter()`   | Label `TRACK`, overlay neutralisé, transition TRACK. |
| Track exit               | `ui_mode_reset_context()` (SEQ) | Libère le flag `track.active`, restaure label overlay/keyboard. |

## 5. Tests & vérifications

* Nouveau test host `tests/ui_mode_edgecase_tests.c` :
  * Vérifie la purge PMute lors du retour SEQ.
  * Assure l'entrée Track depuis Keyboard (SHIFT+BS11) et la remise à zéro automatique du flag Track. 【F:tests/ui_mode_edgecase_tests.c†L1-L68】
* `make check-host` exécute désormais quatre binaire host (`seq_model`, `seq_hold_runtime`, `ui_mode_transition`, `ui_mode_edgecase`).

Ces changements stabilisent la chaîne `ui_state → ui_mode_transition → ui_led_backend → seq_engine` et offrent une instrumentation fiable pour les diagnostics futurs.
