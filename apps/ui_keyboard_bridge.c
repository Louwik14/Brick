/**
 * @file ui_keyboard_bridge.c
 * @brief Bridge UI ↔ Keyboard App ↔ Backend, latence minimale (émission directe).
 * @ingroup ui_apps
 *
 * @details
 * - Lit Root / Scale / Omnichord via ui_backend_shadow_get() (dest UI).
 * - Lit aussi la page 2 : Note Order & Chord Override (IDs exposés par ui_keyboard_ui.h).
 * - Met à jour immédiatement l’app + le mapper + les LEDs à chaque itération.
 * - Le sink émet directement via ui_backend_note_on/off/all_notes_off → chemin court MIDI.
 */

#include "ui_keyboard_bridge.h"

#include "ui_keyboard_app.h"
#include "kbd_input_mapper.h"

/* Backend (fonctions directes) + constantes UI_DEST_* */
#include "ui_backend.h"

/* LEDs (observateur passif) */
#include "ui_led_backend.h"
#include "seq_recorder.h"

/* IDs UI de la vitrine Keyboard (omni/scale/root + page 2) */
#include "ui_keyboard_ui.h"

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------- */
/* Si ui_backend.h n’expose pas (encore) ces 3 APIs “directes”, déclare-les  */
/* ici proprement pour éviter les 'implicit declaration' et garder le build. */
/* Remarque : tu peux aussi les ajouter dans ui_backend.h si tu préfères.    */
/* -------------------------------------------------------------------------- */
#ifndef UI_BACKEND_NOTE_API_DECLARED
#define UI_BACKEND_NOTE_API_DECLARED 1
extern void ui_backend_note_on(uint8_t note, uint8_t velocity);
extern void ui_backend_note_off(uint8_t note);
extern void ui_backend_all_notes_off(void);
#endif

/* ============================ Note sink (direct backend) ================== */

#define DEFAULT_MIDI_CHANNEL 0
#define DEFAULT_VELOCITY     100

static void sink_note_on(uint8_t ch, uint8_t note, uint8_t vel) {
  (void)ch;
  seq_recorder_handle_note_on(note, vel);
  ui_backend_note_on(note, (vel ? vel : DEFAULT_VELOCITY));
}

static void sink_note_off(uint8_t ch, uint8_t note, uint8_t vel) {
  (void)ch; (void)vel;
  seq_recorder_handle_note_off(note);
  ui_backend_note_off(note);
}

static void sink_all_notes_off(uint8_t ch) {
  (void)ch;
  /* NOTE OFF events are flushed individually; avoid global All Notes Off. */
}

static ui_keyboard_note_sink_t g_sink = {
  .note_on = sink_note_on,
  .note_off = sink_note_off,
  .all_notes_off = sink_all_notes_off,
  .midi_channel = DEFAULT_MIDI_CHANNEL,
  .velocity = DEFAULT_VELOCITY
};

/* ============================ Helpers mapping UI ========================= */

/* IDs locaux tels qu’exposés par la vitrine Keyboard (voir ui_keyboard_ui.c) */
enum {
  KBD_UI_LOCAL_SCALE = 0x0000u,
  KBD_UI_LOCAL_ROOT  = 0x0001u,
  KBD_UI_LOCAL_ARP   = 0x0002u
};
/* Omnichord / Note Order / Chord Override sont exposés par ui_keyboard_ui.h :
   - KBD_OMNICHORD_ID
   - KBD_NOTE_ORDER_ID
   - KBD_CHORD_OVERRIDE_ID
   Ce sont des IDs **locaux** (sans UI_DEST_UI). */

#define KBD_UI_ID(local)  (uint16_t)(UI_DEST_UI | ((local) & 0x1FFFu))

