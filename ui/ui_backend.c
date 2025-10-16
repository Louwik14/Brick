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

#include "ui_shortcuts.h"
#include "ui_led_backend.h"
#include "ui_model.h"
#include "ui_overlay.h"
#include "ui_mute_backend.h"
#include "seq_led_bridge.h"
#include "seq_engine_runner.h"
#include "seq_recorder.h"
#include "clock_manager.h"
#include "ui_seq_ui.h"
#include "ui_seq_ids.h"
#include "ui_arp_ui.h"
#include "ui_arp_menu.h" // --- ARP: sous-menu clavier ---
#include "ui_keyboard_bridge.h" // --- ARP: STOP clavier ---
#include "ui_keyboard_ui.h"
#include "ui_keyboard_app.h"
#include "ui_controller.h"
#include "kbd_input_mapper.h"

#include "midi.h"                /* midi_note_on/off(), midi_cc() */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/* -------------------------------------------------------------------------- */
/* Runtime UI mode context + helpers                                          */
/* -------------------------------------------------------------------------- */

static ui_mode_context_t s_mode_ctx;
static char s_mode_label[8] = "SEQ";

static const ui_cart_spec_t *s_seq_mode_spec_banner   = &seq_ui_spec;
static const ui_cart_spec_t *s_seq_setup_spec_banner  = &seq_setup_ui_spec;
static const ui_cart_spec_t *s_arp_mode_spec_banner   = &arp_ui_spec;
static const ui_cart_spec_t *s_arp_setup_spec_banner  = &arp_setup_ui_spec;
static const ui_cart_spec_t *s_kbd_keyboard_spec_banner     = &ui_keyboard_spec; // --- ARP: vitrine clavier ---
static const ui_cart_spec_t *s_kbd_arp_config_spec_banner   = &ui_keyboard_arp_menu_spec; // --- ARP: vitrine arpégiateur ---

static void _set_mode_label(const char *label) {
    if (!label || label[0] == '\0') {
        label = "SEQ";
    }
    (void)snprintf(s_mode_label, sizeof(s_mode_label), "%s", label);
    s_mode_label[sizeof(s_mode_label) - 1U] = '\0';
    ui_model_set_active_overlay_tag(s_mode_label);
}

static void _reset_overlay_banner_tags(void) {
    const char *tag = ui_backend_get_mode_label();
    ui_overlay_update_banner_tag(tag);
}

static void _publish_keyboard_tag(int8_t shift) {
    char tag[32];
    if (shift == 0) {
        (void)snprintf(tag, sizeof(tag), "KEY");
    } else {
        (void)snprintf(tag, sizeof(tag), "KEY%+d", (int)shift);
    }
    _set_mode_label(tag);
}

static void _neutralize_overlay_for_mute(void) {
    if (!ui_overlay_is_active()) {
        return;
    }
    ui_overlay_update_banner_tag(NULL);
}

static void _restore_overlay_visuals_after_mute(void) {
    const ui_cart_spec_t *cur = ui_overlay_get_spec();
    if (cur == s_seq_mode_spec_banner || cur == s_seq_setup_spec_banner) {
        _set_mode_label("SEQ");
        _reset_overlay_banner_tags();
        ui_led_backend_set_mode(UI_LED_MODE_SEQ);
        s_mode_ctx.overlay_active     = true;
        s_mode_ctx.overlay_id         = UI_OVERLAY_SEQ;
        s_mode_ctx.overlay_submode    = (cur == s_seq_setup_spec_banner) ? 1u : 0u;
        s_mode_ctx.keyboard.overlay_visible = false;
    } else if (cur == s_arp_mode_spec_banner || cur == s_arp_setup_spec_banner) {
        _set_mode_label("ARP");
        _reset_overlay_banner_tags();
        ui_led_backend_set_mode(UI_LED_MODE_ARP);
        s_mode_ctx.overlay_active     = true;
        s_mode_ctx.overlay_id         = UI_OVERLAY_ARP;
        s_mode_ctx.overlay_submode    = (cur == s_arp_setup_spec_banner) ? 1u : 0u;
        s_mode_ctx.keyboard.overlay_visible = false;
    } else if ((cur == s_kbd_keyboard_spec_banner) || (cur == s_kbd_arp_config_spec_banner)) {
        s_mode_ctx.overlay_active     = true;
        s_mode_ctx.overlay_id         = UI_OVERLAY_SEQ;
        s_mode_ctx.keyboard.arp_submenu_active = (cur == s_kbd_arp_config_spec_banner); // --- ARP: restaurer sélection ---
        s_mode_ctx.overlay_submode    = s_mode_ctx.keyboard.arp_submenu_active ? 1u : 0u;
        s_mode_ctx.keyboard.overlay_visible = true;
        _publish_keyboard_tag(s_mode_ctx.keyboard.octave);
        ui_overlay_update_banner_tag(ui_backend_get_mode_label());
        ui_led_backend_set_mode(UI_LED_MODE_KEYBOARD);
    } else {
        if (s_mode_ctx.keyboard.active) {
            _publish_keyboard_tag(s_mode_ctx.keyboard.octave);
            ui_overlay_update_banner_tag(ui_backend_get_mode_label());
            ui_led_backend_set_mode(UI_LED_MODE_KEYBOARD);
        } else {
            _set_mode_label("SEQ");
            _reset_overlay_banner_tags();
            ui_led_backend_set_mode(UI_LED_MODE_SEQ);
        }
    }
}

