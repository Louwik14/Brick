#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "apps/seq_engine_runner.h"
#include "cart/cart_bus.h"
#include "cart/cart_registry.h"
#include "core/seq/seq_model.h"
#if SEQ_FEATURE_PLOCK_POOL
#include "core/seq/seq_plock_pool.h"
#endif
#include "core/seq/seq_plock_ids.h"
#include "core/seq/seq_project.h"
#include "core/seq/seq_runtime.h"

/* -------------------------------------------------------------------------- */
/* Stubs                                                                      */
/* -------------------------------------------------------------------------- */

typedef enum {
    LOG_CART,
    LOG_NOTE_ON,
    LOG_NOTE_OFF,
    LOG_ALL_NOTES_OFF
} log_type_t;

typedef struct {
    log_type_t type;
    uint8_t a;
    uint8_t b;
} log_entry_t;

static log_entry_t g_log[32];
static size_t g_log_count = 0U;
static uint8_t g_cart_shadow[512];
static uint8_t g_active_bank = 0U;
static uint8_t g_active_pattern = 0U;

static void log_event(log_type_t type, uint8_t a, uint8_t b) {
    if (g_log_count >= (sizeof(g_log) / sizeof(g_log[0]))) {
        return;
    }
    g_log[g_log_count++] = (log_entry_t){ type, a, b };
}

void midi_probe_reset(void) {
    g_log_count = 0U;
}

void midi_probe_tick_begin(uint32_t tick) {
    (void)tick;
}

void midi_probe_tick_end(void) {}

void midi_probe_log(uint32_t tick, uint8_t ch, uint8_t note, uint8_t vel, uint8_t ty) {
    (void)tick;
    (void)ch;
    switch (ty) {
        case 1U:
            log_event(LOG_NOTE_ON, note, vel);
            break;
        case 2U:
            log_event(LOG_NOTE_OFF, note, vel);
            break;
        case 3U:
            log_event(LOG_ALL_NOTES_OFF, note, vel);
            break;
        default:
            break;
    }
}

void midi_tx3(uint8_t b0, uint8_t b1, uint8_t b2) {
    (void)b0;
    (void)b1;
    (void)b2;
}

void seq_led_bridge_get_active(uint8_t *out_bank, uint8_t *out_pattern) {
    if (out_bank != NULL) {
        *out_bank = g_active_bank;
    }
    if (out_pattern != NULL) {
        *out_pattern = g_active_pattern;
    }
}

void seq_led_bridge_set_active(uint8_t bank, uint8_t pattern) {
    g_active_bank = bank;
    g_active_pattern = pattern;
}

bool ui_mute_backend_is_muted(uint8_t track) {
    (void)track;
    return false;
}

void cart_link_param_changed(uint16_t param_id, uint8_t value, bool is_bitwise, uint8_t bit_mask) {
    (void)is_bitwise;
    (void)bit_mask;
    if (param_id < sizeof(g_cart_shadow)) {
        g_cart_shadow[param_id] = value;
    }
    log_event(LOG_CART, (uint8_t)param_id, value);
}

uint8_t cart_link_shadow_get(cart_id_t cid, uint16_t param_id) {
    (void)cid;
    if (param_id < sizeof(g_cart_shadow)) {
        return g_cart_shadow[param_id];
    }
    return 0U;
}

