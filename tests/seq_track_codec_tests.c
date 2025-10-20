#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "core/seq/seq_project.h"
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

static void populate_track(seq_model_track_t *track) {
    seq_model_track_init(track);

    for (uint8_t step = 0U; step < SEQ_MODEL_STEPS_PER_TRACK; step += 4U) {
        seq_model_step_t *s = &track->steps[step];
        seq_model_step_init_default(s, (uint8_t)(48U + step));
        for (uint8_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
            seq_model_voice_t voice = s->voices[v];
            voice.length = (uint8_t)(4U + v);
            voice.micro_offset = (int8_t)(v - 1);
            if (v == 0U) {
                voice.velocity = (uint8_t)(100U - step);
            }
            seq_model_step_set_voice(s, v, &voice);
        }

        seq_model_plock_t internal = {
            .value = (int16_t)(step * 2),
            .parameter_id = 0U,
            .domain = SEQ_MODEL_PLOCK_INTERNAL,
            .voice_index = 0U,
            .internal_param = SEQ_MODEL_PLOCK_PARAM_LENGTH
        };
        assert(seq_model_step_add_plock(s, &internal));

        seq_model_plock_t cart = {
            .value = (int16_t)(-step),
            .parameter_id = (uint16_t)(step + 1U),
            .domain = SEQ_MODEL_PLOCK_CART,
            .voice_index = 1U,
            .internal_param = 0U
        };
        assert(seq_model_step_add_plock(s, &cart));
    }
}

static bool track_equals(const seq_model_track_t *lhs, const seq_model_track_t *rhs) {
    return memcmp(lhs, rhs, sizeof(*lhs)) == 0;
}

static bool track_has_cart_plocks(const seq_model_track_t *track) {
    for (uint8_t s = 0U; s < SEQ_MODEL_STEPS_PER_TRACK; ++s) {
        const seq_model_step_t *step = &track->steps[s];
        for (uint8_t p = 0U; p < step->plock_count; ++p) {
            if (step->plocks[p].domain == SEQ_MODEL_PLOCK_CART) {
                return true;
            }
        }
    }
    return false;
}

static bool track_has_enabled_voice(const seq_model_track_t *track) {
    for (uint8_t s = 0U; s < SEQ_MODEL_STEPS_PER_TRACK; ++s) {
        const seq_model_step_t *step = &track->steps[s];
        for (uint8_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
            if (step->voices[v].state == SEQ_MODEL_VOICE_ENABLED) {
                return true;
            }
        }
    }
    return false;
}

int main(void) {
    seq_model_track_t original;
    seq_model_track_t decoded_full;
    seq_model_track_t decoded_drop;
    seq_model_track_t decoded_absent;
    uint8_t buffer[SEQ_PROJECT_PATTERN_STORAGE_MAX];
    size_t written = 0U;

    populate_track(&original);

    assert(seq_project_track_steps_encode(&original, buffer, sizeof(buffer), &written));
    assert(written > sizeof(uint16_t));

    assert(seq_project_track_steps_decode(&decoded_full, buffer, written,
                                            SEQ_PROJECT_PATTERN_VERSION,
                                            SEQ_PROJECT_TRACK_DECODE_FULL));
    assert(track_equals(&original, &decoded_full));

    assert(seq_project_track_steps_decode(&decoded_drop, buffer, written,
                                            SEQ_PROJECT_PATTERN_VERSION,
                                            SEQ_PROJECT_TRACK_DECODE_DROP_CART));
    assert(!track_has_cart_plocks(&decoded_drop));

    assert(seq_project_track_steps_decode(&decoded_absent, buffer, written,
                                            SEQ_PROJECT_PATTERN_VERSION,
                                            SEQ_PROJECT_TRACK_DECODE_ABSENT));
    assert(!track_has_enabled_voice(&decoded_absent));

    return 0;
}
