/**
 * @file cart_bus.h
 * @brief Interface du bus série entre Brick et les cartouches XVA.
 *
 * Ce module assure la communication série asynchrone entre le cœur Brick
 * et jusqu’à quatre cartouches d’extension (XVA, etc.).
 * Chaque cartouche est associée à un port UART dédié et dispose de sa propre
 * file d’envoi (mailbox + pool mémoire) gérée par un thread indépendant.
 *
 * ### Mapping UART matériel
 * | Cart | UART   | Broches STM32  |
 * |------|--------|----------------|
 * | 1    | USART1 | PA9 / PA10     |
 * | 2    | UART4  | PE8 / PE7      |
 * | 3    | USART3 | PB10 / PB11    |
 * | 4    | USART2 | PA2 / PA3      |
 *
 * @ingroup cart
 */

#ifndef BRICK_CART_CART_BUS_H
#define BRICK_CART_CART_BUS_H
#include <stdint.h>
#include <stdbool.h>

#ifndef CART_QUEUE_LEN
#define CART_QUEUE_LEN 32U
#endif

/* ===========================================================
 * Types et structures
 * =========================================================== */

/**
 * @brief Identifiants logiques des cartouches physiques.
 */
typedef enum {
    CART1 = 0,  /**< Port 1 → USART1 (PA9/PA10)  */
    CART2 = 1,  /**< Port 2 → UART4  (PE8/PE7)   */
    CART3 = 2,  /**< Port 3 → USART3 (PB10/PB11) */
    CART4 = 3,  /**< Port 4 → USART2 (PA2/PA3)   */
    CART_COUNT
} cart_id_t;

/**
 * @brief Statistiques de transmission par cartouche.
 */
typedef struct {
    volatile uint32_t tx_sent;     /**< Nombre total de trames envoyées */
    volatile uint32_t tx_dropped;  /**< Trames perdues faute de pool mémoire */
    volatile uint32_t mb_full;     /**< Nombre de saturations de mailbox */
    volatile uint16_t mb_high_water; /**< Occupation max observée de la mailbox */
} cart_tx_stats_t;

/**
 * @brief Tableau global des statistiques de chaque port.
 */
extern cart_tx_stats_t cart_stats[CART_COUNT];

/* ===========================================================
 * API publique
 * =========================================================== */

/**
 * @brief Initialise tous les ports série de cartouche et crée les threads TX.
 */
void cart_bus_init(void);

/**
 * @brief Envoie une commande SET (écriture de paramètre) vers une cartouche.
 *
 * @param id     Identifiant de la cartouche (CART1..CART4)
 * @param param  Identifiant du paramètre (dest_id)
 * @param value  Nouvelle valeur (brute, 8 bits)
 * @return `true` si la commande a été postée avec succès, sinon `false`.
 */
bool cart_set_param(cart_id_t id, uint16_t param, uint8_t value);

/**
 * @brief Envoie une commande GET (lecture de paramètre) vers une cartouche.
 *
 * @param id     Identifiant de la cartouche (CART1..CART4)
 * @param param  Identifiant du paramètre (dest_id)
 * @return `true` si la commande a été postée avec succès, sinon `false`.
 */
bool cart_get_param(cart_id_t id, uint16_t param);

/** @brief Retourne le high-water mark de la mailbox pour un port donné. */
uint16_t cart_bus_get_mailbox_high_water(cart_id_t id);
/** @brief Retourne le remplissage courant de la mailbox pour un port donné. */
uint16_t cart_bus_get_mailbox_fill(cart_id_t id);
/** @brief Réinitialise les compteurs de saturation/mailbox. */
void cart_bus_reset_mailbox_stats(void);
#endif /* BRICK_CART_CART_BUS_H */
