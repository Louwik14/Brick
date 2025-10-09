/**
 * @file cart_proto.h
 * @brief API d’encodage du protocole Cart Bus (inspiré du XVA1).
 *
 * Fournit les fonctions d’encodage des trames UART pour le protocole
 * Cart Bus, dérivé du protocole historique XVA1.
 * Compatible avec toute cartouche utilisant la même structure d’adressage
 * de paramètres.
 *
 * @ingroup cart
 */

#ifndef BRICK_CART_CART_PROTO_H
#define BRICK_CART_CART_PROTO_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Construit une trame “set param value” prête à envoyer sur UART.
 * @param param  Identifiant du paramètre (0–511)
 * @param value  Valeur à envoyer (0–255)
 * @param out    Buffer de sortie (min. 4 octets)
 * @return Taille de la trame (3 ou 4 octets)
 */
size_t cart_proto_build_set(uint16_t param, uint8_t value, uint8_t out[4]);

/**
 * @brief Construit une trame “get param” prête à envoyer sur UART.
 * @param param  Identifiant du paramètre à lire (0–511)
 * @param out    Buffer de sortie (min. 4 octets)
 * @return Taille de la trame (2 ou 3 octets)
 */
size_t cart_proto_build_get(uint16_t param, uint8_t out[4]);
#endif /* BRICK_CART_CART_PROTO_H */
