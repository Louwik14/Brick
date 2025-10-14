# Changelog

## 2025-04-18
- Retrait des artefacts hérités (`Brick4_labelstab_uistab_phase4.zip`, `drivers/drivers.zip`, `log.txt`, `tableaudebord.txt`).
- Dépublication des API orphelines `seq_led_bridge_set_plock_mask()` / `seq_led_bridge_plock_clear()`.
- Mise à jour de `docs/seq_refactor_plan.md` et ajout du mémo `docs/HOST_VS_TARGET_RUNTIME.md`.
- Renforcement de `make check-host` (détection de `HOST_CC` côté Windows, nouveau test clavier/LED vert vs bleu).

## 2025-04-19
- `make check-host` lie maintenant `apps/seq_recorder.c` pour rejouer le chemin STM32 : le test `seq_recorder_commits_length_and_led_state` vérifie longueur de note et couleur LED.
- Nettoyage des stubs superflus (`clock_manager.h`) et documentation mise à jour (`docs/HOST_VS_TARGET_RUNTIME.md`).
