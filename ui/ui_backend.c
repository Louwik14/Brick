/**
 * @file ui_backend.c
 * @brief Pont neutre entre UI et couches basses (CartLink, UI interne, MIDI) + shadow UI local.
 * @ingroup ui
 *
 * @details
 * Implémente la fonction de routage centrale `ui_backend_param_changed()` et un
 * **shadow local** pour les paramètres `UI_DEST_UI` (vitrine / overlays).
 *
 * Destinations supportées :
 * - **CART** (`UI_DEST_CART`) → `cart_link_param_changed()`
 * - **UI interne** (`UI_DEST_UI`) → mise à jour du *shadow UI local* + `ui_backend_handle_ui()`
 * - **MIDI** (`UI_DEST_MIDI`) → traduction NOTE ON/OFF/PANIC vers `midi.c`
 *
 * Points importants :
 * - `ui_backend_shadow_get()` et `ui_backend_shadow_set()` gèrent **à présent**
 *   les deux espaces : `UI_DEST_UI` (shadow local) **et** `UI_DEST_CART`
 *   (shadow cartouche via CartLink).
 * - Le PANIC utilise le standard MIDI **CC#123** via `midi_cc(...)`.
 */

#include "ui_backend.h"
#include "ui_backend_midi_ids.h" /* UI_MIDI_NOTE_ON_BASE_LOCAL, etc. */

#include "cart_link.h"
#include "cart_registry.h"
#include "brick_config.h"

#include "midi.h"                /* midi_note_on/off(), midi_cc() */
#include "seq_engine.h"
#include "clock_manager.h"
#include "seq_led_bridge.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Masques de destination (répliqués pour compilation locale)                 */
/* -------------------------------------------------------------------------- */
#define UI_DEST_MASK   0xE000U
#define UI_DEST_CART   0x0000U  /**< Paramètre destiné à la cartouche active.  */
#define UI_DEST_UI     0x8000U  /**< Paramètre purement interne à l'UI.       */
#define UI_DEST_MIDI   0x4000U  /**< Paramètre routé vers la pile MIDI.       */
#define UI_DEST_ID(x)  ((x) & 0x1FFFU) /**< ID local sur 13 bits. */

enum {
    SEQ_ALL_TRANSP = 0x0000,
    SEQ_ALL_VEL,
    SEQ_ALL_LEN,
    SEQ_ALL_MIC,

    SEQ_V1_NOTE, SEQ_V1_VEL, SEQ_V1_LEN, SEQ_V1_MIC,
    SEQ_V2_NOTE, SEQ_V2_VEL, SEQ_V2_LEN, SEQ_V2_MIC,
    SEQ_V3_NOTE, SEQ_V3_VEL, SEQ_V3_LEN, SEQ_V3_MIC,
    SEQ_V4_NOTE, SEQ_V4_VEL, SEQ_V4_LEN, SEQ_V4_MIC,

    SEQ_SETUP_CLOCK,
    SEQ_SETUP_SWING,
    SEQ_SETUP_STEPS,
    SEQ_SETUP_QUANT,

    SEQ_SETUP_CH1,
    SEQ_SETUP_CH2,
    SEQ_SETUP_CH3,
    SEQ_SETUP_CH4
};

/* -------------------------------------------------------------------------- */
/* Paramètres MIDI par défaut                                                 */
/* -------------------------------------------------------------------------- */
#define UI_MIDI_DEFAULT_CH     0u
#define UI_MIDI_DEFAULT_VELOC  100u

/* -------------------------------------------------------------------------- */
/* Shadow UI local (pour l’espace UI_DEST_UI)                                 */
/* -------------------------------------------------------------------------- */
/**
 * @brief Petite table (id,val) pour mémoriser l’état des paramètres UI.
 * @note
 * - Les IDs sont des **locaux** (13 bits) *ou composés* avec UI_DEST_UI selon usage.
 * - On stocke la valeur **déjà encodée** (0..255) telle qu’envoyée à `ui_backend_param_changed`.
 * - Taille volontairement modeste : ajustable si besoin.
 */
typedef struct { uint16_t id; uint8_t val; } ui_local_kv_t;

#ifndef UI_BACKEND_UI_SHADOW_MAX
#define UI_BACKEND_UI_SHADOW_MAX  32u
#endif

static ui_local_kv_t s_ui_shadow[UI_BACKEND_UI_SHADOW_MAX];
static uint8_t       s_ui_shadow_count = 0;