static void _update_seq_runtime_from_bridge(void) {
    s_mode_ctx.seq.page_count = seq_led_bridge_get_max_pages();
    s_mode_ctx.seq.page_index = seq_led_bridge_get_visible_page();
}

static bool _resolve_seq_param(uint16_t local_id,
                               seq_hold_param_id_t *out_param,
                               const ui_param_state_t **out_state) {
    ui_state_t *state = ui_model_get_state();
    if (state == NULL) {
        return false;
    }

    if (local_id <= SEQ_UI_LOCAL_ALL_MIC) {
        if (out_param != NULL) {
            *out_param = (seq_hold_param_id_t)(SEQ_HOLD_PARAM_ALL_TRANSP + (local_id - SEQ_UI_LOCAL_ALL_TRANSP));
        }
        if (out_state != NULL) {
            *out_state = &state->vals.menus[0].pages[0].params[local_id - SEQ_UI_LOCAL_ALL_TRANSP];
        }
        return true;
    }

    if (local_id >= SEQ_UI_LOCAL_V1_NOTE && local_id <= SEQ_UI_LOCAL_V4_MIC) {
        uint16_t rel = (uint16_t)(local_id - SEQ_UI_LOCAL_V1_NOTE);
        uint8_t voice = (uint8_t)(rel / 4U);
        uint8_t slot = (uint8_t)(rel % 4U);
        if (voice >= 4U || slot >= 4U) {
            return false;
        }
        if (out_param != NULL) {
            *out_param = (seq_hold_param_id_t)(SEQ_HOLD_PARAM_V1_NOTE + rel);
        }
        if (out_state != NULL) {
            *out_state = &state->vals.menus[0].pages[1U + voice].params[slot];
        }
        return true;
    }

    return false;
}

static void _handle_shortcut_action(const ui_shortcut_action_t *act);
static void _route_default_event(const ui_input_event_t *evt, bool consumed);

const ui_mode_context_t *ui_backend_get_mode_context(void) {
    return &s_mode_ctx;
}

const char *ui_backend_get_mode_label(void) {
    if (s_mode_label[0] == '\0') {
        _set_mode_label("SEQ");
    }
    return s_mode_label;
}

void ui_backend_process_input(const ui_input_event_t *evt) {
    if (!evt) {
        return;
    }

    ui_shortcut_map_result_t map = ui_shortcut_map_process(evt, &s_mode_ctx);

    for (uint8_t i = 0; i < map.action_count; ++i) {
        _handle_shortcut_action(&map.actions[i]);
    }

    _update_seq_runtime_from_bridge();

    _route_default_event(evt, map.consumed);
}

