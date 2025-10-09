/**
 * @file ui_labels_common.c
 * @brief Définitions des labels UI communs et universels pour Brick.
 *
 * @ingroup ui
 *
 * @details
 * Ce module fournit des ensembles de chaînes de caractères partagés
 * entre plusieurs cartouches et modules UI.
 *
 * Il ne contient **aucun contenu spécifique à un moteur DSP particulier** :
 * seules les valeurs “génériques” sont incluses (On/Off, formes d’onde de base...).
 *
 * Ces labels peuvent être utilisés directement dans les définitions
 * `ui_param_spec_t.meta.en.labels` pour tout paramètre commun.
 *
 * Exemple d’utilisation :
 * @code
 * .meta.en = { ui_labels_onoff, 2 }
 * .meta.en = { ui_labels_basic_waves, 4 }
 * @endcode
 */

#include "ui_labels_common.h"

/* ============================================================
 * Labels binaires / booléens
 * ============================================================ */

/**
 * @brief Labels génériques pour les paramètres booléens.
 *
 * Utilisés pour tout paramètre activable / désactivable.
 * Exemple : bypass, mute, sync, enable...
 */
const char* const ui_labels_onoff[2] = { "Off", "On" };

/* ============================================================
 * Labels pour formes d’onde génériques
 * ============================================================ */

/**
 * @brief Formes d’onde de base, communes à la majorité des synthétiseurs.
 *
 * Utilisées pour les oscillateurs, LFO ou modulateurs
 * lorsque le moteur ne possède pas de formes d’onde propriétaires.
 *
 * Ordre :
 *  - Sine : sinusoïde pure
 *  - Square : onde carrée
 *  - Tri : triangle
 *  - Saw : scie montante
 */
const char* const ui_labels_basic_waves[4] = {
    "Sine", "Square", "Tri", "Saw"
};
