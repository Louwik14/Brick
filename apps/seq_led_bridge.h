/**
 * @file seq_led_bridge.h
 * @brief Sequencer LED bridge (pages, quick-step, param-only) backed by seq_model.
 * @ingroup ui_led_backend
 * @ingroup ui_modes
 * @ingroup ui_seq
 */

#ifndef BRICK_SEQ_LED_BRIDGE_H
#define BRICK_SEQ_LED_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

#include "core/seq/seq_access.h"
#include "ui_led_seq.h"
#include "ui_seq_ids.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ====== API Bridge ======================================================= */
void seq_led_bridge_init(void);
void seq_led_bridge_publish(void);

void seq_led_bridge_set_max_pages(uint8_t max_pages);
void seq_led_bridge_set_total_span(uint16_t total_steps);

void seq_led_bridge_page_next(void);
void seq_led_bridge_page_prev(void);
void seq_led_bridge_set_visible_page(uint8_t page);

void seq_led_bridge_set_active(uint8_t bank, uint8_t pattern);

/** @brief Page visible courante (0..N-1). */
uint8_t seq_led_bridge_get_visible_page(void);

/** @brief Nombre maximum de pages configuré. */
uint8_t seq_led_bridge_get_max_pages(void);

/* Edition simple */
void seq_led_bridge_quick_toggle_step(uint8_t i);
void seq_led_bridge_set_step_param_only(uint8_t i, bool on);

/* Hooks transport */
void seq_led_bridge_on_play(void);
void seq_led_bridge_on_stop(void);

/* Preview P-Lock */
void seq_led_bridge_plock_add(uint8_t i);
void seq_led_bridge_plock_remove(uint8_t i);
void seq_led_bridge_begin_plock_preview(uint16_t held_mask);
void seq_led_bridge_apply_plock_param(seq_hold_param_id_t param_id,
                                      int32_t value,
                                      uint16_t held_mask);
void seq_led_bridge_end_plock_preview(void);
void seq_led_bridge_apply_cart_param(uint16_t parameter_id,
                                     int32_t value,
                                     uint16_t held_mask);

typedef struct {
    bool available;   /**< True if at least one held step exposed a value. */
    bool mixed;       /**< True when held steps differ on the parameter. */
    bool plocked;     /**< True if any held step carries a p-lock for the parameter. */
    int32_t value;    /**< Aggregated value (valid when !mixed && available). */
} seq_led_bridge_hold_param_t;

typedef struct {
    bool active;                               /**< Hold/tweak mode currently active. */
    uint16_t mask;                             /**< Mask of held steps on the visible page. */
    uint8_t step_count;                        /**< Number of steps contributing to the view. */
    seq_led_bridge_hold_param_t params[SEQ_HOLD_PARAM_COUNT]; /**< Aggregated parameters. */
} seq_led_bridge_hold_view_t;

const seq_led_bridge_hold_view_t *seq_led_bridge_get_hold_view(void);
bool seq_led_bridge_hold_get_cart_param(uint16_t parameter_id,
                                        seq_led_bridge_hold_param_t *out);

/* Helpers (exposés si besoin moteur) */
void seq_led_bridge_step_clear(uint8_t i);
void seq_led_bridge_step_set_voice(uint8_t i, uint8_t voice_idx, uint8_t pitch, uint8_t velocity);
void seq_led_bridge_step_set_has_plock(uint8_t i, bool on);

seq_model_track_t *seq_led_bridge_access_track(void);
const seq_model_track_t *seq_led_bridge_get_track(void);
const seq_model_gen_t *seq_led_bridge_get_generation(void);

seq_project_t *seq_led_bridge_get_project(void);
const seq_project_t *seq_led_bridge_get_project_const(void);
uint8_t seq_led_bridge_get_track_index(void);
uint8_t seq_led_bridge_get_track_count(void);
bool seq_led_bridge_select_track(uint8_t track);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_SEQ_LED_BRIDGE_H */