static void _apply_seq_overlay_cycle(void) {
    const ui_cart_spec_t *cart_spec = ui_overlay_get_host_cart();
    if (!cart_spec) {
        cart_spec = ui_get_cart();
    }
    _set_mode_label("SEQ");
    ui_overlay_prepare_banner(&seq_ui_spec, &seq_setup_ui_spec,
                              &s_seq_mode_spec_banner, &s_seq_setup_spec_banner,
                              cart_spec, ui_backend_get_mode_label());
    _reset_overlay_banner_tags();

    if (!ui_overlay_is_active()) {
        ui_overlay_enter(UI_OVERLAY_SEQ, s_seq_mode_spec_banner);
        s_mode_ctx.overlay_submode = 0u;
    } else if (ui_overlay_get_spec() == s_seq_mode_spec_banner) {
        ui_overlay_switch_subspec(s_seq_setup_spec_banner);
        s_mode_ctx.overlay_submode = 1u;
    } else if (ui_overlay_get_spec() == s_seq_setup_spec_banner) {
        ui_overlay_switch_subspec(s_seq_mode_spec_banner);
        s_mode_ctx.overlay_submode = 0u;
    } else {
        ui_overlay_enter(UI_OVERLAY_SEQ, s_seq_mode_spec_banner);
        s_mode_ctx.overlay_submode = 0u;
    }

    s_mode_ctx.overlay_active            = true;
    s_mode_ctx.overlay_id                = UI_OVERLAY_SEQ;
    s_mode_ctx.custom_mode               = UI_CUSTOM_SEQ;
    s_mode_ctx.keyboard.active           = false;
    s_mode_ctx.keyboard.overlay_visible  = false;

    ui_overlay_set_custom_mode(UI_CUSTOM_SEQ);
    ui_led_backend_set_mode(UI_LED_MODE_SEQ);
    ui_mark_dirty();
}

static void _apply_arp_overlay_cycle(void) {
    const ui_cart_spec_t *cart_spec = ui_overlay_get_host_cart();
    if (!cart_spec) {
        cart_spec = ui_get_cart();
    }
    _set_mode_label("ARP");
    ui_overlay_prepare_banner(&arp_ui_spec, &arp_setup_ui_spec,
                              &s_arp_mode_spec_banner, &s_arp_setup_spec_banner,
                              cart_spec, ui_backend_get_mode_label());
    _reset_overlay_banner_tags();

    if (!ui_overlay_is_active()) {
        ui_overlay_enter(UI_OVERLAY_ARP, s_arp_mode_spec_banner);
        s_mode_ctx.overlay_submode = 0u;
    } else if (ui_overlay_get_spec() == s_arp_mode_spec_banner) {
        ui_overlay_switch_subspec(s_arp_setup_spec_banner);
        s_mode_ctx.overlay_submode = 1u;
    } else if (ui_overlay_get_spec() == s_arp_setup_spec_banner) {
        ui_overlay_switch_subspec(s_arp_mode_spec_banner);
        s_mode_ctx.overlay_submode = 0u;
    } else {
        ui_overlay_enter(UI_OVERLAY_ARP, s_arp_mode_spec_banner);
        s_mode_ctx.overlay_submode = 0u;
    }

    s_mode_ctx.overlay_active            = true;
    s_mode_ctx.overlay_id                = UI_OVERLAY_ARP;
    s_mode_ctx.custom_mode               = UI_CUSTOM_ARP;
    s_mode_ctx.keyboard.active           = false;
    s_mode_ctx.keyboard.overlay_visible  = false;

    ui_overlay_set_custom_mode(UI_CUSTOM_ARP);
    ui_led_backend_set_mode(UI_LED_MODE_ARP);
    ui_mark_dirty();
}

static void _prepare_keyboard_specs(void) {
    const ui_cart_spec_t *cart_spec = ui_overlay_get_host_cart();
    if (!cart_spec) {
        cart_spec = ui_get_cart();
    }
    const char *banner = (cart_spec && cart_spec->cart_name) ? cart_spec->cart_name : "";
    s_kbd_keyboard_spec_banner   = &ui_keyboard_spec;
    s_kbd_arp_config_spec_banner = &ui_keyboard_arp_menu_spec;
    ui_overlay_set_banner_override(banner, NULL);
}

