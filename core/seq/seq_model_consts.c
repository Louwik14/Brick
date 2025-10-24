/**
 * @file seq_model_consts.c
 * @brief Sequencer model default templates stored in flash (.rodata).
 */

#include "seq_model.h"

const seq_model_step_t k_seq_model_step_default = {
    .voices = {
        {
            .note = 60U,
            .velocity = SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY,
            .length = 16U,
            .micro_offset = 0,
            .state = SEQ_MODEL_VOICE_DISABLED,
        },
        {
            .note = 60U,
            .velocity = SEQ_MODEL_DEFAULT_VELOCITY_SECONDARY,
            .length = 16U,
            .micro_offset = 0,
            .state = SEQ_MODEL_VOICE_DISABLED,
        },
        {
            .note = 60U,
            .velocity = SEQ_MODEL_DEFAULT_VELOCITY_SECONDARY,
            .length = 16U,
            .micro_offset = 0,
            .state = SEQ_MODEL_VOICE_DISABLED,
        },
        {
            .note = 60U,
            .velocity = SEQ_MODEL_DEFAULT_VELOCITY_SECONDARY,
            .length = 16U,
            .micro_offset = 0,
            .state = SEQ_MODEL_VOICE_DISABLED,
        },
    },
#if SEQ_FEATURE_PLOCK_POOL
    .pl_ref = {
        .offset = 0U,
        .count = 0U,
    },
#endif
    .offsets = {
        .velocity = 0,
        .transpose = 0,
        .length = 0,
        .micro = 0,
    },
    .flags = {
        .active = 0,
        .automation = 0,
        .reserved = 0,
    },
};

const seq_model_track_config_t k_seq_model_track_config_default = {
    .quantize = {
        .enabled = false,
        .grid = SEQ_MODEL_QUANTIZE_1_16,
        .strength = 100U,
    },
    .transpose = {
        .global = 0,
        .per_voice = { 0, 0, 0, 0 },
    },
    .scale = {
        .enabled = false,
        .root = 0U,
        .mode = SEQ_MODEL_SCALE_CHROMATIC,
    },
};
