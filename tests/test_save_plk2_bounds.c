#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include "core/seq/seq_model.h"
#include "core/seq/seq_plock_ids.h"
#include "core/seq/seq_plock_pool.h"
#include "core/seq/seq_project.h"
#include "core/seq/runtime/seq_runtime_cold.h"
#include "cart/cart_bus.h"
#include "cart/cart_registry.h"

bool board_flash_init(void) { return true; }
bool board_flash_is_ready(void) { return true; }
uint32_t board_flash_get_capacity(void) { return BOARD_FLASH_CAPACITY_BYTES; }
uint32_t board_flash_get_sector_size(void) { return BOARD_FLASH_SECTOR_SIZE; }
bool board_flash_read(uint32_t address, void *buffer, size_t length) {
    (void)address;
    (void)buffer;
    (void)length;
    return true;
}
bool board_flash_write(uint32_t address, const void *data, size_t length) {
    (void)address;
    (void)data;
    (void)length;
    return true;
}
bool board_flash_erase(uint32_t address, size_t length) {
    (void)address;
    (void)length;
    return true;
}
bool board_flash_erase_sector(uint32_t address) {
    (void)address;
    return true;
}

void cart_registry_init(void) {}
void cart_registry_register(cart_id_t id, const struct ui_cart_spec_t *spec) {
    (void)id;
    (void)spec;
}
const struct ui_cart_spec_t *cart_registry_get_ui_spec(cart_id_t id) {
    (void)id;
    return NULL;
}
const struct ui_cart_spec_t *cart_registry_switch(cart_id_t id) {
    (void)id;
    return NULL;
}
cart_id_t cart_registry_get_active_id(void) { return CART1; }
bool cart_registry_is_present(cart_id_t id) {
    (void)id;
    return false;
}
void cart_registry_set_uid(cart_id_t id, uint32_t uid) {
    (void)id;
    (void)uid;
}
uint32_t cart_registry_get_uid(cart_id_t id) {
    (void)id;
    return 0U;
}
bool cart_registry_find_by_uid(uint32_t uid, cart_id_t *out) {
    (void)uid;
    if (out != NULL) {
        *out = CART1;
    }
    return false;
}

seq_cold_view_t seq_runtime_cold_view(seq_cold_domain_t domain) {
    (void)domain;
    seq_cold_view_t view = {0};
    return view;
}

static size_t find_plk2(const uint8_t *buffer, size_t length) {
    if (length < 4U) {
        return length;
    }
    for (size_t i = 0U; i + 4U <= length; ++i) {
        if ((buffer[i] == 'P') && (buffer[i + 1U] == 'L') &&
            (buffer[i + 2U] == 'K') && (buffer[i + 3U] == '2')) {
            return i;
        }
    }
    return length;
}

int main(void) {
    seq_plock_pool_reset();

    seq_model_track_t track;
    seq_model_track_init(&track);

    seq_model_step_t *step0 = &track.steps[0];
    seq_model_voice_t voice = step0->voices[0];
    voice.state = SEQ_MODEL_VOICE_ENABLED;
    voice.velocity = 100U;
    seq_model_step_set_voice(step0, 0U, &voice);

    const uint8_t count = 255U;
    uint8_t ids[255];
    uint8_t values[255];
    uint8_t flags[255];
    for (uint16_t i = 0U; i < count; ++i) {
        ids[i] = (uint8_t)(0x40U + (i & 0x3FU));
        values[i] = (uint8_t)i;
        flags[i] = (uint8_t)(i & 0x0FU);
    }
    assert(seq_model_step_set_plocks_pooled(step0, ids, values, flags, count) == 0);

    uint8_t legacy_buffer[4096];
    ssize_t legacy_written = seq_codec_write_track_with_plk2(legacy_buffer, sizeof(legacy_buffer), &track, 0);
    assert(legacy_written > 0);
    assert(find_plk2(legacy_buffer, (size_t)legacy_written) == (size_t)legacy_written);

    uint8_t buffer[4096];
    ssize_t written = seq_codec_write_track_with_plk2(buffer, sizeof(buffer), &track, 1);
    assert(written > 0);
    const size_t expected_delta = 4U + 1U + (size_t)count * 3U;
    assert(written >= legacy_written);
    assert((size_t)(written - legacy_written) == expected_delta);

    size_t pos = find_plk2(buffer, (size_t)written);
    assert(pos < (size_t)written);
    assert(pos + expected_delta <= (size_t)written);
    assert(buffer[pos + 4U] == count);
    assert(buffer[pos + 5U] == ids[0]);
    assert(buffer[pos + 6U] == values[0]);
    assert(buffer[pos + 7U] == flags[0]);

    const size_t last_offset = pos + 5U + (size_t)(count - 1U) * 3U;
    assert(buffer[last_offset] == ids[count - 1U]);
    assert(buffer[last_offset + 1U] == values[count - 1U]);
    assert(buffer[last_offset + 2U] == flags[count - 1U]);

    assert(seq_codec_write_track_with_plk2(buffer, (size_t)written - 1U, &track, 1) == -1);

    return 0;
}
