/**
 * @file cart_link.h
 * @brief Interface de communication entre Brick et les cartouches externes.
 *
 * @defgroup cart_link Cart Link
 * @ingroup cart
 *
 * @details
 * Fournit les primitives pour :
 * - notifier les changements de paramètres issus de l’UI,
 * - gérer les registres shadow locaux (pour éviter les envois redondants),
 * - assurer la cohérence des valeurs entre firmware et DSP externe.
 *
 * @note Implémente le pont attendu par la couche UI via `ui_backend`.
 * @note Header public : ne dépend que de `cart_bus.h` (types `cart_id_t`, `CARTx`).
 */

#ifndef BRICK_CORE_CART_LINK_H
#define BRICK_CORE_CART_LINK_H


#include <stdint.h>
#include <stdbool.h>
#include "cart_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise la couche Cart Link et réinitialise les registres shadow.
 * @ingroup cart_link
 * @post Les valeurs shadow sont initialisées pour toutes les cartouches connues.
 */
void cart_link_init(void);

/**
 * @brief Notifie un changement de paramètre vers la cartouche active.
 * @ingroup cart_link
 *
 * @param param_id   Identifiant de paramètre (a.k.a. `dest_id`)
 * @param value      Nouvelle valeur (0/1 ou 0–255)
 * @param is_bitwise Si true, applique un masque binaire (écriture partielle)
 * @param bit_mask   Masque binaire à appliquer si @p is_bitwise est true
 *
 * @note L’implémentation peut ignorer l’envoi si la valeur est identique au shadow.
 */
void cart_link_param_changed(uint16_t param_id,
                             uint8_t value,
                             bool is_bitwise,
                             uint8_t bit_mask);

/**
 * @brief Lecture du shadow local pour une cartouche donnée.
 * @ingroup cart_link
 *
 * @param cid       Identifiant de cartouche (CART1..CART4)
 * @param param_id  Identifiant de paramètre
 * @return La valeur courante en cache (shadow) pour ce paramètre.
 *
 * @warning Ne déclenche aucun I/O : lecture purement locale.
 */
uint8_t cart_link_shadow_get(cart_id_t cid, uint16_t param_id);

/**
 * @brief Écriture directe dans le shadow local (sans envoi immédiat).
 * @ingroup cart_link
 *
 * @param cid       Identifiant de cartouche
 * @param param_id  Identifiant de paramètre
 * @param v         Valeur à écrire dans le cache
 *
 * @note Utile pour synchroniser l’UI sans trafic UART (ex. bitfields).
 */
void cart_link_shadow_set(cart_id_t cid, uint16_t param_id, uint8_t v);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_CORE_CART_LINK_H */
