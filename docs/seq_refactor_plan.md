# Brick Sequencer — Refactor Plan (2025-04 audit)

## 1. État du dépôt
- **Cible confirmée** : STM32F429 + ChibiOS 21.11.x (voir `agents.md`).
- **Architecture en production** : séparation effective Model / Engine / Runner / UI / Cart.
- **Modules clefs** :
  - `core/seq/seq_model.c` : stockage pattern/steps/voices, flags vert/bleu.
  - `core/seq/seq_engine.c` : lecture pattern → callbacks runner (NOTE ON/OFF, cart).
  - `apps/seq_led_bridge.c` : snapshot runtime, hold preview, classification LED.
  - `apps/seq_recorder.c` + `core/seq/seq_live_capture.c` : capture clavier, durée, micro-timing.
  - `ui/ui_backend.c` : dispatcher UI, shadow global, handlers hold/preview.
  - `ui/ui_led_backend.c` + `ui/ui_led_seq.c` : rendu LED (pages, couleurs, playhead).
- **Tooling actuel** : `make check-host` (tests modèle + runtime hold), `make lint-cppcheck`, compilation STM32 via ChibiOS.

## 2. Cartographie dépendances (haut niveau)
```
clock_manager → ui_task::_on_clock_step → {seq_led_bridge_tick, seq_recorder_on_clock_step,
                                          seq_engine_runner_on_clock_step}
                                      ↓
                             ui_led_seq_update_from_app (couleurs vert/bleu)
```
- `ui_backend_param_changed` → détecte hold → `seq_led_bridge_apply_plock_param` / `seq_led_bridge_apply_cart_param`.
- `ui_keyboard_bridge` → clavier physique → `seq_recorder_handle_note_on_at/off_at` + `ui_backend_note_on/off`. // --- ARP FIX: timestamps synchronisés ---
- `seq_live_capture` → convertit timestamps clavier en p-locks SEQ (note, vélocité, longueur, micro).

## 3. Dette identifiée en avril 2025
1. **API LED orphelines** : `seq_led_bridge_set_plock_mask` et `seq_led_bridge_plock_clear` plus utilisées.
2. **Artefacts hérités** : archives Brick4 + logs dans le dépôt, bruit pour les nouvelles branches.
3. **Documentation** : `docs/seq_refactor_plan.md` et anciens paragraphes mentionnaient encore l’absence de `seq_model`/`seq_engine`.
4. **Tests host** : couverture existante pour hold/preview, mais pas de vérification directe “note_off ≠ All Notes Off global”.

## 4. Plan incrémental (P0→P2)
| Étape | Priorité | Action | Modules | Risque | Vérification |
|-------|----------|--------|---------|--------|--------------|
| P0-1  | P0 | Retirer les artefacts Brick4/logs du dépôt | Racine | Nul | `git status`, `make -j8 all` |
| P0-2  | P0 | Dépublier les API LED orphelines (retirer du header) | `apps/seq_led_bridge.*` | Faible | `make check-host` |
| P1-1  | P1 | Mettre à jour cette feuille de route (réalité vs intention) | `docs/seq_refactor_plan.md` | Nul | Relecture |
| P1-2  | P1 | Étendre `make check-host` : détection `HOST_CC`, tests clavier All Notes Off + LED verts/bleus | `Makefile`, `tests/seq_hold_runtime_tests.c` | Faible | `make check-host` |
| P2-1  | P2 | Commentaires FR pédagogiques sur les handlers hold/preview (séparé) | `ui/ui_backend.c`, `apps/seq_led_bridge.c` | Faible | Build + tests |

## 5. Suivi à moyen terme (post P1)
- Factoriser les helpers UI/LED communs (playhead/page) dans un module partagé.
- Documenter dans Doxygen le flux complet “hold preview → commit → LED update”.
- Ajouter des tests host ciblant `seq_live_capture_plan_event` pour les cas limites (overlap, voix multiples).

