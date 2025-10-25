#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "apps/seq_engine_runner.h"
#include "cart/cart_registry.h"
#include "core/seq/seq_midi_routing.h"
#include "core/seq/seq_model.h"
#include "core/seq/seq_project.h"
#include "core/seq/seq_runtime.h"
#include "tests/runtime_compat.h"

/* -------------------------------------------------------------------------- */
/* MIDI logging                                                               */
/* -------------------------------------------------------------------------- */

typedef enum {
    MIDI_PORT_USB = 0,
    MIDI_PORT_DIN = 1,
} midi_port_t;

typedef enum {
    MIDI_MSG_NOTE_ON,
    MIDI_MSG_NOTE_OFF,
    MIDI_MSG_CC,
    MIDI_MSG_CHANNEL_PRESSURE,
    MIDI_MSG_PITCH_BEND,
    MIDI_MSG_POLY_AFTERTOUCH,
} midi_msg_t;

typedef struct {
    midi_port_t port;
    midi_msg_t type;
    uint8_t channel;
    uint8_t data1;
    uint8_t data2;
} midi_log_entry_t;

static midi_log_entry_t g_midi_log[256];
static size_t g_midi_log_count = 0U;

static void midi_log_reset(void) {
    g_midi_log_count = 0U;
    memset(g_midi_log, 0, sizeof(g_midi_log));
}

static void midi_log_append(midi_port_t port, midi_msg_t type, uint8_t ch0,
                            uint8_t data1, uint8_t data2) {
    if (g_midi_log_count >= (sizeof(g_midi_log) / sizeof(g_midi_log[0]))) {
        return;
    }
    midi_log_entry_t *entry = &g_midi_log[g_midi_log_count++];
    entry->port = port;
    entry->type = type;
    entry->channel = (uint8_t)(ch0 + 1U);
    entry->data1 = data1;
    entry->data2 = data2;
}

static void midi_log_channel_message(midi_msg_t type, uint8_t ch0,
                                     uint8_t data1, uint8_t data2) {
    midi_log_append(MIDI_PORT_USB, type, ch0, data1, data2);
    midi_log_append(MIDI_PORT_DIN, type, ch0, data1, data2);
}

static size_t midi_log_count(midi_port_t port, midi_msg_t type, uint8_t channel) {
    size_t count = 0U;
    for (size_t i = 0U; i < g_midi_log_count; ++i) {
        const midi_log_entry_t *entry = &g_midi_log[i];
        if ((entry->port == port) && (entry->type == type) && (entry->channel == channel)) {
            ++count;
        }
    }
    return count;
}

static size_t midi_log_count_cc123(midi_port_t port, uint8_t channel) {
    size_t count = 0U;
    for (size_t i = 0U; i < g_midi_log_count; ++i) {
        const midi_log_entry_t *entry = &g_midi_log[i];
        if ((entry->port == port) && (entry->type == MIDI_MSG_CC) &&
            (entry->channel == channel) && (entry->data1 == 123U) &&
            (entry->data2 == 0U)) {
            ++count;
        }
    }
    return count;
}

/* -------------------------------------------------------------------------- */
/* Stubs                                                                      */
/* -------------------------------------------------------------------------- */

void midi_probe_tick_begin(uint32_t tick) {
    (void)tick;
}

void midi_probe_tick_end(void) {}

void midi_probe_log(uint32_t tick, uint8_t ch, uint8_t note, uint8_t vel, uint8_t ty) {
    (void)tick;
    (void)ch;
    (void)note;
    (void)vel;
    (void)ty;
}

static uint8_t g_stub_active_bank = 0U;
static uint8_t g_stub_active_pattern = 0U;

void seq_led_bridge_set_active(uint8_t bank, uint8_t pattern) {
    g_stub_active_bank = bank;
    g_stub_active_pattern = pattern;
    seq_project_t *project = seq_runtime_compat_access_project_mut();
    if (project != NULL) {
        (void)seq_project_set_active_slot(project, bank, pattern);
    }
}

void seq_led_bridge_get_active(uint8_t *out_bank, uint8_t *out_pattern) {
    if (out_bank != NULL) {
        *out_bank = g_stub_active_bank;
    }
    if (out_pattern != NULL) {
        *out_pattern = g_stub_active_pattern;
    }
}

bool ui_mute_backend_is_muted(uint8_t track) {
    (void)track;
    return false;
}

void cart_link_param_changed(uint16_t param_id, uint8_t value, bool is_bitwise, uint8_t bit_mask) {
    (void)param_id;
    (void)value;
    (void)is_bitwise;
    (void)bit_mask;
}

uint8_t cart_link_shadow_get(cart_id_t cid, uint16_t param_id) {
    (void)cid;
    (void)param_id;
    return 0U;
}