/* Cherche l’index d’un id (UI_DEST_UI | local) dans la table ; -1 si absent. */
static int _ui_shadow_find(uint16_t id_full) {
    for (uint8_t i = 0; i < s_ui_shadow_count; ++i) {
        if (s_ui_shadow[i].id == id_full) return (int)i;
    }
    return -1;
}

static void _ui_shadow_set(uint16_t id_full, uint8_t v) {
    int idx = _ui_shadow_find(id_full);
    if (idx >= 0) {
        s_ui_shadow[(uint8_t)idx].val = v;
        return;
    }
    if (s_ui_shadow_count < UI_BACKEND_UI_SHADOW_MAX) {
        s_ui_shadow[s_ui_shadow_count].id  = id_full;
        s_ui_shadow[s_ui_shadow_count].val = v;
        s_ui_shadow_count++;
        return;
    }
    /* Table pleine : remplace LRU naïf (slot 0) pour rester O(1) */
    s_ui_shadow[0].id  = id_full;
    s_ui_shadow[0].val = v;
}

static uint8_t _ui_shadow_get(uint16_t id_full) {
    int idx = _ui_shadow_find(id_full);
    return (idx >= 0) ? s_ui_shadow[(uint8_t)idx].val : 0u;
}

static int decode_seq_linear(uint8_t wire, int mn, int mx) {
    int span = mx - mn;
    if (span <= 0) {
        return mn;
    }
    if (mn >= 0 && mx <= 255) {
        if (wire < (uint8_t)mn) wire = (uint8_t)mn;
        if (wire > (uint8_t)mx) wire = (uint8_t)mx;
        return (int)wire;
    }
    if (span == 255) {
        return mn + wire;
    }
    int value = ((int)wire * span + 127) / 255;
    return mn + value;
}

/* -------------------------------------------------------------------------- */
/* Prototypes internes                                                        */
/* -------------------------------------------------------------------------- */
static void ui_backend_handle_ui(uint16_t local_id, uint8_t prev_wire, uint8_t new_wire,
                                 bool bitwise, uint8_t mask);
static void ui_backend_handle_midi(uint16_t local_id, uint8_t val);

/* -------------------------------------------------------------------------- */
/* Implémentation                                                             */
/* -------------------------------------------------------------------------- */

void ui_backend_param_changed(uint16_t id, uint8_t val, bool bitwise, uint8_t mask) {
    const uint16_t dest     = (id & UI_DEST_MASK);
    const uint16_t local_id = UI_DEST_ID(id);

    switch (dest) {
    case UI_DEST_CART:
        /* Route vers la cartouche active (shadow + éventuelle propagation) */
        cart_link_param_changed(local_id, val, bitwise, mask);
        break;

    case UI_DEST_UI: {
        /* Met à jour le shadow UI **avant** la notification locale */
        const uint8_t prev = _ui_shadow_get(id);
        uint8_t newv = val;

        if (bitwise) {
            /* Lire l’état courant, appliquer le masque, puis stocker. */
            uint8_t reg = prev;
            if (mask) {
                /* bitwise + mask → activer/désactiver les bits */
                if (val) reg |= mask;
                else     reg &= (uint8_t)~mask;
            }
            newv = reg;
        }
        _ui_shadow_set(id, newv);

        /* Interception locale UI (facultatif) */
        ui_backend_handle_ui(local_id, prev, newv, bitwise, mask);
        break;
    }

    case UI_DEST_MIDI:
        /* Routage vers la pile MIDI (NOTE ON/OFF/PANIC, CC, etc.) */
        ui_backend_handle_midi(local_id, val);
        break;

    default:
        /* Destination inconnue : ignore */
        break;
    }
}

uint8_t ui_backend_shadow_get(uint16_t id) {
    const uint16_t dest = (id & UI_DEST_MASK);
    if (dest == UI_DEST_UI) {
        /* Lire le shadow UI local */
        return _ui_shadow_get(id);
    }
    /* Par défaut : shadow cartouche (CART) */
    cart_id_t cid = cart_registry_get_active_id();
    return cart_link_shadow_get(cid, id);
}

void ui_backend_shadow_set(uint16_t id, uint8_t val) {
    const uint16_t dest = (id & UI_DEST_MASK);
    if (dest == UI_DEST_UI) {
        _ui_shadow_set(id, val);
        return;
    }
    cart_id_t cid = cart_registry_get_active_id();
    cart_link_shadow_set(cid, id, val);
}