/* Mapping vitrine → moteur (ordre comme dans ui_keyboard_ui.c) */
static kbd_scale_t _map_scale_from_ui(uint8_t ui_scale_id) {
  switch (ui_scale_id) {
    case 0: return KBD_SCALE_MAJOR;
    case 1: return KBD_SCALE_NAT_MINOR;
    case 2: return KBD_SCALE_PENT_MAJOR;
    case 3: return KBD_SCALE_DORIAN;
    case 4: return KBD_SCALE_MIXOLYDIAN;
    default: return KBD_SCALE_MAJOR;
  }
}

static inline uint8_t _shadow_ui_local(uint16_t local_id) {
  return ui_backend_shadow_get(KBD_UI_ID(local_id));
}

/* ================================ Bridge ================================= */

void ui_keyboard_bridge_init(void) {
  /* Init app avec sink direct (chemin court vers midi.c) */
  ui_keyboard_app_init(&g_sink);

  /* Lecture initiale via shadow UI */
  const uint8_t root_idx   = (uint8_t)(_shadow_ui_local(KBD_UI_LOCAL_ROOT)  & 0x7Fu); /* 0..11 */
  const uint8_t scale_idx  = (uint8_t)(_shadow_ui_local(KBD_UI_LOCAL_SCALE) & 0x1Fu);
  const bool    omni       = (_shadow_ui_local(KBD_OMNICHORD_ID) != 0);

  /* Page 2 (si non branchée, valeurs lues = 0) */
  const uint8_t order_val  = (uint8_t)(_shadow_ui_local(KBD_NOTE_ORDER_ID) & 0x03u);
  const bool    override   = (_shadow_ui_local(KBD_CHORD_OVERRIDE_ID) != 0);

  /* Root MIDI base: C4 (60) + offset root depuis vitrine */
  const uint8_t root_midi = (uint8_t)(60u + (root_idx % 12u));

  /* Appliquer à l’app + mapper + LEDs */
  ui_keyboard_app_set_params(root_midi, _map_scale_from_ui(scale_idx), omni);
  ui_keyboard_app_set_note_order((order_val == 1) ? NOTE_ORDER_FIFTHS : NOTE_ORDER_NATURAL);
  ui_keyboard_app_set_chord_override(override);

  kbd_input_mapper_init(omni); /* état initial */
  // --- FIX: ne pas écraser le mode LED SEQ au démarrage (on laisse le bridge LED décider) ---
  ui_led_backend_set_keyboard_omnichord(omni);
}

void ui_keyboard_bridge_update_from_model(void) {
  /* Lecture fréquente (chaque itération de la boucle UI) */
  const uint8_t root_idx   = (uint8_t)(_shadow_ui_local(KBD_UI_LOCAL_ROOT)  & 0x7Fu);
  const uint8_t scale_idx  = (uint8_t)(_shadow_ui_local(KBD_UI_LOCAL_SCALE) & 0x1Fu);
  const bool    omni       = (_shadow_ui_local(KBD_OMNICHORD_ID) != 0);

  const uint8_t order_val  = (uint8_t)(_shadow_ui_local(KBD_NOTE_ORDER_ID) & 0x03u);
  const bool    override   = (_shadow_ui_local(KBD_CHORD_OVERRIDE_ID) != 0);

  const uint8_t root_midi  = (uint8_t)(60u + (root_idx % 12u));

  /* MAJ immédiate de l’app (idempotent) */
  ui_keyboard_app_set_params(root_midi, _map_scale_from_ui(scale_idx), omni);
  ui_keyboard_app_set_note_order((order_val == 1) ? NOTE_ORDER_FIFTHS : NOTE_ORDER_NATURAL);
  ui_keyboard_app_set_chord_override(override);

  /* MAJ du mapper + LEDs (idempotents) */
  kbd_input_mapper_set_omnichord_state(omni);
  ui_led_backend_set_keyboard_omnichord(omni);
}

void ui_keyboard_bridge_tick(uint32_t elapsed_ms) {
  (void)elapsed_ms;
  ui_keyboard_app_tick(elapsed_ms);
}
