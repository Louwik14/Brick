Rapport codex:

"Added host-side ChibiOS stubs and new regression cases in tests/seq_hold_runtime_tests.c to exercise seq_engine_process_step() and verify quick-step NOTE_OFF guards plus orphan NOTE_OFF suppression during live capture.

Extended the ChibiOS stub header with semaphore, thread, and macro definitions required to link the sequencer engine inside host tests.

Wired core/seq/seq_engine.c into the host test build and documented the additional quick-step/live capture coverage in the README and architecture guide for visibility.
"
patch:
" (cd "$(git rev-parse --show-toplevel)" && git apply --3way <<'EOF' 
diff --git a/apps/seq_engine_runner.c b/apps/seq_engine_runner.c
index 90cea09793ce784205aebdf916d1beb4f634f1d8..e1be82872a2f286fe75caa91f43ed89d4de5a544 100644
--- a/apps/seq_engine_runner.c
+++ b/apps/seq_engine_runner.c
@@ -1,143 +1,213 @@
 /**
  * @file seq_engine_runner.c
  * @brief Bridge between the sequencer engine and runtime I/O backends.
  */
 
 #include "seq_engine_runner.h"
 
 #include <stdint.h>
 #include <string.h>
 
 #include "brick_config.h"
 #include "cart_link.h"
 #include "cart_registry.h"
 #include "ch.h"
 #include "midi.h"
 #include "seq_engine.h"
+#include "seq_led_bridge.h"
 #include "ui_mute_backend.h"
 
 #ifdef BRICK_DEBUG_PLOCK
 #include "chprintf.h"
 #ifndef BRICK_DEBUG_PLOCK_STREAM
 #define BRICK_DEBUG_PLOCK_STREAM ((BaseSequentialStream *)NULL)
 #endif
 #define BRICK_DEBUG_PLOCK_LOG(tag, param, value, time) \
     do { \
         if (BRICK_DEBUG_PLOCK_STREAM != NULL) { \
             chprintf(BRICK_DEBUG_PLOCK_STREAM, "[PLOCK][%s] param=%u value=%u t=%lu\r\n", \
                      (tag), (unsigned)(param), (unsigned)(value), (unsigned long)(time)); \
         } \
     } while (0)
 #else
 #define BRICK_DEBUG_PLOCK_LOG(tag, param, value, time) \
     do { (void)(tag); (void)(param); (void)(value); (void)(time); } while (0)
 #endif
 
 #ifndef SEQ_ENGINE_RUNNER_MIDI_CHANNEL
 #define SEQ_ENGINE_RUNNER_MIDI_CHANNEL 0U
 #endif
 
 #ifndef SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS
 #define SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS 24U
 #endif
 
-static CCM_DATA seq_engine_t s_engine;
+#ifndef SEQ_ENGINE_RUNNER_TRACK_CAPACITY
+#define SEQ_ENGINE_RUNNER_TRACK_CAPACITY SEQ_PROJECT_MAX_TRACKS
+#endif
+
+typedef struct {
+    bool active;
+    uint8_t track;
+    seq_model_pattern_t *pattern;
+    seq_engine_t engine;
+} seq_engine_runner_track_ctx_t;
 
 typedef struct {
     bool active;
     cart_id_t cart;
     uint16_t param_id;
     uint8_t depth;
     uint8_t previous;
 } seq_engine_runner_plock_state_t;
 
 static CCM_DATA seq_engine_runner_plock_state_t s_plock_state[SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS];
+static CCM_DATA seq_engine_runner_track_ctx_t s_tracks[SEQ_ENGINE_RUNNER_TRACK_CAPACITY];
+static seq_engine_runner_track_ctx_t *s_current_track_ctx;
+static seq_engine_callbacks_t s_runner_callbacks;
 
 static msg_t _runner_note_on_cb(const seq_engine_note_on_t *note_on, systime_t scheduled_time);
 static msg_t _runner_note_off_cb(const seq_engine_note_off_t *note_off, systime_t scheduled_time);
 static msg_t _runner_plock_cb(const seq_engine_plock_t *plock, systime_t scheduled_time);
 static bool _runner_track_muted(uint8_t track);
 static uint8_t _runner_clamp_u8(int16_t value);
 static seq_engine_runner_plock_state_t *_runner_plock_find(cart_id_t cart, uint16_t param_id);
 static seq_engine_runner_plock_state_t *_runner_plock_acquire(cart_id_t cart, uint16_t param_id);
 static void _runner_plock_release(seq_engine_runner_plock_state_t *slot);
