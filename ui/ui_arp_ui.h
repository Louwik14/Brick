/**
 * @file ui_arp_ui.h
 * @brief Spécification UI interne du mode ARP (Mode + Setup).
 * @ingroup ui_modes
 *
 * @details
 * Cartouche virtuelle purement UI (aucune interaction bus/cart).
 * Sert de démonstration pour les overlays custom.
 */

#ifndef BRICK_UI_ARP_UI_H
#define BRICK_UI_ARP_UI_H

#include "ui_spec.h"
#include "ui_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Cartouche virtuelle du mode ARP (pages principales). */
extern const ui_cart_spec_t arp_ui_spec;

/** @brief Cartouche virtuelle du sous-mode SETUP du ARP. */
extern const ui_cart_spec_t arp_setup_ui_spec;

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_ARP_UI_H */
