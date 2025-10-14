/**
 * @file ui_seq_ids.h
 * @brief Shared identifiers for the SEQ UI parameters and hold rendering.
 */

#ifndef BRICK_UI_SEQ_IDS_H
#define BRICK_UI_SEQ_IDS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Local identifiers (13-bit payload) used by the SEQ UI cart.
 */
typedef enum {
    SEQ_UI_LOCAL_ALL_TRANSP = 0x0000,
    SEQ_UI_LOCAL_ALL_VEL,
    SEQ_UI_LOCAL_ALL_LEN,
    SEQ_UI_LOCAL_ALL_MIC,

    SEQ_UI_LOCAL_V1_NOTE,
    SEQ_UI_LOCAL_V1_VEL,
    SEQ_UI_LOCAL_V1_LEN,
    SEQ_UI_LOCAL_V1_MIC,

    SEQ_UI_LOCAL_V2_NOTE,
    SEQ_UI_LOCAL_V2_VEL,
    SEQ_UI_LOCAL_V2_LEN,
    SEQ_UI_LOCAL_V2_MIC,

    SEQ_UI_LOCAL_V3_NOTE,
    SEQ_UI_LOCAL_V3_VEL,
    SEQ_UI_LOCAL_V3_LEN,
    SEQ_UI_LOCAL_V3_MIC,

    SEQ_UI_LOCAL_V4_NOTE,
    SEQ_UI_LOCAL_V4_VEL,
    SEQ_UI_LOCAL_V4_LEN,
    SEQ_UI_LOCAL_V4_MIC,

    /* Setup menu locals follow (not used for hold rendering). */
    SEQ_UI_LOCAL_SETUP_CLOCK,
    SEQ_UI_LOCAL_SETUP_SWING,
    SEQ_UI_LOCAL_SETUP_STEPS,
    SEQ_UI_LOCAL_SETUP_QUANT,

    SEQ_UI_LOCAL_SETUP_CH1,
    SEQ_UI_LOCAL_SETUP_CH2,
    SEQ_UI_LOCAL_SETUP_CH3,
    SEQ_UI_LOCAL_SETUP_CH4
} seq_ui_local_id_t;

/**
 * @brief Identifiers used when aggregating held-step parameters for rendering.
 */
typedef enum {
    SEQ_HOLD_PARAM_ALL_TRANSP = 0,
    SEQ_HOLD_PARAM_ALL_VEL,
    SEQ_HOLD_PARAM_ALL_LEN,
    SEQ_HOLD_PARAM_ALL_MIC,

    SEQ_HOLD_PARAM_V1_NOTE,
    SEQ_HOLD_PARAM_V1_VEL,
    SEQ_HOLD_PARAM_V1_LEN,
    SEQ_HOLD_PARAM_V1_MIC,

    SEQ_HOLD_PARAM_V2_NOTE,
    SEQ_HOLD_PARAM_V2_VEL,
    SEQ_HOLD_PARAM_V2_LEN,
    SEQ_HOLD_PARAM_V2_MIC,

    SEQ_HOLD_PARAM_V3_NOTE,
    SEQ_HOLD_PARAM_V3_VEL,
    SEQ_HOLD_PARAM_V3_LEN,
    SEQ_HOLD_PARAM_V3_MIC,

    SEQ_HOLD_PARAM_V4_NOTE,
    SEQ_HOLD_PARAM_V4_VEL,
    SEQ_HOLD_PARAM_V4_LEN,
    SEQ_HOLD_PARAM_V4_MIC,

    SEQ_HOLD_PARAM_COUNT
} seq_hold_param_id_t;

/** Utility macro returning the hold parameter base index for a voice. */
#define SEQ_HOLD_PARAM_VOICE_BASE(v) (SEQ_HOLD_PARAM_V1_NOTE + ((v) * 4))

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_SEQ_IDS_H */