static void _apply_keyboard_overlay(void) {
    _prepare_keyboard_specs();

    const ui_cart_spec_t *current = ui_overlay_is_active() ? ui_overlay_get_spec() : NULL;
    const ui_cart_spec_t *target = s_mode_ctx.keyboard.arp_submenu_active
                                       ? s_kbd_arp_config_spec_banner
                                       : s_kbd_keyboard_spec_banner;

    if (!ui_overlay_is_active() ||
        (current != s_kbd_keyboard_spec_banner && current != s_kbd_arp_config_spec_banner)) {
        ui_overlay_enter(UI_OVERLAY_SEQ, target);
    } else if (current != target) {
        ui_overlay_switch_subspec(target);
    }

    s_mode_ctx.keyboard.active          = true;
    s_mode_ctx.keyboard.overlay_visible = true;
    s_mode_ctx.keyboard.octave          = ui_keyboard_app_get_octave_shift();
    s_mode_ctx.custom_mode              = UI_CUSTOM_NONE;
    s_mode_ctx.overlay_active           = true;
    s_mode_ctx.overlay_id               = UI_OVERLAY_SEQ;
    s_mode_ctx.overlay_submode          = s_mode_ctx.keyboard.arp_submenu_active ? 1u : 0u;

    ui_overlay_set_custom_mode(UI_CUSTOM_NONE);
    ui_led_backend_set_mode(UI_LED_MODE_KEYBOARD);
    _publish_keyboard_tag(s_mode_ctx.keyboard.octave);
    ui_overlay_update_banner_tag(ui_backend_get_mode_label());
    ui_mark_dirty();
}

static void _keyboard_toggle_submenu(void) {
    s_mode_ctx.keyboard.arp_submenu_active = !s_mode_ctx.keyboard.arp_submenu_active; // --- ARP: cycle submenu ---
    _apply_keyboard_overlay();
}

