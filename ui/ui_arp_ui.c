/**
 * @file ui_arp_ui.c
 * @brief Définition du mode custom ARP (Mode + Setup).
 * @ingroup ui_modes
 *
 * @details
 * Deux menus dans la même cartouche UI :
 *  - Menu[0] : "ARP"  (pages Mode)
 *  - Menu[1] : "Setup" (pages MIDI)
 *
 *  Cycle BM1 entre les deux (resume=false).
 *  Tous les dest_id sont internes (UI_DEST_UI).
 *  Le label de bannière ("ARP") est fourni par le backend partagé.
 */

#include "ui_arp_ui.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

enum {
    /* --- ARP mode --- */
    ARP_ENABLE = 0x0300, // --- FIX: espace ID dédié overlay ARP ---
    ARP_RATE,
    ARP_OCTAVE,

    /* --- Setup --- */
    ARP_SETUP_SYNC = 0x0310,
    ARP_SETUP_CHANNEL
};

#define ARP_UI(idlocal) (UI_DEST_UI | ((uint16_t)(idlocal) & 0x1FFF))

static const char* const kArpOnOffLabels[] = { "Off", "On" };
static const char* const kArpRateLabels[] = { "1/1", "1/2", "1/4", "1/8", "1/16" };
static const char* const kArpSyncLabels[] = { "Int", "Ext" };
static const ui_page_spec_t kEmptyPage = {
    .params = { {0}, {0}, {0}, {0} },
    .header_label = NULL
};

static const ui_menu_spec_t arp_menu_mode = {
    .name = "ARP",
    .page_titles = { "Mode", "-", "-", "-", "-" },
    .pages = {
        {
            .params = {
                { .label="On/Off", .kind=UI_PARAM_ENUM, .dest_id=ARP_UI(ARP_ENABLE),
                  .meta.en={ .labels=kArpOnOffLabels, .count=2 }, .is_bitwise=false },
                { .label="Rate",   .kind=UI_PARAM_ENUM, .dest_id=ARP_UI(ARP_RATE),
                  .meta.en={ .labels=kArpRateLabels, .count=5 }, .is_bitwise=false },
                { .label="Oct",    .kind=UI_PARAM_CONT, .dest_id=ARP_UI(ARP_OCTAVE),
                  .meta.range={.min=1,.max=4,.step=1}, .is_bitwise=false },
                { .label="-", .kind=UI_PARAM_NONE }
            },
            .header_label = "Mode"
        },
        kEmptyPage,
        kEmptyPage,
        kEmptyPage,
        kEmptyPage
    }
};

static const ui_menu_spec_t arp_menu_setup = {
    .name = "Setup",
    .page_titles = { "Setup", "-", "-", "-", "-" },
    .pages = {
        {
            .params = {
                { .label="Sync", .kind=UI_PARAM_ENUM, .dest_id=ARP_UI(ARP_SETUP_SYNC),
                  .meta.en={ .labels=kArpSyncLabels, .count=2 }, .is_bitwise=false },
                { .label="Chan", .kind=UI_PARAM_CONT, .dest_id=ARP_UI(ARP_SETUP_CHANNEL),
                  .meta.range={.min=1,.max=16,.step=1}, .is_bitwise=false },
                { .label="-", .kind=UI_PARAM_NONE },
                { .label="-", .kind=UI_PARAM_NONE }
            },
            .header_label = "Setup"
        },
        kEmptyPage,
        kEmptyPage,
        kEmptyPage,
        kEmptyPage
    }
};

/* ============================================================
 * Cartouches UI exposées
 * ============================================================ */
const ui_cart_spec_t arp_ui_spec = {
    .cart_name = "ARP UI",
    .menus = {
        [0] = arp_menu_mode,
        [1] = arp_menu_setup
    },
    .cycles = {
        [0] = { .count=2, .idxs={0,1}, .resume=false },
        [1] = { .count=0 }, [2] = { .count=0 }, [3] = { .count=0 },
        [4] = { .count=0 }, [5] = { .count=0 }, [6] = { .count=0 }, [7] = { .count=0 }
    }
};

const ui_cart_spec_t arp_setup_ui_spec = {
    .cart_name = "ARP SETUP UI",
    .menus = { [0] = arp_menu_setup },
    .cycles = { [0] = { .count = 0 } }
};
