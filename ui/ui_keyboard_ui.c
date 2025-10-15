/**
 * @file ui_keyboard_ui.c
 * @brief Spécification UI du mode custom **KEYBOARD** (menu unique “Mode”).
 * @ingroup ui_modes
 *
 * @details
 * Ce module déclare la vitrine UI du **Keyboard Mode**, intégrée en Phase 6.
 * Il expose :
 *  - Page 1 “Play Setup” : Gamme, Root, Arp, Omnichord.
 *  - Page 2 “Keyboard Settings” : **Note order** (Natural / Circle of Fifths),
 *    et **Chord buttons override scale** (autorise les accidentals pour les accords).
 *
 * Le champ `cart_name` est volontairement vide afin de ne pas remplacer
 * le nom de la cartouche active à l’écran. Le label de bannière provient
 * désormais du `ui_backend` (ex. "KEY", "KEY+1").
 */

#include "ui_keyboard_ui.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ============================================================================
 *  Identifiants locaux (dans l’espace UI_DEST_UI)
 * ==========================================================================*/
enum {
    KBD_SCALE = 0x0100,   /**< ENUM : Major, Minor, Pent, Dorian, Mixolydian. */ // --- FIX: espace ID dédié Keyboard ---
    KBD_ROOT,             /**< ENUM : C..B. */
    KBD_ARP,              /**< ENUM : Off/On. */
    KBD_OMNICHORD,        /**< ENUM : Off/On (pilote le backend LED Keyboard). */

    /* Page 2 */
    KBD_NOTE_ORDER = 0x0110,     /**< ENUM : Natural / Circle of Fifths. */ // --- FIX: éviter le recouvrement inter-modes ---
    KBD_CHORD_OVERRIDE           /**< ENUM : Off/On (bypass quantization pour accords Omnichord). */
};

/** @brief Alias d’IDs locaux pour usage externe (bridge, contrôleur, etc.). */
const uint16_t KBD_OMNICHORD_ID     = KBD_OMNICHORD;
const uint16_t KBD_NOTE_ORDER_ID    = KBD_NOTE_ORDER;
const uint16_t KBD_CHORD_OVERRIDE_ID= KBD_CHORD_OVERRIDE;

#define KBD_UI(idlocal) (UI_DEST_UI | ((uint16_t)(idlocal) & 0x1FFF))

/* ============================================================================
 *  Tables d’énumérations
 * ==========================================================================*/

/* 1) Gammes proposées (page 1) */
static const char* const kbd_scale_labels[] = {
    "Major", "Minor", "Pent", "Dorian", "Mixol"
};
enum { KBD_SCALE_COUNT = (int)(sizeof(kbd_scale_labels)/sizeof(kbd_scale_labels[0])) };

/* 2) Noms de notes pour Root (page 1) */
static const char* const kbd_root_labels[] = {
    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};
enum { KBD_ROOT_COUNT = (int)(sizeof(kbd_root_labels)/sizeof(kbd_root_labels[0])) };

/* 3) ON/OFF générique (page 1 & page 2) */
static const char* const kbd_onoff_labels[] = { "Off", "On" };

/* 4) Note order (page 2) */
static const char* const kbd_note_order_labels[] = {
    "Natural", "Fifths"
};
enum { KBD_NOTE_ORDER_COUNT = (int)(sizeof(kbd_note_order_labels)/sizeof(kbd_note_order_labels[0])) };

/* ============================================================================
 *  Page 1 “Play Setup” (4 paramètres)
 * ==========================================================================*/

static const ui_page_spec_t kbd_page_play = {
    .params = {
        { .label="Gamme", .kind=UI_PARAM_ENUM, .dest_id=KBD_UI(KBD_SCALE),
          .default_value=0,
          .meta.en={ .labels=kbd_scale_labels, .count=KBD_SCALE_COUNT },
          .is_bitwise=false, .bit_mask=0 },

        { .label="Root", .kind=UI_PARAM_ENUM, .dest_id=KBD_UI(KBD_ROOT),
          .default_value=0,
          .meta.en={ .labels=kbd_root_labels, .count=KBD_ROOT_COUNT },
          .is_bitwise=false, .bit_mask=0 },

        { .label="Arp", .kind=UI_PARAM_ENUM, .dest_id=KBD_UI(KBD_ARP),
          .default_value=0,
          .meta.en={ .labels=kbd_onoff_labels, .count=2 },
          .is_bitwise=false, .bit_mask=0 },

        { .label="Chord", .kind=UI_PARAM_ENUM, .dest_id=KBD_UI(KBD_OMNICHORD),
          .default_value=0,
          .meta.en={ .labels=kbd_onoff_labels, .count=2 },
          .is_bitwise=false, .bit_mask=0 }
    },
    .header_label = "Setup"
};

/* ============================================================================
 *  Page 2 “Keyboard Settings” (2 paramètres)
 * ==========================================================================*/

static const ui_page_spec_t kbd_page_settings = {
    .params = {
        { .label="Order", .kind=UI_PARAM_ENUM, .dest_id=KBD_UI(KBD_NOTE_ORDER),
          .default_value=0, /* Natural par défaut */
          .meta.en={ .labels=kbd_note_order_labels, .count=KBD_NOTE_ORDER_COUNT },
          .is_bitwise=false, .bit_mask=0 },

        { .label="Outkey", .kind=UI_PARAM_ENUM, .dest_id=KBD_UI(KBD_CHORD_OVERRIDE),
          .default_value=0, /* Off par défaut → diatonique strict */
          .meta.en={ .labels=kbd_onoff_labels, .count=2 },
          .is_bitwise=false, .bit_mask=0 },

        /* Remplissage neutre pour les 2 slots restants (pas affichés si label=NULL) */
        { .label=NULL, .kind=UI_PARAM_NONE, .dest_id=0, .default_value=0, .meta.en={ .labels=NULL, .count=0 }, .is_bitwise=false, .bit_mask=0 },
        { .label=NULL, .kind=UI_PARAM_NONE, .dest_id=0, .default_value=0, .meta.en={ .labels=NULL, .count=0 }, .is_bitwise=false, .bit_mask=0 }
    },
    .header_label = "Settings"
};

/* ============================================================================
 *  Menu unique “KEYBOARD”
 * ==========================================================================*/

static const ui_menu_spec_t kbd_menu = {
    .name = "KEYBOARD",
    .page_titles = { "Play", "Setup", "-", "-", "-" },
    .pages = { kbd_page_play, kbd_page_settings, (ui_page_spec_t){0}, (ui_page_spec_t){0}, (ui_page_spec_t){0} }
};

/* ============================================================================
 *  Spécification complète (cartouche virtuelle)
 * ==========================================================================*/

const ui_cart_spec_t ui_keyboard_spec = {
    .cart_name   = "",        /**< Laisse le nom de la cart active affiché. */
    .menus       = { [0] = kbd_menu },
    .cycles      = {
        [0] = { .count=0 }, [1] = { .count=0 }, [2] = { .count=0 }, [3] = { .count=0 },
        [4] = { .count=0 }, [5] = { .count=0 }, [6] = { .count=0 }, [7] = { .count=0 }
    }
};
