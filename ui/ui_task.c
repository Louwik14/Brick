/**
 * @file ui_task.c
 * @brief Thread principal UI — gestion du pipeline Keyboard / SEQ / LED, latence faible.
 * @ingroup ui
 *
 * @details
 * - Lit les entrées (boutons/encodeurs) et route via `ui_shortcuts`.
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
#include <string.h>
#include <stdio.h>

#include "ui_task.h"
#include "ui_input.h"
#include "ui_controller.h"
#include "ui_renderer.h"
#include "cart_registry.h"
#include "ui_shortcuts.h"
#include "ui_led_backend.h"
#include "drv_buttons_map.h"
#include "clock_manager.h"
#include "ui_overlay.h"
#include "ui_model.h"

/* Keyboard runtime */
#include "ui_keyboard_bridge.h"
#include "ui_keyboard_app.h"
#include "kbd_input_mapper.h"

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
static bool s_rec_mode = false;

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
  ui_led_backend_process_event(UI_LED_EVENT_CLOCK_TICK, step_abs, true);
}

/* ============================================================================
 * Helpers
 * ==========================================================================*/

static bool _keyboard_overlay_active(void) {
  if (!ui_overlay_is_active()) return false;
  const ui_cart_spec_t* spec = ui_overlay_get_spec();
  if (!spec) return false;
  const char* name = spec->menus[0].name;
  return (name && strcmp(name, "KEYBOARD") == 0);
}

static void _update_keyboard_overlay_label_from_shift(int8_t shift) {
  char tag[32];
  if (shift == 0) snprintf(tag, sizeof(tag), "Keys");
  else snprintf(tag, sizeof(tag), "KEY%+d", (int)shift);
  ui_model_set_active_overlay_tag(tag);
}

static bool handle_octave_shift_buttons(const ui_input_event_t *evt) {
  if (!evt->has_button || !evt->btn_pressed) return false;
  if (ui_input_shift_is_pressed()) return false; /* SHIFT+PLUS/MINUS réservé */

  const bool keys_context =
      _keyboard_overlay_active() || ui_shortcuts_is_keys_active();
  if (!keys_context) return false;

  int8_t shift = ui_keyboard_app_get_octave_shift();
  bool changed = false;

  if (evt->btn_id == UI_BTN_PLUS) {
    if (shift < CUSTOM_KEYS_OCT_SHIFT_MAX) { shift++; changed = true; }
  } else if (evt->btn_id == UI_BTN_MINUS) {
    if (shift > CUSTOM_KEYS_OCT_SHIFT_MIN) { shift--; changed = true; }
  } else {
    return false;
  }

  if (changed) {
    ui_keyboard_app_set_octave_shift(shift);
    _update_keyboard_overlay_label_from_shift(shift);
    ui_mark_dirty();
    return true;
  }
  return false;
}

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

  /* 3) Shortcuts + callback horloge */
  ui_shortcuts_init();
  clock_manager_register_step_callback2(_on_clock_step);

  /* 4) Bridge Keyboard + synchro immédiate */
  ui_keyboard_bridge_init();
  ui_keyboard_bridge_update_from_model();

  /* 5) Activer SEQ au boot (LEDs SEQ visibles) */
  ui_led_backend_set_mode(UI_LED_MODE_SEQ);
  ui_model_set_active_overlay_tag("SEQ");

  ui_input_event_t evt;

  for (;;) {
    const bool got = ui_input_poll(&evt, TIME_MS2I(UI_TASK_POLL_MS));

    if (got) {
      if (!ui_shortcuts_handle_event(&evt)) {
        if (handle_octave_shift_buttons(&evt)) {
          /* consommé */
        } else if (evt.has_button) {
          const bool pressed = evt.btn_pressed ? true : false;

          /* Pads SEQ routés vers le mapper Keyboard (inchangé) */
          if (evt.btn_id >= UI_BTN_SEQ1 && evt.btn_id <= UI_BTN_SEQ16) {
            const uint8_t seq_index = (uint8_t)(1u + (evt.btn_id - UI_BTN_SEQ1)); /* 1..16 */
            kbd_input_mapper_process(seq_index, pressed);
          } else if (pressed) {
            switch (evt.btn_id) {
              case UI_BTN_PARAM1: ui_on_button_menu(0);  break;
              case UI_BTN_PARAM2: ui_on_button_menu(1);  break;
              case UI_BTN_PARAM3: ui_on_button_menu(2);  break;
              case UI_BTN_PARAM4: ui_on_button_menu(3);  break;
              case UI_BTN_PARAM5: ui_on_button_menu(4);  break;
              case UI_BTN_PARAM6: ui_on_button_menu(5);  break;
              case UI_BTN_PARAM7: ui_on_button_menu(6);  break;
              case UI_BTN_PARAM8: ui_on_button_menu(7);  break;

              case UI_BTN_PAGE1:  ui_on_button_page(0);  break;
              case UI_BTN_PAGE2:  ui_on_button_page(1);  break;
              case UI_BTN_PAGE3:  ui_on_button_page(2);  break;
              case UI_BTN_PAGE4:  ui_on_button_page(3);  break;
              case UI_BTN_PAGE5:  ui_on_button_page(4);  break;

              case UI_BTN_REC:
                s_rec_mode = !s_rec_mode;
                ui_led_backend_set_record_mode(s_rec_mode);
                break;

              default: break;
            }
          }
        }

        if (evt.has_encoder && evt.enc_delta != 0) {
          ui_on_encoder((int)evt.encoder, (int)evt.enc_delta);
        }
      }
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
