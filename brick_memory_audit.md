# Brick Memory & CPU Audit (STM32F429 / ChibiOS 21.11)

## 1. Diagnostic synthétique
### RAM
- **Surallocation UI overlay** : six copies complètes de `ui_cart_spec_t` (≈8.2 KB chacune) sont conservées dans `ui_backend.c` pour les bannières SEQ/ARP/Keyboard, soit ~50 KB de `.bss` à elles seules.【F:ui/ui_backend.c†L59-L115】【F:ui/ui_spec.h†L200-L205】
- **Pattern SEQ unique volumineux** : l’état `seq_led_bridge_state_t` empile le pattern complet (64 steps × 4 voix × 24 p-locks), le runtime LED et les caches de hold, pour ≈27.1 KB. Il duplique aussi 16 buffers « hold » contenant chacun un `seq_model_step_t` (≈6.8 KB additionnels).【F:apps/seq_led_bridge.c†L48-L83】
- **Caches UI et MIDI extensifs** : la table `s_ui_shadow[512]` (2 KB) et la mailbox USB MIDI (512 messages, 2 KB) occupent une portion notable de la SRAM principale.【F:ui/ui_backend.c†L569-L614】【F:midi/midi.c†L72-L139】
- **Infra cartouche** : chaque port cartouche garde une mailbox de 64 messages et un pool de 64 commandes, plus quatre piles de thread à 512 octets (≈4 KB au total avant même les structures `mailbox_t`/`memory_pool_t`).【F:cart/cart_bus.c†L60-L143】
- **Threads et framebuffers** : plusieurs `THD_WORKING_AREA` (UI 1 KB, MIDI USB 512 B, Cart TX 4×512 B, etc.) et les buffers vidéo/LED restent dans SRAM classique, sans exploitation de la CCM 64 KB.【F:ui/ui_task.c†L45-L107】【F:midi/midi.c†L103-L140】【F:drivers/drv_display.c†L44-L118】【F:drivers/drv_leds_addr.c†L24-L158】

### CPU
- **Hold / preview SEQ coûteux** : `_hold_update()` balaie chaque page de 16 pas et agrège 20 paramètres par pas, avec copies intégrales de `seq_model_step_t`. L’opération est déclenchée à chaque changement de mask ou mutation, donc dans le thread UI.【F:apps/seq_led_bridge.c†L375-L520】
- **Planification audio** : `seq_engine_process_step()` reconstruit des événements NOTE ON/OFF + P-lock pour chaque voix et utilise le scheduler mutexé sur chaque tick (24 PPQN → 64 steps), ce qui devient critique en multi-voix/multi-tracks.【F:core/seq/seq_engine.c†L241-L420】
- **Drivers série** : `cart_tx_thread` et `midi_usb` reposent sur des boucles d’attente actives avec mailboxes et `sdWrite`/`usbStartTransmitI`, sensibles aux priorités Chibi et susceptibles de bloquer si les buffers sont saturés.【F:cart/cart_bus.c†L93-L143】【F:midi/midi.c†L103-L149】
- **Rendu périodique** : `ui_led_backend_refresh()` et `drv_leds_addr_render()` balayent toutes les LED à chaque frame UI (~1 kHz pour certaines boucles), demandant des optimisations en cas de montée en fréquence ou d’élargissement des pads.【F:ui/ui_led_backend.c†L200-L274】【F:drivers/drv_leds_addr.c†L140-L158】

## 2. Top 10 symboles `.bss` (approx.)

| Rang | Symbole / Zone | Taille estimée | Fichier | Commentaire |
| --- | --- | --- | --- | --- |
| 1 | `seq_led_bridge_state_t g` | ≈27.1 KB | `apps/seq_led_bridge.c` | Pattern SEQ + runtime + hold view。【F:apps/seq_led_bridge.c†L48-L58】 |
| 2 | `s_seq_mode_spec_banner` | ≈8.4 KB | `ui/ui_backend.c` | Copie mutable de la spec SEQ MODE.【F:ui/ui_backend.c†L59-L85】 |
| 3 | `s_seq_setup_spec_banner` | ≈8.4 KB | `ui/ui_backend.c` | Copie mutable SEQ SETUP.【F:ui/ui_backend.c†L59-L85】 |
| 4 | `s_arp_mode_spec_banner` | ≈8.4 KB | `ui/ui_backend.c` | Copie mutable ARP MODE.【F:ui/ui_backend.c†L59-L85】 |
| 5 | `s_arp_setup_spec_banner` | ≈8.4 KB | `ui/ui_backend.c` | Copie mutable ARP SETUP.【F:ui/ui_backend.c†L59-L85】 |
| 6 | `s_kbd_keyboard_spec_banner` | ≈8.4 KB | `ui/ui_backend.c` | Copie mutable Keyboard overlay.【F:ui/ui_backend.c†L59-L85】 |
| 7 | `s_kbd_arp_config_spec_banner` | ≈8.4 KB | `ui/ui_backend.c` | Copie mutable sous-menu arpégiateur.【F:ui/ui_backend.c†L59-L85】 |
| 8 | `g_hold_slots[16]` | ≈6.9 KB | `apps/seq_led_bridge.c` | 16 copies complètes de `seq_model_step_t` pour le mode hold.【F:apps/seq_led_bridge.c†L62-L83】 |
| 9 | `midi_usb_queue[512]` | 2 KB | `midi/midi.c` | File circulaire 512 paquets USB MIDI.【F:midi/midi.c†L72-L139】 |
| 10 | `s_ui_shadow[512]` | 2 KB | `ui/ui_backend.c` | Cache local des paramètres UI.【F:ui/ui_backend.c†L569-L614】 |

