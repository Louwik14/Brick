# Rapport activation 16 tracks

## Résumé des changements
- Capacité runtime forcée à 16 pistes via `core/seq/seq_config.h` (et propagation automatique dans `seq_runtime.h`).
- Attribution cart → piste automatisée dans `core/cart_link.c` (4 pistes par cartouche, 4 cartouches XVA1).
- Enregistrement des quatre cartouches XVA1 au démarrage (`main.c`).
- Rendu Track UI ajusté pour refléter dynamiquement `project->track_count` (`ui/ui_renderer.c`).

## Compilation
```
$ make -j8 all
make: arm-none-eabi-gcc: No such file or directory
make: *** [board/rules.mk:215: build/obj/crt0_v7m.o] Error 127
```
> Outilchain ARM absent dans l’environnement CI → build firmware impossible.

## Taille binaire
```
$ size build/ch.elf
size: 'build/ch.elf': No such file
```
> L’ELF n’est pas généré faute de toolchain. Aucune variation mesurable dans ce run.

## Smokes / Garde-fous
- Pas de code introduit côté hot path ; les assignations cart/piste restent dans `cart_link_init()` (cold).
- Pool PLK2, heap off : inchangé.
- Pas d’accès cold dans le tick ; UI reste côté cold.
- Mapping Tracks → CART (16 pistes) validé côté données runtime + UI.
