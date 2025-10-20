/**
 * @file ui_seq_ui.c
 * @brief Définition complète des pages et paramètres du mode SEQ (MODE + SETUP) + cycles.
 * @ingroup ui_modes
 *
 * @details
 * Deux menus dans la même cartouche UI :
 *  - Menu[0] : "SEQ"  (pages All, Voix1..Voix4)
 *  - Menu[1] : "Setup" (pages General, MIDI)
 *
 * Le **BM1** cyclera entre ces deux menus (MODE ↔ SETUP) avec `resume = false`
 * → à chaque retour dans cette UI, on repart sur MODE.
 *
 * Tous les paramètres utilisent des dest_id en espace UI interne (UI_DEST_UI),
 * donc **aucun paquet bus/cart** n’est envoyé (cf. ui_backend).
 * Le label de bannière ("SEQ") est désormais injecté par le backend via
 * `ui_mode_context_t`.
 */

#include "ui_seq_ui.h"
#include "ui_seq_ids.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/* ===========================================================================
 *  Espace d'adressage local pour le SEQ (13 bits utiles via UI_DEST_ID)
 * ===========================================================================*/

/* Helper macro pour encoder un dest_id purement UI. */
#define SEQ_UI(idlocal) (UI_DEST_UI | ((uint16_t)(idlocal) & 0x1FFF))

/* ===========================================================================
 *  Tables partagées
 * ===========================================================================*/

/* Noms MIDI des notes (C-2 → G8, 128 valeurs) */
static const char* const midi_note_labels[128] = {
    "C-2","C#-2","D-2","D#-2","E-2","F-2","F#-2","G-2","G#-2","A-2","A#-2","B-2",
    "C-1","C#-1","D-1","D#-1","E-1","F-1","F#-1","G-1","G#-1","A-1","A#-1","B-1",
    "C0","C#0","D0","D#0","E0","F0","F#0","G0","G#0","A0","A#0","B0",
    "C1","C#1","D1","D#1","E1","F1","F#1","G1","G#1","A1","A#1","B1",
    "C2","C#2","D2","D#2","E2","F2","F#2","G2","G#2","A2","A#2","B2",
    "C3","C#3","D3","D#3","E3","F3","F#3","G3","G#3","A3","A#3","B3",
    "C4","C#4","D4","D#4","E4","F4","F#4","G4","G#4","A4","A#4","B4",
    "C5","C#5","D5","D#5","E5","F5","F#5","G5","G#5","A5","A#5","B5",
    "C6","C#6","D6","D#6","E6","F6","F#6","G6","G#6","A6","A#6","B6",
    "C7","C#7","D7","D#7","E7","F7","F#7","G7","G#7","A7","A#7","B7",
    "C8","C#8","D8","D#8","E8","F8","F#8","G8"
};

static const char* const seq_setup_clock_labels[] = { "Int", "Ext" };
static const char* const seq_setup_quant_labels[] = { "Off", "1/8", "1/16", "1/32" };

/* ===========================================================================
 *  SEQ (pages principales)
 * ===========================================================================*/

static const ui_page_spec_t seq_page_all = {
    .params = {
        { .label="Transp", .kind=UI_PARAM_CONT, .dest_id=SEQ_UI(SEQ_UI_LOCAL_ALL_TRANSP), .default_value=0,
          .meta.range={.min=-12, .max=12, .step=1}, .is_bitwise=false },
        { .label="Vel",    .kind=UI_PARAM_CONT, .dest_id=SEQ_UI(SEQ_UI_LOCAL_ALL_VEL),    .default_value=0,
          .meta.range={.min=-127, .max=127, .step=1}, .is_bitwise=false },
        { .label="Len",    .kind=UI_PARAM_CONT, .dest_id=SEQ_UI(SEQ_UI_LOCAL_ALL_LEN),    .default_value=0,
          .meta.range={.min=-32, .max=32, .step=1}, .is_bitwise=false },
        { .label="Mic",    .kind=UI_PARAM_CONT, .dest_id=SEQ_UI(SEQ_UI_LOCAL_ALL_MIC),    .default_value=0,
          .meta.range={.min=-12, .max=12, .step=1}, .is_bitwise=false }
    },
    .header_label = "All"
};

