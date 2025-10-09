/**
 * @file cart_xva1_spec.h
 * @brief Définition des menus et labels spécifiques à la cartouche **XVA1** (spécification UI).
 *
 * @ingroup cart
 *
 * @details
 * Ce module expose la spécification UI de la cartouche XVA1 :
 * - Tables de labels spécifiques à XVA1 (formes d’onde, filtres, FX, etc.)
 * - Menus (`ui_menu_spec_t`) et cartouche complète (`ui_cart_spec_t`)
 *
 * Il ne contient aucune logique de communication (UART, bus, etc.)
 * et ne dépend que des structures de l’UI (`ui_spec.h`).
 *
 * @note Les labels strictement universels (ex. On/Off) sont fournis par
 *       ui_labels_common.h et utilisés via des alias.
 */

#ifndef CART_XVA1_SPEC_H
#define CART_XVA1_SPEC_H

#include "ui_spec.h"

/* =======================================================
 *   Tables de labels (définies dans cart_xva1_spec.c)
 * ======================================================= */
extern const char* const onOff[2];
extern const char* const filterTypes[22];
extern const char* const lfoWaves[10];
extern const char* const oscWaves[9];
extern const char* const distType[4];
extern const char* const fxRouting[3];
extern const char* const reverbModes[2];
extern const char* const delayModes[3];
extern const char* const chorusModes[4];
extern const char* const phaserModes[3];

/* =======================================================
 *   Spécification de la cartouche (menus)
 * ======================================================= */
extern const ui_cart_spec_t CART_XVA1;

/* Menus exposés (pour cycles BMx) */
extern const ui_menu_spec_t XVA1_MENU_OSC1;
extern const ui_menu_spec_t XVA1_MENU_OSC2;
extern const ui_menu_spec_t XVA1_MENU_OSC3;
extern const ui_menu_spec_t XVA1_MENU_OSC4;
extern const ui_menu_spec_t XVA1_MENU_FILTER;

extern const ui_menu_spec_t XVA1_MENU_ENV_FILTER;
extern const ui_menu_spec_t XVA1_MENU_ENV_AMP;
extern const ui_menu_spec_t XVA1_MENU_ENV_PITCH;

extern const ui_menu_spec_t XVA1_MENU_LFO12;
extern const ui_menu_spec_t XVA1_MENU_LFO_MIDIMOD;
extern const ui_menu_spec_t XVA1_MENU_MIDI_GLOBAL;

extern const ui_menu_spec_t XVA1_MENU_FX1;
extern const ui_menu_spec_t XVA1_MENU_FX2;
extern const ui_menu_spec_t XVA1_MENU_FX3;
extern const ui_menu_spec_t XVA1_MENU_FX4;

#endif /* CART_XVA1_SPEC_H */
