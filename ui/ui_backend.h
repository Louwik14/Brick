/**
 * @file ui_backend.h
 * @brief Pont neutre entre la couche UI (controller/model) et les couches basses (cart, UI interne, MIDI).
 * @ingroup ui
 *
 * @details
 * Le **backend UI** est la seule interface autorisée entre la logique UI (controller)
 * et les sous-systèmes matériels ou logiciels (CartLink, MIDI, etc.).
 * Il implémente un routage centralisé des mises à jour de paramètres,
 * sans exposer les dépendances basses couches à la UI.
 *
 * Trois destinations principales sont supportées :
 *
 * | Destination      | Masque binaire | Description |
 * |------------------|----------------|--------------|
 * | `UI_DEST_CART`   | `0x0000` | Paramètres routés vers la cartouche active |
 * | `UI_DEST_UI`     | `0x8000` | Paramètres internes à l’UI (menus, overlays) |
 * | `UI_DEST_MIDI`   | `0x4000` | Paramètres routés vers la pile MIDI |
 *
 * Invariants :
 * - Aucune I/O bloquante.
 * - Accès uniquement depuis le thread UI.
 * - Compatible 60 FPS : toutes les opérations sont O(1).
 */

#ifndef BRICK_UI_UI_BACKEND_H
#define BRICK_UI_UI_BACKEND_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Espaces de destination (bits hauts de l’identifiant)
 * ============================================================ */

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

/* ============================================================
 * API publique
 * ============================================================ */

/**
 * @brief Notifie un changement de paramètre issu de l’UI.
 * @ingroup ui_backend
 *
 * @param id        Identifiant de paramètre encodé (`UI_DEST_* + ID local`)
 * @param val       Nouvelle valeur (0/1 ou 0–255)
 * @param bitwise   Si `true`, appliquer un masque binaire
 * @param mask      Masque de bits à appliquer lorsque @p bitwise est `true`
 *
 * @details
 * Cette fonction délègue la mise à jour vers la destination correspondante :
 * - `UI_DEST_CART` → `cart_link_param_changed()`
 * - `UI_DEST_UI`   → `ui_backend_handle_ui()`
 * - `UI_DEST_MIDI` → `ui_backend_handle_midi()`
 *
 * Appelée exclusivement depuis le thread UI.
 */
void ui_backend_param_changed(uint16_t id, uint8_t val, bool bitwise, uint8_t mask);

/**
 * @brief Lecture du **shadow register** local (valeur courante affichable).
 * @ingroup ui_backend
 *
 * @param id Identifiant de paramètre.
 * @return Valeur locale en cache pour ce paramètre.
 *
 * @note
 * Aucun accès cartouche : cette fonction lit uniquement le shadow local
 * synchronisé avec la dernière valeur envoyée.
 */
uint8_t ui_backend_shadow_get(uint16_t id);

/**
 * @brief Écriture dans le **shadow register** local (sans envoi immédiat).
 * @ingroup ui_backend
 *
 * @param id  Identifiant de paramètre.
 * @param val Valeur à stocker localement.
 *
 * @note
 * Maintient la cohérence entre l’UI et le shadow sans communication externe.
 * L’envoi réel se fait via `ui_backend_param_changed()`.
 */
void ui_backend_shadow_set(uint16_t id, uint8_t val);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_UI_BACKEND_H */
