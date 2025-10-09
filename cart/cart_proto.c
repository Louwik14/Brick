/**
 * @file cart_proto.c
 * @brief Encodage binaire générique du protocole de communication Cart Bus.
 *
 * Ce protocole, historiquement dérivé du format **XVA1**, définit une trame
 * UART simple et compacte utilisée pour l’échange de paramètres entre le
 * firmware Brick et les cartouches matérielles.
 *
 * ### Format des trames :
 * | Type | Description | Format | Exemple |
 * |------|--------------|--------|----------|
 * | **Écriture** | Envoi d’une valeur vers la cartouche | `'s' param value` | `s 10 127` |
 * | **Lecture** | Demande de la valeur d’un paramètre | `'g' param` | `g 10` |
 *
 * ### Extension d’adresse :
 * Si `param >= 255`, un octet d’extension est ajouté :
 * - **Set étendu** : `'s' 255 (param-256) value`
 * - **Get étendu** : `'g' 255 (param-256)`
 *
 * Cette approche permet d’adresser jusqu’à **512 paramètres par cartouche**
 * tout en restant compatible avec le protocole original XVA1.
 *
 * @ingroup cart
 */

#include "cart_proto.h"

/* =======================================================================
 * Construction des trames “set” et “get”
 * ======================================================================= */

/**
 * @brief Construit une trame binaire pour une commande “set param value”.
 *
 * Encode la commande dans un buffer `out[]` au format du protocole Cart Bus :
 *
 * - Si `param < 255`  → trame courte (`'s' param value`, 3 octets)
 * - Si `param >= 255` → trame étendue (`'s' 255 (param-256) value`, 4 octets)
 *
 * @param param  Identifiant du paramètre (0–511)
 * @param value  Valeur à envoyer (0–255)
 * @param out    Buffer de sortie (min. 4 octets)
 * @return Taille de la trame (3 ou 4 octets)
 */
size_t cart_proto_build_set(uint16_t param, uint8_t value, uint8_t out[4]) {
    out[0] = 's';
    if (param <= 254) {
        out[1] = (uint8_t)param;
        out[2] = value;
        return 3;
    }
    out[1] = 255;
    out[2] = (uint8_t)(param - 256);
    out[3] = value;
    return 4;
}

/**
 * @brief Construit une trame binaire pour une commande “get param”.
 *
 * Encode la commande dans un buffer `out[]` au format du protocole Cart Bus :
 *
 * - Si `param < 255`  → `'g' param` (2 octets)
 * - Si `param >= 255` → `'g' 255 (param-256)` (3 octets)
 *
 * @param param  Identifiant du paramètre à lire (0–511)
 * @param out    Buffer de sortie (min. 4 octets)
 * @return Taille de la trame (2 ou 3 octets)
 */
size_t cart_proto_build_get(uint16_t param, uint8_t out[4]) {
    out[0] = 'g';
    if (param <= 254) {
        out[1] = (uint8_t)param;
        return 2;
    }
    out[1] = 255;
    out[2] = (uint8_t)(param - 256);
    return 3;
}
