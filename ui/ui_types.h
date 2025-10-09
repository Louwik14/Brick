/**
 * @file ui_types.h
 * @brief Types communs de la couche UI Brick (kinds, widgets).
 * @ingroup ui
 */

#ifndef UI_TYPES_H
#define UI_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/** @defgroup ui_types Types UI communs
 *  @ingroup ui
 *  @{
 */

/**
 * @brief Genre logique d’un paramètre UI.
 */
typedef enum {
    UI_PARAM_NONE = 0,
    UI_PARAM_BOOL,
    UI_PARAM_ENUM,
    UI_PARAM_CONT
} ui_param_kind_t;

/**
 * @brief Familles de widgets disponibles pour le rendu.
 */
typedef enum {
    UIW_NONE = 0,
    UIW_SWITCH,
    UIW_KNOB,
    UIW_ENUM_ICON_WAVE,
    UIW_ENUM_ICON_FILTER
} ui_widget_type_t;

/** @} */

#endif /* UI_TYPES_H */
