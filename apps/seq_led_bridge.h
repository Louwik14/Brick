/**
 * @file seq_led_bridge.h
 * @brief Pont d’état SEQ ↔ renderer (pages, quick-step, param-only, publish, P-Lock preview).
 * @ingroup ui_led_backend
 * @ingroup ui_modes
 * @ingroup ui_seq
 *
 * @details
 * - Stockage **multi-pages** des steps (16 pas par page) avec POLY 4 voix + P-Lock.
 * - Publication idempotente via @ref seq_led_bridge_publish → @ref ui_led_seq_update_from_app.
 * - Détermination LED: active(≥1 vel>0) > param_only(has_plock & all vel=0) > off.
 * - **Preview P-Lock** (begin/apply/end) — aucune couleur dédiée (LED = état réel).
 */

#ifndef BRICK_SEQ_LED_BRIDGE_H
#define BRICK_SEQ_LED_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "ui_led_seq.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ====== Modèle de step polyphonique (4 voix) ============================ */

typedef struct {
    uint8_t pitch;     /**< Note MIDI (0..127) */
    uint8_t velocity;  /**< Vélocité (0..127). 0 = muet */
    bool    active;    /**< Simplifie l'accès: true si voix porte une note */
} seq_voice_t;

typedef struct {
    seq_voice_t voices[4];
    uint8_t     num_voices;      /**< Nombre de voix utilisées (0..4) */
    bool        has_param_lock;  /**< Au moins un paramètre P-Locké */
} seq_step_poly_t;

/* ====== API Bridge ======================================================= */
void seq_led_bridge_init(void);
void seq_led_bridge_publish(void);

void seq_led_bridge_set_max_pages(uint8_t max_pages);
void seq_led_bridge_set_total_span(uint16_t total_steps);

void seq_led_bridge_page_next(void);
void seq_led_bridge_page_prev(void);
void seq_led_bridge_set_visible_page(uint8_t page);

/* Edition simple */
void seq_led_bridge_quick_toggle_step(uint8_t i);
void seq_led_bridge_set_step_param_only(uint8_t i, bool on);

/* Hooks transport */
void seq_led_bridge_on_play(void);
void seq_led_bridge_on_stop(void);

/* Preview P-Lock */
void seq_led_bridge_set_plock_mask(uint16_t mask);
void seq_led_bridge_plock_add(uint8_t i);
void seq_led_bridge_plock_remove(uint8_t i);
void seq_led_bridge_plock_clear(void);
void seq_led_bridge_begin_plock_preview(uint16_t held_mask);
void seq_led_bridge_apply_plock_param(uint8_t param_id, int32_t delta, uint16_t held_mask);
void seq_led_bridge_end_plock_preview(void);

/* Helpers (exposés si besoin moteur) */
void seq_led_bridge_step_clear(uint8_t i);
void seq_led_bridge_step_set_voice(uint8_t i, uint8_t voice_idx, uint8_t pitch, uint8_t velocity);
void seq_led_bridge_step_set_has_plock(uint8_t i, bool on);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_SEQ_LED_BRIDGE_H */
