/**
 * @file cart_spec_types.h
 * @brief Définitions neutres des structures de spécifications de cartouches Brick.
 * @ingroup cart
 * @details
 * Ce fichier définit les structures de données **purement descriptives** partagées entre
 * la couche Cart (cart_registry, cart_link) et la couche UI (ui_spec, ui_renderer).
 *
 * Il ne contient **aucune logique** ni dépendance fonctionnelle : uniquement des types et
 * constantes neutres utilisables dans toutes les couches du firmware sans dépendance circulaire.
 *
 * @note Inclus par : cart_registry.h, ui_spec.h, cart_proto_*.h
 */

#ifndef CART_SPEC_TYPES_H
#define CART_SPEC_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @defgroup cart_spec_types Spécifications de cartouches (types neutres)
 * @ingroup cart
 * @{
 */

/** Nombre maximal de menus qu'une cartouche peut exposer. */
#define CART_MAX_MENUS     8

/** Nombre maximal de pages par menu. */
#define CART_MAX_PAGES     8

/** Nombre maximal de paramètres par page. */
#define CART_MAX_PARAMS    16

/**
 * @brief Description d’un paramètre de cartouche.
 */
typedef struct {
    const char* name;   /**< Nom lisible du paramètre. */
    uint16_t    id;     /**< Identifiant interne du paramètre (utilisé pour UART/Link). */
    uint8_t     min;    /**< Valeur minimale autorisée. */
    uint8_t     max;    /**< Valeur maximale autorisée. */
    uint8_t     def;    /**< Valeur par défaut. */
    bool        bitwise;/**< Indique si le paramètre est un masque binaire. */
} cart_param_spec_t;

/**
 * @brief Description d’une page de paramètres.
 */
typedef struct {
    const char* name;                      /**< Nom de la page. */
    const cart_param_spec_t* params;       /**< Tableau des paramètres. */
    uint8_t num_params;                    /**< Nombre de paramètres. */
} cart_page_spec_t;

/**
 * @brief Description d’un menu regroupant plusieurs pages.
 */
typedef struct {
    const char* name;                      /**< Nom du menu. */
    const cart_page_spec_t* pages;         /**< Tableau des pages. */
    uint8_t num_pages;                     /**< Nombre de pages. */
} cart_menu_spec_t;

/**
 * @brief Spécification complète d’une cartouche.
 */
typedef struct {
    const char* name;                      /**< Nom de la cartouche (ex: "XVA1"). */
    const cart_menu_spec_t* menus;         /**< Tableau de menus. */
    uint8_t num_menus;                     /**< Nombre total de menus. */
} cart_spec_t;

/** @} */

#endif /* CART_SPEC_TYPES_H */
