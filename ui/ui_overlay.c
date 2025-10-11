/**
 * @file ui_overlay.c
 * @brief Gestion centralisée des overlays UI (SEQ, ARP, …) pour Brick.
 * @ingroup ui
 *
 * @details
 * Module UI pur (aucune dépendance bus/UART/driver) qui gère :
 *  - l'entrée/sortie d'overlay (exclusifs) ;
 *  - la bascule de sous-spécifications (MODE/SETUP) ;
 *  - un flag « mode custom actif » persistant (pour affichage & règles).
 *
 * Invariants respectés :
 *  - Pas de logique d'état dans le renderer (séparation stricte).
 *  - Aucune altération du backend/cart bus depuis ce module.
 *  - Overlays exclusifs : entrer dans un overlay ferme l'éventuel précédent.
 */

#include <string.h>
#include "ui_overlay.h"
#include "ui_controller.h"  /* ui_switch_cart(), ui_get_cart(), ui_get_state() */
#include "ui_model.h"       /* ui_state_t, ui_model_set_active_overlay_tag() */

/* ============================================================
 * États internes (privés au module)
 * ============================================================ */

/** Overlay actif (exclusif) ; UI_OVERLAY_NONE si aucun. */
static ui_overlay_id_t s_overlay_active = UI_OVERLAY_NONE;

/** Spécification UI courante de l'overlay (MODE ou SETUP) si actif. */
static const ui_cart_spec_t* s_overlay_spec = NULL;

/** Cart « réelle » sauvegardée à l'entrée du (premier) overlay. */
static const ui_cart_spec_t* s_prev_cart = NULL;

/** État UI « réel » (menus/pages/param state) sauvegardé à l'entrée. */
static ui_state_t s_prev_state;

/** Dernier mode custom actif (persistant pour rendu / règles). */
static ui_custom_mode_t s_custom_mode_active = UI_CUSTOM_NONE;

/* ============================================================
 * API
 * ============================================================ */

/**
 * @brief Entre dans un overlay en garantissant la restauration correcte
 *        lors d'un enchaînement d'overlays.
 *
 * @param id   Identifiant de l'overlay (SEQ, ARP, …).
 * @param spec Spécification UI de l'overlay à activer (MODE ou SETUP).
 *
 * @details
 * ✅ Correctif clé (bug « Custom X → Custom Y → BM Z ») :
 * Si un overlay est déjà actif, on le **ferme et restaure** d'abord
 * la cart & l'état **réels**, PUIS on capture ces références (cart/état)
 * comme base de retour et on active le nouvel overlay.
 * Ainsi, `s_prev_cart` pointe toujours vers la cartouche réelle,
 * ce qui garantit que le cycle BM sera correct à la sortie.
 */
void ui_overlay_enter(ui_overlay_id_t id, const ui_cart_spec_t* spec)
{
    /* 1) Si un overlay était actif → le fermer proprement et restaurer la réalité */
    if (s_overlay_active != UI_OVERLAY_NONE) {
        if (s_prev_cart) {
            /* Restaure cart + état "réels" */
            ui_switch_cart(s_prev_cart);
            ui_state_t* st_mut = (ui_state_t*)ui_get_state();
            memcpy(st_mut, &s_prev_state, sizeof(ui_state_t));
        }
        /* Purge l'ancien contexte overlay */
        s_prev_cart      = NULL;
        s_overlay_active = UI_OVERLAY_NONE;
        s_overlay_spec   = NULL;
    }

    /* 2) Maintenant que tout est restauré, CAPTURER la cart & l'état RÉELS */
    const ui_cart_spec_t* real_cart = ui_get_cart();
    memcpy(&s_prev_state, ui_get_state(), sizeof(ui_state_t));
    s_prev_cart = real_cart;

    /* 3) Activer le NOUVEL overlay */
    s_overlay_active = id;
    s_overlay_spec   = spec;
    ui_switch_cart(spec);

    /* 4) Lisibilité : repartir sur menu/page 0/0 dans l'overlay */
    ui_state_t* st = (ui_state_t*)ui_get_state();
    st->cur_menu = 0;
    st->cur_page = 0;
}

/**
 * @brief Ferme l'overlay actif (le cas échéant) et restaure cart & état réels.
 *
 * @details
 * - Ne réinitialise PAS le flag de mode custom persistant : le renderer
 *   peut continuer d'afficher le dernier tag (ex. "SEQ") si souhaité.
 */
