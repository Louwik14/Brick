# Étape 7 — Track Select & Cohérence UI/Engine

## Résumé
- Ajout du mode **Track Select** accessible via **SHIFT + BS11**.
- Nettoyage des transitions LED/Mute pour supprimer le flash rouge lors du retour depuis P-Mute ou "+".
- Synchronisation du moteur, du recorder et du pont LED lors des changements de piste.
- Couverture de régression host avec `tests/ui_mode_transition_tests.c`.

## Track Select (SHIFT+BS11)
- **Entrée** : `ui_track_mode_enter()` affiche le label `track`, publie le mode LED `UI_LED_MODE_TRACK` et fige le shortcut map sur la grille.
- **Grille** : `ui_led_backend_set_track_present()` et `ui_led_backend_set_cart_track_count()` rendent disponibles uniquement les emplacements exposés par les carts (mapping 4×4 fixe).
- **Sélection** : `ui_track_select_from_bs()` relaie l'index BS vers `seq_led_bridge_select_track()` qui :
  - active la piste dans `seq_project_t` ;
  - réinitialise les caches hold/preview/runner ;
  - republie la focus LED via `ui_led_backend_set_track_focus()` ;
  - remappe la cartouche (`cart_registry_switch()`) si nécessaire pour rafraîchir `cart_name`.
- **Sortie** : `ui_track_mode_exit()` restaure le label précédent (`SEQ`, overlay actif ou keyboard) et remet le mode LED courant.

## Correctifs LED / P-Mute
- `ui_led_refresh_state_on_mode_change()` centralise les transitions `SEQ_MODE_DEFAULT/PMUTE/TRACK`.
- `ui_shortcuts` annule toute action overlay/page quand `ctx->track.active` est vrai.
- `ui_led_backend` rend le mode `UI_LED_MODE_TRACK` :
  - pistes disponibles = couleur cartouche ;
  - piste active = vert ;
  - mute conservé en rouge sans flash.
- `ui_mute_backend` reste l'autorité sur les états rouge/preview ; `ui_track_mode_exit()` remet en séquenceur standard.

## Cohérence UI ↔ Engine
- `seq_led_bridge_select_track()` propage l'index vers `seq_engine_runner_attach_pattern()` et `seq_recorder_attach_pattern()`.
- Le focus piste est re-publié dès `_publish_runtime()` pour que `ui_backend` voie `page_count`, `track_count`, `held_mask` à jour.
- Les shortcuts redéfinissent `ctx->track.shift_latched` afin de quitter Track Select dès que SHIFT est relâché + BS11.
- `ui_led_refresh_state_on_mode_change()` est invoqué lors :
  - entrée/sortie P-Mute ;
  - changement de page SEQ ;
  - sélection de piste ;
  - boot initial (`ui_backend_init_runtime()`).

## Mémoire
- Les caches LED (présence piste, focus, file d'évènements) sont en `CCM_DATA` pour ne pas gonfler `.bss`.
- Pas de buffer DMA déplacé ; seules les tables d'état UI sont concernées.

## Tests
- ✅ `make check-host` (inclut `ui_mode_transition_tests`) : vérifie entrée/sortie Track Select, mapping SHIFT+BS11, re-publication du focus piste, et que les shortcuts overlay/page sont bloqués pendant Track Select.
- ⚠️ `make -j4` : échec attendu dans cet environnement (`chibios2111/os/common/ports/ARMv7-M/compilers/GCC/mk/port.mk` absent) — aucune régression ajoutée par l'étape.

## Points ouverts
- Prévoir un rappel visuel côté OLED lors du changement de piste (non demandé dans cette étape).
- Étendre ultérieurement la persistence projet pour mémoriser l'index de piste actif.
