#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "core/seq/seq_model.h"
#include "core/seq/seq_project.h"
#include "core/seq/seq_plock_pool.h"
#include "core/seq/seq_plock_ids.h"
#include "core/seq/reader/seq_reader.h"
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

        const plk2_t entries[2] = {
            {
                .param_id = PL_INT_LEN_V0,
                .value = pl_u8_from_s8((int8_t)(step / 2)),
                .flags = (uint8_t)(SEQ_READER_PL_FLAG_SIGNED |
                                   (0U << SEQ_READER_PL_FLAG_VOICE_SHIFT)),
            },
            {
                .param_id = (uint8_t)(0x40U + ((step / 4U) & 0x1FU)),
                .value = (uint8_t)(0x10U + step),
                .flags = SEQ_READER_PL_FLAG_DOMAIN_CART,
            },
        };
        assert(seq_model_step_set_plocks_pooled(s, entries, 2U) == 0);
    }
}

static bool track_plocks_equal(const seq_model_track_t *lhs, const seq_model_track_t *rhs) {
    if ((lhs == NULL) || (rhs == NULL)) {
        return false;
    }

    for (uint8_t s = 0U; s < SEQ_MODEL_STEPS_PER_TRACK; ++s) {
        const seq_model_step_t *ls = &lhs->steps[s];
        const seq_model_step_t *rs = &rhs->steps[s];
        const uint8_t lc = seq_model_step_plock_count(ls);
        const uint8_t rc = seq_model_step_plock_count(rs);
        if (lc != rc) {
            return false;
        }
        for (uint8_t i = 0U; i < lc; ++i) {
            const plk2_t *le = seq_model_step_get_plock(ls, i);
            const plk2_t *re = seq_model_step_get_plock(rs, i);
            if ((le == NULL) || (re == NULL)) {
                return false;
            }
            if ((le->param_id != re->param_id) ||
                (le->value != re->value) ||
                (le->flags != re->flags)) {
                return false;
            }
        }
    }
    return true;
}

static bool track_has_cart_plocks(const seq_model_track_t *track) {
    for (uint8_t s = 0U; s < SEQ_MODEL_STEPS_PER_TRACK; ++s) {
        const seq_model_step_t *step = &track->steps[s];
        seq_reader_pl_it_t it;
        if (seq_reader_pl_open(&it, step) <= 0) {
            continue;
        }
        uint8_t id = 0U;
        uint8_t value = 0U;
        uint8_t flags = 0U;
        while (seq_reader_pl_next(&it, &id, &value, &flags) != 0) {
            if ((flags & SEQ_READER_PL_FLAG_DOMAIN_CART) != 0U) {
                return true;
            }
            if (pl_is_cart(id)) {
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

    seq_plock_pool_reset();
    populate_track(&original);

    assert(seq_project_track_steps_encode(&original, buffer, sizeof(buffer), &written));
    assert(written > sizeof(uint16_t));

    assert(seq_project_track_steps_decode(&decoded_full, buffer, written,
                                            SEQ_PROJECT_PATTERN_VERSION,
                                            SEQ_PROJECT_TRACK_DECODE_FULL));
    assert(track_plocks_equal(&original, &decoded_full));

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