/* -------------------------------------------------------------------------- */
/* API simple pour émission de notes (utilisé par d’éventuels bridges)        */
/* -------------------------------------------------------------------------- */
void ui_backend_note_on(uint8_t note, uint8_t velocity) {
    midi_note_on(MIDI_DEST_BOTH, UI_MIDI_DEFAULT_CH, note, velocity);
}

void ui_backend_note_off(uint8_t note) {
    midi_note_off(MIDI_DEST_BOTH, UI_MIDI_DEFAULT_CH, note, 0);
}

void ui_backend_all_notes_off(void) {
    /* Standard MIDI: CC#123 = All Notes Off */
    midi_cc(MIDI_DEST_BOTH, UI_MIDI_DEFAULT_CH, 123, 0);
}

/* -------------------------------------------------------------------------- */
/* UI interne                                                                 */
/* -------------------------------------------------------------------------- */
static uint8_t seq_param_slot_from_id(seq_param_id_t param) {
    switch (param) {
        case SEQ_PARAM_NOTE:         return 0u;
        case SEQ_PARAM_VELOCITY:     return 1u;
        case SEQ_PARAM_LENGTH:       return 2u;
        case SEQ_PARAM_MICRO_TIMING: return 3u;
        default:                     return 0u;
    }
}

static int16_t decode_seq_voice_param(seq_param_id_t param, uint8_t wire) {
    switch (param) {
        case SEQ_PARAM_NOTE:
            return wire; /* mapping direct 0..127 */
        case SEQ_PARAM_VELOCITY:
            return decode_seq_linear(wire, 0, 127);
        case SEQ_PARAM_LENGTH:
            return decode_seq_linear(wire, 1, 64);
        case SEQ_PARAM_MICRO_TIMING:
            return decode_seq_linear(wire, -12, 12);
        default:
            return 0;
    }
}

static bool handle_seq_voice_plock(uint16_t local_id, uint8_t prev_wire, uint8_t new_wire) {
    seq_param_id_t param;
    uint8_t voice;

    switch (local_id) {
    case SEQ_V1_NOTE: case SEQ_V1_VEL: case SEQ_V1_LEN: case SEQ_V1_MIC:
        voice = 0; break;
    case SEQ_V2_NOTE: case SEQ_V2_VEL: case SEQ_V2_LEN: case SEQ_V2_MIC:
        voice = 1; break;
    case SEQ_V3_NOTE: case SEQ_V3_VEL: case SEQ_V3_LEN: case SEQ_V3_MIC:
        voice = 2; break;
    case SEQ_V4_NOTE: case SEQ_V4_VEL: case SEQ_V4_LEN: case SEQ_V4_MIC:
        voice = 3; break;
    default:
        return false;
    }

    switch (local_id) {
        case SEQ_V1_NOTE: case SEQ_V2_NOTE: case SEQ_V3_NOTE: case SEQ_V4_NOTE:
            param = SEQ_PARAM_NOTE;
            break;
        case SEQ_V1_VEL: case SEQ_V2_VEL: case SEQ_V3_VEL: case SEQ_V4_VEL:
            param = SEQ_PARAM_VELOCITY;
            break;
        case SEQ_V1_LEN: case SEQ_V2_LEN: case SEQ_V3_LEN: case SEQ_V4_LEN:
            param = SEQ_PARAM_LENGTH;
            break;
        default:
            param = SEQ_PARAM_MICRO_TIMING;
            break;
    }

    uint16_t held = seq_led_bridge_get_preview_mask();
    if (held == 0) {
        return false;
    }

    seq_engine_set_active_voice(voice);

    int16_t prev_val = decode_seq_voice_param(param, prev_wire);
    int16_t new_val  = decode_seq_voice_param(param, new_wire);
    int16_t delta    = new_val - prev_val;
    if (delta == 0) {
        return true;
    }

    // FIX: appliquer immédiatement le delta P-Lock sur tous les steps maintenus.
    seq_led_bridge_apply_plock_param(seq_param_slot_from_id(param), delta, held);
    return true;
}

