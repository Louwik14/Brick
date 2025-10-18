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

## 6. Phase 9 : Stabilisation P-Mute / Track / LEDs

* `ui_mode_reset_context()` préserve l’état du bouton `PLUS` lors des transitions vers PMute afin de laisser la bascule QUICK→PMUTE détecter le relâché/reprise de SHIFT. La trace `UI_MODE_TRACE()` expose désormais le mode cible, les drapeaux PMute/Track et le tag overlay courant pour faciliter le debug croisé. 【F:ui/ui_mode_transition.c†L46-L93】
* `ui_led_refresh_state_on_mode_change()` journalise le mode LED retenu et l’étiquette visible, garantissant la cohérence UI ↔ LEDs ↔ séquenceur. 【F:ui/ui_backend.c†L168-L208】
* `ui_track_mode_enter()` et `ui_track_mode_exit()` rafraîchissent explicitement les tags (`_reset_overlay_banner_tags()`) et ne court-circuitent plus la restauration overlay/keyboard lorsque `track.active` est déjà tombé, ce qui supprime la bannière vide et assure la sortie complète du mode. 【F:ui/ui_backend.c†L210-L244】
* Les raccourcis QUICK/PMUTE/EXIT/COMMIT tracent leur contexte (`SHIFT`, `PLUS`, label actif) pour comprendre les séquences d’entrée/sortie et synchronisent l’étiquette après commit/cancel. 【F:ui/ui_backend.c†L467-L514】
* Le test host `test_quick_to_pmute_sequence()` reproduit la combinaison `SHIFT + +` (maintien de `PLUS`, relâche/represse de SHIFT) et garantit la remise à zéro du flag dans `ui_mode_reset_context()` ; le binaire edge-case embarque désormais le stub `ui_model_*`. 【F:tests/ui_mode_edgecase_tests.c†L1-L105】【F:tests/stubs/ui_model_stub.c†L1-L9】【F:Makefile†L294-L299】

## 7. Phase 10 : Overlay Track & PMute — régressions finales

### Diagnostic

* Le mode Track n'initialisait plus sa vitrine lorsqu'on y entrait depuis SEQ : le garde-fou `if (s_mode_ctx.track.active)` court-circuitait l'appel `ui_led_refresh_state_on_mode_change(SEQ_MODE_TRACK)` lors de la première entrée (état `track.active` déjà prépositionné). Résultat : bannière vide et LED restées en mode SEQ. 【F:ui/ui_backend.c†L206-L223】
* En sortie de PMute, `seq_led_bridge_publish()` continuait de marquer les steps comme `muted` en se basant sur `seq_led_runtime_step_t.muted`; `apps/seq_led_bridge.c` recopiant l'état `muted` réel a donc repeint la grille SEQ en rouge alors qu'on venait de quitter le mode. 【F:apps/seq_led_bridge.c†L742-L753】
* Lorsqu'on ré-ouvrait PMute, la LED de la piste réellement mutée restait grise : le backend LED ne conservait pas l'état `s_track_muted[]` faute d'API de lecture côté tests, et une saturation silencieuse de la queue masquait les évènements en rafale. 【F:ui/ui_led_backend.c†L38-L87】

### Correctifs ciblés

* `ui_track_mode_enter()` vérifie désormais **le mode séquenceur courant** avant d'esquiver la réinitialisation, et trace le rafraîchissement forcé. La première entrée dans Track re-synchronise donc bannière et LEDs. 【F:ui/ui_backend.c†L206-L223】
* Le runtime SEQ ne propage plus l'état `muted` de la piste lorsqu'on revient en mode SEQ : `dst->muted` est forcé à `false` et documenté comme “mute visible uniquement en mode MUTE”. 【F:apps/seq_led_bridge.c†L742-L753】
* Le backend LED expose des hooks de test (`ui_led_backend_debug_*`) et journalise les drops. La file a été doublée (64 entrées) pour absorber tous les événements MUTE/PMute d'une rafale SHIFT+PLUS. 【F:ui/ui_led_backend.h†L18-L88】【F:ui/ui_led_backend.c†L20-L336】
* Nouveau stub `drv_leds_addr_stub.c` + en-tête minimal garantissent un état LED accessible côté host, sans importer le driver complet. 【F:tests/stubs/drv_leds_addr_stub.c†L1-L42】【F:tests/stubs/drv_leds_addr.h†L1-L44】

### Validation

* Le binaire host `ui_track_pmute_regression_tests` rejoue l'entrée/sortie Track (SHIFT+BS11) et la séquence QUICK Mute → SEQ → PMute. Les assertions vérifient la bannière, le mode LED actif, l'absence de queue drops et la couleur des pads SEQ. 【F:tests/ui_track_pmute_regression_tests.c†L1-L102】
* `Makefile` compile désormais ce test avec les stubs LED/flash/registry nécessaires pour isoler la pile UI. 【F:Makefile†L307-L320】【F:tests/stubs/board_flash_stub.c†L1-L27】【F:tests/stubs/ui_backend_test_stubs.c†L1-L140】
* `make check-host` exécute le scénario complet ; la trace `[ui-led] set_mode …` / `[ui-mode] …` confirme la chaîne `ui_backend → ui_led_backend` et l'absence de saturation. 【f03272†L1-L73】
