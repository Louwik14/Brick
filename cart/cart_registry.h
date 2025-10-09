/**
 * @file cart_registry.h
 * @brief Registre central des cartouches Brick.
 *
 * @defgroup cart_registry Cart Registry
 * @ingroup cart
 *
 * @details
 * Ce module maintient l'association entre les ports matériels (CART1..CART4)
 * et leurs descriptions UI (struct ui_cart_spec_t). Il gère également la
 * cartouche actuellement active (identifiant `cart_id_t`) afin que les couches
 * hautes (UI, moteur temps réel) puissent interroger la configuration courante
 * sans dépendre des couches basses (bus/uart).
 *
 * Règles d’architecture :
 * - Header **public** : ne dépend que de @ref cart_bus.h (pour `cart_id_t`).
 * - Aucune inclusion de headers UI : on utilise une *forward declaration*
 *   `struct ui_cart_spec_t` pour éviter toute dépendance montante vers l’UI.
 * - L’implémentation (.c) peut inclure `ui/ui_spec.h` si nécessaire.
 *
 * @note Le registre ne réalise aucune I/O. Il stocke des pointeurs vers des
 *       descriptions statiques (tables) et expose l’ID de la cartouche active.
 */

#ifndef BRICK_CART_CART_REGISTRY_H
#define BRICK_CART_CART_REGISTRY_H

#include <stdint.h>
#include <stdbool.h>
#include "cart_bus.h"   /**< Fournit cart_id_t, CART1..CART4 */

/* Forward-decl : struct taguée définie dans ui/ui_spec.h
   (on évite toute dépendance UI au niveau du header public). */
struct ui_cart_spec_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise le registre des cartouches.
 * @ingroup cart_registry
 *
 * Réinitialise la table interne et positionne la cartouche active par défaut
 * (typiquement CART1). Doit être appelé une seule fois au démarrage.
 */
void cart_registry_init(void);

/**
 * @brief Enregistre la spécification UI d'une cartouche pour un port donné.
 * @ingroup cart_registry
 *
 * @param id      Identifiant/port (CART1..CART4)
 * @param ui_spec Pointeur constant vers la spécification UI de la cartouche
 *
 * @note Ne modifie pas la cartouche active : voir @ref cart_registry_switch.
 */
void cart_registry_register(cart_id_t id, const struct ui_cart_spec_t* ui_spec);

/**
 * @brief Récupère la spécification UI d'une cartouche sans la rendre active.
 * @ingroup cart_registry
 *
 * @param id Identifiant/port (CART1..CART4)
 * @return Pointeur constant vers `struct ui_cart_spec_t`, ou NULL si non enregistrée.
 */
const struct ui_cart_spec_t* cart_registry_get_ui_spec(cart_id_t id);

/**
 * @brief Rend active une cartouche et renvoie sa spécification UI.
 * @ingroup cart_registry
 *
 * @param id Identifiant/port à activer (CART1..CART4)
 * @return Pointeur constant vers la spec UI de la cartouche désormais active,
 *         ou la précédente si @p id est invalide.
 *
 * @note Ne réalise aucune I/O : la sélection active est un état logiciel.
 */
const struct ui_cart_spec_t* cart_registry_switch(cart_id_t id);

/**
 * @brief Renvoie l'identifiant (cid) de la cartouche active.
 * @ingroup cart_registry
 *
 * @return `cart_id_t` (CART1..CART4)
 *
 * @note À utiliser par les couches bas niveau (cart_link) pour router les accès.
 */
cart_id_t cart_registry_get_active_id(void);

/**
 * @brief Indique si une cartouche est enregistrée (présente) pour un port donné.
 * @ingroup cart_registry
 *
 * @param id Identifiant/port (CART1..CART4)
 * @return true si une spec UI est enregistrée pour @p id, false sinon.
 */
bool cart_registry_is_present(cart_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_CART_CART_REGISTRY_H */