static void ui_backend_handle_ui(uint16_t local_id, uint8_t prev_wire, uint8_t new_wire,
                                 bool bitwise, uint8_t mask) {
    (void)bitwise;
    (void)mask;

    switch (local_id) {
    case SEQ_ALL_TRANSP:
        seq_engine_set_global_offset(SEQ_PARAM_NOTE, decode_seq_linear(new_wire, -12, 12));
        seq_led_bridge_publish();
        break;
    case SEQ_ALL_VEL:
        seq_engine_set_global_offset(SEQ_PARAM_VELOCITY, decode_seq_linear(new_wire, -127, 127));
        seq_led_bridge_publish();
        break;
    case SEQ_ALL_LEN:
        seq_engine_set_global_offset(SEQ_PARAM_LENGTH, decode_seq_linear(new_wire, -32, 32));
        seq_led_bridge_publish();
        break;
    case SEQ_ALL_MIC:
        seq_engine_set_global_offset(SEQ_PARAM_MICRO_TIMING, decode_seq_linear(new_wire, -12, 12));
        seq_led_bridge_publish();
        break;

    case SEQ_V1_NOTE: case SEQ_V1_VEL: case SEQ_V1_LEN: case SEQ_V1_MIC:
        if (handle_seq_voice_plock(local_id, prev_wire, new_wire)) {
            break;
        }
        seq_engine_set_active_voice(0);
        break;
    case SEQ_V2_NOTE: case SEQ_V2_VEL: case SEQ_V2_LEN: case SEQ_V2_MIC:
        if (handle_seq_voice_plock(local_id, prev_wire, new_wire)) {
            break;
        }
        seq_engine_set_active_voice(1);
        break;
    case SEQ_V3_NOTE: case SEQ_V3_VEL: case SEQ_V3_LEN: case SEQ_V3_MIC:
        if (handle_seq_voice_plock(local_id, prev_wire, new_wire)) {
            break;
        }
        seq_engine_set_active_voice(2);
        break;
    case SEQ_V4_NOTE: case SEQ_V4_VEL: case SEQ_V4_LEN: case SEQ_V4_MIC:
        if (handle_seq_voice_plock(local_id, prev_wire, new_wire)) {
            break;
        }
        seq_engine_set_active_voice(3);
        break;

    case SEQ_SETUP_CLOCK:
        clock_manager_set_source(new_wire ? CLOCK_SRC_MIDI : CLOCK_SRC_INTERNAL);
        break;
    case SEQ_SETUP_STEPS: {
        int steps = decode_seq_linear(new_wire, 1, 64);
        for (uint8_t v = 0; v < SEQ_MODEL_VOICE_COUNT; ++v) {
            seq_engine_set_voice_length(v, (uint16_t)steps);
        }
        seq_led_bridge_set_total_span((uint16_t)(steps));
        break;
    }
    case SEQ_SETUP_CH1:
        seq_engine_set_voice_channel(0, new_wire);
        seq_led_bridge_publish();
        break;
    case SEQ_SETUP_CH2:
        seq_engine_set_voice_channel(1, new_wire);
        seq_led_bridge_publish();
        break;
    case SEQ_SETUP_CH3:
        seq_engine_set_voice_channel(2, new_wire);
        seq_led_bridge_publish();
        break;
    case SEQ_SETUP_CH4:
        seq_engine_set_voice_channel(3, new_wire);
        seq_led_bridge_publish();
        break;
    default:
        break;
    }
}

/* -------------------------------------------------------------------------- */
/* MIDI : traduction des IDs locaux vers midi.h                               */
/* -------------------------------------------------------------------------- */
static void ui_backend_handle_midi(uint16_t local_id, uint8_t val) {
    const midi_dest_t dest = MIDI_DEST_BOTH;
    const uint8_t ch = UI_MIDI_DEFAULT_CH;

    /* PANIC (All Notes Off) — utiliser CC#123 */
    if (local_id == (UI_MIDI_ALL_NOTES_OFF_LOCAL & 0x1FFFu)) {
        midi_cc(dest, ch, 123, 0);
        return;
    }

    /* NOTE ON */
    if (local_id >= UI_MIDI_NOTE_ON_BASE_LOCAL &&
        local_id <  (UI_MIDI_NOTE_ON_BASE_LOCAL + 128u)) {
        const uint8_t note = (uint8_t)(local_id - UI_MIDI_NOTE_ON_BASE_LOCAL);
        const uint8_t vel  = (val == 0) ? UI_MIDI_DEFAULT_VELOC : (val & 0x7Fu);
        midi_note_on(dest, ch, note, vel);
        return;
    }

    /* NOTE OFF */
    if (local_id >= UI_MIDI_NOTE_OFF_BASE_LOCAL &&
        local_id <  (UI_MIDI_NOTE_OFF_BASE_LOCAL + 128u)) {
        const uint8_t note = (uint8_t)(local_id - UI_MIDI_NOTE_OFF_BASE_LOCAL);
        midi_note_off(dest, ch, note, 0);
        return;
    }

    /* TODO: ajouter plus tard CC/NRPN/etc. si tu mappes d'autres IDs */
}