> **Autres postes >1 KB** : `waCartTx[CART_COUNT]` (~2 KB), `s_port[CART_COUNT]` (~2 KB), `waUI` (1 KB), `drv_display` framebuffer (1 KB), piles `waMidiUsbTx` (512 B) / `waMidiClk` (256 B), buffer police 4×6 (380 B).【F:cart/cart_bus.c†L60-L143】【F:ui/ui_task.c†L45-L107】【F:drivers/drv_display.c†L44-L118】【F:midi/midi.c†L103-L140】【F:core/midi_clock.c†L30-L112】【F:ui/font.c†L51-L97】

## 3. Structures prioritaires à refactoriser
1. **Overlays UI (`ui_cart_spec_t`)** : remplacer les copies complètes par des vues légères (pointeurs vers les `const ui_cart_spec_t` d’origine ou structures compressées).【F:ui/ui_backend.c†L59-L115】
2. **`seq_led_bridge_state_t`** : séparer le pattern partagé (modèle) du cache UI, et stocker les snapshots hold sous forme de deltas/index plutôt que de steps complets.【F:apps/seq_led_bridge.c†L48-L83】【F:apps/seq_led_bridge.c†L375-L520】
3. **`seq_model_step_t` / `seq_model_pattern_t`** : réduire les tailles (bitfields pour les flags, compaction des p-locks, table de voix compressée) en prévision d’un project 16 tracks × 4 voix.【F:core/seq/seq_model.h†L108-L147】
4. **Caches runtime UI (`s_ui_shadow`)** : limiter la capacité ou basculer vers une table LRU compacte (p. ex. 128 entrées) avec sérialisation différée.【F:ui/ui_backend.c†L569-L614】
5. **Queues série (`cart_port_t`, `midi_usb_queue`)** : recalibrer les profondeurs et explorer le déplacement en CCM lorsque le périphérique n’exige pas l’AXI SRAM.【F:cart/cart_bus.c†L60-L143】【F:midi/midi.c†L72-L139】
6. **Stacks threads** : profiler `waUI`, `waCartTx`, `waDisplay`, `waButtons`, `waMidiUsbTx`, `waMidiClk` pour réduire les marges ou migrer ceux sans DMA vers CCM.【F:ui/ui_task.c†L45-L107】【F:cart/cart_bus.c†L91-L142】【F:drivers/drv_display.c†L200-L240】【F:drivers/drv_buttons.c†L90-L140】【F:midi/midi.c†L103-L149】【F:core/midi_clock.c†L30-L112】

## 4. Plan d’action multi-étapes
1. **Étape 1 – Audit mémoire & CPU (terminé)**
   - Consolider les métriques ci-dessus, mesurer la taille réelle via `arm-none-eabi-size` et capturer une baseline `.map`.

2. **Étape 2 – Migration vers CCM / const**
   - Transformer les overlays `ui_cart_spec_t` en structures `const` partagées ou en vues réduites ; déplacer les caches volumineux (UI shadow, queues série, piles de threads non DMA) vers la CCM RAM via attributs (`__attribute__((section(".ccmram")))`) en vérifiant la compatibilité périphérique.
   - Ajuster les profondeurs (`UI_BACKEND_UI_SHADOW_MAX`, `MIDI_USB_QUEUE_LEN`, `CART_QUEUE_LEN`) avec instrumentation des taux de drop.【F:ui/ui_backend.c†L569-L614】【F:midi/midi.c†L72-L149】【F:cart/cart_bus.c†L60-L143】

3. **Étape 3 – Compression du modèle SEQ**
   - Redéfinir `seq_model_step_t` : compacter les p-locks (listes à taille variable, tables séparées par domaine), compresser les voix (bitfields état/velocity), et isoler les offsets globaux.
   - Modifier `seq_led_bridge` pour ne stocker que des références (`step_index`, `voice_mask`, delta de paramètres) plutôt que des copies intégrales dans `g_hold_slots` et dans le runtime LED.【F:apps/seq_led_bridge.c†L48-L83】【F:apps/seq_led_bridge.c†L375-L520】