void cart_link_shadow_set(cart_id_t cid, uint16_t param_id, uint8_t value) {
    (void)cid;
    (void)param_id;
    (void)value;
}

bool cart_set_param(cart_id_t id, uint16_t param, uint8_t value) {
    (void)id;
    (void)param;
    (void)value;
    return true;
}

cart_id_t cart_registry_get_active_id(void) {
    return CART1;
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
    if (out_id != NULL) {
        *out_id = CART1;
    }
    (void)uid;
    return false;
}

void midi_tx3(uint8_t b0, uint8_t b1, uint8_t b2) {
    const uint8_t status = b0 & 0xF0U;
    const uint8_t channel = b0 & 0x0FU;

    switch (status) {
        case 0x90U:
            if (b2 != 0U) {
                midi_log_channel_message(MIDI_MSG_NOTE_ON, channel, b1, b2);
            } else {
                midi_log_channel_message(MIDI_MSG_NOTE_OFF, channel, b1, 64U);
            }
            break;
        case 0x80U:
            midi_log_channel_message(MIDI_MSG_NOTE_OFF, channel, b1, (b2 != 0U) ? b2 : 64U);
            break;
        case 0xB0U:
            midi_log_channel_message(MIDI_MSG_CC, channel, b1, b2);
            break;
        case 0xA0U:
            midi_log_channel_message(MIDI_MSG_POLY_AFTERTOUCH, channel, b1, b2);
            break;
        case 0xD0U:
            midi_log_channel_message(MIDI_MSG_CHANNEL_PRESSURE, channel, b1, b2);
            break;
        case 0xE0U:
            midi_log_channel_message(MIDI_MSG_PITCH_BEND, channel, b1, b2);
            break;
        default:
            break;
    }
}

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

static clock_step_info_t make_tick(uint32_t step_abs) {
    clock_step_info_t info = {
        .now = 0U,
        .step_idx_abs = step_abs,
        .bpm = 120.0f,
        .tick_st = 1U,
        .step_st = 6U,
        .ext_clock = false,
    };
    return info;
}

static void prepare_project(void) {
    seq_runtime_init();

    seq_project_t *project = seq_runtime_compat_access_project_mut();
    assert(project != NULL);
    (void)seq_project_set_active_slot(project, 0U, 0U);

    for (uint8_t track = 0U; track < 16U; ++track) {
        seq_model_track_t *model_track = seq_runtime_compat_access_track_mut(track);
        assert(model_track != NULL);

        for (uint8_t step = 0U; step < SEQ_MODEL_STEPS_PER_TRACK; ++step) {
            seq_model_step_make_neutral(&model_track->steps[step]);
        }

        seq_model_step_t *step0 = &model_track->steps[0];
        step0->voices[0].note = (uint8_t)(60U + track);
        step0->voices[0].velocity = SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY;
        step0->voices[0].length = 1U;
        step0->voices[0].state = SEQ_MODEL_VOICE_ENABLED;
        seq_model_step_recompute_flags(step0);
        seq_model_gen_bump(&model_track->generation);
    }

    seq_led_bridge_set_active(0U, 0U);
}

/* -------------------------------------------------------------------------- */
/* Test                                                                       */
/* -------------------------------------------------------------------------- */

int main(void) {
    assert(seq_midi_channel_for_track(0U) == 1U);
    assert(seq_midi_channel_for_track(15U) == 16U);
    assert(seq_midi_channel_for_track(63U) == 16U);

    midi_log_reset();
    prepare_project();
    seq_engine_runner_init();

    clock_step_info_t tick0 = make_tick(0U);
    seq_engine_runner_on_clock_step(&tick0);

    for (uint8_t track = 0U; track < 16U; ++track) {
        const uint8_t expected_channel = (uint8_t)(track + 1U);
        assert(midi_log_count(MIDI_PORT_USB, MIDI_MSG_NOTE_ON, expected_channel) == 1U);
        assert(midi_log_count(MIDI_PORT_DIN, MIDI_MSG_NOTE_ON, expected_channel) == 1U);
    }

    seq_engine_runner_on_transport_stop();

    for (uint8_t track = 0U; track < 16U; ++track) {
        const uint8_t expected_channel = (uint8_t)(track + 1U);
        assert(midi_log_count(MIDI_PORT_USB, MIDI_MSG_NOTE_OFF, expected_channel) >= 1U);
        assert(midi_log_count(MIDI_PORT_DIN, MIDI_MSG_NOTE_OFF, expected_channel) >= 1U);
        assert(midi_log_count_cc123(MIDI_PORT_USB, expected_channel) == 1U);
        assert(midi_log_count_cc123(MIDI_PORT_DIN, expected_channel) == 1U);
    }

    return 0;
}
