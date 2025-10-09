/**
 * @file ui_backend.h
 * @brief Pont neutre entre la couche UI et les couches basses (CartLink, UI interne, MIDI).
 *
 * @defgroup ui_backend UI Backend Bridge
 * @ingroup ui
 *
 * @details
 * Cette interface agit comme un adaptateur : elle permet à la couche UI
 * (controller, renderer, widgets) d’interagir avec les couches inférieures
 * sans dépendre directement de `cart_link`.
 *
 * Depuis la refonte 2025, le backend gère plusieurs destinations logiques :
 *
 * - **UI_DEST_CART** : paramètres destinés à la cartouche active (comportement historique)
 * - **UI_DEST_UI**   : paramètres internes à l’UI (menus custom, séquenceur)
 * - **UI_DEST_MIDI** : paramètres routés vers la pile MIDI (ex. CC, NRPN)
 *
 * Le routage est assuré dans `ui_backend_param_changed()` en fonction du masque
 * des bits de poids fort de l'identifiant (`UI_DEST_MASK`).
 *
 * @note Toutes les fonctions sont non bloquantes et appelées depuis le thread UI.
 */

#ifndef BRICK_UI_UI_BACKEND_H
#define BRICK_UI_UI_BACKEND_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Espaces de destination (bits hauts de l’identifiant)                       */
/* -------------------------------------------------------------------------- */

/**
 * @name UI Backend Destination Masks
 * @brief Masques de routage pour les paramètres UI.
 * @{
 */
#define UI_DEST_MASK   0xE000U  /**< Masque général des bits de destination. */
#define UI_DEST_CART   0x0000U  /**< Paramètre destiné à la cartouche active. */
#define UI_DEST_UI     0x8000U  /**< Paramètre purement interne à l'UI. */
#define UI_DEST_MIDI   0x4000U  /**< Paramètre routé vers la pile MIDI. */
#define UI_DEST_ID(x)  ((x) & 0x1FFFU) /**< Extrait l'identifiant local sur 13 bits. */
/** @} */

/* -------------------------------------------------------------------------- */
/* API publique                                                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Notifie un changement de paramètre issu de l’UI.
 * @ingroup ui_backend
 *
 * @param id        Identifiant de paramètre encodé (UI_DEST_* + ID local)
 * @param val       Nouvelle valeur (0/1 ou 0–255)
 * @param bitwise   Si true, appliquer un masque binaire
 * @param mask      Masque de bits à appliquer lorsque @p bitwise est true
 *
 * @details
 * Le routage de la mise à jour dépend du champ de destination dans @p id :
 *
 * | Destination | Masque | Action |
 * |--------------|--------|--------|
 * | `UI_DEST_CART` | `0x0000` | Envoi vers `cart_link_param_changed()` |
 * | `UI_DEST_UI`   | `0x8000` | Appel interne `ui_backend_handle_ui()` |
 * | `UI_DEST_MIDI` | `0x4000` | Routage vers `ui_backend_handle_midi()` |
 */
void ui_backend_param_changed(uint16_t id, uint8_t val, bool bitwise, uint8_t mask);

/**
 * @brief Lecture du shadow register local (valeur courante affichable).
 * @ingroup ui_backend
 *
 * @param id Identifiant de paramètre
 * @return Valeur locale en cache pour ce paramètre.
 *
 * @note
 * Cette fonction ne déclenche **aucune** I/O cartouche :
 * elle lit uniquement le shadow local associé à la cartouche active.
 */
uint8_t ui_backend_shadow_get(uint16_t id);

/**
 * @brief Écriture dans le shadow register local (sans envoi immédiat).
 * @ingroup ui_backend
 *
 * @param id  Identifiant de paramètre
 * @param val Valeur à stocker localement
 *
 * @note
 * Permet de maintenir la cohérence du modèle UI sans émettre sur le bus.
 */
void ui_backend_shadow_set(uint16_t id, uint8_t val);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_UI_BACKEND_H */
