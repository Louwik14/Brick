/**
 * @file ui_spec.h
 * @brief Spécifications UI des cartouches Brick (menus/pages/paramètres + cycles BM).
 *
 * @ingroup ui
 *
 * @details
 * Ce header décrit les structures **purement UI** utilisées pour :
 * - définir les menus, pages et paramètres affichés (OLED),
 * - typer chaque paramètre (booléen, enum, continu),
 * - décrire les cycles de menus (BM1..BM8) via des indices statiques,
 * - fournir les métadonnées nécessaires au rendu et au pilotage (labels, plages, step).
 *
 * ## Points clés
 * - Les cycles BM sont **déclaratifs** : chaque cartouche peut définir pour
 *   ses boutons BMx un groupe de menus cyclés (ex. BM8 → FX1→FX2→FX3→FX4).
 * - Aucune dépendance logique : le contrôleur UI lit ces infos mais la cartouche
 *   ne connaît pas `ui_controller.h`.
 * - Les structures ici ne contiennent **aucune logique** et ne dépendent pas
 *   du backend ou du bus — c’est du **pur modèle UI**.
 */

#ifndef BRICK_UI_UI_SPEC_H
#define BRICK_UI_UI_SPEC_H

#include <stdint.h>
#include <stdbool.h>
#include "ui_types.h"  /**< ui_param_kind_t, etc. */

/* -------------------------------------------------------------------------- */
/* Capacités par défaut (modifiable au besoin)                                */
/* -------------------------------------------------------------------------- */

/**
 * @def UI_PARAMS_PER_PAGE
 * @brief Nombre de paramètres par page UI (par défaut 4).
 */
#ifndef UI_PARAMS_PER_PAGE
#define UI_PARAMS_PER_PAGE 4
#endif

/**
 * @def UI_PAGES_PER_MENU
 * @brief Nombre de pages par menu UI (par défaut 5).
 */
#ifndef UI_PAGES_PER_MENU
#define UI_PAGES_PER_MENU 5
#endif

/**
 * @def UI_MENUS_PER_CART
 * @brief Nombre maximal de menus exposés par une cartouche (par défaut 16).
 */
#ifndef UI_MENUS_PER_CART
#define UI_MENUS_PER_CART 16
#endif

/**
 * @def UI_CYCLE_MAX_OPTS
 * @brief Nombre maximal d’options cyclables par bouton BM (par défaut 4).
 */
#ifndef UI_CYCLE_MAX_OPTS
#define UI_CYCLE_MAX_OPTS 4
#endif

/* -------------------------------------------------------------------------- */
/* Méta-données de paramètres                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Plage d’un paramètre **continu** (CONT).
 *
 * @details
 * - `min` / `max` sont en **int16_t** pour supporter 0..255 et des bornes négatives.
 * - `step` est le pas appliqué à chaque “cran” d’encodeur (>= 1).
 */
typedef struct ui_param_range_t {
    int16_t min;   /**< Borne minimale (peut être négative). */
    int16_t max;   /**< Borne maximale (peut aller jusqu’à 255 sans overflow). */
    uint8_t step;  /**< Pas d’incrément/décrément pour l’encodeur. */
} ui_param_range_t;

/**
 * @brief Métadonnées d’un paramètre **énuméré** (ENUM).
 *
 * @details
 * - `labels` peut être NULL (valeurs numériques brutes).
 * - `count` est le nombre d’entrées dans l’énumération.
 */
typedef struct ui_param_enum_t {
    const char* const *labels;  /**< Tableau de libellés, ou NULL. */
    int                count;   /**< Nombre d’entrées de l’énumération. */
} ui_param_enum_t;

/**
 * @brief Union des métadonnées de paramètre (continu **ou** énuméré).
 */
typedef union ui_param_meta_t {
    ui_param_range_t range;  /**< Métadonnées pour un paramètre CONT. */
    ui_param_enum_t  en;     /**< Métadonnées pour un paramètre ENUM. */
} ui_param_meta_t;

/* -------------------------------------------------------------------------- */
/* Paramètre UI                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief Spécification d’un paramètre UI.
 *
 * @details
 * Ordre des champs **important** (compatibilité initialisations XVA1) :
 * `label → kind → dest_id → default_value → meta → is_bitwise → bit_mask`.
 *
 * - `dest_id` : identifiant “cartouche” (utilisé côté bus/uart via ui_backend).
 * - `default_value` : valeur initiale côté UI/model.
 * - `is_bitwise`/`bit_mask` : support des paramètres empaquetés (bitfields).
 */
