#pragma once

#include <stdint.h>

#include "core/seq/seq_config.h"

#ifdef __cplusplus
extern "C" {
#endif

uint8_t xva1_num_cartridges(void);
uint8_t xva1_tracks_per_cartridge(void);
uint8_t xva1_total_tracks(void);

#ifdef __cplusplus
}
#endif
