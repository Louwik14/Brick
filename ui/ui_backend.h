/**
 * @file ui_backend.h
 * @brief Pont neutre entre la couche UI (controller/model) et les couches basses (cart, UI interne, MIDI).
 * @ingroup ui
 *
 * @details
 * Le **backend UI** est la seule interface autorisée entre la logique UI (controller)
 * et les sous-systèmes matériels ou logiciels (CartLink, MIDI, etc.).
 * Il implémente un routage centralisé des mises à jour de paramètres,
 * sans exposer les dépendances basses couches à la UI.
 *
 * Trois destinations principales sont supportées :
 *
 * | Destination      | Masque binaire | Description |
 * |------------------|----------------|--------------|
 * | `UI_DEST_CART`   | `0x0000` | Paramètres routés vers la cartouche active |
 * | `UI_DEST_UI`     | `0x8000` | Paramètres internes à l’UI (menus, overlays) |
 * | `UI_DEST_MIDI`   | `0x4000` | Paramètres routés vers la pile MIDI |
 *
 * Invariants :
 * - Aucune I/O bloquante.
 * - Accès uniquement depuis le thread UI.
 * - Compatible 60 FPS : toutes les opérations sont O(1).
 */

#ifndef BRICK_UI_UI_BACKEND_H
#define BRICK_UI_UI_BACKEND_H

#include <stdint.h>
#include <stdbool.h>
#include "ch.h"            /* systime_t */
#include "ui_input.h"      /* ui_input_event_t */
#include "ui_overlay.h"    /* ui_custom_mode_t, ui_overlay_id_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Espaces de destination (bits hauts de l’identifiant)
 * ============================================================ */

/**
 * @name UI Backend Destination Masks
 * @brief Masques de routage pour les paramètres UI.
 * @{
 */
#define UI_DEST_MASK   0xE000U  /**< Masque général des bits de destination. */
#define UI_DEST_CART   0x0000U  /**< Paramètre destiné à la cartouche active. */
#define UI_DEST_UI     0x8000U  /**< Paramètre purement interne à l'UI. */
#define UI_DEST_MIDI   0x4000U  /**< Paramètre routé vers la pile MIDI. */
#define UI_DEST_ID(x)  ((x) & 0x1FFFU) /**< Extrait l'identifiant local sur 13 bits. */
/** @} */

/* ============================================================
 * Contexte mode UI (état centralisé partagé)
 * ============================================================ */

/**
 * @brief État MUTE courant.
 */
typedef enum {
    UI_MUTE_STATE_OFF = 0,   /**< Aucun mute actif. */
    UI_MUTE_STATE_QUICK,     /**< Mute rapide (SHIFT + + maintenu). */
    UI_MUTE_STATE_PMUTE      /**< Préparation de mute (PMUTE). */
} ui_mute_state_t;

/**
 * @brief État transport global.
 */
typedef struct {
    bool playing;            /**< Transport en lecture. */
    bool recording;          /**< Mode enregistrement actif. */
} ui_transport_state_t;

/**
 * @brief État seq (pages + steps maintenus).
 */
typedef struct {
    uint8_t  page_index;                     /**< Page visible (0..N-1). */
    uint8_t  page_count;                     /**< Nombre total de pages. */
    uint16_t held_mask;                      /**< Steps maintenus (bits). */
    bool     held_flags[16];                 /**< Drapeaux runtime pour chaque step. */
    systime_t hold_start[16];                /**< Timestamp d'appui (pour long-press). */
    uint8_t  track_index;                    /**< Piste active dans le projet multi-track. */
    uint8_t  track_count;                    /**< Nombre de pistes disponibles. */
} ui_seq_runtime_t;

/**
 * @brief État interne du sélecteur de piste.
 */
typedef struct {
    bool active;            /**< Mode Track Select actif ? */
    bool shift_latched;     /**< État précédent de SHIFT pour la détection de sortie. */
} ui_track_state_t;

/**
 * @brief État Keyboard (mode custom latched).
 */
typedef struct {
    bool  active;            /**< Mode Keyboard actif (overlay visible ou latched). */
    bool  overlay_visible;   /**< L'overlay Keyboard est affiché. */
    int8_t octave;           /**< Décalage d'octave courant. */
    bool  arp_submenu_active;/**< true si le sous-menu Arpégiateur est affiché. */ // --- ARP: persistance sous-menu ---
} ui_keyboard_state_t;

/**
 * @brief Contexte runtime partagé entre shortcuts/backend.
 *
 * Contient :
 * - mode custom actif (`custom_mode`),
 * - overlay courant (`overlay_id`) + sous-mode (0 = MODE, 1 = SETUP),
 * - état MUTE, transport, Keyboard,
 * - pages SEQ visibles et steps maintenus.
 */
