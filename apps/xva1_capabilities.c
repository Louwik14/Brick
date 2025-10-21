#include "xva1_capabilities.h"

uint8_t xva1_num_cartridges(void) {
    return XVA1_NUM_CARTRIDGES;
}

uint8_t xva1_tracks_per_cartridge(void) {
    return XVA1_TRACKS_PER_CART;
}

uint8_t xva1_total_tracks(void) {
    return (uint8_t)(XVA1_NUM_CARTRIDGES * XVA1_TRACKS_PER_CART);
}
