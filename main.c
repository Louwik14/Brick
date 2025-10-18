/**
 * @file main.c
 * @brief Point d’entrée principal du firmware **Brick Control Platform** — Phase 6 (LED backend intégré).
 * @ingroup core
 *
 * @details
 * Ce module orchestre l’initialisation complète du système :
 * - Initialisation du noyau **ChibiOS** (`halInit()`, `chSysInit()`).
 * - Démarrage de tous les **drivers matériels** (boutons, LEDs, encodeurs…).
 * - Démarrage de la pile **USB device** et de l’interface **USB MIDI**.
 * - Initialisation du **MIDI DIN** (UART @ 31250) et des threads MIDI.
 * - Initialisation de la **clock MIDI 24 PPQN** (GPT + thread @ `NORMALPRIO+3`).
 * - Initialisation du **bus cartouche**, du **registre** et du **lien cart**.
 * - Chargement du module **UI** avec la spécification du synthé `XVA1`
 *   et configuration des **cycles de menus dynamiques**.
 * - Lancement du **thread principal de l’interface utilisateur**.
 * - Initialisation du **backend LED** et rafraîchissement continu dans la boucle principale.
 *
 * Contraintes d’architecture respectées :
 * - L’ordre d’initialisation garantit que l’I/O temps réel (USB/MIDI/Clock)
 *   est prêt **avant** tout usage par l’application/UI.
 * - Les couches sont strictement séparées : l’UI ne touche pas aux drivers/bus,
 *   et le pont unique pour les LEDs reste `ui_led_backend`.
 */

#include "ch.h"
#include "hal.h"

/* --- Core Cart / Drivers --- */
#include "cart/cart_xva1_spec.h"
#include "drivers.h"
#include "cart_bus.h"
#include "cart_link.h"
#include "cart_registry.h"

/* --- UI --- */
#include "ui_task.h"
#include "ui_spec.h"
#include "ui_controller.h"
#include "ui_led_backend.h"   /* Phase 6 : backend LED adressable */
#include "brick_config.h"

/* --- I/O Temps Réel --- */
#include "usb_device.h"
#include "midi.h"
#include "midi_clock.h"

#if DEBUG_ENABLE
#include "chprintf.h"
#include "chdebug.h"
#endif

extern CCM_DATA volatile systime_t ui_task_last_tick;

#if CH_CFG_USE_REGISTRY && DEBUG_ENABLE
__attribute__((weak)) void chThdDump(BaseSequentialStream *stream) {
  if (stream != NULL) {
    chprintf(stream, "[watchdog] chThdDump() stub\r\n");
  }
}
#endif


/* ===========================================================
 * INITIALISATION EN BLOCS
 * ===========================================================*/

/**
 * @brief Initialise le système (ChibiOS + HAL).
 */
static void system_init(void) {
  halInit();
  chSysInit();
}

/**
 * @brief Initialise les piles I/O temps réel (USB, MIDI, Clock).
 *
 * Ordre recommandé :
 * 1) USB device pour assurer l’énumération et la disponibilité de l’EP,
 * 2) MIDI (UART DIN + threads USB TX),
 * 3) Clock 24 PPQN (GPT + thread haute priorité).
 */
static void io_realtime_init(void) {
  usb_device_start();   /* USB device + réénumération (usbcfg/usbd) */
  midi_init();          /* UART DIN @ 31250 + mailbox USB + thread TX */
  midi_clock_init();    /* GPT + thread Clock @ NORMALPRIO+3 */
}

/**
 * @brief Initialise les drivers matériels et la pile cartouche.
 *
 * Le lien cart (`cart_link_init`) doit précéder l’initialisation de l’UI.
 * Les cartouches sont ensuite enregistrées dans le registre global.
 */
static void drivers_and_cart_init(void) {
  drivers_init_all();

  cart_bus_init();
  cart_registry_init();
  cart_link_init();

  /* Enregistre les cartouches disponibles */
  cart_registry_register(CART1, &CART_XVA1);
  // cart_registry_register(CART2, &CART_FX);
  // cart_registry_register(CART3, &CART_SAMPLER);
}

/**
 * @brief Initialise l’interface utilisateur et configure les cycles de menus.
 */
static void ui_init_all(void) {
  /* Charge la cartouche XVA1 par défaut */
  ui_init(&CART_XVA1);
}

/* ===========================================================
 * MAIN
 * ===========================================================*/

/**
 * @brief Fonction principale : point d’entrée du firmware Brick.
 *
 * Effectue toutes les initialisations nécessaires puis lance la tâche UI
 * avant d’entrer dans la boucle principale de rendu (LEDs).
 */
int main(void) {
  system_init();

  /* I/O temps réel d’abord (USB/MIDI/Clock), puis drivers/cart, puis UI */
  io_realtime_init();
  drivers_and_cart_init();
  ui_init_all();

  /* Phase 6 : Initialisation du backend LED avant démarrage UI */
  ui_led_backend_init();

  /* Démarre le thread de gestion de l’interface utilisateur */
  ui_task_start();

  while (true) {
    chThdSleepMilliseconds(20);

    const systime_t now = chVTGetSystemTimeX();
    const systime_t last_ui = ui_task_last_tick;
    if ((last_ui != 0) && ((now - last_ui) > TIME_MS2I(500))) {
#if CH_CFG_USE_REGISTRY && DEBUG_ENABLE
      BaseSequentialStream *stream = (BaseSequentialStream *)&SD2;
      chprintf(stream, "\r\n[watchdog] UI stalled, dumping threads...\r\n");
      chThdDump(stream);
#endif
      panic("UI stalled");
    }
  }
}
