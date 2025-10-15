#ifndef BRICK_APPS_UI_ARP_MENU_H
#define BRICK_APPS_UI_ARP_MENU_H

// --- ARP: IDs UI du sous-menu Arpégiateur Keyboard ---

// --- ARP FIX: expose UI_DEST_UI and UI spec types ---
#include <stdint.h>
#include "ui_spec.h"
#include "ui_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- ARP FIX: grille UI actualisée (Hold group, VelAcc, Sync déplacé) ---
enum {
  KBD_ARP_LOCAL_HOLD = 0x0200,
  KBD_ARP_LOCAL_RATE,
  KBD_ARP_LOCAL_OCT_RANGE,
  KBD_ARP_LOCAL_PATTERN,
  KBD_ARP_LOCAL_GATE,
  KBD_ARP_LOCAL_SWING,
  KBD_ARP_LOCAL_ACCENT,
  KBD_ARP_LOCAL_VEL_ACC,
  KBD_ARP_LOCAL_STRUM_MODE,
  KBD_ARP_LOCAL_STRUM_OFFSET,
  KBD_ARP_LOCAL_REPEAT,
  KBD_ARP_LOCAL_TRANSPOSE,
  KBD_ARP_LOCAL_SPREAD,
  /* 0x20D laissé libre pour l'ancien OctSh afin de préserver les états shadow. */
  KBD_ARP_LOCAL_DIRECTION_BEHAV = 0x020E,
  KBD_ARP_LOCAL_SYNC_MODE
};

#define KBD_ARP_UI_ID(local) (uint16_t)(UI_DEST_UI | ((uint16_t)(local) & 0x1FFFu))

extern const ui_cart_spec_t ui_keyboard_arp_menu_spec;

#ifdef __cplusplus
}
#endif

#endif /* BRICK_APPS_UI_ARP_MENU_H */