4. **Étape 4 – Introduction du séquenceur multi-tracks (`seq_project_t`)**
   - Créer une structure projet englobant 16 tracks × 4 voix, mutualiser la gestion des patterns, et mettre à jour `seq_engine`, `seq_led_bridge`, `seq_recorder`, `ui` pour naviguer dans plusieurs pistes.
   - Préparer des API de sérialisation pour charger/sauver les tracks sans duplication de données.【F:core/seq/seq_engine.c†L241-L420】【F:apps/seq_led_bridge.c†L720-L778】

5. **Étape 5 – Optimisation threads / UI / drivers**
   - Reprofiler les stacks (UI, display, cart, MIDI) et réduire les latences (`ui_led_backend_refresh`, `drv_leds_addr_render`).
   - Envisager DMA/UART non bloquant pour `cart_tx_thread`, batching des commandes, et hiérarchisation des priorités RTOS.【F:cart/cart_bus.c†L60-L143】【F:ui/ui_led_backend.c†L200-L274】【F:drivers/drv_leds_addr.c†L140-L158】

6. **Étape 6 – Tests & benchmarks**
   - Scripts automatisés : `make clean && make`, `arm-none-eabi-size build/brick.elf`, instrumentation des stacks (`chThdGetSelfX` + watermarks), tests unitaires (`tests/seq_*`).
   - Mesurer usage RAM (map) et charge CPU (traceur SysTick) avant/après chaque étape.

## 5. Risques RTOS & dépendances
- **DMA / périphériques** : les buffers utilisés par USB, SPI (OLED) ou WS2812 doivent rester en SRAM accessible par DMA/bit-banging ; attention lors du déplacement vers la CCM.【F:drivers/drv_display.c†L44-L118】【F:drivers/drv_leds_addr.c†L24-L158】
- **Priorités ChibiOS** : `midi_clock` (NORMALPRIO+3), `cart_tx` (NORMALPRIO+2) et `UI` (NORMALPRIO) sont ordonnés finement ; toute modification de stack ou de partage de mutex doit conserver l’ordre pour éviter l’inversion de priorité.【F:core/midi_clock.c†L30-L112】【F:cart/cart_bus.c†L91-L142】【F:ui/ui_task.c†L45-L107】
- **Couplage UI ↔ modèle** : `ui_backend` accède directement au pattern via `seq_led_bridge_access_pattern`; les refactors devront maintenir l’interface ou introduire des wrappers atomiques pour éviter des races entre UI et moteur.【F:ui/ui_backend.c†L566-L637】【F:apps/seq_led_bridge.c†L720-L833】
- **Hold / recorder** : `seq_recorder` et `seq_led_bridge` partagent le pattern ; toute compression devra préserver les pointeurs ou ajouter des couches d’adaptation.【F:apps/seq_recorder.c†L1-L120】

## 6. Métriques estimées & objectifs
- **Baseline** : ~195 KB `.bss` pour 192 KB SRAM disponibles (overflow imminent selon le contexte projet).
- **Gains visés** :
  - Suppression/constification des 6 overlays UI : ~50 KB libérés.
  - Refactor hold/runtime SEQ : ≥6 KB économisés (garde-fou pour multi-tracks).
  - Ajustements caches/queues (`s_ui_shadow`, `midi_usb_queue`, `cart_port`) : 3–5 KB.
  - Compression du step & partage du pattern entre modules : objectif −25 % sur `seq_model_pattern_t` (≈7 KB gagnés) avant multi-track.
- **Budget cible** : ramener `.bss` <140 KB pour laisser ~30 % de marge au futur séquenceur 16 tracks.
- **CPU** : viser <40 µs par tick 1/16 (contre ~non-mesuré) après optimisation, avec monitoring via SysTick ou instrumentation `chVTGetSystemTimeX` dans le moteur.【F:core/seq/seq_engine.c†L241-L420】

## 7. Conclusion & prochaines instructions
Le firmware consomme actuellement une part critique de la SRAM à cause des duplications de specs UI et des structures SEQ monolithiques. Les optimisations listées ci-dessus constituent la trajectoire pour accueillir un séquenceur 16 tracks tout en restaurant une marge mémoire confortable.

**Instructions pour les passes suivantes :**
- **Étape 2 :** migrer les buffers statiques (overlays UI, caches, queues, stacks) vers la CCM ou en `const`, et recalibrer leurs tailles.
- **Étape 3 :** compresser le modèle `pattern/step/p-locks` en prévision du multi-track.
- **Étape 4 :** intégrer le séquenceur multi-tracks (`seq_project_t`) et adapter moteur/UI.
- **Étape 5 :** optimiser les threads, caches UI et drivers.
- **Étape 6 :** exécuter la batterie de tests et benchmarks mémoire/CPU.