void cart_link_shadow_set(cart_id_t cid, uint16_t param_id, uint8_t value) {
    (void)cid;
    if (param_id < sizeof(g_cart_shadow)) {
        g_cart_shadow[param_id] = value;
    }
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

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

static void reset_log(void) {
    g_log_count = 0U;
    memset(g_log, 0, sizeof(g_log));
}

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

static void neutralise_track(seq_model_track_t *track) {
    for (uint8_t step = 0U; step < SEQ_MODEL_STEPS_PER_TRACK; ++step) {
        seq_model_step_make_neutral(&track->steps[step]);
        for (uint8_t voice = 0U; voice < SEQ_MODEL_VOICES_PER_STEP; ++voice) {
            track->steps[step].voices[voice].velocity = 0U;
            track->steps[step].voices[voice].state = SEQ_MODEL_VOICE_DISABLED;
        }
        seq_model_step_recompute_flags(&track->steps[step]);
    }
}

static void configure_voice_step(seq_model_step_t *step) {
    seq_model_step_make_neutral(step);
    seq_model_voice_t *voice = &step->voices[0];
    voice->note = 60U;
    voice->velocity = 64U;
    voice->length = 2U;
    voice->state = SEQ_MODEL_VOICE_ENABLED;

#if SEQ_FEATURE_PLOCK_POOL
    step->pl_ref.count = 0U;
    step->plock_count = 0U;
    const uint8_t ids[] = {
        PL_INT_ALL_TRANSP,
        PL_INT_ALL_VEL,
        PL_INT_ALL_LEN,
        PL_INT_VEL_V0,
        (uint8_t)(0x40U + 7U)
    };
    const uint8_t values[] = {
        pl_u8_from_s8(2),
        pl_u8_from_s8(-20),
        pl_u8_from_s8(2),
        90U,
        55U
    };
    const uint16_t count = (uint16_t)(sizeof(ids) / sizeof(ids[0]));
    uint16_t offset = 0U;
    int ok = seq_plock_pool_alloc(count, &offset);
    assert(ok == 0);
    for (uint16_t i = 0U; i < count; ++i) {
        seq_plock_entry_t *entry = seq_plock_pool_get(offset, i);
        assert(entry != NULL);
        entry->param_id = ids[i];
        entry->value = values[i];
        entry->flags = 0U;
    }
    step->pl_ref.offset = offset;
    step->pl_ref.count = (uint8_t)count;
#else
    step->plock_count = 5U;
    step->plocks[0].domain = SEQ_MODEL_PLOCK_INTERNAL;
    step->plocks[0].internal_param = SEQ_MODEL_PLOCK_PARAM_GLOBAL_TR;
    step->plocks[0].value = 2;

    step->plocks[1].domain = SEQ_MODEL_PLOCK_INTERNAL;
    step->plocks[1].internal_param = SEQ_MODEL_PLOCK_PARAM_GLOBAL_VE;
    step->plocks[1].value = -20;

    step->plocks[2].domain = SEQ_MODEL_PLOCK_INTERNAL;
    step->plocks[2].internal_param = SEQ_MODEL_PLOCK_PARAM_GLOBAL_LE;
    step->plocks[2].value = 2;

    step->plocks[3].domain = SEQ_MODEL_PLOCK_INTERNAL;
    step->plocks[3].internal_param = SEQ_MODEL_PLOCK_PARAM_VELOCITY;
    step->plocks[3].voice_index = 0U;
    step->plocks[3].value = 90;

    step->plocks[4].domain = SEQ_MODEL_PLOCK_CART;
    step->plocks[4].parameter_id = 7U;
    step->plocks[4].value = 55;
#endif

    seq_model_step_recompute_flags(step);
}

static void configure_automation_step(seq_model_step_t *step) {
    seq_model_step_make_neutral(step);
    seq_model_step_make_automation_only(step);
#if SEQ_FEATURE_PLOCK_POOL
    step->plock_count = 0U;
    uint16_t offset = 0U;
    int ok = seq_plock_pool_alloc(1U, &offset);
    assert(ok == 0);
    seq_plock_entry_t *entry = seq_plock_pool_get(offset, 0U);
    assert(entry != NULL);
    entry->param_id = (uint8_t)(0x40U + 3U);
    entry->value = 99U;
    entry->flags = 0U;
    step->pl_ref.offset = offset;
    step->pl_ref.count = 1U;
#else
    step->plock_count = 1U;
    step->plocks[0].domain = SEQ_MODEL_PLOCK_CART;
    step->plocks[0].parameter_id = 3U;
    step->plocks[0].value = 99;
#endif
    seq_model_step_recompute_flags(step);
}

/* -------------------------------------------------------------------------- */
/* Test                                                                        */
/* -------------------------------------------------------------------------- */

int main(void) {
    memset(g_cart_shadow, 0, sizeof(g_cart_shadow));

    seq_runtime_init();
    seq_project_t *project = seq_runtime_access_project_mut();
    assert(project != NULL);
    (void)seq_project_set_active_slot(project, 0U, 0U);
    (void)seq_project_set_active_track(project, 0U);

    seq_model_track_t *track = seq_runtime_access_track_mut(0U);
    assert(track != NULL);

    neutralise_track(track);

#if SEQ_FEATURE_PLOCK_POOL
    seq_plock_pool_reset();
#endif

    configure_voice_step(&track->steps[0]);
    configure_automation_step(&track->steps[1]);
    seq_model_gen_bump(&track->generation);

    seq_led_bridge_set_active(0U, 0U);

    seq_engine_runner_init();

    /* Tick 0: voice step with both MIDI and cart p-locks. */
    reset_log();
    clock_step_info_t info0 = make_tick(0U);
    seq_engine_runner_on_clock_step(&info0);
    assert(g_log_count >= 2U);
    assert(g_log[0].type == LOG_CART);
    assert(g_log[0].a == 7U);
    assert(g_log[0].b == 55U);
    assert(g_log[1].type == LOG_NOTE_ON);
    /* Base velocity 90 with all offset -20 -> 70. Base note 60 + transpose 2 -> 62. */
    assert(g_log[1].a == 62U);
    assert(g_log[1].b == 70U);

    /* Tick 1: automation-only step should emit cart p-lock without NOTE_ON. */
    reset_log();
    clock_step_info_t info1 = make_tick(1U);
    seq_engine_runner_on_clock_step(&info1);
    assert(g_log_count >= 1U);
    for (size_t i = 0U; i < g_log_count; ++i) {
        assert(g_log[i].type == LOG_CART);
    }
    assert(g_log[g_log_count - 1U].a == 3U);

    /* Intermediate ticks (2,3) carry no events. */
    reset_log();
    clock_step_info_t info2 = make_tick(2U);
    seq_engine_runner_on_clock_step(&info2);
    assert(g_log_count == 1U);
    assert(g_log[0].type == LOG_CART);
    assert(g_log[0].a == 3U);

    reset_log();
    clock_step_info_t info3 = make_tick(3U);
    seq_engine_runner_on_clock_step(&info3);
    assert(g_log_count == 0U);

    /* Tick 4: NOTE_OFF triggered after length=2 with +2 offset -> off at step 4. */
    reset_log();
    clock_step_info_t info4 = make_tick(4U);
    seq_engine_runner_on_clock_step(&info4);
    assert(g_log_count == 1U);
    assert(g_log[0].type == LOG_NOTE_OFF);
    assert(g_log[0].a == 62U);

    return 0;
}