+static seq_engine_runner_track_ctx_t *_runner_find_track_ctx(uint8_t track);
+static seq_engine_runner_track_ctx_t *_runner_acquire_track_ctx(uint8_t track);
+static uint8_t _runner_resolve_track_index(seq_model_pattern_t *pattern);
 
 void seq_engine_runner_init(seq_model_pattern_t *pattern) {
-    seq_engine_callbacks_t callbacks = {
-        .note_on = _runner_note_on_cb,
-        .note_off = _runner_note_off_cb,
-        .plock = _runner_plock_cb
-    };
-
-    seq_engine_config_t config = {
-        .pattern = pattern,
-        .callbacks = callbacks,
-        .is_track_muted = _runner_track_muted
-    };
-
-    seq_engine_init(&s_engine, &config);
+    s_runner_callbacks.note_on = _runner_note_on_cb;
+    s_runner_callbacks.note_off = _runner_note_off_cb;
+    s_runner_callbacks.plock = _runner_plock_cb;
+
+    memset(s_tracks, 0, sizeof(s_tracks));
     memset(s_plock_state, 0, sizeof(s_plock_state));
+    s_current_track_ctx = NULL;
+
+    seq_project_t *project = seq_led_bridge_get_project();
+    if (project != NULL) {
+        const uint8_t count = seq_project_get_track_count(project);
+        for (uint8_t t = 0U; t < count; ++t) {
+            seq_model_pattern_t *track_pattern = seq_project_get_track(project, t);
+            if (track_pattern != NULL) {
+                seq_engine_runner_attach_pattern(track_pattern);
+            }
+        }
+    } else if (pattern != NULL) {
+        seq_engine_runner_attach_pattern(pattern);
+    }
 }
 
 void seq_engine_runner_attach_pattern(seq_model_pattern_t *pattern) {
-    seq_engine_attach_pattern(&s_engine, pattern);
+    if (pattern == NULL) {
+        return;
+    }
+
+    uint8_t track = _runner_resolve_track_index(pattern);
+    if (track >= SEQ_ENGINE_RUNNER_TRACK_CAPACITY) {
+        if (seq_led_bridge_get_project_const() != NULL) {
+            return;
+        }
+        track = 0U;
+    }
+
+    seq_engine_runner_track_ctx_t *ctx = _runner_find_track_ctx(track);
+    if (ctx == NULL) {
+        ctx = _runner_acquire_track_ctx(track);
+        if (ctx == NULL) {
+            return;
+        }
+        seq_engine_config_t config = {
+            .pattern = pattern,
+            .callbacks = s_runner_callbacks,
+            .is_track_muted = _runner_track_muted
+        };
+        seq_engine_init(&ctx->engine, &config);
+    } else {
+        seq_engine_attach_pattern(&ctx->engine, pattern);
+    }
+
+    ctx->pattern = pattern;
 }
 
 void seq_engine_runner_on_transport_play(void) {
-    (void)seq_engine_start(&s_engine);
+    for (size_t i = 0U; i < SEQ_ENGINE_RUNNER_TRACK_CAPACITY; ++i) {
+        seq_engine_runner_track_ctx_t *ctx = &s_tracks[i];
+        if (!ctx->active || (ctx->pattern == NULL)) {
+            continue;
+        }
+        (void)seq_engine_start(&ctx->engine);
+    }
 }
 
 void seq_engine_runner_on_transport_stop(void) {
-    seq_engine_stop(&s_engine);
+    for (size_t i = 0U; i < SEQ_ENGINE_RUNNER_TRACK_CAPACITY; ++i) {
+        seq_engine_runner_track_ctx_t *ctx = &s_tracks[i];
+        if (!ctx->active || (ctx->pattern == NULL)) {
+            continue;
+        }
+        seq_engine_stop(&ctx->engine);
+    }
     cart_id_t cart = cart_registry_get_active_id();
     if (cart < CART_COUNT) {
         for (size_t i = 0U; i < SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS; ++i) {
             seq_engine_runner_plock_state_t *slot = &s_plock_state[i];
             if (!slot->active) {
                 continue;
             }
             if (slot->cart == cart) {
                 BRICK_DEBUG_PLOCK_LOG("RUNNER_PLOCK_RESTORE", slot->param_id, slot->previous, chVTGetSystemTimeX());
                 cart_link_param_changed(slot->param_id, slot->previous, false, 0U);
             }
             _runner_plock_release(slot);
         }
     } else {
         for (size_t i = 0U; i < SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS; ++i) {
             _runner_plock_release(&s_plock_state[i]);
         }
     }
 
     // --- FIX: STOP doit forcer un All Notes Off immédiat sur tous les canaux actifs ---
     for (uint8_t ch = 0U; ch < 16U; ++ch) {
         midi_all_notes_off(MIDI_DEST_BOTH, ch);
     }
 }
 
 void seq_engine_runner_on_clock_step(const clock_step_info_t *info) {
-    seq_engine_process_step(&s_engine, info);
+    for (size_t i = 0U; i < SEQ_ENGINE_RUNNER_TRACK_CAPACITY; ++i) {
+        seq_engine_runner_track_ctx_t *ctx = &s_tracks[i];
+        if (!ctx->active || (ctx->pattern == NULL)) {
+            continue;
+        }
+        s_current_track_ctx = ctx;
+        seq_engine_process_step(&ctx->engine, info);
+    }
+    s_current_track_ctx = NULL;
 }
 
 static msg_t _runner_note_on_cb(const seq_engine_note_on_t *note_on, systime_t scheduled_time) {
     (void)scheduled_time;
     if (note_on == NULL) {
         return MSG_OK;
     }
     midi_note_on(MIDI_DEST_BOTH, SEQ_ENGINE_RUNNER_MIDI_CHANNEL, note_on->note, note_on->velocity);
     return MSG_OK;
 }
 
 static msg_t _runner_note_off_cb(const seq_engine_note_off_t *note_off, systime_t scheduled_time) {
     (void)scheduled_time;
     if (note_off == NULL) {
         return MSG_OK;
     }
     midi_note_off(MIDI_DEST_BOTH, SEQ_ENGINE_RUNNER_MIDI_CHANNEL, note_off->note, 0U);
     return MSG_OK;
 }
 
 static msg_t _runner_plock_cb(const seq_engine_plock_t *plock, systime_t scheduled_time) {
     if ((plock == NULL) || (plock->plock.domain != SEQ_MODEL_PLOCK_CART)) {
         return MSG_OK;
     }
 
@@ -162,51 +232,101 @@ static msg_t _runner_plock_cb(const seq_engine_plock_t *plock, systime_t schedul
         }
         BRICK_DEBUG_PLOCK_LOG("RUNNER_PLOCK_APPLY", payload->parameter_id, value, scheduled_time);
         cart_link_param_changed(payload->parameter_id, value, false, 0U);
         break;
     }
     case SEQ_ENGINE_PLOCK_RESTORE: {
         seq_engine_runner_plock_state_t *slot = _runner_plock_find(cart, payload->parameter_id);
         if ((slot != NULL) && (slot->depth > 0U)) {
             slot->depth--;
             if (slot->depth == 0U) {
                 BRICK_DEBUG_PLOCK_LOG("RUNNER_PLOCK_RESTORE", payload->parameter_id, slot->previous, scheduled_time);
                 cart_link_param_changed(payload->parameter_id, slot->previous, false, 0U);
                 _runner_plock_release(slot);
             }
         }
         break;
     }
     default:
         break;
     }
 
     return MSG_OK;
 }
 
 static bool _runner_track_muted(uint8_t track) {
-    return ui_mute_backend_is_muted(track);
+    (void)track;
+    if (s_current_track_ctx == NULL) {
+        return false;
+    }
+    return ui_mute_backend_is_muted(s_current_track_ctx->track);
+}
+
+static seq_engine_runner_track_ctx_t *_runner_find_track_ctx(uint8_t track) {
+    for (size_t i = 0U; i < SEQ_ENGINE_RUNNER_TRACK_CAPACITY; ++i) {
+        seq_engine_runner_track_ctx_t *ctx = &s_tracks[i];
+        if (ctx->active && (ctx->track == track)) {
+            return ctx;
+        }
+    }
+    return NULL;
+}
+
+static seq_engine_runner_track_ctx_t *_runner_acquire_track_ctx(uint8_t track) {
+    seq_engine_runner_track_ctx_t *ctx = _runner_find_track_ctx(track);
+    if (ctx != NULL) {
+        return ctx;
+    }
+
+    for (size_t i = 0U; i < SEQ_ENGINE_RUNNER_TRACK_CAPACITY; ++i) {
+        ctx = &s_tracks[i];
+        if (!ctx->active) {
+            ctx->active = true;
+            ctx->track = track;
+            ctx->pattern = NULL;
+            memset(&ctx->engine, 0, sizeof(ctx->engine));
+            return ctx;
+        }
+    }
+
+    return NULL;
+}
+
+static uint8_t _runner_resolve_track_index(seq_model_pattern_t *pattern) {
+    const seq_project_t *project = seq_led_bridge_get_project_const();
+    if ((project == NULL) || (pattern == NULL)) {
+        return SEQ_ENGINE_RUNNER_TRACK_CAPACITY;
+    }
+
+    const uint8_t count = seq_project_get_track_count(project);
+    for (uint8_t t = 0U; t < count; ++t) {
+        if (seq_project_get_track_const(project, t) == pattern) {
+            return t;
+        }
+    }
+
+    return SEQ_ENGINE_RUNNER_TRACK_CAPACITY;
 }
 
 static uint8_t _runner_clamp_u8(int16_t value) {
     if (value < 0) {
         return 0U;
     }
     if (value > 127) {
         return 127U;
     }
     return (uint8_t)value;
 }
 
 static seq_engine_runner_plock_state_t *_runner_plock_find(cart_id_t cart, uint16_t param_id) {
     for (size_t i = 0U; i < SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS; ++i) {
         seq_engine_runner_plock_state_t *slot = &s_plock_state[i];
         if (slot->active && (slot->cart == cart) && (slot->param_id == param_id)) {
             return slot;
         }
     }
     return NULL;
 }
 
 static seq_engine_runner_plock_state_t *_runner_plock_acquire(cart_id_t cart, uint16_t param_id) {
     seq_engine_runner_plock_state_t *slot = _runner_plock_find(cart, param_id);
     if (slot != NULL) {
 
EOF
)"

" (cd "$(git rev-parse --show-toplevel)" && git apply --3way <<'EOF' 
diff --git a/core/seq/seq_engine.c b/core/seq/seq_engine.c
index e9bebef12674adea5ce591334d76853b33522f62..ce2f53cb9fa5555750296b58ba14dd8aafc7df76 100644
--- a/core/seq/seq_engine.c
+++ b/core/seq/seq_engine.c
@@ -603,51 +603,60 @@ static void _seq_engine_handle_step(seq_engine_t *engine,
                 if (voice_index < SEQ_MODEL_VOICES_PER_STEP) {
                     note_value += cfg->transpose.per_voice[voice_index];
                 }
             }
             if (note_value < 0) {
                 note_value = 0;
             } else if (note_value > 127) {
                 note_value = 127;
             }
             uint8_t note = (uint8_t)note_value;
 
             if (cfg != NULL) {
                 note = _seq_engine_apply_scale(note, &cfg->scale);
             }
 
             int32_t micro = (int32_t)voice->micro_offset + (int32_t)offsets->micro;
             if (micro < -SEQ_ENGINE_MICRO_MAX) {
                 micro = -SEQ_ENGINE_MICRO_MAX;
             } else if (micro > SEQ_ENGINE_MICRO_MAX) {
                 micro = SEQ_ENGINE_MICRO_MAX;
             }
 
             int64_t t_on_raw = (int64_t)step_start + _seq_engine_micro_to_delta(step_duration, micro);
             const systime_t note_on_time = _seq_engine_saturate_time(t_on_raw);
             int64_t t_off_raw = t_on_raw + ((int64_t)step_duration * (int64_t)length_steps);
-            const systime_t note_off_time = _seq_engine_saturate_time(t_off_raw);
+            systime_t note_off_time = _seq_engine_saturate_time(t_off_raw);
+            if (note_off_time <= note_on_time) {
+                const int64_t guard = (int64_t)note_on_time + 1;
+                const systime_t bumped = _seq_engine_saturate_time(guard);
+                if (bumped > note_on_time) {
+                    note_off_time = bumped;
+                } else if ((systime_t)(note_on_time + 1U) > note_on_time) {
+                    note_off_time = (systime_t)(note_on_time + 1U);
+                }
+            }
 
             seq_engine_event_t ev;
             memset(&ev, 0, sizeof(ev));
             ev.type = SEQ_ENGINE_EVENT_NOTE_ON;
             ev.scheduled_time = note_on_time;
             ev.data.note_on.voice = voice_index;
             ev.data.note_on.note = note;
             ev.data.note_on.velocity = (uint8_t)velocity;
             note_events[note_event_count++] = ev;
             BRICK_DEBUG_PLOCK_LOG("ENGINE_SCHED_NOTE_ON",
                                   (uint16_t)(((uint16_t)voice_index << 8) | note),
                                   velocity,
                                   note_on_time);
 
             memset(&ev, 0, sizeof(ev));
             ev.type = SEQ_ENGINE_EVENT_NOTE_OFF;
             ev.scheduled_time = note_off_time;
             ev.data.note_off.voice = voice_index;
             ev.data.note_off.note = note;
             note_events[note_event_count++] = ev;
             BRICK_DEBUG_PLOCK_LOG("ENGINE_SCHED_NOTE_OFF",
                                   (uint16_t)(((uint16_t)voice_index << 8) | note),
                                   0,
                                   note_off_time);
 
 
EOF
)"

