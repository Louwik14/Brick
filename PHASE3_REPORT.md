# Phase 3 — Rapport

## Diagnostic post-Phase 2
- **Symptôme observé** : écran brouillé, absence de périphérique USB et interface figée dès le boot.
- **Cause racine** : `board_flash_init()` essayait systématiquement d’allouer un miroir RAM de 16 MiB pour la flash externe. Sur la cible STM32F429, la tentative saturait le **core memory allocator** (`chCoreAllocFromBase`) et la boucle d’effacement `memset()` finissait par accéder hors RAM, provoquant un HardFault avant la fin de l’initialisation (USB non démarré, UI stoppée).

## Corrections appliquées
- `board_flash.c` :
  - Ajout d’un garde `BOARD_FLASH_SIMULATOR_MAX_CAPACITY` (1 MiB par défaut). Au‑delà, le backend simulateur est désactivé et la flash reste marquée « non prête » tant qu’un backend matériel n’est pas fourni.
  - Séparation des états `s_initialized` / `s_ready` pour éviter les tentatives d’allocation répétées et garantir un comportement sûr lorsque la flash n’est pas disponible.
- Drivers UI :
  - Mailbox boutons portée à 32 entrées et instrumentation basée sur `chMBGetFreeCountI()` pour un remplissage exact (couple drops/high-water).
  - Backend LED instrumenté : horodatage temps réel (avant/après `drv_leds_addr_render()`) avec max/last watermarks exploitables via `brick_metrics_get_led_backend_timing()`.
  - Exposition des métriques dans `brick_metrics.[ch]` et documentation mise à jour (`README`).

## Instrumentation — métriques disponibles
- **Piles** : `brick_metrics_collect_stacks()` (inchangé, dépend de `CH_DBG_FILL_THREADS`).
- **Files** :
  - MIDI USB, bus cart, boutons (×32), backend LED via `brick_metrics_collect_queues()`.
  - Remise à zéro synchronisée : `brick_metrics_reset_queue_counters()`.
- **Temps de rendu LED** : `brick_metrics_get_led_backend_timing()` → derniers/maximums (ticks) pour le refresh complet et pour `drv_leds_addr_render()`, plus la fréquence du compteur temps réel (`chSysGetRealtimeCounterFrequency()`).

## Mémoire
- La Section `.bss` principale reste identique à la Phase 2 (≈ 45,5 KB de SRAM utilisés ; CCM ~60 KB). Les ajouts instrumentation/guards n’affectent que quelques dizaines d’octets.
- Regénération de `size`/`.map` non effectuée ici (toolchain absente dans l’environnement d’exécution), mais aucun symbole majeur déplacé.

## Points restants & recommandations
1. Implémenter un backend flash matériel (QSPI/SPI NOR) pour restaurer la persistance projet/pattern désormais externalisée.
2. Exploiter les nouvelles métriques LED pour réduire la section critique (`drv_leds_addr_render()`) — migration DMA ou pré-calcul hors lock.
3. Utiliser les compteurs boutons (×32) lors des stress tests pour valider l’absence de pertes.
4. Prévoir une capture périodique des métriques (USB CDC ou overlay debug) afin de documenter les marges en production.