typedef struct {
    ui_custom_mode_t    custom_mode;      /**< Dernier mode custom actif. */
    ui_overlay_id_t     overlay_id;       /**< Overlay affiché (ou NONE). */
    uint8_t             overlay_submode;  /**< 0 = MODE, 1 = SETUP. */
    bool                overlay_active;   /**< Overlay visible ? */

    ui_mute_state_t     mute_state;       /**< Machine MUTE (OFF/QUICK/PMUTE). */
    bool                mute_plus_down;   /**< Bouton PLUS maintenu ? */
    bool                mute_shift_latched; /**< SHIFT actif lors du dernier event. */

    ui_transport_state_t transport;       /**< Transport global. */
    ui_seq_runtime_t     seq;             /**< Runtime SEQ (page + holds). */
    ui_keyboard_state_t  keyboard;        /**< Runtime Keyboard. */
    ui_track_state_t     track;           /**< État du mode Track Select. */
} ui_mode_context_t;

/**
 * @brief Modes séquenceur utilisés pour la synchronisation LED.
 */
typedef enum {
    SEQ_MODE_DEFAULT = 0,   /**< Mode séquenceur normal (SEQ / overlays). */
    SEQ_MODE_PMUTE,         /**< Mode MUTE / PMUTE actif. */
    SEQ_MODE_TRACK          /**< Mode Track Select actif. */
} seq_mode_t;

/* ============================================================
 * API publique
 * ============================================================ */

/**
 * @brief Initialise le contexte mode + tables runtime.
 */
void ui_backend_init_runtime(void);

/**
 * @brief Retourne le contexte runtime courant (lecture seule).
 */
const ui_mode_context_t* ui_backend_get_mode_context(void);

/**
 * @brief Renvoie le label court du mode actif (bandeau supérieur).
 *
 * @details
 *  - La chaîne retournée est possédée par le backend et reste valide jusqu’à
 *    la prochaine transition de mode.
 *  - Toujours non NULL ; fallback "SEQ" si aucun mode n’est encore défini.
 */
const char* ui_backend_get_mode_label(void);

/**
 * @brief Traite un évènement d'entrée complet (bouton/encodeur).
 * @param evt Évènement à traiter (doit être non NULL).
 */
void ui_backend_process_input(const ui_input_event_t* evt);

/**
 * @brief Notifie un changement de paramètre issu de l’UI.
 * @ingroup ui_backend
 *
 * @param id        Identifiant de paramètre encodé (`UI_DEST_* + ID local`)
 * @param val       Nouvelle valeur (0/1 ou 0–255)
 * @param bitwise   Si `true`, appliquer un masque binaire
 * @param mask      Masque de bits à appliquer lorsque @p bitwise est `true`
 *
 * @details
 * Cette fonction délègue la mise à jour vers la destination correspondante :
 * - `UI_DEST_CART` → `cart_link_param_changed()`
 * - `UI_DEST_UI`   → `ui_backend_handle_ui()`
 * - `UI_DEST_MIDI` → `ui_backend_handle_midi()`
 *
 * Appelée exclusivement depuis le thread UI.
 */
void ui_backend_param_changed(uint16_t id, uint8_t val, bool bitwise, uint8_t mask);

/**
 * @brief Lecture du **shadow register** local (valeur courante affichable).
 * @ingroup ui_backend
 *
 * @param id Identifiant de paramètre.
 * @return Valeur locale en cache pour ce paramètre.
 *
 * @note
 * Aucun accès cartouche : cette fonction lit uniquement le shadow local
 * synchronisé avec la dernière valeur envoyée.
 */
uint8_t ui_backend_shadow_get(uint16_t id);
bool ui_backend_shadow_try_get(uint16_t id, uint8_t *out_val); // --- FIX: détecter shadow non initialisé ---

/**
 * @brief Écriture dans le **shadow register** local (sans envoi immédiat).
 * @ingroup ui_backend
 *
 * @param id  Identifiant de paramètre.
 * @param val Valeur à stocker localement.
 *
 * @note
 * Maintient la cohérence entre l’UI et le shadow sans communication externe.
 * L’envoi réel se fait via `ui_backend_param_changed()`.
 */
void ui_backend_shadow_set(uint16_t id, uint8_t val);

/* ========================================================================== */
/* API Track Select / synchronisation LED                                     */
/* ========================================================================== */

/**
 * @brief Active le mode Track Select (SHIFT + BS11).
 */
void ui_track_mode_enter(void);

/**
 * @brief Quitte le mode Track Select.
 */
void ui_track_mode_exit(void);

/**
 * @brief Traite la sélection d'une piste depuis un bouton Step/BS.
 * @param bs_index Index BS 0..15 correspondant à la grille 4×4.
 */
void ui_track_select_from_bs(uint8_t bs_index);

/**
 * @brief Force la republication de l'état LED lors d'un changement de mode.
 * @param new_mode Nouveau mode séquenceur.
 */
void ui_led_refresh_state_on_mode_change(seq_mode_t new_mode);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_UI_BACKEND_H */
