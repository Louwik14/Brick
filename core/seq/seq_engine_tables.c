/**
 * @file seq_engine_tables.c
 * @brief Sequencer engine lookup tables stored in flash (.rodata).
 */

#include "seq_engine.h"

const uint16_t k_seq_engine_scale_masks[SEQ_ENGINE_SCALE_MASK_COUNT] = {
    0x0FFFU, /* Chromatic */
    0x0AB5U, /* Major: 0,2,4,5,7,9,11 */
    0x05ADU, /* Natural minor: 0,2,3,5,7,8,10 */
    0x06ADU, /* Dorian: 0,2,3,5,7,9,10 */
    0x06B5U, /* Mixolydian: 0,2,4,5,7,9,10 */
};