static void _handle_shortcut_action(const ui_shortcut_action_t *act) {
    if (!act) {
        return;
    }

    switch (act->type) {
    case UI_SHORTCUT_ACTION_ENTER_MUTE_QUICK:
        s_mode_ctx.mute_state = UI_MUTE_STATE_QUICK;
        _neutralize_overlay_for_mute();
        ui_led_backend_set_mode(UI_LED_MODE_MUTE);
        _set_mode_label("MUTE");
        ui_mark_dirty();
        break;

    case UI_SHORTCUT_ACTION_ENTER_MUTE_PMUTE:
        s_mode_ctx.mute_state = UI_MUTE_STATE_PMUTE;
        _neutralize_overlay_for_mute();
        ui_led_backend_set_mode(UI_LED_MODE_MUTE);
        _set_mode_label("PMUTE");
        ui_mute_backend_publish_state(); // --- FIX: re-synchroniser les LEDs préparées à chaque entrée PMUTE ---
        ui_mark_dirty();
        break;

    case UI_SHORTCUT_ACTION_EXIT_MUTE:
        s_mode_ctx.mute_state       = UI_MUTE_STATE_OFF;
        s_mode_ctx.mute_plus_down   = false;
        s_mode_ctx.mute_shift_latched = ui_input_shift_is_pressed();
        ui_mute_backend_cancel();
        _reset_overlay_banner_tags();
        _restore_overlay_visuals_after_mute();
        ui_mark_dirty();
        break;

    case UI_SHORTCUT_ACTION_TOGGLE_MUTE_TRACK:
        ui_mute_backend_toggle(act->data.mute.track);
        break;

    case UI_SHORTCUT_ACTION_PREPARE_PMUTE_TRACK:
        ui_mute_backend_toggle_prepare(act->data.mute.track);
        break;

    case UI_SHORTCUT_ACTION_COMMIT_PMUTE:
        ui_mute_backend_commit();
        s_mode_ctx.mute_state = UI_MUTE_STATE_OFF;
        _reset_overlay_banner_tags();
        _restore_overlay_visuals_after_mute();
        ui_mark_dirty();
        break;

    case UI_SHORTCUT_ACTION_OPEN_SEQ_OVERLAY:
        _apply_seq_overlay_cycle();
        break;

    case UI_SHORTCUT_ACTION_OPEN_ARP_OVERLAY:
        _apply_arp_overlay_cycle();
        break;

    case UI_SHORTCUT_ACTION_OPEN_KBD_OVERLAY:
        _apply_keyboard_overlay();
        break;
    case UI_SHORTCUT_ACTION_KEYBOARD_TOGGLE_SUBMENU:
        if (!ui_overlay_is_active() ||
            (ui_overlay_get_spec() != s_kbd_keyboard_spec_banner &&
             ui_overlay_get_spec() != s_kbd_arp_config_spec_banner)) {
            _apply_keyboard_overlay();
        } else {
            _keyboard_toggle_submenu();
        }
        break;

    case UI_SHORTCUT_ACTION_TRANSPORT_PLAY:
        seq_engine_runner_on_transport_play();
        clock_manager_start();
        seq_led_bridge_on_play();
        s_mode_ctx.transport.playing = true;
        break;

    case UI_SHORTCUT_ACTION_TRANSPORT_STOP:
        ui_keyboard_bridge_on_transport_stop(); // --- ARP: flush avant STOP ---
        seq_engine_runner_on_transport_stop();
        clock_manager_stop();
        seq_led_bridge_on_stop();
        s_mode_ctx.transport.playing = false;
        break;

    case UI_SHORTCUT_ACTION_TRANSPORT_REC_TOGGLE:
        s_mode_ctx.transport.recording = !s_mode_ctx.transport.recording;
        ui_led_backend_set_record_mode(s_mode_ctx.transport.recording);
        seq_recorder_set_recording(s_mode_ctx.transport.recording);
        break;

    case UI_SHORTCUT_ACTION_SEQ_PAGE_NEXT:
        seq_led_bridge_page_next();
        ui_led_backend_set_mode(UI_LED_MODE_SEQ);
        _set_mode_label("SEQ");
        _reset_overlay_banner_tags();
        ui_mark_dirty();
        break;

    case UI_SHORTCUT_ACTION_SEQ_PAGE_PREV:
        seq_led_bridge_page_prev();
        ui_led_backend_set_mode(UI_LED_MODE_SEQ);
        _set_mode_label("SEQ");
        _reset_overlay_banner_tags();
        ui_mark_dirty();
        break;

    case UI_SHORTCUT_ACTION_SEQ_STEP_HOLD:
        seq_led_bridge_plock_add(act->data.seq_step.index);
        seq_led_bridge_begin_plock_preview(s_mode_ctx.seq.held_mask);
        ui_mark_dirty();
        break;

    case UI_SHORTCUT_ACTION_SEQ_STEP_RELEASE:
        seq_led_bridge_plock_remove(act->data.seq_step.index);
        if (s_mode_ctx.seq.held_mask != 0U) {
            seq_led_bridge_begin_plock_preview(s_mode_ctx.seq.held_mask);
        } else {
            seq_led_bridge_end_plock_preview();
        }
        if (!act->data.seq_step.long_press) {
            seq_led_bridge_quick_toggle_step(act->data.seq_step.index);
        }
        ui_mark_dirty();
        break;

    case UI_SHORTCUT_ACTION_SEQ_ENCODER_TOUCH: {
        uint16_t mask = act->data.seq_mask.mask;
        for (uint8_t i = 0; i < 16; ++i) {
            if (mask & (1u << i)) {
                seq_led_bridge_set_step_param_only(i, true);
            }
        }
        seq_led_bridge_begin_plock_preview(mask);
        ui_mark_dirty();
        break;
    }

    case UI_SHORTCUT_ACTION_KEY_OCTAVE_UP: {
        int8_t shift = s_mode_ctx.keyboard.octave;
        if (shift < CUSTOM_KEYS_OCT_SHIFT_MAX) {
            shift++;
            s_mode_ctx.keyboard.octave = shift;
            ui_keyboard_app_set_octave_shift(shift);
            _publish_keyboard_tag(shift);
            ui_mark_dirty();
        }
        break;
    }

    case UI_SHORTCUT_ACTION_KEY_OCTAVE_DOWN: {
        int8_t shift = s_mode_ctx.keyboard.octave;
        if (shift > CUSTOM_KEYS_OCT_SHIFT_MIN) {
            shift--;
            s_mode_ctx.keyboard.octave = shift;
            ui_keyboard_app_set_octave_shift(shift);
            _publish_keyboard_tag(shift);
            ui_mark_dirty();
        }
        break;
    }

    case UI_SHORTCUT_ACTION_NONE:
    default:
        break;
    }
}

