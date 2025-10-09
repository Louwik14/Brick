/**
 * @file ui_backend.c
 * @brief Pont neutre entre UI et couches basses (CartLink, UI interne, MIDI).
 * @details
 * Implémente la fonction de routage principale `ui_backend_param_changed()`,
 * qui distribue les changements de paramètres selon leur destination :
 *
 * - **CART** (`UI_DEST_CART`) → transmission vers `cart_link_param_changed()`.
 * - **UI interne** (`UI_DEST_UI`) → interception locale (`ui_backend_handle_ui()`).
 * - **MIDI** (`UI_DEST_MIDI`) → routage vers pile MIDI (`ui_backend_handle_midi()`).
 *
 * Les appels `ui_backend_shadow_get()` et `ui_backend_shadow_set()` restent
 * strictement associés à la cartouche active (shadow local).
 *
 * @ingroup ui
 */

#include "ui_backend.h"
#include "cart_link.h"       // cart_link_param_changed, cart_link_shadow_{get,set}
#include "cart_registry.h"   // cart_registry_get_active_id()
#include "brick_config.h"    // configuration globale (optionnel pour debug/log)
#include "ch.h"
#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------- */
/* Définitions internes de destination (masques 16 bits)                      */
/* -------------------------------------------------------------------------- */

#define UI_DEST_MASK   0xE000U
#define UI_DEST_CART   0x0000U  /**< Paramètre destiné à la cartouche active.  */
#define UI_DEST_UI     0x8000U  /**< Paramètre purement interne à l'UI.       */
#define UI_DEST_MIDI   0x4000U  /**< Paramètre routé vers la pile MIDI.       */
#define UI_DEST_ID(x)  ((x) & 0x1FFFU) /**< ID local sur 13 bits. */

/* -------------------------------------------------------------------------- */
/* Prototypes internes                                                        */
/* -------------------------------------------------------------------------- */

/**
 * @brief Gestion d'un paramètre interne UI (menu custom, séquenceur, etc.).
 * @param local_id  Identifiant local (13 bits).
 * @param val       Valeur brute.
 * @param bitwise   Mode bitmask (true = modif bits, false = valeur absolue).
 * @param mask      Masque de bits actif si `bitwise` est vrai.
 */
static void ui_backend_handle_ui(uint16_t local_id, uint8_t val, bool bitwise, uint8_t mask);

/**
 * @brief Gestion d'un paramètre MIDI (CC/NRPN/etc.).
 * @param local_id  Identifiant MIDI (CC ou autre).
 * @param val       Valeur brute (0–127 typiquement).
 */
static void ui_backend_handle_midi(uint16_t local_id, uint8_t val);

/* -------------------------------------------------------------------------- */
/* Implémentation                                                             */
/* -------------------------------------------------------------------------- */

void ui_backend_param_changed(uint16_t id, uint8_t val, bool bitwise, uint8_t mask) {
    uint16_t dest = id & UI_DEST_MASK;
    uint16_t did  = UI_DEST_ID(id);

    switch (dest) {
        case UI_DEST_CART:
            /* Routage classique vers la cartouche active */
            cart_link_param_changed(did, val, bitwise, mask);
            break;

        case UI_DEST_UI:
            /* Paramètre interne UI : ne passe pas par le bus cartouche */
            ui_backend_handle_ui(did, val, bitwise, mask);
            break;

        case UI_DEST_MIDI:
            /* Paramètre MIDI : routage vers la pile MIDI interne */
            ui_backend_handle_midi(did, val);
            break;

        default:
            /* Valeur hors plage connue : ignorée (sécurité) */
#if DEBUG_ENABLE
            debug_log("ui_backend: unknown dest %04x\n", dest);
#endif
            break;
    }
}

/* -------------------------------------------------------------------------- */
/* Shadow accessors (inchangés, cartouche active uniquement)                  */
/* -------------------------------------------------------------------------- */

uint8_t ui_backend_shadow_get(uint16_t id) {
    cart_id_t cid = cart_registry_get_active_id();
    return cart_link_shadow_get(cid, id);
}

void ui_backend_shadow_set(uint16_t id, uint8_t val) {
    cart_id_t cid = cart_registry_get_active_id();
    cart_link_shadow_set(cid, id, val);
}

/* -------------------------------------------------------------------------- */
/* Hooks internes — stubs par défaut                                          */
/* -------------------------------------------------------------------------- */

static void ui_backend_handle_ui(uint16_t local_id, uint8_t val, bool bitwise, uint8_t mask) {
    (void)local_id;
    (void)val;
    (void)bitwise;
    (void)mask;
#if DEBUG_ENABLE
    debug_log("[UI] id=%u val=%u (bitwise=%d)\n", local_id, val, bitwise);
#endif
    /* TODO: Implémenter la logique des menus UI internes ou SEQ ici */
}

static void ui_backend_handle_midi(uint16_t local_id, uint8_t val) {
    (void)local_id;
    (void)val;
#if DEBUG_ENABLE
    debug_log("[MIDI] id=%u val=%u\n", local_id, val);
#endif
    /* TODO: Intégrer midi_send_cc() ou midi_send_nrpn() selon mapping */
}
