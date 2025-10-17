/**
 * @file ui_shortcuts.h
 * @brief Raccourcis (SHIFT, MUTE/PMUTE), overlays (SEQ/ARP/KEY), et routage SEQ (pages, pads).
 * @ingroup ui
 *
 * @details
 * Objectifs (Elektron-like) :
 * - Tap court = Quick Step / Quick Clear.
 * - Maintien (un ou plusieurs steps) = **Preview P-Lock** (affichage des valeurs P-Lock,
 *   encodeurs modifient les P-Lock des steps maintenus). Aucune couleur "focus violet".
 * - À la relâche de tous les steps, fin de preview et retour à l’état normal.
 *
 * Invariants :
 * - MUTE prioritaire ; pas de dépendances circulaires ; zéro régression Keyboard/MIDI.
 */

#ifndef BRICK_UI_SHORTCUTS_H
#define BRICK_UI_SHORTCUTS_H

#include <stdbool.h>
#include <stdint.h>
#include "ui_backend.h"   /* ui_mode_context_t */
#include "ui_input.h"      /* ui_input_event_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Types d'actions générées par la couche de mapping.
 */
typedef enum {
    UI_SHORTCUT_ACTION_NONE = 0,            /**< Aucun effet secondaire. */
    UI_SHORTCUT_ACTION_ENTER_MUTE_QUICK,    /**< Entrée dans MUTE rapide. */
    UI_SHORTCUT_ACTION_ENTER_MUTE_PMUTE,    /**< Transition QUICK → PMUTE. */
    UI_SHORTCUT_ACTION_EXIT_MUTE,           /**< Sortie de MUTE/PMUTE. */
    UI_SHORTCUT_ACTION_TOGGLE_MUTE_TRACK,   /**< Toggle direct d'une piste (QUICK). */
    UI_SHORTCUT_ACTION_PREPARE_PMUTE_TRACK, /**< Prépare une piste pour PMUTE. */
    UI_SHORTCUT_ACTION_COMMIT_PMUTE,        /**< Valide les PMUTE préparés. */
    UI_SHORTCUT_ACTION_OPEN_SEQ_OVERLAY,    /**< Active overlay SEQ (MODE/SETUP). */
    UI_SHORTCUT_ACTION_OPEN_ARP_OVERLAY,    /**< Active overlay ARP (MODE/SETUP). */
    UI_SHORTCUT_ACTION_OPEN_KBD_OVERLAY,    /**< Active overlay Keyboard. */
    UI_SHORTCUT_ACTION_KEYBOARD_TOGGLE_SUBMENU, /**< Cycle Keyboard ↔ Arpégiateur. */ // --- ARP: action dédiée ---
    UI_SHORTCUT_ACTION_ENTER_TRACK_MODE,    /**< Active le mode Track Select. */
    UI_SHORTCUT_ACTION_EXIT_TRACK_MODE,     /**< Quitte le mode Track Select. */
    UI_SHORTCUT_ACTION_TRACK_SELECT,        /**< Sélectionne une piste depuis la grille. */
    UI_SHORTCUT_ACTION_TRANSPORT_PLAY,      /**< PLAY global. */
    UI_SHORTCUT_ACTION_TRANSPORT_STOP,      /**< STOP global. */
    UI_SHORTCUT_ACTION_TRANSPORT_REC_TOGGLE,/**< Toggle REC global. */
    UI_SHORTCUT_ACTION_SEQ_PAGE_NEXT,       /**< Page SEQ suivante. */
    UI_SHORTCUT_ACTION_SEQ_PAGE_PREV,       /**< Page SEQ précédente. */
    UI_SHORTCUT_ACTION_SEQ_STEP_HOLD,       /**< Maintien d'un pad SEQ. */
    UI_SHORTCUT_ACTION_SEQ_STEP_RELEASE,    /**< Relâche d'un pad SEQ. */
    UI_SHORTCUT_ACTION_SEQ_ENCODER_TOUCH,   /**< Mouvement encodeur pendant hold. */
    UI_SHORTCUT_ACTION_KEY_OCTAVE_UP,       /**< Octave + (mode Keyboard). */
    UI_SHORTCUT_ACTION_KEY_OCTAVE_DOWN      /**< Octave - (mode Keyboard). */
} ui_shortcut_action_type_t;

/**
 * @brief Données associées à une action de shortcut.
 */
typedef struct {
    ui_shortcut_action_type_t type; /**< Type d'action. */
    union {
        struct { uint8_t track; } mute; /**< Index piste (mute). */
        struct { uint8_t index; } track; /**< Index piste (track select). */
        struct {
            uint8_t index;             /**< Index step 0..15. */
            bool    long_press;        /**< Vrai si long-press détecté. */
        } seq_step;
        struct { uint16_t mask; } seq_mask; /**< Masque de steps maintenus. */
    } data;
} ui_shortcut_action_t;

/** Nombre max d'actions générées par évènement. */
#define UI_SHORTCUT_MAX_ACTIONS 6u

/**
 * @brief Résultat produit par la couche de mapping.
 */
typedef struct {
    ui_shortcut_action_t actions[UI_SHORTCUT_MAX_ACTIONS]; /**< Actions détectées. */
    uint8_t action_count;                                  /**< Nombre d'actions. */
    bool    consumed;                                      /**< true si l'évènement est consommé. */
} ui_shortcut_map_result_t;

/**
 * @brief Initialise le contexte runtime côté mapping.
 */
void ui_shortcut_map_init(ui_mode_context_t *ctx);

/**
 * @brief Réinitialise le contexte runtime (alias de init).
 */
void ui_shortcut_map_reset(ui_mode_context_t *ctx);

/**
 * @brief Map un évènement brut vers un ensemble d'actions.
 * @param evt  Évènement brut.
 * @param ctx  Contexte runtime partagé (in/out).
 * @return Résultat contenant les actions + flag consumed.
 */
ui_shortcut_map_result_t ui_shortcut_map_process(const ui_input_event_t *evt,
                                                 ui_mode_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_SHORTCUTS_H */
