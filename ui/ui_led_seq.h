/**
 * @file ui_led_seq.h
 * @brief Rendu LED du mode SEQ (playhead absolu, P-Lock, param-only).
 * @ingroup ui_led_backend
 * @ingroup ui_seq
 */
#ifndef BRICK_UI_LED_SEQ_H
#define BRICK_UI_LED_SEQ_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool active;
    bool recorded;
    bool param_only;
} ui_seq_led_step_t;

typedef struct {
    uint8_t           visible_page;        /* 0..N */
    uint8_t           steps_per_page;      /* 16 pads visibles */
    uint16_t          plock_selected_mask; /* bits 0..15 pour la page visible */
    ui_seq_led_step_t steps[16];
} ui_seq_led_surface_t;

/* Publication snapshot (copie locale) */
void ui_led_seq_update_from_app(const ui_seq_led_surface_t *surface);

/* Tick d’horloge : **index absolu** recommandé (0..pages*16-1), sinon relatif 0..15. */
void ui_led_seq_on_clock_tick(uint8_t step_index);

/* START/STOP explicite pour stop dur du chenillard */
void ui_led_seq_set_running(bool running);

/* Longueur totale du séquenceur (pages * 16). Défaut = 4 * 16 = 64. Bornée à [16..256]. */
void ui_led_seq_set_total_span(uint16_t total_steps);

/* Rendu de la page visible */
void ui_led_seq_render(void);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_LED_SEQ_H */
