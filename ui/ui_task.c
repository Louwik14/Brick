/**
 * @file ui_task.c
 * @brief Thread principal UI — gestion du pipeline Keyboard / SEQ / LED, latence faible.
 * @ingroup ui
 *
 * @details
 * - Lit les entrées (boutons/encodeurs) et les délègue au `ui_backend`.
 * - Synchronise Keyboard ↔ App.
 * - Rafraîchit LEDs et affiche (renderer).
 *
 * Horloge & SEQ :
 * - Initialise `clock_manager` et enregistre `_on_clock_step`.
 * - Forwarde **l’index absolu** de pas vers le backend LED (plus de modulo 16 ici).
 * - Le backend relaie ensuite vers `ui_led_seq_on_clock_tick()` sans dépendance à clock_manager.
 *
 * Invariants :
 * - Pas de dépendance circulaire.
 * - Zéro régression côté Keyboard/MIDI.
 */

#include "ch.h"
#include "hal.h"

#include <stdbool.h>
#include <stdint.h>

#include "ui_task.h"
#include "ui_input.h"
#include "ui_controller.h"
#include "ui_renderer.h"
#include "cart_registry.h"
#include "ui_backend.h"
#include "clock_manager.h"
#include "ui_led_backend.h"

/* Keyboard runtime */
#include "ui_keyboard_bridge.h"

/* (Optionnel) Pont SEQ si utilisé pour publier un snapshot au boot */
// #include "seq_led_bridge.h"

#ifndef UI_TASK_STACK
#define UI_TASK_STACK  (1024)
#endif
#ifndef UI_TASK_PRIO
#define UI_TASK_PRIO   (NORMALPRIO)
#endif
#ifndef UI_TASK_POLL_MS
#define UI_TASK_POLL_MS (2)
#endif

static THD_WORKING_AREA(waUI, UI_TASK_STACK);
static thread_t* s_ui_thread = NULL;

/* ============================================================================
 * Horloge → LEDs (callback)
 * ==========================================================================*/

/**
 * @brief Callback horloge (appelé à chaque step 1/16).
 *
 * Forwarde **l’index absolu** (0..∞) sur 8 bits (0..255),
 * le renderer SEQ se charge du modulo sur la longueur totale.
 */
static void _on_clock_step(const clock_step_info_t* info) {
  if (!info) return;
  const uint8_t step_abs = (uint8_t)(info->step_idx_abs & 0xFFu);  /* <-- plus de & 15U */
  ui_led_backend_post_event_i(UI_LED_EVENT_CLOCK_TICK, step_abs, true);
}

/* ============================================================================
 * Helpers
 * ==========================================================================*/

/* ============================================================================
 * Thread principal UI
 * ==========================================================================*/

static THD_FUNCTION(UIThread, arg) {
  (void)arg;
#if CH_CFG_USE_REGISTRY
  chRegSetThreadName("UI");
#endif

  /* 1) Init UI depuis la cart active */
  {
    cart_id_t active = cart_registry_get_active_id();
    const ui_cart_spec_t* init_spec = cart_registry_get_ui_spec(active);
    ui_init(init_spec);
  }

  /* 2) Init clock manager (tick 24 PPQN → step 1/16) */
  clock_manager_init(CLOCK_SRC_INTERNAL);  /* enregistre on_midi_tick, prépare GPT */
  clock_manager_register_step_callback2(_on_clock_step);

  /* 3) Initialisation backend + bridge Keyboard */
  ui_backend_init_runtime();
  ui_keyboard_bridge_init();
  ui_keyboard_bridge_update_from_model();

  ui_input_event_t evt;

  for (;;) {
    const bool got = ui_input_poll(&evt, TIME_MS2I(UI_TASK_POLL_MS));

    if (got) {
      ui_backend_process_input(&evt);
    }

    /* Sync Keyboard runtime (root/scale/omni & p2) */
    ui_keyboard_bridge_update_from_model();

    /* LEDs + affichage */
    ui_led_backend_refresh();

    if (ui_is_dirty()) {
      ui_render();
      ui_clear_dirty();
    }

    chThdSleepMilliseconds(1);
  }
}

/* ============================== API Publique ============================= */

void ui_task_start(void) {
  if (!s_ui_thread) {
    s_ui_thread = chThdCreateStatic(waUI, sizeof(waUI), UI_TASK_PRIO, UIThread, NULL);
  }
}

bool ui_task_is_running(void) {
  return s_ui_thread != NULL;
}