static void _route_default_event(const ui_input_event_t *evt, bool consumed) {
    if (!evt || consumed) {
        return;
    }

    if (evt->has_button) {
        if (evt->btn_id >= UI_BTN_SEQ1 && evt->btn_id <= UI_BTN_SEQ16) {
            const uint8_t seq_index = (uint8_t)(1u + (evt->btn_id - UI_BTN_SEQ1));
            kbd_input_mapper_process(seq_index, evt->btn_pressed);
        } else if (evt->btn_pressed) {
            switch (evt->btn_id) {
            case UI_BTN_PARAM1: ui_on_button_menu(0); break;
            case UI_BTN_PARAM2: ui_on_button_menu(1); break;
            case UI_BTN_PARAM3: ui_on_button_menu(2); break;
            case UI_BTN_PARAM4: ui_on_button_menu(3); break;
            case UI_BTN_PARAM5: ui_on_button_menu(4); break;
            case UI_BTN_PARAM6: ui_on_button_menu(5); break;
            case UI_BTN_PARAM7: ui_on_button_menu(6); break;
            case UI_BTN_PARAM8: ui_on_button_menu(7); break;

            case UI_BTN_PAGE1: ui_on_button_page(0); break;
            case UI_BTN_PAGE2: ui_on_button_page(1); break;
            case UI_BTN_PAGE3: ui_on_button_page(2); break;
            case UI_BTN_PAGE4: ui_on_button_page(3); break;
            case UI_BTN_PAGE5: ui_on_button_page(4); break;

            default:
                break;
            }
        }
    }

    if (evt->has_encoder && evt->enc_delta != 0) {
        ui_on_encoder((int)evt->encoder, (int)evt->enc_delta);
    }
}

/* -------------------------------------------------------------------------- */
/* Masques de destination (répliqués pour compilation locale)                 */
/* -------------------------------------------------------------------------- */
#define UI_DEST_MASK   0xE000U
#define UI_DEST_CART   0x0000U  /**< Paramètre destiné à la cartouche active.  */
#define UI_DEST_UI     0x8000U  /**< Paramètre purement interne à l'UI.       */
#define UI_DEST_MIDI   0x4000U  /**< Paramètre routé vers la pile MIDI.       */
#define UI_DEST_ID(x)  ((x) & 0x1FFFU) /**< ID local sur 13 bits. */

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
#define UI_BACKEND_UI_SHADOW_MAX  512u // --- FIX: étendre le cache UI pour isoler les modes custom ---
#endif

static CCM_DATA ui_local_kv_t s_ui_shadow[UI_BACKEND_UI_SHADOW_MAX];
static uint16_t      s_ui_shadow_count = 0;      // --- FIX: suivre la nouvelle capacité étendue ---
static uint16_t      s_ui_shadow_next_evict = 0; // --- FIX: pointeur de remplacement circulaire ---

/* Cherche l’index d’un id (UI_DEST_UI | local) dans la table ; -1 si absent. */
static int _ui_shadow_find(uint16_t id_full) {
    for (uint16_t i = 0; i < s_ui_shadow_count; ++i) {
        if (s_ui_shadow[i].id == id_full) return (int)i;
    }
    return -1;
}

static void _ui_shadow_set(uint16_t id_full, uint8_t v) {
    int idx = _ui_shadow_find(id_full);
    if (idx >= 0) {
        s_ui_shadow[(uint16_t)idx].val = v; // --- FIX: gérer >255 entrées sans tronquer l'index ---
        return;
    }
    if (s_ui_shadow_count < UI_BACKEND_UI_SHADOW_MAX) {
        s_ui_shadow[s_ui_shadow_count].id  = id_full;
        s_ui_shadow[s_ui_shadow_count].val = v;
        s_ui_shadow_count++;
        return;
    }
    /* --- FIX: table saturée → rotation pour éviter d'écraser un autre mode --- */
    s_ui_shadow[s_ui_shadow_next_evict].id  = id_full;
    s_ui_shadow[s_ui_shadow_next_evict].val = v;
    s_ui_shadow_next_evict = (uint16_t)((s_ui_shadow_next_evict + 1u) % UI_BACKEND_UI_SHADOW_MAX);
}

static uint8_t _ui_shadow_get(uint16_t id_full) {
    int idx = _ui_shadow_find(id_full);
    return (idx >= 0) ? s_ui_shadow[(uint16_t)idx].val : 0u; // --- FIX: accès sécurisé au cache étendu ---
}