static const ui_page_spec_t seq_page_voix1 = {
    .params = {
        { .label="Note", .kind=UI_PARAM_ENUM, .dest_id=SEQ_UI(SEQ_UI_LOCAL_V1_NOTE),
          .default_value=60, .meta.en={midi_note_labels,128}, .is_bitwise=false },
        { .label="Vel",  .kind=UI_PARAM_CONT, .dest_id=SEQ_UI(SEQ_UI_LOCAL_V1_VEL),
          .default_value=0, .meta.range={.min=0,.max=127,.step=1}, .is_bitwise=false }, // --- FIX: valeur neutre par défaut ---
        { .label="Len",  .kind=UI_PARAM_CONT, .dest_id=SEQ_UI(SEQ_UI_LOCAL_V1_LEN),
          .default_value=1, .meta.range={.min=1,.max=64,.step=1}, .is_bitwise=false },
        { .label="Mic",  .kind=UI_PARAM_CONT, .dest_id=SEQ_UI(SEQ_UI_LOCAL_V1_MIC),
          .default_value=0, .meta.range={.min=-12,.max=12,.step=1}, .is_bitwise=false }
    },
    .header_label = "Voix1"
};

static const ui_page_spec_t seq_page_voix2 = {
    .params = {
        { .label="Note", .kind=UI_PARAM_ENUM, .dest_id=SEQ_UI(SEQ_UI_LOCAL_V2_NOTE),
          .default_value=60, .meta.en={midi_note_labels,128}, .is_bitwise=false },
        { .label="Vel",  .kind=UI_PARAM_CONT, .dest_id=SEQ_UI(SEQ_UI_LOCAL_V2_VEL),
          .default_value=0, .meta.range={.min=0,.max=127,.step=1}, .is_bitwise=false },
        { .label="Len",  .kind=UI_PARAM_CONT, .dest_id=SEQ_UI(SEQ_UI_LOCAL_V2_LEN),
          .default_value=1, .meta.range={.min=1,.max=64,.step=1}, .is_bitwise=false },
        { .label="Mic",  .kind=UI_PARAM_CONT, .dest_id=SEQ_UI(SEQ_UI_LOCAL_V2_MIC),
          .default_value=0, .meta.range={.min=-12,.max=12,.step=1}, .is_bitwise=false }
    },
    .header_label = "Voix2"
};

static const ui_page_spec_t seq_page_voix3 = {
    .params = {
        { .label="Note", .kind=UI_PARAM_ENUM, .dest_id=SEQ_UI(SEQ_UI_LOCAL_V3_NOTE),
          .default_value=60, .meta.en={midi_note_labels,128}, .is_bitwise=false },
        { .label="Vel",  .kind=UI_PARAM_CONT, .dest_id=SEQ_UI(SEQ_UI_LOCAL_V3_VEL),
          .default_value=0, .meta.range={.min=0,.max=127,.step=1}, .is_bitwise=false },
        { .label="Len",  .kind=UI_PARAM_CONT, .dest_id=SEQ_UI(SEQ_UI_LOCAL_V3_LEN),
          .default_value=1, .meta.range={.min=1,.max=64,.step=1}, .is_bitwise=false },
        { .label="Mic",  .kind=UI_PARAM_CONT, .dest_id=SEQ_UI(SEQ_UI_LOCAL_V3_MIC),
          .default_value=0, .meta.range={.min=-12,.max=12,.step=1}, .is_bitwise=false }
    },
    .header_label = "Voix3"
};

static const ui_page_spec_t seq_page_voix4 = {
    .params = {
        { .label="Note", .kind=UI_PARAM_ENUM, .dest_id=SEQ_UI(SEQ_UI_LOCAL_V4_NOTE),
          .default_value=60, .meta.en={midi_note_labels,128}, .is_bitwise=false },
        { .label="Vel",  .kind=UI_PARAM_CONT, .dest_id=SEQ_UI(SEQ_UI_LOCAL_V4_VEL),
          .default_value=0, .meta.range={.min=0,.max=127,.step=1}, .is_bitwise=false },
        { .label="Len",  .kind=UI_PARAM_CONT, .dest_id=SEQ_UI(SEQ_UI_LOCAL_V4_LEN),
          .default_value=1, .meta.range={.min=1,.max=64,.step=1}, .is_bitwise=false },
        { .label="Mic",  .kind=UI_PARAM_CONT, .dest_id=SEQ_UI(SEQ_UI_LOCAL_V4_MIC),
          .default_value=0, .meta.range={.min=-12,.max=12,.step=1}, .is_bitwise=false }
    },
    .header_label = "Voix4"
};

/* Menu[0] : SEQ (principal) */
static const ui_menu_spec_t seq_menu = {
    .name = "SEQ",
    .page_titles = { "All", "Voix1", "Voix2", "Voix3", "Voix4" },
    .pages = { seq_page_all, seq_page_voix1, seq_page_voix2, seq_page_voix3, seq_page_voix4 }
};

