#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/seq/reader/seq_reader.h"
#include "core/seq/seq_runtime.h"
#include "core/seq/seq_model.h"
#include "core/seq/seq_project.h"
#include "cart/cart_registry.h"

bool board_flash_init(void) { return true; }
bool board_flash_is_ready(void) { return true; }
uint32_t board_flash_get_capacity(void) { return BOARD_FLASH_CAPACITY_BYTES; }
uint32_t board_flash_get_sector_size(void) { return BOARD_FLASH_SECTOR_SIZE; }
bool board_flash_read(uint32_t address, void *buffer, size_t length) {
    (void)address;
    if ((buffer != NULL) && (length > 0U)) {
        memset(buffer, 0, length);
    }
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
bool cart_registry_find_by_uid(uint32_t uid, cart_id_t *out_id) {
    (void)uid;
    if (out_id != NULL) {
        *out_id = CART1;
    }
    return false;
}

static void prepare_track(void) {
    seq_runtime_init();

    seq_model_track_t *track = seq_runtime_access_track_mut(0U);
    assert(track != NULL);

    seq_model_step_t *step = &track->steps[0U];
    seq_model_step_init(step);

    seq_model_voice_t primary = step->voices[0];
    primary.note = 64U;
    primary.velocity = 100U;
    primary.length = 12U;
    primary.micro_offset = -2;
    primary.state = SEQ_MODEL_VOICE_ENABLED;
    assert(seq_model_step_set_voice(step, 0U, &primary));

    seq_model_plock_t plock = {
        .domain = SEQ_MODEL_PLOCK_INTERNAL,
        .voice_index = 0U,
        .parameter_id = 0U,
        .value = 42,
        .internal_param = SEQ_MODEL_PLOCK_PARAM_NOTE,
    };
    assert(seq_model_step_add_plock(step, &plock));
}

static void test_reader_get_step(void) {
    prepare_track();

    seq_track_handle_t handle = {
        .bank = 0U,
        .pattern = 0U,
        .track = 0U,
    };

    seq_step_view_t view;
    const bool ok = seq_reader_get_step(handle, 0U, &view);
    assert(ok);
    assert(view.note == 64U);
    assert(view.vel == 100U);
    assert(view.length == 12U);
    assert(view.micro == -2);
}

static void test_reader_plock_iter(void) {
    prepare_track();

    seq_track_handle_t handle = {
        .bank = 0U,
        .pattern = 0U,
        .track = 0U,
    };

    seq_plock_iter_t it;
    assert(seq_reader_plock_iter_open(handle, 0U, &it));

    uint16_t param_id = 0U;
    int32_t value = 0;
    assert(seq_reader_plock_iter_next(&it, &param_id, &value));
    assert(param_id != 0U);
    assert(value == 42);
    assert(!seq_reader_plock_iter_next(&it, &param_id, &value));
}

static void test_invalid_handle(void) {
    prepare_track();

    seq_track_handle_t handle = {
        .bank = 1U,
        .pattern = 0U,
        .track = 0U,
    };

    seq_step_view_t view;
    memset(&view, 0xAA, sizeof(view));
    assert(!seq_reader_get_step(handle, 0U, &view));
    for (size_t i = 0; i < sizeof(view); ++i) {
        assert(((const uint8_t *)&view)[i] == 0U);
    }

    seq_plock_iter_t it;
    assert(!seq_reader_plock_iter_open(handle, 0U, &it));
}

int main(void) {
    test_reader_get_step();
    test_reader_plock_iter();
    test_invalid_handle();

    printf("seq_reader_tests: OK\n");
    return 0;
}