void ui_backend_init_runtime(void) {
    ui_shortcut_map_init(&s_mode_ctx);
    s_mode_ctx.custom_mode     = ui_overlay_get_custom_mode();
    s_mode_ctx.overlay_id      = UI_OVERLAY_NONE;
    s_mode_ctx.overlay_active  = false;
    s_mode_ctx.overlay_submode = 0u;
    s_mode_ctx.keyboard.octave = ui_keyboard_app_get_octave_shift();
    s_mode_ctx.keyboard.arp_submenu_active = false; // --- ARP: état initial ---
    s_mode_ctx.transport.playing   = false;
    s_mode_ctx.transport.recording = false;

    memset(s_ui_shadow, 0, sizeof(s_ui_shadow));           // --- FIX: purge du cache étendu par mode ---
    s_ui_shadow_count      = 0;                            // --- FIX: reset compteur shadow isolé ---
    s_ui_shadow_next_evict = 0;                            // --- FIX: reset pointeur round-robin ---

    _set_mode_label("SEQ");
    _reset_overlay_banner_tags();
    _update_seq_runtime_from_bridge();

    ui_led_backend_set_mode(UI_LED_MODE_SEQ);
}

/* -------------------------------------------------------------------------- */
/* Prototypes internes                                                        */
/* -------------------------------------------------------------------------- */
static void ui_backend_handle_ui(uint16_t local_id, uint8_t val, bool bitwise, uint8_t mask);
static void ui_backend_handle_midi(uint16_t local_id, uint8_t val);

/* -------------------------------------------------------------------------- */
/* Implémentation                                                             */
/* -------------------------------------------------------------------------- */

void ui_backend_param_changed(uint16_t id, uint8_t val, bool bitwise, uint8_t mask) {
    const uint16_t dest     = (id & UI_DEST_MASK);
    const uint16_t local_id = UI_DEST_ID(id);

    switch (dest) {
    case UI_DEST_CART:
        if (s_mode_ctx.seq.held_mask != 0U) {
            seq_led_bridge_apply_cart_param(local_id, (int32_t)val, s_mode_ctx.seq.held_mask);
            seq_led_bridge_begin_plock_preview(s_mode_ctx.seq.held_mask);
            ui_mark_dirty();
            break;
        }
        /* Route vers la cartouche active (shadow + éventuelle propagation) */
        cart_link_param_changed(local_id, val, bitwise, mask);
        break;

    case UI_DEST_UI: {
        /* Met à jour le shadow UI **avant** la notification locale */
        uint8_t newv = val;

        if (bitwise) {
            /* Lire l’état courant, appliquer le masque, puis stocker. */
            const uint8_t prev = _ui_shadow_get(id);
            uint8_t reg = prev;
            if (mask) {
                /* bitwise + mask → activer/désactiver les bits */
                if (val) reg |= mask;
                else     reg &= (uint8_t)~mask;
            }
            newv = reg;
        }

        if (s_mode_ctx.seq.held_mask == 0U) {
            _ui_shadow_set(id, newv);
        }

        /* Interception locale UI (facultatif) */
        ui_backend_handle_ui(local_id, newv, bitwise, mask);
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

bool ui_backend_shadow_try_get(uint16_t id, uint8_t *out_val) {
    const uint16_t dest = (id & UI_DEST_MASK);
    if (dest == UI_DEST_UI) {
        int idx = _ui_shadow_find(id);
        if (idx < 0) {
            if (out_val) {
                *out_val = 0u;
            }
            return false; // --- FIX: shadow jamais initialisé pour ce paramètre UI ---
        }
        if (out_val) {
            *out_val = s_ui_shadow[(uint16_t)idx].val; // --- FIX: lecture safe du cache étendu ---
        }
        return true;
    }
    cart_id_t cid = cart_registry_get_active_id();
    uint8_t val = cart_link_shadow_get(cid, id);
    if (out_val) {
        *out_val = val;
    }
    return true;
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
static void ui_backend_handle_ui(uint16_t local_id, uint8_t val, bool bitwise, uint8_t mask) {
    (void)val;
    (void)bitwise;
    (void)mask;

    if (s_mode_ctx.seq.held_mask == 0U) {
        return;
    }

    seq_hold_param_id_t param_id;
    const ui_param_state_t *state_param = NULL;
    if (!_resolve_seq_param(local_id, &param_id, &state_param)) {
        return;
    }

    int32_t value = 0;
    if (state_param != NULL) {
        value = state_param->value;
    }

    seq_led_bridge_apply_plock_param(param_id, value, s_mode_ctx.seq.held_mask);
    seq_led_bridge_begin_plock_preview(s_mode_ctx.seq.held_mask);
    ui_mark_dirty();
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