/* ===========================================================================
 *  SETUP (General / MIDI)
 * ===========================================================================*/

/* Page 1 : General */
static const ui_param_spec_t seq_setup_page_general_params[4] = {
    { .label = "Clock", .kind = UI_PARAM_ENUM, .dest_id = SEQ_UI(SEQ_UI_LOCAL_SETUP_CLOCK),
      .meta.en = { .labels = seq_setup_clock_labels, .count = 2 } },
    { .label = "Swing", .kind = UI_PARAM_CONT, .dest_id = SEQ_UI(SEQ_UI_LOCAL_SETUP_SWING),
      .meta.range = { .min = 0, .max = 100, .step = 1 } },
    { .label = "Steps", .kind = UI_PARAM_CONT, .dest_id = SEQ_UI(SEQ_UI_LOCAL_SETUP_STEPS),
      .meta.range = { .min = 1, .max = 64, .step = 1 } },
    { .label = "Quant", .kind = UI_PARAM_ENUM, .dest_id = SEQ_UI(SEQ_UI_LOCAL_SETUP_QUANT),
      .meta.en = { .labels = seq_setup_quant_labels, .count = 4 } }
};

/* Page 2 : MIDI */
static const ui_param_spec_t seq_setup_page_midi_params[4] = {
    { .label = "Ch1", .kind = UI_PARAM_CONT, .dest_id = SEQ_UI(SEQ_UI_LOCAL_SETUP_CH1),
      .meta.range = { .min = 1, .max = 16, .step = 1 } },
    { .label = "Ch2", .kind = UI_PARAM_CONT, .dest_id = SEQ_UI(SEQ_UI_LOCAL_SETUP_CH2),
      .meta.range = { .min = 1, .max = 16, .step = 1 } },
    { .label = "Ch3", .kind = UI_PARAM_CONT, .dest_id = SEQ_UI(SEQ_UI_LOCAL_SETUP_CH3),
      .meta.range = { .min = 1, .max = 16, .step = 1 } },
    { .label = "Ch4", .kind = UI_PARAM_CONT, .dest_id = SEQ_UI(SEQ_UI_LOCAL_SETUP_CH4),
      .meta.range = { .min = 1, .max = 16, .step = 1 } }
};

/* Menu[1] : SETUP */
static const ui_menu_spec_t seq_setup_menu = {
    .name = "Setup",
    .page_titles = { "General", "MIDI", "-", "-", "-" },
    .pages = {
        // --- FIX: inlined page specs to satisfy nested brace initialization ---
        [0] = {
            .params = {
                seq_setup_page_general_params[0],
                seq_setup_page_general_params[1],
                seq_setup_page_general_params[2],
                seq_setup_page_general_params[3],
            },
            .header_label = "General",
        },
        [1] = {
            .params = {
                seq_setup_page_midi_params[0],
                seq_setup_page_midi_params[1],
                seq_setup_page_midi_params[2],
                seq_setup_page_midi_params[3],
            },
            .header_label = "MIDI",
        },
    }
};

/* ===========================================================================
 *  Cartouches UI exposées
 * ===========================================================================*/

/**
 * @brief Cartouche virtuelle : SEQ UI (2 menus : MODE + SETUP)
 * @details
 * - **Cycle BM1** entre les deux menus (indices {0,1}).
 * - `resume=false` ⇒ quand on revient dans le SEQ, on redémarre sur MODE.
 */
const ui_cart_spec_t seq_ui_spec = {
    .cart_name = "SEQ UI",
    .menus = {
        [0] = seq_menu,
        [1] = seq_setup_menu
        /* autres menus non utilisés = zero-init */
    },
    .cycles = {
        /* BM1 : MODE ↔ SETUP, sans resume */
        [0] = { .count = 2, .idxs = { 0, 1 }, .resume = false },

        /* autres BM sans cycle */
        [1] = { .count = 0 },
        [2] = { .count = 0 },
        [3] = { .count = 0 },
        [4] = { .count = 0 },
        [5] = { .count = 0 },
        [6] = { .count = 0 },
        [7] = { .count = 0 },
    }
};

/**
 * @brief Cartouche virtuelle : SEQ SETUP (optionnelle, compat)
 * @details
 * Conservée au cas où des modules externes référenceraient encore “SEQ SETUP”
 * comme cartouche indépendante. Aucun cycle ici.
 */
const ui_cart_spec_t seq_setup_ui_spec = {
    .cart_name = "SEQ SETUP UI",
    .menus = { [0] = seq_setup_menu },
    .cycles = { [0] = { .count = 0 } }
};
