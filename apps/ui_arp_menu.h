#ifndef BRICK_APPS_UI_ARP_MENU_H
#define BRICK_APPS_UI_ARP_MENU_H

// --- ARP: IDs UI du sous-menu Arpégiateur Keyboard ---

#include <stdint.h>
#include "ui_spec.h"
#include "ui_backend.h"  // Nécessaire pour UI_DEST_UI et les structures de menu

#ifdef __cplusplus
extern "C" {
#endif

// --- ARP: IDs locaux (espace UI_DEST_UI) ---
enum {
  KBD_ARP_LOCAL_ONOFF = 0x0200,
  KBD_ARP_LOCAL_RATE,
  KBD_ARP_LOCAL_OCT_RANGE,
  KBD_ARP_LOCAL_PATTERN,
  KBD_ARP_LOCAL_GATE,
  KBD_ARP_LOCAL_SWING,
  KBD_ARP_LOCAL_ACCENT,
  KBD_ARP_LOCAL_VEL_RAND,
  KBD_ARP_LOCAL_STRUM_MODE,
  KBD_ARP_LOCAL_STRUM_OFFSET,
  KBD_ARP_LOCAL_REPEAT,
  KBD_ARP_LOCAL_TRIGGER,
  KBD_ARP_LOCAL_TRANSPOSE,
  KBD_ARP_LOCAL_SPREAD,
  KBD_ARP_LOCAL_OCT_SHIFT,
  KBD_ARP_LOCAL_DIRECTION_BEHAV,
  KBD_ARP_LOCAL_PATTERN_SELECT,
  KBD_ARP_LOCAL_PATTERN_MORPH,
  KBD_ARP_LOCAL_LFO_TARGET,
  KBD_ARP_LOCAL_LFO_DEPTH,
  KBD_ARP_LOCAL_LFO_RATE,
  KBD_ARP_LOCAL_SYNC_MODE
};

// --- Macro de création d’ID global ---
#define KBD_ARP_UI_ID(local) ((uint16_t)(UI_DEST_UI | ((uint16_t)(local) & 0x1FFFu)))

// --- Spécification du menu ARP exportée ---
extern const ui_cart_spec_t ui_keyboard_arp_menu_spec;

#ifdef __cplusplus
}
#endif

#endif /* BRICK_APPS_UI_ARP_MENU_H */