void ui_overlay_exit(void)
{
    if (s_overlay_active == UI_OVERLAY_NONE)
        return;

    if (s_prev_cart) {
        ui_switch_cart(s_prev_cart);
        ui_state_t* st_mut = (ui_state_t*)ui_get_state();
        memcpy(st_mut, &s_prev_state, sizeof(ui_state_t));
    }

    s_overlay_active = UI_OVERLAY_NONE;
    s_overlay_spec   = NULL;
    s_prev_cart      = NULL;
}

/**
 * @brief Indique si un overlay est actuellement actif.
 * @return true si actif, false sinon.
 */
bool ui_overlay_is_active(void)
{
    return s_overlay_active != UI_OVERLAY_NONE;
}

/**
 * @brief Bascule d'une sous-spécification d'overlay à une autre (MODE ↔ SETUP).
 *
 * @param spec Nouvelle sous-spec de l'overlay (toujours côté overlay).
 *
 * @details
 * - Ne modifie pas la cart « réelle » sauvegardée.
 * - Réinitialise menu/page à 0/0 pour cohérence de navigation.
 */
void ui_overlay_switch_subspec(const ui_cart_spec_t* spec)
{
    if (!ui_overlay_is_active())
        return;

    s_overlay_spec = spec;
    ui_switch_cart(spec);

    /* Reset en-tête pour consistance */
    ui_state_t* st = (ui_state_t*)ui_get_state();
    st->cur_menu = 0;
    st->cur_page = 0;
}

/**
 * @brief Retourne la spec UI courante de l'overlay (MODE/SETUP) si actif.
 * @return Pointeur sur la spec overlay active, ou NULL si aucun overlay.
 */
const ui_cart_spec_t* ui_overlay_get_spec(void)
{
    return s_overlay_spec;
}

/* --------- Mode custom persistant (rendu / règles) ---------------------- */

/**
 * @brief Définit le dernier mode custom actif (persistant).
 *
 * @param mode Valeur du mode (SEQ, ARP, NONE).
 *
 * @details
 * Publie également un tag texte persistant vers le modèle pour le renderer.
 * Par design, le tag peut rester affiché même hors overlay (comportement
 * souhaité : indication du dernier mode utilisé).
 */
void ui_overlay_set_custom_mode(ui_custom_mode_t mode)
{
    s_custom_mode_active = mode;

    /* Publie le tag texte persistant pour l'affichage (renderer) */
    switch (mode) {
        case UI_CUSTOM_SEQ: ui_model_set_active_overlay_tag("SEQ"); break;
        case UI_CUSTOM_ARP: ui_model_set_active_overlay_tag("ARP"); break;
        case UI_CUSTOM_NONE:
        default:
            /* On conserve le dernier tag par défaut (pas de reset agressif). */
            /* Si désiré : ui_model_set_active_overlay_tag(""); */
            break;
    }
}

/**
 * @brief Lit le dernier mode custom actif (persistant).
 * @return Mode courant (SEQ, ARP, NONE).
 */
ui_custom_mode_t ui_overlay_get_custom_mode(void)
{
    return s_custom_mode_active;
}

/* --------- Préparation bannières (MODE/SETUP) --------------------------- */

/**
 * @brief Prépare deux bannières d'overlay (MODE/SETUP) à partir de sources.
 *
 * @param src_mode   Spécification source pour l'overlay MODE.
 * @param src_setup  Spécification source pour l'overlay SETUP.
 * @param dst_mode   Destination écrite pour MODE (copie superficielle).
 * @param dst_setup  Destination écrite pour SETUP (copie superficielle).
 * @param prev_cart  Cart « réelle » (pour injecter son nom dans la bannière).
 * @param mode_tag   Tag texte de l'overlay (ex.: "SEQ", "ARP").
 *
 * @details
 * - Copie superficielle : sous-structures (pages/params) restent partagées.
 * - Injecte `cart_name` (de la cart réelle) et `overlay_tag` pour l'affichage.
 */
void ui_overlay_prepare_banner(const ui_cart_spec_t* src_mode,
                               const ui_cart_spec_t* src_setup,
                               ui_cart_spec_t* dst_mode,
                               ui_cart_spec_t* dst_setup,
                               const ui_cart_spec_t* prev_cart,
                               const char* mode_tag)
{
    const char* banner = (prev_cart && prev_cart->cart_name) ? prev_cart->cart_name : "UI";

    /* Copie superficielle : structures principales dupliquées, sous-objets partagés */
    *dst_mode  = *src_mode;
    *dst_setup = *src_setup;

    /* Insertion du nom de cart & tag overlay pour le rendu (bandeau) */
    dst_mode->cart_name   = banner;
    dst_setup->cart_name  = banner;
    dst_mode->overlay_tag = mode_tag;
    dst_setup->overlay_tag = mode_tag;
}
