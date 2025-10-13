/**
 * @file seq_service.c
 * @brief Sequencer bootstrap service wiring engine and runtime together.
 * @ingroup core
 */

#include "seq_service.h"

#include <string.h>

#include "seq_engine.h"
#include "seq_runtime.h"

void seq_service_init(void) {
    seq_runtime_init();

    seq_engine_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.dest = MIDI_DEST_BOTH;
    for (uint8_t v = 0; v < SEQ_MODEL_VOICE_COUNT; ++v) {
        cfg.midi_channel[v] = (uint8_t)(v + 1);
    }

    seq_engine_init(&cfg);
}
