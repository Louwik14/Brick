/**
 * @file cart_link.c
 * @brief Gestion du lien logique entre le firmware principal et les cartouches (Cart Bus).
 *
 * Ce module fournit une API de haut niveau pour transférer des paramètres
 * vers la cartouche active via `cart_set_param()`. Il maintient également
 * un registre local “shadow” pour chaque cartouche, garantissant la cohérence
 * des valeurs entre l’UI et le bus physique UART.
 *
 * @ingroup cart
 */

#include "cart_link.h"
#include "cart_registry.h"   // pour connaître la cartouche active
#include "core/seq/seq_project.h"
#include "core/seq/seq_runtime.h"
#include <string.h>

/* =======================================================================
 *   Configuration interne
 * ======================================================================= */

/**
 * @brief Nombre maximum de paramètres shadowés par cartouche.
 *
 * @note Ajuster selon la taille réelle des registres cibles.
 */
#ifndef CART_LINK_MAX_DEST_ID
#define CART_LINK_MAX_DEST_ID 512
#endif

/** Registres shadow : un tableau par cartouche. */
static uint8_t g_shadow_params[CART_COUNT][CART_LINK_MAX_DEST_ID];

enum { CART_LINK_TRACKS_PER_CART = XVA1_TRACKS_PER_CART };

static void _cart_link_assign_tracks(void) {
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    seq_project_t *project = seq_runtime_access_project_mut();
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    if (project == NULL) {
        return;
    }

    const uint16_t total_tracks = seq_project_get_track_count(project);
    if (total_tracks == 0U) {
        return;
    }

    const uint8_t tracks_per_cart = CART_LINK_TRACKS_PER_CART;
    const uint16_t max_tracks = (uint16_t)CART_COUNT * (uint16_t)tracks_per_cart;
    const uint16_t assignable = (total_tracks < max_tracks) ? total_tracks : max_tracks;

    for (uint16_t idx = 0U; idx < assignable; ++idx) {
        seq_model_track_t *track = seq_project_get_track(project, (uint8_t)idx);
        if (track == NULL) {
            continue;
        }

        const uint8_t slot = (uint8_t)(idx / tracks_per_cart);
        seq_project_cart_ref_t cart_ref = {
            .cart_id = cart_registry_get_uid((cart_id_t)slot),
            .slot_id = slot,
            .capabilities = SEQ_PROJECT_CART_CAP_NONE,
            .flags = SEQ_PROJECT_CART_FLAG_NONE,
            .reserved = 0U,
        };
        seq_project_set_track_cart(project, idx, &cart_ref);
    }
}

/* =======================================================================
 *   Initialisation
 * ======================================================================= */

/**
 * @brief Initialise le sous-système CartLink.
 *
 * Réinitialise les registres shadow pour toutes les cartouches.
 */
void cart_link_init(void) {
    memset(g_shadow_params, 0, sizeof(g_shadow_params));
    _cart_link_assign_tracks();
}

/* =======================================================================
 *   Gestion des paramètres
 * ======================================================================= */

/**
 * @brief Notification d’un changement de paramètre depuis la couche UI.
 *
 * Met à jour le registre shadow local et envoie la valeur correspondante
 * à la cartouche actuellement active via `cart_set_param()`.
 *
 * @param param_id   Identifiant du paramètre (correspond au `dest_id`)
 * @param value      Nouvelle valeur (0/1 pour booléen, ou valeur brute)
 * @param is_bitwise Si vrai, applique le `bit_mask` sur la valeur existante
 * @param bit_mask   Masque binaire pour les modifications partielles (bits)
 */
void cart_link_param_changed(uint16_t param_id, uint8_t value,
                             bool is_bitwise, uint8_t bit_mask) {
    cart_id_t active = cart_registry_get_active_id();
    if (active >= CART_COUNT || param_id >= CART_LINK_MAX_DEST_ID) {
        return;
    }

    uint8_t *shadow = &g_shadow_params[active][param_id];
    uint8_t out;

    if (is_bitwise) {
        if (value) {
            *shadow |= bit_mask;
        } else {
            *shadow &= (uint8_t)~bit_mask;
        }
        out = *shadow;
    } else {
        *shadow = value;
        out = value;
    }

    /* Envoi bas-niveau au bus cartouche */
    cart_set_param(active, param_id, out);
}

/* =======================================================================
 *   API “Shadow” : accès local aux registres
 * ======================================================================= */

/**
 * @brief Lit la valeur shadow d’un paramètre.
 *
 * @param cid       Identifiant de la cartouche
 * @param param_id  ID du paramètre à lire
 * @return Valeur shadow actuelle (0 si hors bornes)
 */
uint8_t cart_link_shadow_get(cart_id_t cid, uint16_t param_id) {
    if (cid >= CART_COUNT || param_id >= CART_LINK_MAX_DEST_ID) return 0;
    return g_shadow_params[cid][param_id];
}

/**
 * @brief Écrit une valeur dans le shadow local (sans envoi bus).
 *
 * @param cid       Identifiant de la cartouche
 * @param param_id  ID du paramètre
 * @param v         Nouvelle valeur
 */
void cart_link_shadow_set(cart_id_t cid, uint16_t param_id, uint8_t v) {
    if (cid >= CART_COUNT || param_id >= CART_LINK_MAX_DEST_ID) return;
    g_shadow_params[cid][param_id] = v;
}