typedef struct ui_param_spec_t {
    const char*     label;         /**< Libellé à afficher sur l’OLED. */
    ui_param_kind_t kind;          /**< Genre logique (BOOL/ENUM/CONT). */
    uint16_t        dest_id;       /**< Identifiant cartouche (UART / link). */
    uint8_t         default_value; /**< Valeur par défaut côté UI/model. */
    ui_param_meta_t meta;          /**< Métadonnées (plage ou énumération). */
    bool            is_bitwise;    /**< true si ce paramètre est un bitfield. */
    uint8_t         bit_mask;      /**< Masque binaire si `is_bitwise` est true. */
} ui_param_spec_t;

/* -------------------------------------------------------------------------- */
/* Page UI                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief Spécification d’une page UI (groupe de paramètres).
 *
 * @details
 * - `params` : tableau fixe de @ref UI_PARAMS_PER_PAGE paramètres.
 * - `header_label` : en-tête optionnel affiché sur la page (peut être NULL).
 */
typedef struct ui_page_spec_t {
    ui_param_spec_t params[UI_PARAMS_PER_PAGE];  /**< Paramètres de la page. */
    const char*     header_label;                /**< En-tête optionnel (peut être NULL). */
} ui_page_spec_t;

/* -------------------------------------------------------------------------- */
/* Menu UI                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief Spécification d’un menu UI (ensemble de pages).
 *
 * @details
 * - `name` : nom court du menu (affichage).
 * - `page_titles` : titres individuels des pages (peuvent être NULL).
 * - `pages` : tableau fixe de @ref UI_PAGES_PER_MENU pages.
 */
typedef struct ui_menu_spec_t {
    const char*     name;                               /**< Nom du menu (OLED). */
    const char*     page_titles[UI_PAGES_PER_MENU];     /**< Titres de pages (optionnels). */
    ui_page_spec_t  pages[UI_PAGES_PER_MENU];           /**< Pages du menu. */
} ui_menu_spec_t;

/* -------------------------------------------------------------------------- */
/* Définition déclarative des cycles BM                                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Décrit un cycle pour un bouton menu (BMx) via des indices de menus.
 *
 * @details
 * - `count` : nombre d’options dans le cycle (0 = désactivé).
 * - `idxs` : indices des menus cibles dans `ui_cart_spec_t::menus[]`.
 * - `resume` : si true, le cycle reprend là où il était laissé.
 *
 * Exemple : BM8 → FX1→FX2→FX3→FX4
 * ```c
 * [7] = { .count=4, .idxs={4,5,6,7}, .resume=true },
 * ```
 */
typedef struct ui_cycle_idx_spec_t {
    uint8_t count;                      /**< Nombre d’options dans le cycle. */
    uint8_t idxs[UI_CYCLE_MAX_OPTS];    /**< Indices des menus cyclés (0..UI_MENUS_PER_CART-1). */
    bool    resume;                     /**< Si true : conserve l’indice courant. */
} ui_cycle_idx_spec_t;

/* -------------------------------------------------------------------------- */
/* Spécification UI de cartouche                                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Spécification UI complète d’une cartouche.
 *
 * @details
 * - `cart_name` : nom affiché dans la barre de titre / entête.
 * - `menus` : tableau fixe de @ref UI_MENUS_PER_CART menus.
 * - `cycles` : configuration optionnelle des boutons BM cyclés.
 *
 * Cette structure reste **purement déclarative** et ne référence
 * aucune logique UI ou backend.
 */
typedef struct ui_cart_spec_t {
    const char*     cart_name;                          /**< Nom affiché. */
    const char* overlay_tag;   /**< (optionnel) Tag visuel du mode custom actif, ex: "SEQ" */
    ui_menu_spec_t  menus[UI_MENUS_PER_CART];           /**< Menus exposés par la cartouche. */
    ui_cycle_idx_spec_t cycles[8];                      /**< Configuration des cycles BM1..BM8. */
} ui_cart_spec_t;

#endif /* BRICK_UI_UI_SPEC_H */
